#include "VocalChain.h"
namespace mouth {
namespace {
float db(float v) { return juce::Decibels::gainToDecibels(v,-100.f); }
float gain(float v) { return juce::Decibels::decibelsToGain(v); }
struct EnvelopeRates {
    double attack, release;
    EnvelopeRates(double attackMs, double releaseMs, double sr)
        : attack(std::exp(-1 / (sr * .001 * attackMs))), release(std::exp(-1 / (sr * .001 * releaseMs))) {}
};
float follower(float sample, float env, const EnvelopeRates& rates) {
    const double c = sample > env ? rates.attack : rates.release;
    return float(c * env + (1 - c) * sample);
}
float delayed(const juce::AudioBuffer<float>& b,int ch,int write,double delay) {
    double index=write-delay;while(index<0)index+=b.getNumSamples();
    int i=int(index)%b.getNumSamples();float f=float(index-std::floor(index));
    return b.getSample(ch,i)*(1-f)+b.getSample(ch,(i+1)%b.getNumSamples())*f;
}
}
bool VocalChain::validOrder(uint32_t order) { unsigned seen=0;for(int i=0;i<8;++i){unsigned m=(order>>(i*3))&7;if(seen&(1u<<m))return false;seen|=1u<<m;}return seen==255 && (order>>24)==0; }
void VocalChain::prepare(double rate,int maxBlock,int channels) {
    oversampling=std::make_unique<juce::dsp::Oversampling<float>>(size_t(channels),1,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
    oversampling->initProcessing(size_t(maxBlock));sr=rate*2;
    echo.setSize(channels,int(sr*4)+4);preDelay.setSize(channels,int(sr*.25)+4);modDelay.setSize(channels,int(sr*.08)+4);wet.setSize(channels,maxBlock*2);
    reverb.setSampleRate(sr); reset();
}
void VocalChain::reset() {
    if(oversampling)oversampling->reset();reverb.reset();echo.clear();preDelay.clear();modDelay.clear();wet.clear();
    for(auto& channel:eq)for(auto& filter:channel)filter.reset();for(auto& f:sibilance)f.reset();for(auto& f:echoTone)f.reset();
    echoCursor=preCursor=modCursor=0;phase=0;smoothDelay=sr*.375;compEnv=deessEnv=gateEnv=0;gateGain=1;gateOpen=true;
}
void VocalChain::process(juce::AudioBuffer<float>& b,const juce::AudioProcessorValueTreeState& p,uint32_t order,double bpm) {
    juce::dsp::AudioBlock<float> base(b);auto up=oversampling->processSamplesUp(base);
    for(int slot=0;slot<8;++slot)module(int((order>>(slot*3))&7),up,p,bpm);
    oversampling->processSamplesDown(base);
}
void VocalChain::module(int m,juce::dsp::AudioBlock<float>& b,const juce::AudioProcessorValueTreeState& p,double bpm) {
    static const char* enabled[]{"eqOn","compOn","deessOn","gateOn","satOn","reverbOn","delayOn","modOn"};
    if(value(p,enabled[m])<.5f)return;
    int channels=int(b.getNumChannels()),n=int(b.getNumSamples());
    auto v=[&](const char* id){return value(p,id);};
    using C=juce::dsp::IIR::ArrayCoefficients<float>;
    if(m==0) {
        for(int ch=0;ch<channels;++ch) {
            eq[size_t(ch)][0].set(C::makeHighPass(sr,v("highpass")));
            eq[size_t(ch)][1].set(C::makeLowShelf(sr,180,.707f,gain(v("lowgain"))));
            eq[size_t(ch)][2].set(C::makePeakFilter(sr,v("midfreq"),v("midq"),gain(v("midgain"))));
            eq[size_t(ch)][3].set(C::makeHighShelf(sr,9000,.707f,gain(v("air"))));
            for(int i=0;i<n;++i)for(auto& f:eq[size_t(ch)]) b.getChannelPointer(size_t(ch))[i]=f.tick(b.getChannelPointer(size_t(ch))[i]);
        }
    } else if(m==1) {
        const EnvelopeRates rates(v("compAttack"), v("compRelease"), sr);
        const float threshold = v("compThreshold"), slope = 1 - 1 / v("compRatio"), makeup = v("compMakeup");
        for(int i=0;i<n;++i) {
            float level=0;for(int ch=0;ch<channels;++ch)level=std::max(level,std::abs(b.getChannelPointer(size_t(ch))[i]));
            compEnv=follower(level,compEnv,rates);
            float over=db(compEnv)-threshold,knee=6,curve=0;
            if(over>knee*.5f)curve=over;else if(over>-knee*.5f)curve=(over+knee*.5f)*(over+knee*.5f)/(2*knee);
            float g=gain(-curve*slope+makeup);
            for(int ch=0;ch<channels;++ch)b.getChannelPointer(size_t(ch))[i]*=g;
        }
    } else if(m==2) {
        const EnvelopeRates rates(1, 65, sr);
        const float threshold = v("deessThreshold"), amount = v("deessAmount");
        for(int ch=0;ch<channels;++ch)sibilance[size_t(ch)].set(C::makeHighPass(sr,v("deessFreq")));
        for(int i=0;i<n;++i) {
            float high[2]{},level=0;
            for(int ch=0;ch<channels;++ch){high[ch]=sibilance[size_t(ch)].tick(b.getChannelPointer(size_t(ch))[i]);level=std::max(level,std::abs(high[ch]));}
            deessEnv=follower(level,deessEnv,rates);
            float g=gain(-std::min(amount,std::max(0.f,db(deessEnv)-threshold)*.75f));
            for(int ch=0;ch<channels;++ch)b.getChannelPointer(size_t(ch))[i]+=high[ch]*(g-1);
        }
    } else if(m==3) {
        const EnvelopeRates detectorRates(1, 40, sr), gainRates(2, v("gateRelease"), sr);
        const float threshold = v("gateThreshold"), closedGain = gain(-v("gateRange"));
        for(int i=0;i<n;++i) {
            float level=0;for(int ch=0;ch<channels;++ch)level=std::max(level,std::abs(b.getChannelPointer(size_t(ch))[i]));
            gateEnv=follower(level,gateEnv,detectorRates);float d=db(gateEnv);
            if(d>threshold)gateOpen=true;else if(d<threshold-4)gateOpen=false;
            gateGain=follower(gateOpen?1.f:closedGain,gateGain,gainRates);
            for(int ch=0;ch<channels;++ch)b.getChannelPointer(size_t(ch))[i]*=gateGain;
        }
    } else if(m==4) {
        float drive=gain(v("drive")),mix=v("satMix")*.01f,norm=1/std::tanh(drive);
        for(int ch=0;ch<channels;++ch)for(int i=0;i<n;++i) {auto& x=b.getChannelPointer(size_t(ch))[i];x=x*(1-mix)+std::tanh(x*drive)*norm*mix;}
    } else if(m==5) {
        const float preDelayMs=v("preDelay");int delay=juce::jlimit(1,preDelay.getNumSamples()-1,int(sr*preDelayMs*.001));
        for(int i=0;i<n;++i) {
            for(int ch=0;ch<channels;++ch) {
                float x=b.getChannelPointer(size_t(ch))[i];preDelay.setSample(ch,preCursor,x);
                wet.setSample(ch,i,preDelayMs<1?x:delayed(preDelay,ch,preCursor,delay));
            } preCursor=(preCursor+1)%preDelay.getNumSamples();
        }
        juce::Reverb::Parameters r;r.roomSize=v("room")*.01f;r.damping=v("damping")*.01f;r.wetLevel=1;r.dryLevel=0;r.width=1;r.freezeMode=0;reverb.setParameters(r);
        if(channels==2)reverb.processStereo(wet.getWritePointer(0),wet.getWritePointer(1),n);else reverb.processMono(wet.getWritePointer(0),n);
        float mix=v("reverbMix")*.01f;
        for(int ch=0;ch<channels;++ch)for(int i=0;i<n;++i)b.getChannelPointer(size_t(ch))[i]=b.getChannelPointer(size_t(ch))[i]*(1-mix)+wet.getSample(ch,i)*mix;
    } else if(m==6) {
        static const double beats[]{.25,.5,.75,1,1.5,2};
        double target=sr*(v("delaySync")>.5f?60/std::max(20.,bpm)*beats[juce::jlimit(0,5,int(v("division")))]:v("delayTime")*.001);
        target=juce::jlimit(1.,double(echo.getNumSamples()-2),target);
        for(int ch=0;ch<channels;++ch)echoTone[size_t(ch)].set(C::makeLowPass(sr,v("delayTone")));
        float mix=v("delayMix")*.01f,feedback=v("feedback")*.01f;
        for(int i=0;i<n;++i) {
            smoothDelay+=.0003*(target-smoothDelay);
            for(int ch=0;ch<channels;++ch) {
                auto& x=b.getChannelPointer(size_t(ch))[i];float d=echoTone[size_t(ch)].tick(delayed(echo,ch,echoCursor,smoothDelay));
                echo.setSample(ch,echoCursor,std::tanh(x+d*feedback));x=x*(1-mix)+d*mix;
            } echoCursor=(echoCursor+1)%echo.getNumSamples();
        }
    } else if(m==7) {
        float mix=v("modMix")*.01f,depth=v("modDepth")*.01f;int style=int(v("modStyle"));const double phaseStep=juce::MathConstants<double>::twoPi*v("modRate")/sr;
        for(int i=0;i<n;++i) {
            for(int ch=0;ch<channels;++ch) {
                auto& x=b.getChannelPointer(size_t(ch))[i];double wave=std::sin(phase+(ch==1?juce::MathConstants<double>::halfPi:0));
                modDelay.setSample(ch,modCursor,x);
                float effected=style==2?x*float(1-depth*(.5+.5*wave)):delayed(modDelay,ch,modCursor,sr*((style==0?.018:.002)+depth*(style==0?.008:.0015)*wave));
                x=x*(1-mix)+effected*mix;
            }modCursor=(modCursor+1)%modDelay.getNumSamples();phase+=phaseStep;if(phase>juce::MathConstants<double>::twoPi)phase-=juce::MathConstants<double>::twoPi;
        }
    }
}
}
