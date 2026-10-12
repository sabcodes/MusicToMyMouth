#include "Parameters.h"
namespace mouth {
const std::vector<ParameterSpec>& continuousParameters() {
    static const std::vector<ParameterSpec> specs {
        {"input", "Input", "dB", -24, 24, 0, .1f, 1},
        {"output", "Output", "dB", -24, 12, 0, .1f, 1},
        {"mix", "Global mix", "%", 0, 100, 100, .1f, 1},
        {"character", "Natural / robotic", "%", 0, 100, 20, .1f, 1},
        {"strength", "Correction", "%", 0, 100, 85, .1f, 1},
        {"retune", "Retune", "ms", 0, 300, 70, .1f, .5f},
        {"humanize", "Humanize", "%", 0, 100, 35, .1f, 1},
        {"transpose", "Transpose", "st", -12, 12, 0, .01f, 1},
        {"formant", "Formant", "st", -12, 12, 0, .01f, 1},
        {"highpass", "High pass", "Hz", 20, 500, 80, 1, .45f},
        {"lowgain", "Low shelf", "dB", -12, 12, 0, .1f, 1},
        {"midfreq", "Mid frequency", "Hz", 200, 8000, 2200, 1, .4f},
        {"midgain", "Mid gain", "dB", -12, 12, 0, .1f, 1},
        {"midq", "Mid width Q", "", .3f, 5, .8f, .01f, .5f},
        {"air", "Air shelf", "dB", -12, 12, 2, .1f, 1},
        {"compThreshold", "Compressor threshold", "dB", -60, 0, -20, .1f, 1},
        {"compRatio", "Compressor ratio", ":1", 1, 12, 3, .1f, .5f},
        {"compAttack", "Compressor attack", "ms", .1f, 100, 12, .1f, .4f},
        {"compRelease", "Compressor release", "ms", 10, 1000, 120, 1, .4f},
        {"compMakeup", "Compressor makeup", "dB", 0, 24, 3, .1f, 1},
        {"deessFreq", "De-esser frequency", "Hz", 2000, 12000, 6500, 1, .6f},
        {"deessThreshold", "De-esser threshold", "dB", -60, 0, -25, .1f, 1},
        {"deessAmount", "De-esser reduction", "dB", 0, 18, 6, .1f, 1},
        {"gateThreshold", "Gate threshold", "dB", -90, -10, -60, .1f, 1},
        {"gateRelease", "Gate release", "ms", 20, 1000, 180, 1, .5f},
        {"gateRange", "Gate range", "dB", 0, 80, 30, .1f, 1},
        {"drive", "Saturation drive", "dB", 0, 30, 3, .1f, 1},
        {"satMix", "Saturation mix", "%", 0, 100, 20, .1f, 1},
        {"room", "Reverb size", "%", 0, 100, 45, .1f, 1},
        {"damping", "Reverb damping", "%", 0, 100, 55, .1f, 1},
        {"preDelay", "Reverb pre-delay", "ms", 0, 200, 25, 1, 1},
        {"reverbMix", "Reverb mix", "%", 0, 100, 12, .1f, 1},
        {"delayTime", "Delay time", "ms", 20, 2000, 375, 1, .6f},
        {"feedback", "Delay feedback", "%", 0, 90, 25, .1f, 1},
        {"delayTone", "Delay low pass", "Hz", 500, 16000, 5000, 1, .5f},
        {"delayMix", "Delay mix", "%", 0, 100, 10, .1f, 1},
        {"modRate", "Modulation rate", "Hz", .05f, 10, .8f, .01f, .5f},
        {"modDepth", "Modulation depth", "%", 0, 100, 25, .1f, 1},
        {"modMix", "Modulation mix", "%", 0, 100, 15, .1f, 1}
    }; return specs;
}
const juce::StringArray& moduleNames() { static const juce::StringArray n {"EQ", "Compression", "De-esser", "Gate", "Saturation", "Reverb", "Delay", "Modulation"}; return n; }
const juce::StringArray& keyNames() { static const juce::StringArray n {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}; return n; }
const juce::StringArray& scaleNames() { static const juce::StringArray n {"Chromatic", "Major", "Natural minor", "Harmonic minor", "Major pentatonic", "Minor pentatonic"}; return n; }
juce::AudioProcessorValueTreeState::ParameterLayout makeParameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    for (auto& s : continuousParameters())
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(s.id, 1), s.name,
            juce::NormalisableRange<float>(s.low, s.high, s.step, s.skew), s.initial,
            juce::AudioParameterFloatAttributes().withLabel(s.unit)));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("key",1), "Key", keyNames(), 0));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("scale",1), "Scale", scaleNames(), 1));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("division",1), "Delay division", juce::StringArray{"1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2"}, 3));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("modStyle",1), "Modulation style", juce::StringArray{"Chorus", "Flanger", "Tremolo"}, 0));
    const std::pair<const char*, const char*> switches[] {
        {"tuneOn", "Automatic tuning"}, {"preserveFormant", "Preserve formants"},
        {"eqOn", "EQ enabled"}, {"compOn", "Compression enabled"},
        {"deessOn", "De-esser enabled"}, {"gateOn", "Gate enabled"},
        {"satOn", "Saturation enabled"}, {"reverbOn", "Reverb enabled"},
        {"delayOn", "Delay enabled"}, {"modOn", "Modulation enabled"},
        {"delaySync", "Delay tempo sync"}, {"editedOn", "Play edited Capture"}
    };
    for (const auto& [id, name] : switches)
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id, 1), name,
            juce::String(id) != "gateOn" && juce::String(id) != "modOn"));
    return p;
}
float value(const juce::AudioProcessorValueTreeState& p, const char* id) { return p.getRawParameterValue(id)->load(); }
const std::vector<Preset>& factoryPresets() {
    static const std::vector<Preset> presets {
        {"Natural / Studio lead", {{"character",10},{"strength",75},{"retune",100},{"humanize",65},{"compRatio",3},{"air",2},{"delayMix",6},{"reverbMix",9}}},
        {"Natural / Intimate", {{"character",0},{"strength",60},{"retune",150},{"humanize",80},{"compRatio",2},{"air",1},{"delayMix",0},{"reverbMix",7},{"satMix",10}}},
        {"Natural / Pop shine", {{"character",25},{"strength",90},{"retune",60},{"humanize",40},{"air",4},{"compRatio",4},{"compMakeup",4},{"reverbMix",14}}},
        {"Hard tune / Modern lead", {{"character",90},{"strength",100},{"retune",8},{"humanize",0},{"compRatio",4},{"delayMix",16},{"reverbMix",13}}},
        {"Hard tune / Instant snap", {{"character",100},{"strength",100},{"retune",0},{"humanize",0},{"drive",7},{"satMix",28},{"delayMix",9}}},
        {"Hard tune / Melodic space", {{"character",80},{"strength",100},{"retune",15},{"humanize",5},{"delayMix",24},{"feedback",40},{"room",75},{"reverbMix",24}}},
        {"Robot / Deep formant", {{"character",100},{"strength",100},{"retune",0},{"humanize",0},{"formant",-5},{"drive",10},{"satMix",40},{"reverbMix",8}}},
        {"Robot / Bright alien", {{"character",100},{"strength",100},{"retune",0},{"humanize",0},{"formant",5},{"transpose",3},{"modOn",1},{"modMix",20}}},
        {"Robot / Telephone pulse", {{"character",100},{"strength",100},{"retune",0},{"humanize",0},{"highpass",400},{"air",-12},{"modOn",1},{"modStyle",2},{"modDepth",70},{"modRate",4}}},
        {"Space / Wide ambience", {{"room",85},{"reverbMix",30},{"delayMix",24},{"feedback",42},{"modOn",1},{"modMix",12}}},
        {"Space / Slap lead", {{"delaySync",0},{"delayTime",110},{"feedback",12},{"delayMix",22},{"reverbMix",8},{"preDelay",10}}},
        {"Space / Dotted echoes", {{"division",4},{"delayMix",30},{"feedback",48},{"delayTone",3600},{"room",65},{"reverbMix",20}}}
    }; return presets;
}
}
