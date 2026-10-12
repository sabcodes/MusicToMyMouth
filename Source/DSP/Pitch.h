#pragma once
#include "../Parameters.h"
#include <rubberband/RubberBandStretcher.h>
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
    std::array<float, 1024> ring{}, frame{}, diff{}, rawDiff{};
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
    struct Diagnostics : RubberBand::RubberBandStretcher::Logger {
        std::atomic<int> errors{0}, warnings{0};
        void count(const char* message);
        void log(const char* message) override { count(message); }
        void log(const char* message, double) override { count(message); }
        void log(const char* message, double, double) override { count(message); }
    };
    void drain();
    std::shared_ptr<Diagnostics> diagnostics = std::make_shared<Diagnostics>();
    std::unique_ptr<RubberBand::RubberBandStretcher> shifter;
    YinDetector detector;
    juce::AudioBuffer<float> input, staging, fifo;
    double sampleRate = 48000, shiftSemitones = 0;
    int block = 512, cursor = 0, delay = 0;
    int read = 0, write = 0, queued = 0, discard = 0;
public:
    int engineErrors() const { return diagnostics->errors.load(); }
    int engineWarnings() const { return diagnostics->warnings.load(); }
    std::atomic<int> underruns{0}, overflows{0};
};
}
