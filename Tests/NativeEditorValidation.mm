#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AUCocoaUIView.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace {
void require(bool okay,const char* message) {if(!okay)throw std::runtime_error(message);}
void check(OSStatus status,const char* operation) {
    if(status!=noErr)throw std::runtime_error(std::string(operation)+": OSStatus "+std::to_string(status));
}
void pump() {[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.05]];}
struct Unit {
    AudioUnit value=nullptr;
    ~Unit(){if(value){AudioUnitUninitialize(value);AudioComponentInstanceDispose(value);}}
};
struct Source {
    double rate;int sample=0;
    static OSStatus input(void* context,AudioUnitRenderActionFlags*,const AudioTimeStamp*,UInt32,UInt32 count,AudioBufferList* io) {
        auto& source=*static_cast<Source*>(context);
        for(UInt32 ch=0;ch<io->mNumberBuffers;++ch) {
            auto* dest=static_cast<float*>(io->mBuffers[ch].mData);if(!dest)return kAudio_ParamError;
            for(UInt32 i=0;i<count;++i)dest[i]=float(.18*std::sin(6.283185307179586*220*(source.sample+i)/source.rate));
        }
        return noErr;
    }
};
std::shared_ptr<void> cocoaView(AudioUnit unit) {
    @autoreleasepool {
        UInt32 size=0;Boolean writable=false;
        check(AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_CocoaUI,kAudioUnitScope_Global,0,&size,&writable),"Cocoa UI info");
        std::vector<unsigned char> storage(size);
        auto* info=reinterpret_cast<AudioUnitCocoaViewInfo*>(storage.data());
        check(AudioUnitGetProperty(unit,kAudioUnitProperty_CocoaUI,kAudioUnitScope_Global,0,info,&size),"Cocoa UI factory");
        NSURL* url=(NSURL*)info->mCocoaAUViewBundleLocation;
        NSBundle* bundle=[NSBundle bundleWithURL:url];[bundle load];
        Class factoryClass=NSClassFromString((NSString*)info->mCocoaAUViewClass[0]);
        CFRelease(info->mCocoaAUViewClass[0]);CFRelease(info->mCocoaAUViewBundleLocation);
        require(factoryClass!=Nil,"AU Cocoa factory class");
        id<AUCocoaUIBase> factory=[[factoryClass alloc] init];
        NSView* view=[factory uiViewForAudioUnit:unit withSize:NSMakeSize(1120,800)];
        [view retain];[(NSObject*)factory release];require(view!=nil,"AU editor created");
        auto owner=std::shared_ptr<void>((void*)view,[](void* p){NSView* v=(NSView*)p;[v removeFromSuperview];[v release];});
        for(NSSize bounds:{NSMakeSize(960,720),NSMakeSize(1440,1100),NSMakeSize(1120,800)}) {
            [view setFrameSize:bounds];[view setNeedsDisplay:YES];pump();[view displayIfNeeded];
        }
        return owner;
    }
}

void run(double rate,int channels) {
    AudioComponentDescription description{};description.componentType=kAudioUnitType_Effect;
    description.componentSubType='Mtmm';description.componentManufacturer='Sbcd';
    auto component=AudioComponentFindNext(nullptr,&description);require(component!=nullptr,"installed AU available");
    Unit unit;check(AudioComponentInstanceNew(component,&unit.value),"instantiate AU");
    AudioStreamBasicDescription format{};format.mSampleRate=rate;format.mFormatID=kAudioFormatLinearPCM;
    format.mFormatFlags=kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved;
    format.mBytesPerPacket=format.mBytesPerFrame=sizeof(float);format.mFramesPerPacket=1;
    format.mChannelsPerFrame=UInt32(channels);format.mBitsPerChannel=32;
    for(auto scope:{kAudioUnitScope_Input,kAudioUnitScope_Output})
        check(AudioUnitSetProperty(unit.value,kAudioUnitProperty_StreamFormat,scope,0,&format,sizeof(format)),"stream format");
    UInt32 block=512;check(AudioUnitSetProperty(unit.value,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&block,sizeof(block)),"block size");
    Source source{rate};AURenderCallbackStruct callback{Source::input,&source};
    check(AudioUnitSetProperty(unit.value,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&callback,sizeof(callback)),"source callback");
    // Logic may request its editor before audio initialization.
    {auto editor=cocoaView(unit.value);}
    check(AudioUnitInitialize(unit.value),"initialize AU");
    {auto editor=cocoaView(unit.value);}
    auto editor=cocoaView(unit.value);
    Float64 delay=0;UInt32 bytes=sizeof(delay);
    check(AudioUnitGetProperty(unit.value,kAudioUnitProperty_Latency,kAudioUnitScope_Global,0,&delay,&bytes),"latency");
    std::vector<unsigned char> storage(sizeof(AudioBufferList)+sizeof(AudioBuffer)*size_t(channels-1));
    auto* buffers=reinterpret_cast<AudioBufferList*>(storage.data());buffers->mNumberBuffers=UInt32(channels);
    std::vector<std::vector<float>> audio(static_cast<size_t>(channels),std::vector<float>(block));
    double energy=0;int measured=0;
    for(int offset=0;offset<int(rate*2);offset+=int(block)) {
        source.sample=offset;
        for(int ch=0;ch<channels;++ch){buffers->mBuffers[ch].mNumberChannels=1;buffers->mBuffers[ch].mDataByteSize=block*sizeof(float);buffers->mBuffers[ch].mData=audio[size_t(ch)].data();}
        AudioTimeStamp time{};time.mSampleTime=offset;time.mFlags=kAudioTimeStampSampleTimeValid;AudioUnitRenderActionFlags flags=0;
        check(AudioUnitRender(unit.value,&flags,&time,0,block,buffers),"render with editor open");
        for(int ch=0;ch<channels;++ch)for(UInt32 i=0;i<block;++i) {
            float sample=audio[size_t(ch)][i];require(std::isfinite(sample),"finite audio with editor open");
            if(offset+int(i)>delay*rate){energy+=sample*sample;++measured;}
        }
        if(offset%4096==0)pump();
    }
    double rms=std::sqrt(energy/measured);require(rms>.01,"non-silent audio with editor open");
    std::cout<<"PASS installed Cocoa editor open before initialize/reopen/resize and audio "<<rate<<" Hz "<<channels<<" channels RMS "<<rms<<std::endl;
}
}
int main() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        try {
            for(double rate:{44100.,48000.})for(int channels:{1,2})run(rate,channels);
            std::cout<<"FAILURES 0"<<std::endl;return 0;
        }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<std::endl;return 1;}
    }
}
