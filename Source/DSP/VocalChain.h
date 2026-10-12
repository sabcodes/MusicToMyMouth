#pragma once
#include "../Parameters.h"
namespace mouth {
class Biquad {
public:
    void set(const std::array<float,6>& c) { for(int i=0;i<6;++i) a[size_t(i)]=c[size_t(i)]/c[3]; }
    float tick(float x) { float y=a[0]*x+z1;z1=a[1]*x-a[4]*y+z2;z2=a[2]*x-a[5]*y;return y; }
    void reset() { z1=z2=0; }
private: std::array<float,6> a{1,0,0,1,0,0}; float z1=0,z2=0;
};
class VocalChain {
public:
    void prepare(double rate,int maxBlock,int channels);
    void reset();
    void process(juce::AudioBuffer<float>&,const juce::AudioProcessorValueTreeState&,uint32_t order,double bpm);
    int latency() const { return oversampling ? int(oversampling->getLatencyInSamples()) : 0; }
    static constexpr uint32_t defaultOrder = 0 | (1<<3) | (2<<6) | (3<<9) | (4<<12) | (5<<15) | (6<<18) | (7<<21);
    static bool validOrder(uint32_t);
private:
    void module(int,juce::dsp::AudioBlock<float>&,const juce::AudioProcessorValueTreeState&,double bpm);
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::array<std::array<Biquad,4>,2> eq;
    std::array<Biquad,2> sibilance,echoTone;
    juce::Reverb reverb;
    juce::AudioBuffer<float> echo,preDelay,modDelay,wet;
    double sr=96000,phase=0;
    int echoCursor=0,preCursor=0,modCursor=0;
    double smoothDelay=36000;
    float compEnv=0,deessEnv=0,gateEnv=0,gateGain=1;
    bool gateOpen=true;
};
}
