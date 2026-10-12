#pragma once
#include "PluginProcessor.h"
class MusicToMyMouthEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MusicToMyMouthEditor(MusicToMyMouthProcessor&);
    ~MusicToMyMouthEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Theme;
    struct Control;
    struct ModuleCard;
    struct PianoRoll;
    void timerCallback() override;
    void selectNote(int);
    void applyNote();
    void presetDialog(bool saving);
    void addControl(juce::Component&,const char* id,std::vector<std::unique_ptr<Control>>&);
    void addToggle(juce::Component&,juce::ToggleButton&,const char* id,const juce::String& label);
    MusicToMyMouthProcessor& processor;
    std::unique_ptr<Theme> theme;
    juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
    juce::Component tunePage,rackPage,editPage,rackContent;
    juce::Viewport rackViewport,noteViewport;
    std::vector<std::unique_ptr<Control>> tuneControls;
    std::vector<std::unique_ptr<ModuleCard>> modules;
    std::unique_ptr<PianoRoll> piano;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> combos;
    juce::ComboBox presets,key,scale;
    juce::TextButton savePreset{"Save sound"},loadPreset{"Load sound"};
    juce::ToggleButton tuneOn,preserveFormant,editedOn;
    juce::TextButton captureButton{"Arm Capture"},stopCapture{"Finish"},clearCapture{"Clear"},undoButton{"Undo"},applyButton{"Apply note edit"};
    juce::Label captureStatus,noteDetails,pitchStatus;
    juce::Slider noteShift,noteOffset,noteLength,noteCorrection,zoom;
    juce::Label shiftLabel,offsetLabel,lengthLabel,correctionLabel,zoomLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<const mouth::Take> displayedTake;
    int selected=-1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicToMyMouthEditor)
};
