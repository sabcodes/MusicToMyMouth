#include "Capture.h"
namespace mouth {
CaptureManager::CaptureManager():juce::Thread("MusicToMyMouth capture analysis") {startThread(juce::Thread::Priority::low);}
CaptureManager::~CaptureManager() {signalThreadShouldExit();notify();stopThread(-1);active=nullptr;}
void CaptureManager::prepare(double rate,int channels) {
    // Called while host processing is stopped. A take is independent of host sample rate.
    juce::ScopedLock raw(rawLock);
    if(stage()==Recording||stage()==Armed||stage()==Queued) {status=Empty;written=0;}
    captureRate=rate;int frames=std::min(int(rate*180),int(128*1024*1024/(sizeof(float)*size_t(channels))));captureBuffer.setSize(channels,frames);maximumSeconds.store(frames/rate);displayRate.store(rate);captureBuffer.clear();
}
bool CaptureManager::arm(int selectedKey,int selectedScale) {
    auto s=stage();if(clearRequested.load()||s==Working||s==Queued||s==Recording||s==Armed)return false;
    active=nullptr;key=selectedKey;scale=selectedScale;written=0;finishRequested=false;clearRequested=false;
    {juce::ScopedLock l(documentLock);lastMessage="Capture armed. Play the required vocal section once.";}
    status.store(Armed,std::memory_order_release);return true;
}
void CaptureManager::finish() {finishRequested=true;}
void CaptureManager::clear() {if(stage()==Working||stage()==Queued)return;clearRequested=true;finishRequested=true;notify();}
void CaptureManager::capture(const juce::AudioBuffer<float>& b,double seconds,bool playing) {
    if(clearRequested.load()&&stage()!=Working&&stage()!=Queued) {status=Empty;written=0;active=nullptr;return;}
    auto s=stage();
    if(s==Armed&&finishRequested.load()) {status=Empty;return;}
    if(s==Armed&&playing) {
        captureAnchor=seconds;expectedSeconds=seconds;written=0;
        int expected=Armed;if(!status.compare_exchange_strong(expected,Recording))return;s=Recording;
    }
    if(s!=Recording)return;
    if(!playing||finishRequested.load()||std::abs(seconds-expectedSeconds)>2.0/captureRate) {
        int expected=Recording;status.compare_exchange_strong(expected,written.load()>0?Queued:Empty);return;
    }
    int pos=written.load(),count=std::min(b.getNumSamples(),captureBuffer.getNumSamples()-pos);
    for(int ch=0;ch<captureBuffer.getNumChannels();++ch)captureBuffer.copyFrom(ch,pos,b,ch,0,count);
    written.store(pos+count);expectedSeconds=seconds+b.getNumSamples()/captureRate;
    if(pos+count==captureBuffer.getNumSamples()){int expected=Recording;status.compare_exchange_strong(expected,Queued);}
}
void CaptureManager::replace(juce::AudioBuffer<float>& b,bool* mask,double seconds,bool use,double rate) {
    std::fill(mask,mask+b.getNumSamples(),false);
    if(!use||stage()==Armed||stage()==Recording)return;
    const Render* r=nullptr;
    do {r=active.load(std::memory_order_seq_cst);hazard.store(r,std::memory_order_seq_cst);}while(r!=active.load(std::memory_order_seq_cst));
    if(r)for(int i=0;i<b.getNumSamples();++i) {
        double position=(seconds+i/rate-r->anchor)*r->rate;
        if(position<0||position>=r->audio.getNumSamples()-1)continue;
        int index=int(position);float fraction=float(position-index);mask[i]=r->manual[size_t(index)]!=0;
        for(int ch=0;ch<b.getNumChannels();++ch) {
            int source=std::min(ch,r->audio.getNumChannels()-1);
            b.setSample(ch,i,r->audio.getSample(source,index)*(1-fraction)+r->audio.getSample(source,index+1)*fraction);
        }
    }
    hazard.store(nullptr,std::memory_order_seq_cst);
}
std::shared_ptr<const Take> CaptureManager::document() const {juce::ScopedLock l(documentLock);return take;}
juce::String CaptureManager::message() const {juce::ScopedLock l(documentLock);return lastMessage;}
bool CaptureManager::editNote(size_t index,const VocalNote& note) {
    juce::ScopedLock l(documentLock);
    if(stage()!=Available||!take||index>=take->notes.size())return false;
    double start=note.start+note.offset,end=start+note.duration*note.stretch,total=take->audio->getNumSamples()/take->rate;
    if(!std::isfinite(note.shift)||!std::isfinite(start)||!std::isfinite(end)||!std::isfinite(note.correction)||note.shift<-12||note.shift>12||note.stretch<.5||note.stretch>2||note.correction<0||note.correction>1||start<0||end>total)return false;
    if(index>0){auto& prev=take->notes[index-1];if(start<prev.start+prev.offset+prev.duration*prev.stretch)return false;}
    if(index+1<take->notes.size()){auto& next=take->notes[index+1];if(end>next.start+next.offset)return false;}
    undoStack.push_back(take);if(undoStack.size()>20)undoStack.erase(undoStack.begin());
    pendingTake=std::make_shared<Take>(*take);pendingTake->notes[index]=note;take=pendingTake;status=Working;lastMessage="Rendering note edits...";notify();return true;
}
void CaptureManager::undo() {
    juce::ScopedLock l(documentLock);if(stage()!=Available||undoStack.empty())return;
    pendingTake=undoStack.back();take=pendingTake;undoStack.pop_back();status=Working;lastMessage="Restoring previous edit...";notify();
}
void CaptureManager::analyse(Take& t) {
    YinDetector detector;detector.prepare(t.rate);
    int hop=std::max(1,int(t.rate*.01));VocalNote current;bool open=false;int silence=0;
    std::vector<double> pitches;
    for(int i=0;i<t.audio->getNumSamples()&&!threadShouldExit();++i) {
        float mono=0;for(int ch=0;ch<t.audio->getNumChannels();++ch)mono+=t.audio->getSample(ch,i)/t.audio->getNumChannels();
        auto p=detector.push(mono);if(i%hop!=0)continue;
        // The detection window is about 85 ms; center timestamps on that window.
        double time=std::max(0.,i/t.rate-.043);
        bool voiced=p.voiced&&p.confidence>.85;
        bool changed=open&&voiced&&std::abs(p.midi-current.midi)>.7;
        if(open&&(!voiced||changed)) {
            if(!voiced&&++silence<3)continue;
            current.duration=std::max(.01,time-current.start);
            if(current.duration>=.065) {std::sort(pitches.begin(),pitches.end());current.midi=pitches[pitches.size()/2];t.notes.push_back(current);}
            open=false;pitches.clear();
        }
        if(voiced&&!open) {current={time,.01,p.midi,0,0,1,0};open=true;silence=0;}
        if(voiced&&open){pitches.push_back(p.midi);silence=0;}
    }
    if(open&&!pitches.empty()){current.duration=t.audio->getNumSamples()/t.rate-current.start;std::sort(pitches.begin(),pitches.end());current.midi=pitches[pitches.size()/2];if(current.duration>=.065)t.notes.push_back(current);}
    // Keep note boundaries disjoint for independent edits.
    for(size_t i=1;i<t.notes.size();++i)t.notes[i-1].duration=std::min(t.notes[i-1].duration,t.notes[i].start-t.notes[i-1].start);
}
std::unique_ptr<Render> CaptureManager::render(const Take& t) {
    auto result=std::make_unique<Render>();result->audio.makeCopyOf(*t.audio);result->manual.resize(size_t(t.audio->getNumSamples()),0);result->rate=t.rate;result->anchor=t.anchor;
    // Clear every edited source span before placing any relocated note. Otherwise
    // clearing a later original span could erase an earlier note's destination.
    for (const auto& note : t.notes) {
        double semitones = note.shift + (nearestNote(note.midi, t.selectedKey, t.selectedScale) - note.midi) * note.correction;
        if (std::abs(semitones) < .001 && std::abs(note.offset) < 1e-6 && std::abs(note.stretch - 1) < 1e-6) continue;
        int start = juce::jlimit(0, t.audio->getNumSamples() - 1, int(note.start * t.rate));
        int length = std::min(int(note.duration * t.rate), t.audio->getNumSamples() - start);
        if (length >= 32) for (int ch = 0; ch < t.audio->getNumChannels(); ++ch) result->audio.clear(ch, start, length);
    }
    for(auto& note:t.notes) {
        if(threadShouldExit())return nullptr;
        double semitones=note.shift+(nearestNote(note.midi,t.selectedKey,t.selectedScale)-note.midi)*note.correction;
        if(std::abs(semitones)<.001&&std::abs(note.offset)<1e-6&&std::abs(note.stretch-1)<1e-6)continue;
        int start=juce::jlimit(0,t.audio->getNumSamples()-1,int(note.start*t.rate)),length=std::min(int(note.duration*t.rate),t.audio->getNumSamples()-start);
        if(length<32)continue;
        int destination=int((note.start+note.offset)*t.rate),outLength=std::max(1,int(length*note.stretch));
        using R=RubberBand::RubberBandStretcher;
        R engine(size_t(t.rate),size_t(t.audio->getNumChannels()),R::OptionProcessOffline|R::OptionEngineFiner|R::OptionChannelsTogether|R::OptionFormantPreserved,note.stretch,std::pow(2.,semitones/12.));
        engine.setDebugLevel(0);engine.setExpectedInputDuration(size_t(length));engine.setMaxProcessSize(1024);
        std::array<const float*,2> in{};std::array<float*,2> out{};
        for(int pos=0;pos<length;pos+=1024){if(threadShouldExit())return nullptr;int count=std::min(1024,length-pos);for(int ch=0;ch<t.audio->getNumChannels();++ch)in[size_t(ch)]=t.audio->getReadPointer(ch,start+pos);engine.study(in.data(),size_t(count),pos+count==length);}
        juce::AudioBuffer<float> rendered(t.audio->getNumChannels(),outLength+4096);rendered.clear();int received=0;
        auto drain=[&]{while(engine.available()>0){int count=std::min(engine.available(),rendered.getNumSamples()-received);if(count<=0)break;for(int ch=0;ch<t.audio->getNumChannels();++ch)out[size_t(ch)]=rendered.getWritePointer(ch,received);received+=int(engine.retrieve(out.data(),size_t(count)));}};
        for(int pos=0;pos<length;pos+=1024){if(threadShouldExit())return nullptr;int count=std::min(1024,length-pos);for(int ch=0;ch<t.audio->getNumChannels();++ch)in[size_t(ch)]=t.audio->getReadPointer(ch,start+pos);engine.process(in.data(),size_t(count),pos+count==length);drain();}
        drain();
        for(int ch=0;ch<t.audio->getNumChannels();++ch) {
            // Short raised-cosine edge fades suppress slice discontinuities.
            for(int i=0;i<std::min(received,outLength)&&destination+i<result->audio.getNumSamples();++i) {
                if(destination+i<0)continue;
                int fade=std::min(128,outLength/4);float w=1;
                if(fade>0){if(i<fade)w=float(.5-.5*std::cos(juce::MathConstants<double>::pi*i/fade));if(i>=outLength-fade)w=float(.5-.5*std::cos(juce::MathConstants<double>::pi*(outLength-1-i)/fade));}
                result->audio.setSample(ch,destination+i,rendered.getSample(ch,i)*w);result->manual[size_t(destination+i)]=1;
            }
        }
    }return result;
}
void CaptureManager::publish(std::unique_ptr<Render> r) {if(!r)return;auto ptr=r.get();renders.push_back(std::move(r));active.store(ptr,std::memory_order_seq_cst);reap();}
void CaptureManager::reap() {auto current=active.load(std::memory_order_seq_cst);auto used=hazard.load(std::memory_order_seq_cst);renders.erase(std::remove_if(renders.begin(),renders.end(),[&](auto& r){return r.get()!=current&&r.get()!=used;}),renders.end());}
void CaptureManager::run() {
    while(!threadShouldExit()) {
        if(clearRequested.load()&&stage()!=Recording&&stage()!=Queued&&stage()!=Working) {
            juce::ScopedLock l(documentLock);++revision;take.reset();pendingTake.reset();undoStack.clear();active=nullptr;written=0;status=Empty;clearRequested=false;lastMessage="Capture cleared. Arm it to transfer another section.";
        }
        std::shared_ptr<Take> job;bool needsAnalysis=false;uint64_t ticket=revision.load();
        {
            juce::ScopedLock raw(rawLock);
            if(stage()==Queued) {
                ticket=revision.load();status=Working;needsAnalysis=true;
                job=std::make_shared<Take>();job->rate=captureRate;job->anchor=captureAnchor;job->selectedKey=key;job->selectedScale=scale;
                job->audio->setSize(captureBuffer.getNumChannels(),written.load());
                for(int ch=0;ch<job->audio->getNumChannels();++ch)job->audio->copyFrom(ch,0,captureBuffer,ch,0,job->audio->getNumSamples());
            }
        }
        if(!job&&stage()==Working){juce::ScopedLock l(documentLock);job=std::move(pendingTake);ticket=revision.load();}
        if(job) {
            if(needsAnalysis){juce::ScopedLock l(documentLock);lastMessage="Analyzing captured notes...";}
            if(needsAnalysis)analyse(*job);
            auto output=render(*job);
            juce::ScopedLock l(documentLock);
            if(ticket==revision.load()&&!threadShouldExit()) {
                publish(std::move(output));take=job;
                if(needsAnalysis)undoStack.clear();
                lastMessage=juce::String(job->notes.size())+" notes ready. Select a note, then apply pitch or timing edits.";status=Available;
            }
        }
        reap();wait(30);
    }
}
juce::ValueTree CaptureManager::save() {
    juce::ValueTree tree("Capture");auto t=document();if(!t)return tree;
    tree.setProperty("rate",t->rate,nullptr);tree.setProperty("anchor",t->anchor,nullptr);tree.setProperty("key",t->selectedKey,nullptr);tree.setProperty("scale",t->selectedScale,nullptr);
    juce::MemoryBlock data;
    std::unique_ptr<juce::OutputStream> stream=std::make_unique<juce::MemoryOutputStream>(data,false);juce::WavAudioFormat wav;
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(t->rate).withNumChannels(t->audio->getNumChannels()).withBitsPerSample(32).withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if(writer){writer->writeFromAudioSampleBuffer(*t->audio,0,t->audio->getNumSamples());writer.reset();tree.setProperty("wav",juce::var(data),nullptr);}
    for(auto& n:t->notes){juce::ValueTree row("Note");row.setProperty("start",n.start,nullptr);row.setProperty("duration",n.duration,nullptr);row.setProperty("midi",n.midi,nullptr);row.setProperty("shift",n.shift,nullptr);row.setProperty("offset",n.offset,nullptr);row.setProperty("stretch",n.stretch,nullptr);row.setProperty("correction",n.correction,nullptr);tree.addChild(row,-1,nullptr);}
    return tree;
}
bool CaptureManager::restore(const juce::ValueTree& tree) {
    if(!tree.isValid()||!tree.hasProperty("wav")){juce::ScopedLock l(documentLock);++revision;active=nullptr;take.reset();pendingTake.reset();undoStack.clear();status=Empty;return true;}
    auto media=tree["wav"];auto* bytes=media.getBinaryData();
    if(!bytes||bytes->getSize()>140000000)return false;
    juce::WavAudioFormat wav;auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(new juce::MemoryInputStream(*bytes,false),true));
    if(!reader||reader->numChannels<1||reader->numChannels>2||reader->lengthInSamples<=0||reader->lengthInSamples>reader->sampleRate*180||reader->sampleRate<8000||reader->sampleRate>192000)return false;
    auto t=std::make_shared<Take>();t->rate=reader->sampleRate;t->anchor=double(tree["anchor"]);t->selectedKey=juce::jlimit(0,11,int(tree["key"]));t->selectedScale=juce::jlimit(0,5,int(tree["scale"]));
    if(!std::isfinite(t->anchor))return false;
    t->audio->setSize(int(reader->numChannels),int(reader->lengthInSamples));if(!reader->read(t->audio.get(),0,t->audio->getNumSamples(),0,true,true))return false;
    double total=t->audio->getNumSamples()/t->rate,previous=0;
    for(auto row:tree){VocalNote n{double(row["start"]),double(row["duration"]),double(row["midi"]),double(row["shift"]),double(row["offset"]),double(row["stretch"]),double(row["correction"])};
        if(!std::isfinite(n.start)||!std::isfinite(n.duration)||!std::isfinite(n.midi)||!std::isfinite(n.shift)||!std::isfinite(n.offset)||!std::isfinite(n.stretch)||!std::isfinite(n.correction)||n.start<0||n.duration<=0||n.start+n.duration>total+.001||n.start+n.offset<previous||n.start+n.offset+n.duration*n.stretch>total+.001||n.stretch<.5||n.stretch>2||std::abs(n.shift)>12||n.correction<0||n.correction>1)return false;
        previous=n.start+n.offset+n.duration*n.stretch;t->notes.push_back(n);if(t->notes.size()>10000)return false;
    }
    juce::ScopedLock l(documentLock);++revision;active=nullptr;take=t;pendingTake=t;undoStack.clear();status=Working;lastMessage="Restoring captured audio and edits...";notify();return true;
}
}
