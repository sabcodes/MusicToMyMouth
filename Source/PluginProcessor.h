#pragma once
#include "DSP/Pitch.h"
#include "DSP/VocalChain.h"
#include "Model/Capture.h"
class MusicToMyMouthProcessor final : public juce::AudioProcessor {
public:
    MusicToMyMouthProcessor();
    ~MusicToMyMouthProcessor() override = default;
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {return true;}
    const juce::String getName() const override {return "MusicToMyMouth";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 12;}
    int getNumPrograms() override {return int(mouth::factoryPresets().size());}
    int getCurrentProgram() override {return currentPreset.load();}
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    void setParameterValue(const juce::String&,float);
    bool savePreset(const juce::File&);
    bool loadPreset(const juce::File&);
    void moveModule(int slot,int delta);
    uint32_t moduleOrder() const {return order.load();}
    juce::AudioProcessorValueTreeState parameters;
    mouth::PitchEngine pitch;
    mouth::CaptureManager capture;
    std::atomic<float> inputLevel{0},outputLevel{0};
    std::atomic<bool> hostTimelineAvailable{false};
private:
    void processSection(juce::AudioBuffer<float>&,double,bool,double);
    juce::ValueTree soundState();
    bool applySoundState(const juce::ValueTree&);
    mouth::VocalChain chain;
    juce::AudioBuffer<float> dryDelay,dryScratch;
    std::unique_ptr<bool[]> manualMask;
    juce::SmoothedValue<float> inputGain,outputGain,mix;
    std::atomic<uint32_t> order{mouth::VocalChain::defaultOrder};
    std::atomic<int> currentPreset{0};
    int dryCursor=0,blockCapacity=512;
    double rate=48000;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicToMyMouthProcessor)
};
