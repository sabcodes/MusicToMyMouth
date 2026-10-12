#pragma once
#include <JuceHeader.h>
namespace mouth {
struct ParameterSpec { const char* id; const char* name; const char* unit; float low, high, initial, step, skew; };
const std::vector<ParameterSpec>& continuousParameters();
juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
float value(const juce::AudioProcessorValueTreeState&, const char*);
struct Preset { juce::String name; std::map<juce::String, float> values; };
const std::vector<Preset>& factoryPresets();
constexpr int moduleCount = 8;
const juce::StringArray& moduleNames();
const juce::StringArray& scaleNames();
const juce::StringArray& keyNames();
}
