#pragma once
#include "../DSP/Pitch.h"
#include <rubberband/RubberBandStretcher.h>
namespace mouth {
struct VocalNote {
    double start=0, duration=0, midi=60;
    double shift=0, offset=0, stretch=1, correction=0;
};
struct Take {
    std::shared_ptr<juce::AudioBuffer<float>> audio = std::make_shared<juce::AudioBuffer<float>>();
    double rate=48000, anchor=0;
    std::vector<VocalNote> notes;
    int selectedKey=0, selectedScale=1;
};
struct Render { std::vector<uint8_t> manual; juce::AudioBuffer<float> audio; double rate=48000,anchor=0; };
class CaptureManager : private juce::Thread {
public:
    enum Stage { Empty, Armed, Recording, Queued, Working, Available };
    CaptureManager();
    ~CaptureManager() override;
    void prepare(double rate,int channels);
    bool arm(int key,int scale);
    void finish();
    void clear();
    void capture(const juce::AudioBuffer<float>&,double seconds,bool playing);
    void replace(juce::AudioBuffer<float>&,bool* manualMask,double seconds,bool useEdits,double hostRate);
    Stage stage() const {return Stage(status.load());}
    double capturedSeconds() const {return double(written.load())/displayRate.load();}
    double limitSeconds() const {return maximumSeconds.load();}
    std::shared_ptr<const Take> document() const;
    bool editNote(size_t index,const VocalNote&);
    void undo();
    juce::ValueTree save();
    bool restore(const juce::ValueTree&);
    juce::String message() const;
private:
    void run() override;
    void analyse(Take&);
    std::unique_ptr<Render> render(const Take&);
    void publish(std::unique_ptr<Render>);
    void reap();
    std::atomic<int> status{Empty},written{0};
    std::atomic<uint64_t> revision{0};
    juce::CriticalSection rawLock;
    std::atomic<bool> finishRequested{false},clearRequested{false};
    juce::AudioBuffer<float> captureBuffer;
    std::atomic<double> displayRate{48000},maximumSeconds{180};
    double captureRate=48000,captureAnchor=0,expectedSeconds=0;
    int key=0,scale=1;
    mutable juce::CriticalSection documentLock;
    std::shared_ptr<Take> take,pendingTake;
    std::vector<std::shared_ptr<Take>> undoStack;
    juce::String lastMessage="Arm Capture, then play a clean vocal section.";
    std::atomic<const Render*> active{nullptr},hazard{nullptr};
    std::vector<std::unique_ptr<Render>> renders;
};
}
