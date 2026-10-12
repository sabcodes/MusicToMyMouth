#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
using Buffer=juce::AudioBuffer<float>;
struct Host : juce::AudioPlayHead {
    int64_t frame=0;double rate=44100;
    juce::Optional<PositionInfo> getPosition() const override {PositionInfo p;p.setTimeInSamples(frame);p.setTimeInSeconds(frame/rate);p.setBpm(129.2);p.setIsPlaying(true);return p;}
};
void disableEffects(MusicToMyMouthProcessor& p) {
    for(auto id:{"eqOn","compOn","deessOn","gateOn","satOn","reverbOn","delayOn","modOn"})p.setParameterValue(id,0);
}
void save(const juce::File& file,const Buffer& b,double sr) {
    file.deleteFile();std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();juce::WavAudioFormat format;
    auto writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(sr).withNumChannels(b.getNumChannels()).withBitsPerSample(24));
    if(!writer||!writer->writeFromAudioSampleBuffer(b,0,b.getNumSamples()))throw std::runtime_error("write failed");
}
struct Stats {
    int frames=0,voiced=0,corrected=0;double totalEnergy=0,voicedEnergy=0,correctionEnergy=0,correctionSquared=0;
};
Buffer render(const Buffer& source,double sr,int preset,const juce::String& mode,Stats& stats) {
    MusicToMyMouthProcessor p;p.setCurrentProgram(preset);p.setParameterValue("key",0);p.setParameterValue("scale",2);
    if(mode=="unity"||mode=="hard-only"||mode=="octave-up")disableEffects(p);
    if(mode=="unity"||mode=="octave-up"||mode=="effects-only")p.setParameterValue("tuneOn",0);
    if(mode=="octave-up")p.setParameterValue("transpose",12);
    p.setRateAndBufferSizeDetails(sr,512);p.prepareToPlay(sr,512);Host host;host.rate=sr;p.setPlayHead(&host);
    int latency=p.getLatencySamples(),length=source.getNumSamples()+latency;Buffer delayed(source.getNumChannels(),length);delayed.clear();juce::MidiBuffer midi;
    for(int offset=0;offset<length;offset+=512) {
        int count=std::min(512,length-offset);Buffer b(source.getNumChannels(),count);b.clear();
        if(offset<source.getNumSamples())for(int ch=0;ch<b.getNumChannels();++ch)b.copyFrom(ch,0,source,ch,offset,std::min(count,source.getNumSamples()-offset));
        double energy=0;for(int ch=0;ch<b.getNumChannels();++ch)for(int i=0;i<count;++i){double x=b.getSample(ch,i);energy+=x*x;}
        host.frame=offset;p.processBlock(b,midi);
        if(offset<source.getNumSamples()) {
            ++stats.frames;stats.totalEnergy+=energy;
            if(p.pitch.detectedMidi.load()>0){++stats.voiced;stats.voicedEnergy+=energy;}
            double cents=std::abs(p.pitch.correctionCents.load());
            if(cents>10){++stats.corrected;stats.correctionEnergy+=energy;}
            stats.correctionSquared+=cents*cents*energy;
        }
        for(int ch=0;ch<b.getNumChannels();++ch)delayed.copyFrom(ch,offset,b,ch,0,count);
    }
    Buffer out(source.getNumChannels(),source.getNumSamples());for(int ch=0;ch<out.getNumChannels();++ch)out.copyFrom(ch,0,delayed,ch,latency,out.getNumSamples());return out;
}
struct Window { int start=0; double correctionRms=0,voicedFraction=0; };
Window findWindow(juce::AudioFormatReader& reader) {
    double sr=reader.sampleRate;Buffer audio(int(reader.numChannels),int(reader.lengthInSamples));
    if(!reader.read(&audio,0,audio.getNumSamples(),0,true,true))throw std::runtime_error("scan read failed");
    mouth::YinDetector detector;detector.prepare(sr);
    struct Frame {double energy=0,voicedEnergy=0,score=0;};std::vector<Frame> frames;
    Frame f;mouth::PitchReading reading;
    for(int i=0;i<audio.getNumSamples();++i) {
        float mono=0;for(int ch=0;ch<audio.getNumChannels();++ch){float x=audio.getSample(ch,i);mono+=x/audio.getNumChannels();f.energy+=double(x)*x;}
        reading=detector.push(mono);
        if((i+1)%512==0) {
            if(reading.voiced){double cents=100*(mouth::nearestNote(reading.midi,0,2)-reading.midi);f.voicedEnergy=f.energy;f.score=f.energy*cents*cents;}
            frames.push_back(f);f={};
        }
    }
    int count=int(std::round(sr*8/512));Window best;double bestScore=-1;
    double energy=0,voice=0,score=0;
    for(int i=0;i<int(frames.size());++i) {
        energy+=frames[size_t(i)].energy;voice+=frames[size_t(i)].voicedEnergy;score+=frames[size_t(i)].score;
        if(i>=count){energy-=frames[size_t(i-count)].energy;voice-=frames[size_t(i-count)].voicedEnergy;score-=frames[size_t(i-count)].score;}
        if(i+1<count||energy<.02*.02*count*512*audio.getNumChannels())continue;
        // Require substantial voiced energy so a noisy incidental pitch estimate
        // cannot win by itself. This score is a detector estimate, not ground truth.
        double voiced=voice/std::max(1e-9,energy),weighted=score/std::max(1e-9,energy);
        if(voiced>=.5&&weighted>bestScore){bestScore=weighted;best={int((i-count+1)*512),std::sqrt(weighted),voiced};}
    }
    if(bestScore<0)throw std::runtime_error("No suitable pitched window found");return best;
}
double rms(const Buffer& b) {double e=0;for(int ch=0;ch<b.getNumChannels();++ch)for(int i=0;i<b.getNumSamples();++i){double x=b.getSample(ch,i);e+=x*x;}return std::sqrt(e/(b.getNumChannels()*double(b.getNumSamples())));}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if(argc!=3&&argc!=4)return 2;juce::WavAudioFormat format;auto reader=std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(new juce::FileInputStream(juce::File(argv[1])),true));if(!reader)return 2;
    auto dir=juce::File(argv[2]);dir.createDirectory();double sr=reader->sampleRate;int count=int(sr*8);
    auto window=findWindow(*reader);if(argc==4){double start=std::stod(argv[3]);if(!std::isfinite(start)||start<0||start+8>reader->lengthInSamples/sr)return 2;window.start=int(start*sr);}
    std::cout<<"Selected start "<<window.start/sr<<"s, estimated correction RMS "<<window.correctionRms<<" cents, voiced energy "<<window.voicedFraction*100<<"%"<<std::endl;
    Buffer source(int(reader->numChannels),count);reader->read(&source,0,count,window.start,true,true);save(dir.getChildFile("original.wav"),source,sr);
    std::vector<Buffer> segments;segments.push_back(source);juce::Array<juce::var> data;bool engagementPass=false;
    for(auto [name,preset]:std::vector<std::pair<juce::String,int>>{{"unity",0},{"natural",0},{"hard-only",4},{"effects-only",4},{"hard-full",4},{"deep-formant",6},{"octave-up",4}}) {
        Stats stats;auto output=render(source,sr,preset,name,stats);save(dir.getChildFile(name+".wav"),output,sr);
        auto* d=new juce::DynamicObject;d->setProperty("mode",name);d->setProperty("voicedFramesPercent",100.*stats.voiced/stats.frames);d->setProperty("voicedEnergyPercent",100.*stats.voicedEnergy/stats.totalEnergy);d->setProperty("energyWithCorrectionOver10CentsPercent",100.*stats.correctionEnergy/stats.totalEnergy);d->setProperty("rmsCorrectionCents",std::sqrt(stats.correctionSquared/stats.totalEnergy));data.add(juce::var(d));std::cout<<juce::JSON::toString(juce::var(d),true)<<std::endl;
        if(name=="hard-full")engagementPass=std::sqrt(stats.correctionSquared/stats.totalEnergy)>=25 && stats.correctionEnergy/stats.totalEnergy>=.5;
        if(name=="natural"||name=="hard-full"||name=="deep-formant"||name=="octave-up")segments.push_back(std::move(output));
    }
    // One player with successive copies of the same phrase avoids ambiguity about
    // which separate preview player is running. Gain-match for a useful comparison.
    double target=.085;int gap=int(sr*.5);Buffer combined(source.getNumChannels(),int(segments.size())*(count+gap));combined.clear();
    for(size_t segment=0;segment<segments.size();++segment) {
        auto& b=segments[segment];double g=target/std::max(1e-9,rms(b));float peak=0;for(int ch=0;ch<b.getNumChannels();++ch)peak=std::max(peak,b.getMagnitude(ch,0,count));g=std::min(g,.9/std::max(1e-9,double(peak)));
        b.applyGain(float(g));
        int fade=std::max(1,int(sr*.008));b.applyGainRamp(0,fade,0,1);b.applyGainRamp(count-fade,fade,1,0);
        for(int ch=0;ch<b.getNumChannels();++ch)combined.copyFrom(ch,int(segment)*(count+gap),b,ch,0,count);
    }
    save(dir.getChildFile("contrast-check.wav"),combined,sr);
    auto* metadata=new juce::DynamicObject;metadata->setProperty("sourceStartSeconds",window.start/sr);metadata->setProperty("sectionSeconds",8);metadata->setProperty("gapSeconds",.5);metadata->setProperty("sections",juce::Array<juce::var>{"original","natural","hard-full","deep-formant","octave-up"});
    dir.getChildFile("contrast-sections.json").replaceWithText(juce::JSON::toString(juce::var(metadata),false));dir.getChildFile("engagement.json").replaceWithText(juce::JSON::toString(juce::var(data),false));std::cout<<(engagementPass?"PASS":"FAIL")<<" preview correction engagement (>=25 cents RMS and >=50% signal energy corrected over 10 cents)"<<std::endl;return engagementPass?0:1;
}
