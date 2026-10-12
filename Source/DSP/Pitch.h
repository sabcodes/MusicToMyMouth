#pragma once
#include "../Parameters.h"
#include <rubberband/RubberBandLiveShifter.h>
namespace mouth {
struct PitchReading { double midi = 0, confidence = 0; bool voiced = false; };
class YinDetector {
public:
    void prepare(double sampleRate);
    PitchReading push(float sample);
    PitchReading analyse(const float* samples, int count);
    void reset();
private:
    PitchReading detect();
    double rate = 12000;
    std::array<float, 1024> ring{}, frame{}, diff{};
    int write = 0, hop = 0, filled = 0;
    int decimation = 4, decimationCount = 0;
    float sum = 0;
    PitchReading last;
};
double nearestNote(double midi, int key, int scale);
class PitchEngine {
public:
    void prepare(double rate, int channels);
    void reset();
    void process(juce::AudioBuffer<float>&, const juce::AudioProcessorValueTreeState&, const bool* manualMask);
    int latency() const { return delay; }
    std::atomic<float> detectedMidi{0}, confidence{0}, correctionCents{0};
private:
    std::unique_ptr<RubberBand::RubberBandLiveShifter> shifter;
    YinDetector detector;
    juce::AudioBuffer<float> input, output;
    double sampleRate = 48000, shiftSemitones = 0;
    int block = 512, cursor = 0, delay = 0;
};
}
