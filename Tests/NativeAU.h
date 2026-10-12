#pragma once
#include <AudioToolbox/AudioToolbox.h>

namespace nativeAU {
inline void check(OSStatus status,const char* operation) {
    if(status!=noErr)throw std::runtime_error(std::string(operation)+": OSStatus "+std::to_string(status));
}
struct Source {
    const juce::AudioBuffer<float>* audio;
    double rate;
    int64_t sample=0;
    static OSStatus input(void* context,AudioUnitRenderActionFlags*,const AudioTimeStamp*,UInt32,UInt32 count,AudioBufferList* io) {
        auto& s=*static_cast<Source*>(context);
        for(UInt32 ch=0;ch<io->mNumberBuffers;++ch) {
            auto* dest=static_cast<float*>(io->mBuffers[ch].mData);
            if(!dest)return kAudio_ParamError;
            for(UInt32 i=0;i<count;++i)dest[i]=s.sample+i<s.audio->getNumSamples()?s.audio->getSample(int(ch),int(s.sample+i)):0.f;
        }return noErr;
    }
    static OSStatus tempo(void* context,Float64* beat,Float64* bpm) {
        auto& s=*static_cast<Source*>(context);if(bpm)*bpm=129.2;if(beat)*beat=(s.sample/s.rate)*129.2/60;return noErr;
    }
    static OSStatus transport(void* context,Boolean* playing,Boolean* changed,Float64* sample,Boolean* cycling,Float64* begin,Float64* end) {
        auto& s=*static_cast<Source*>(context);if(playing)*playing=true;if(changed)*changed=false;if(sample)*sample=double(s.sample);if(cycling)*cycling=false;if(begin)*begin=0;if(end)*end=0;return noErr;
    }
};
struct Unit {
    AudioUnit unit=nullptr;
    ~Unit(){if(unit){AudioUnitUninitialize(unit);AudioComponentInstanceDispose(unit);}}
};
inline juce::AudioBuffer<float> render(const juce::AudioBuffer<float>& source,double sr,int preset,int& latency,bool bypassed=false) {
    AudioComponentDescription description{};description.componentType=kAudioUnitType_Effect;description.componentSubType='Mtmm';description.componentManufacturer='Sbcd';
    auto component=AudioComponentFindNext(nullptr,&description);if(!component)throw std::runtime_error("Installed AU unavailable");
    Unit u;check(AudioComponentInstanceNew(component,&u.unit),"instantiate AU");
    AudioStreamBasicDescription format{};format.mSampleRate=sr;format.mFormatID=kAudioFormatLinearPCM;
    format.mFormatFlags=kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved;
    format.mBytesPerPacket=format.mBytesPerFrame=sizeof(float);format.mFramesPerPacket=1;format.mChannelsPerFrame=UInt32(source.getNumChannels());format.mBitsPerChannel=32;
    check(AudioUnitSetProperty(u.unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&format,sizeof(format)),"AU input format");
    check(AudioUnitSetProperty(u.unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,sizeof(format)),"AU output format");
    UInt32 block=512;check(AudioUnitSetProperty(u.unit,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&block,sizeof(block)),"AU block size");
    Source input{&source,sr};AURenderCallbackStruct callback{Source::input,&input};
    check(AudioUnitSetProperty(u.unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&callback,sizeof(callback)),"AU source callback");
    HostCallbackInfo host{};host.hostUserData=&input;host.beatAndTempoProc=Source::tempo;host.transportStateProc=Source::transport;
    check(AudioUnitSetProperty(u.unit,kAudioUnitProperty_HostCallbacks,kAudioUnitScope_Global,0,&host,sizeof(host)),"AU host transport");
    AUPreset selected{preset,CFSTR("Validation sound")};check(AudioUnitSetProperty(u.unit,kAudioUnitProperty_PresentPreset,kAudioUnitScope_Global,0,&selected,sizeof(selected)),"AU preset");
    // Stable AU parameter IDs are JUCE's string hashes, as published by auval.
    check(AudioUnitSetParameter(u.unit,UInt32(juce::String("key").hashCode()),kAudioUnitScope_Global,0,0,0),"AU song key");
    check(AudioUnitSetParameter(u.unit,UInt32(juce::String("scale").hashCode()),kAudioUnitScope_Global,0,2,0),"AU song scale");
    check(AudioUnitInitialize(u.unit),"initialize AU");
    UInt32 bypass=bypassed?1u:0u;check(AudioUnitSetProperty(u.unit,kAudioUnitProperty_BypassEffect,kAudioUnitScope_Global,0,&bypass,sizeof(bypass)),"AU host bypass");
    Float64 delay=0;UInt32 bytes=sizeof(delay);check(AudioUnitGetProperty(u.unit,kAudioUnitProperty_Latency,kAudioUnitScope_Global,0,&delay,&bytes),"AU latency");latency=int(std::round(delay*sr));
    juce::AudioBuffer<float> output(source.getNumChannels(),source.getNumSamples()+latency);output.clear();
    std::vector<unsigned char> storage(sizeof(AudioBufferList)+sizeof(::AudioBuffer)*(size_t(source.getNumChannels())-1));
    auto* buffers=reinterpret_cast<AudioBufferList*>(storage.data());buffers->mNumberBuffers=UInt32(source.getNumChannels());
    for(int offset=0;offset<output.getNumSamples();offset+=int(block)) {
        UInt32 count=UInt32(std::min(int(block),output.getNumSamples()-offset));input.sample=offset;
        for(int ch=0;ch<output.getNumChannels();++ch){buffers->mBuffers[ch].mNumberChannels=1;buffers->mBuffers[ch].mDataByteSize=count*sizeof(float);buffers->mBuffers[ch].mData=output.getWritePointer(ch,offset);}
        AudioTimeStamp time{};time.mSampleTime=offset;time.mFlags=kAudioTimeStampSampleTimeValid;AudioUnitRenderActionFlags flags=0;
        check(AudioUnitRender(u.unit,&flags,&time,0,count,buffers),"AU stem render");
    }return output;
}
}
