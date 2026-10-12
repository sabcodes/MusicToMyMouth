#include <AudioToolbox/AudioToolbox.h>
#include "../Source/PluginProcessor.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <numeric>
#include "NativeAU.h"

namespace {
using Buffer = juce::AudioBuffer<float>;
struct Host : juce::AudioPlayHead {
    int64_t sample = 0;
    double rate = 44100, bpm = 129.2;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;
        p.setTimeInSamples(sample);p.setTimeInSeconds(sample/rate);
        p.setBpm(bpm);p.setIsPlaying(playing);return p;
    }
};
struct Suite {
    juce::Array<juce::var> checks;
    int failures = 0;
    void expect(bool okay, const juce::String& name, juce::var details = {}) {
        auto* entry = new juce::DynamicObject;
        entry->setProperty("name", name);entry->setProperty("pass", okay);
        entry->setProperty("details", details);checks.add(juce::var(entry));
        if (!okay) ++failures;
        std::cout << (okay ? "PASS " : "FAIL ") << name << " " << juce::JSON::toString(details,true) << std::endl;
    }
};
void configure(MusicToMyMouthProcessor& p, double sr, int channels, int block) {
    auto layout = p.getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
    layout.outputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
    if (!p.setBusesLayout(layout)) throw std::runtime_error("Layout rejected");
    p.setRateAndBufferSizeDetails(sr,block);p.prepareToPlay(sr,block);
}
void neutral(MusicToMyMouthProcessor& p) {
    for (auto id : {"eqOn","compOn","deessOn","gateOn","satOn","reverbOn","delayOn","modOn","tuneOn"}) p.setParameterValue(id,0);
    for (auto id : {"input","output","formant","transpose"}) p.setParameterValue(id,0);
    p.setParameterValue("mix",100);
}
Buffer sine(double sr,double midi,double seconds,int channels=1) {
    Buffer b(channels,int(sr*seconds));double hz=440*std::pow(2.,(midi-69)/12.);
    for (int i=0;i<b.getNumSamples();++i) for(int ch=0;ch<channels;++ch)
        b.setSample(ch,i,float(.18*std::sin(juce::MathConstants<double>::twoPi*hz*i/sr)));
    return b;
}
struct Timings { std::vector<double> calls; };
Buffer render(MusicToMyMouthProcessor& p,const Buffer& source,Host& host,int block,bool bypass=false,Timings* timings=nullptr) {
    int latency=p.getLatencySamples(),length=source.getNumSamples()+latency;
    Buffer result(source.getNumChannels(),length);result.clear();juce::MidiBuffer midi;
    int64_t anchor=host.sample;
    for(int offset=0;offset<length;offset+=block) {
        int count=std::min(block,length-offset);Buffer b(source.getNumChannels(),count);b.clear();
        if(offset<source.getNumSamples())for(int ch=0;ch<b.getNumChannels();++ch)
            b.copyFrom(ch,0,source,ch,offset,std::min(count,source.getNumSamples()-offset));
        host.sample=anchor+offset;
        auto start=std::chrono::steady_clock::now();
        if(bypass)p.processBlockBypassed(b,midi);else p.processBlock(b,midi);
        if(timings)timings->calls.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());
        for(int ch=0;ch<b.getNumChannels();++ch)result.copyFrom(ch,offset,b,ch,0,count);
    }
    return result;
}
Buffer aligned(const Buffer& b,int latency,int length) {
    Buffer out(b.getNumChannels(),length);
    for(int ch=0;ch<b.getNumChannels();++ch)out.copyFrom(ch,0,b,ch,latency,length);
    return out;
}
bool finite(const Buffer& b) {
    for(int ch=0;ch<b.getNumChannels();++ch)for(int i=0;i<b.getNumSamples();++i)
        if(!std::isfinite(b.getSample(ch,i)))return false;
    return true;
}
double peak(const Buffer& b) {double v=0;for(int ch=0;ch<b.getNumChannels();++ch)v=std::max(v,double(b.getMagnitude(ch,0,b.getNumSamples())));return v;}
double rms(const Buffer& b) {double e=0;for(int ch=0;ch<b.getNumChannels();++ch)for(int i=0;i<b.getNumSamples();++i){double x=b.getSample(ch,i);e+=x*x;}return std::sqrt(e/(b.getNumChannels()*double(b.getNumSamples())));}
double difference(const Buffer& a,const Buffer& b) {
    if(a.getNumSamples()!=b.getNumSamples()||a.getNumChannels()!=b.getNumChannels())return 1e6;
    double e=0;for(int ch=0;ch<a.getNumChannels();++ch)for(int i=0;i<a.getNumSamples();++i)e=std::max(e,std::abs(double(a.getSample(ch,i)-b.getSample(ch,i))));return e;
}
double frequency(const Buffer& b,double sr) {
    std::vector<double> crossings;int from=b.getNumSamples()/2;
    for(int i=from+1;i<b.getNumSamples();++i)if(b.getSample(0,i-1)<=0&&b.getSample(0,i)>0) {
        double a=b.getSample(0,i-1),c=b.getSample(0,i);crossings.push_back(i-1-a/(c-a));
    }
    if(crossings.size()<3)return 0;
    std::vector<double> periods;for(size_t i=1;i<crossings.size();++i)periods.push_back(crossings[i]-crossings[i-1]);
    std::sort(periods.begin(),periods.end());return sr/periods[periods.size()/2];
}
void writeWav(const juce::File& file,const Buffer& b,double rate) {
    file.deleteFile();std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();
    juce::WavAudioFormat wav;auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(b.getNumChannels()).withBitsPerSample(24));
    if(!writer||!writer->writeFromAudioSampleBuffer(b,0,b.getNumSamples()))throw std::runtime_error("WAV write failed");
}
bool waitCapture(mouth::CaptureManager& c,int timeoutMs=30000) {
    auto start=std::chrono::steady_clock::now();
    while(c.stage()!=mouth::CaptureManager::Available) {
        if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()>timeoutMs)return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }return true;
}
void technicalChecks(Suite& s) {
    for(double rate : {44100.,48000.,96000.})for(int channels : {1,2})for(int block : {64,137,512,4096}) {
        MusicToMyMouthProcessor p;neutral(p);configure(p,rate,channels,block);Host h;h.rate=rate;p.setPlayHead(&h);
        auto source=sine(rate,69.37,.35,channels);p.setParameterValue("mix",0);p.prepareToPlay(rate,block);
        auto output=aligned(render(p,source,h,block),p.getLatencySamples(),source.getNumSamples());
        double error=difference(source,output);
        s.expect(error<1e-6&&finite(output),"dry latency "+juce::String(rate)+"Hz / "+juce::String(channels)+"ch / "+juce::String(block),error);
    }
    for(double rate:{44100.,48000.,96000.})for(double note:{45.37,57.37,69.37,81.37}) {
        MusicToMyMouthProcessor p;neutral(p);p.setParameterValue("tuneOn",1);p.setParameterValue("scale",0);p.setParameterValue("retune",0);p.setParameterValue("strength",100);p.setParameterValue("humanize",0);p.setParameterValue("character",100);configure(p,rate,1,137);
        Host h;h.rate=rate;p.setPlayHead(&h);auto source=sine(rate,note,2.5);
        auto output=aligned(render(p,source,h,137),p.getLatencySamples(),source.getNumSamples());
        double hz=frequency(output,rate),target=440*std::pow(2.,(std::round(note)-69)/12.);double cents=hz>0?1200*std::log2(hz/target):1e6;
        s.expect(std::abs(cents)<2&&finite(output)&&p.pitch.underruns.load()==0&&p.pitch.engineErrors()==0,"hard correction "+juce::String(rate)+"Hz / MIDI "+juce::String(note,2),cents);
    }
    for(double semitones:{-12.,-3.,3.,12.}) {
        MusicToMyMouthProcessor p;neutral(p);p.setParameterValue("transpose",float(semitones));configure(p,48000,1,512);
        Host h;h.rate=48000;p.setPlayHead(&h);auto source=sine(48000,69,2.5);
        auto output=aligned(render(p,source,h,512),p.getLatencySamples(),source.getNumSamples());
        double target=440*std::pow(2.,semitones/12.),hz=frequency(output,48000),cents=hz>0?1200*std::log2(hz/target):1e6;
        s.expect(std::abs(cents)<7&&finite(output),"transpose "+juce::String(semitones),cents);
    }
    {
        MusicToMyMouthProcessor p;neutral(p);configure(p,48000,1,137);Host h;h.rate=48000;p.setPlayHead(&h);
        auto source=sine(48000,69,.5);auto output=aligned(render(p,source,h,137,true),p.getLatencySamples(),source.getNumSamples());
        s.expect(difference(source,output)<1e-6,"host bypass preserves latency",difference(source,output));
    }
    {
        MusicToMyMouthProcessor p;neutral(p);configure(p,48000,2,137);Host h;h.rate=48000;p.setPlayHead(&h);
        Buffer source(2,48000);source.clear();source.setSample(0,10000,.4f);source.setSample(1,10000,.4f);
        auto wet=aligned(render(p,source,h,137),p.getLatencySamples(),source.getNumSamples());
        int first=0;for(int i=0;i<wet.getNumSamples();++i)if(std::abs(wet.getSample(0,i))>std::abs(wet.getSample(0,first)))first=i;
        s.expect(std::abs(first-10000)<=1,"unity wet impulse matches reported latency",first-10000);
        s.expect(finite(wet)&&p.pitch.underruns.load()==0,"unity streaming stays finite without underruns");
    }
    {
        // Verify actual manual-render pitch and timing, rather than only the saved edit metadata.
        mouth::CaptureManager capture;capture.prepare(48000,1);capture.arm(0,0);
        auto source=sine(48000,69,2);
        for(int offset=0;offset<source.getNumSamples();offset+=512) {
            int count=std::min(512,source.getNumSamples()-offset);
            Buffer view(source.getArrayOfWritePointers(),1,offset,count);
            capture.capture(view,20+offset/48000.,true);
        }
        capture.finish();Buffer stop(1,512);stop.clear();capture.capture(stop,22,false);
        if(!waitCapture(capture)||!capture.document()||capture.document()->notes.empty())throw std::runtime_error("Synthetic note Capture failed");
        auto note=capture.document()->notes.front();note.shift=3;note.stretch=.75;
        if(!capture.editNote(0,note)||!waitCapture(capture))throw std::runtime_error("Synthetic note edit failed");
        Buffer edited;edited.makeCopyOf(source);auto mask=std::make_unique<bool[]>(size_t(edited.getNumSamples()));
        capture.replace(edited,mask.get(),20,true,48000);
        int begin=int((note.start+.25)*48000),count=24000;Buffer centre(1,count);centre.copyFrom(0,0,edited,0,begin,count);
        double hz=frequency(centre,48000),target=440*std::pow(2.,3/12.),cents=hz>0?1200*std::log2(hz/target):1e6;
        s.expect(std::abs(cents)<2,"manual note +3 semitones measured pitch",cents);
        double end=note.start+note.duration*note.stretch;int tail=int((end+.1)*48000),tailCount=std::min(4800,edited.getNumSamples()-tail);
        s.expect(tailCount>0&&edited.getMagnitude(0,tail,tailCount)<1e-7,"manual duration contracts audio to edited span");
    }
    s.expect(mouth::nearestNote(71.7,0,1)==72&&mouth::nearestNote(59.7,0,1)==60&&mouth::nearestNote(63.1,0,2)==63,"scale quantization at octave boundaries");
}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI juce;
    if(argc<3||argc>4){std::cerr<<"usage: MusicToMyMouthValidation input.wav output-directory\n";return 2;}
    Suite suite;juce::File out(argv[2]);out.createDirectory();
    try {
        technicalChecks(suite);
        juce::WavAudioFormat wav;auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(new juce::FileInputStream(juce::File(argv[1])),true));
        if(!reader)throw std::runtime_error("Cannot read stem");
        double sr=reader->sampleRate;int length=std::min(int(sr*12),int(reader->lengthInSamples));
        Buffer source(int(reader->numChannels),length);
        // Find a vocal-rich 12-second excerpt using RMS of one-second windows.
        int bestStart=0;double bestEnergy=0;
        for(int start=0;start+length<=reader->lengthInSamples;start+=int(sr*6)) {
            Buffer probe(int(reader->numChannels),length);reader->read(&probe,0,length,start,true,true);double energy=rms(probe);
            if(energy>bestEnergy){bestEnergy=energy;bestStart=start;}
        }
        reader->read(&source,0,length,bestStart,true,true);writeWav(out.getChildFile("01-original.wav"),source,sr);
        auto* sourceInfo=new juce::DynamicObject;sourceInfo->setProperty("startSeconds",bestStart/sr);sourceInfo->setProperty("durationSeconds",length/sr);sourceInfo->setProperty("sampleRate",sr);sourceInfo->setProperty("peak",peak(source));sourceInfo->setProperty("rms",rms(source));
        suite.expect(finite(source)&&peak(source)>0,"stem input",juce::var(sourceInfo));
        for(int index=0;index<int(mouth::factoryPresets().size());++index) {
            MusicToMyMouthProcessor p;p.setCurrentProgram(index);p.setParameterValue("key",0);p.setParameterValue("scale",2);configure(p,sr,source.getNumChannels(),137);Host h;h.rate=sr;p.setPlayHead(&h);
            auto start=std::chrono::steady_clock::now();auto rendered=render(p,source,h,137);double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            auto output=aligned(rendered,p.getLatencySamples(),source.getNumSamples());
            auto* stats=new juce::DynamicObject;stats->setProperty("peak",peak(output));stats->setProperty("rms",rms(output));stats->setProperty("renderSeconds",elapsed);stats->setProperty("realtimeRatio",elapsed/(length/sr));stats->setProperty("latencySamples",p.getLatencySamples());stats->setProperty("engineErrors",p.pitch.engineErrors());stats->setProperty("engineWarnings",p.pitch.engineWarnings());stats->setProperty("underruns",p.pitch.underruns.load());stats->setProperty("overflows",p.pitch.overflows.load());
            suite.expect(finite(output)&&peak(output)<1&&rms(output)>1e-6&&p.pitch.engineErrors()==0&&p.pitch.engineWarnings()==0&&p.pitch.underruns.load()==0&&p.pitch.overflows.load()==0,"vocal preset "+p.getProgramName(index),juce::var(stats));
            if(index==0||index==4||index==6||index==9)writeWav(out.getChildFile(juce::String::formatted("%02d-",index+2)+p.getProgramName(index).replaceCharacters(" /","--")+".wav"),output,sr);
        }
        {
            MusicToMyMouthProcessor p;p.setCurrentProgram(4);p.setParameterValue("key",0);p.setParameterValue("scale",2);configure(p,sr,source.getNumChannels(),512);Host h;h.rate=sr;p.setPlayHead(&h);
            auto expected=aligned(render(p,source,h,512),p.getLatencySamples(),source.getNumSamples());
            int latency=0;auto nativeRender=nativeAU::render(source,sr,4,latency);auto installed=aligned(nativeRender,latency,source.getNumSamples());
            suite.expect(latency==p.getLatencySamples(),"installed AU latency agrees with processor",latency);
            suite.expect(finite(installed)&&difference(expected,installed)<1e-5,"installed AU stem matches offline processor",difference(expected,installed));
            int bypassLatency=0;auto bypassedRender=nativeAU::render(source,sr,4,bypassLatency,true);
            auto bypassed=aligned(bypassedRender,bypassLatency,source.getNumSamples());
            suite.expect(difference(source,bypassed)<1e-6,"installed AU host bypass preserves raw audio timing",difference(source,bypassed));
        }
        {
            MusicToMyMouthProcessor p;p.setCurrentProgram(6);p.moveModule(0,1);auto savedOrder=p.moduleOrder();
            auto file=out.getChildFile("round-trip.mtmm");bool saved=p.savePreset(file);
            MusicToMyMouthProcessor other;other.setParameterValue("key",11);other.setParameterValue("scale",5);other.setParameterValue("editedOn",0);
            bool loaded=other.loadPreset(file);
            suite.expect(saved&&loaded&&mouth::value(other.parameters,"formant")==-5&&other.moduleOrder()==savedOrder,"sound preset and order recall");
            suite.expect(mouth::value(other.parameters,"key")==11&&mouth::value(other.parameters,"scale")==5&&mouth::value(other.parameters,"editedOn")==0,"sound presets preserve song and Capture settings");
        }
        if(argc==4&&juce::String(argv[3])=="--full") {
            Buffer whole(int(reader->numChannels),int(reader->lengthInSamples));reader->read(&whole,0,whole.getNumSamples(),0,true,true);
            for(int index:{0,4}) {
                MusicToMyMouthProcessor p;p.setCurrentProgram(index);p.setParameterValue("key",0);p.setParameterValue("scale",2);configure(p,sr,whole.getNumChannels(),512);Host h;h.rate=sr;p.setPlayHead(&h);Timings timings;
                auto start=std::chrono::steady_clock::now();auto rendered=render(p,whole,h,512,false,&timings);
                double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
                std::sort(timings.calls.begin(),timings.calls.end());double p99=timings.calls[size_t(timings.calls.size()*.99)];
                auto* stats=new juce::DynamicObject;stats->setProperty("durationSeconds",whole.getNumSamples()/sr);stats->setProperty("renderSeconds",elapsed);stats->setProperty("peak",peak(rendered));stats->setProperty("engineErrors",p.pitch.engineErrors());stats->setProperty("engineWarnings",p.pitch.engineWarnings());stats->setProperty("underruns",p.pitch.underruns.load());stats->setProperty("overflows",p.pitch.overflows.load());stats->setProperty("callbackP99Ms",p99*1000);stats->setProperty("callbackDeadlineMs",512/sr*1000);
                suite.expect(finite(rendered)&&p.pitch.engineErrors()==0&&p.pitch.engineWarnings()==0&&p.pitch.underruns.load()==0&&p.pitch.overflows.load()==0,"full stem "+p.getProgramName(index),juce::var(stats));
                suite.expect(p99<512/sr,"full stem callback p99 under 512-frame budget / "+p.getProgramName(index),p99*1000);
            }
        }
        {
            MusicToMyMouthProcessor p;neutral(p);configure(p,sr,source.getNumChannels(),137);Host h;h.rate=sr;h.sample=int64_t(sr*20);p.setPlayHead(&h);
            suite.expect(p.capture.arm(0,2),"Capture arm");
            render(p,source,h,137);h.playing=false;Buffer stop(source.getNumChannels(),137);stop.clear();juce::MidiBuffer midi;p.processBlock(stop,midi);
            bool ready=waitCapture(p.capture);auto doc=p.capture.document();suite.expect(ready&&doc&&!doc->notes.empty(),"Capture analysis",doc?int(doc->notes.size()):0);
            if(doc&&!doc->notes.empty()) {
                int selected=0;while(selected<int(doc->notes.size())-1&&doc->notes[size_t(selected)].duration<.13)++selected;
                auto edit=doc->notes[size_t(selected)];edit.shift=3;edit.stretch=.8;
                bool edited=p.capture.editNote(size_t(selected),edit)&&waitCapture(p.capture);
                suite.expect(edited,"manual pitch and duration render");
                juce::MemoryBlock state;p.getStateInformation(state);
                MusicToMyMouthProcessor restored;neutral(restored);configure(restored,sr,source.getNumChannels(),512);restored.setStateInformation(state.getData(),int(state.getSize()));
                bool reopened=waitCapture(restored.capture);auto recovered=restored.capture.document();
                suite.expect(reopened&&recovered&&recovered->notes.size()==doc->notes.size()&&std::abs(recovered->notes[size_t(selected)].shift-3)<1e-9,"Capture project recall",int(state.getSize()));
                suite.expect(recovered&&difference(*doc->audio,*recovered->audio)<1e-7,"Capture media round trip",recovered?difference(*doc->audio,*recovered->audio):1e6);
                h.sample=int64_t(sr*20);h.playing=true;p.prepareToPlay(sr,137);
                auto changed=aligned(render(p,source,h,137),p.getLatencySamples(),source.getNumSamples());
                Host other;other.rate=sr;other.sample=int64_t(sr*20);restored.setPlayHead(&other);restored.prepareToPlay(sr,512);
                auto recalled=aligned(render(restored,source,other,512),restored.getLatencySamples(),source.getNumSamples());
                suite.expect(difference(changed,recalled)<1e-5,"recalled render across block sizes",difference(changed,recalled));
                writeWav(out.getChildFile("15-manual-note-edit.wav"),changed,sr);
                // A stopped host must never retrigger captured vocals at a stationary timeline.
                p.prepareToPlay(sr,137);h.sample=int64_t(sr*20);h.playing=false;
                Buffer silence(source.getNumChannels(),int(sr*.5));silence.clear();auto stopped=render(p,silence,h,137);
                suite.expect(rms(stopped)<1e-9,"stopped host does not replay Capture");
                p.capture.undo();suite.expect(waitCapture(p.capture)&&std::abs(p.capture.document()->notes[size_t(selected)].shift)<1e-9,"manual edit undo");
            }
        }
    } catch(const std::exception& e) {suite.expect(false,"runner exception",juce::String(e.what()));}
    auto* report=new juce::DynamicObject;report->setProperty("failures",suite.failures);report->setProperty("checks",suite.checks);
    out.getChildFile("results.json").replaceWithText(juce::JSON::toString(juce::var(report),false));
    std::cout<<"FAILURES "<<suite.failures<<std::endl;return suite.failures?1:0;
}
