#include "PluginEditor.h"
namespace {
const juce::Colour background{0xff141719},card{0xff202629},text{0xffeee8df},muted{0xff9da9aa},accent{0xffffac66};
void label(juce::Label& l,const juce::String& name) {l.setText(name,juce::dontSendNotification);l.setColour(juce::Label::textColourId,muted);}
}
struct MusicToMyMouthEditor::Theme : juce::LookAndFeel_V4 {
    Theme() {setColour(juce::ResizableWindow::backgroundColourId,background);setColour(juce::Slider::thumbColourId,accent);setColour(juce::Slider::trackColourId,accent);setColour(juce::Slider::backgroundColourId,juce::Colour(0xff374247));setColour(juce::Slider::textBoxTextColourId,text);setColour(juce::Slider::textBoxBackgroundColourId,background);setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);setColour(juce::ComboBox::backgroundColourId,card);setColour(juce::ComboBox::textColourId,text);setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff485559));setColour(juce::TextButton::buttonColourId,juce::Colour(0xff344248));setColour(juce::TextButton::textColourOffId,text);setColour(juce::ToggleButton::textColourId,text);setColour(juce::ToggleButton::tickColourId,accent);setColour(juce::TabbedButtonBar::tabTextColourId,muted);setColour(juce::TabbedButtonBar::frontTextColourId,accent);setColour(juce::Label::textColourId,text);}
};
struct MusicToMyMouthEditor::Control : juce::Component {
    juce::Label name;juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    Control(MusicToMyMouthProcessor& p,const char* id) {
        auto* parameter=p.parameters.getParameter(id);label(name,parameter->getName(64));addAndMakeVisible(name);addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,24);
        slider.setTextValueSuffix(" "+parameter->getLabel());attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,id,slider);
    }
    void resized() override {auto b=getLocalBounds();name.setBounds(b.removeFromTop(22));slider.setBounds(b.reduced(2,0));}
};
struct MusicToMyMouthEditor::ModuleCard : juce::Component {
    MusicToMyMouthProcessor& p;int module,slot=0;
    juce::ToggleButton enabled;juce::TextButton up{"Earlier"},down{"Later"};juce::ComboBox choice;
    juce::ToggleButton sync;std::vector<std::unique_ptr<Control>> controls;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment,syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> choiceAttachment;
    ModuleCard(MusicToMyMouthProcessor& processor,int m):p(processor),module(m) {
        static const char* enabledIds[]{"eqOn","compOn","deessOn","gateOn","satOn","reverbOn","delayOn","modOn"};
        static const std::vector<std::vector<const char*>> ids{{"highpass","lowgain","midfreq","midgain","midq","air"},{"compThreshold","compRatio","compAttack","compRelease","compMakeup"},{"deessFreq","deessThreshold","deessAmount"},{"gateThreshold","gateRelease","gateRange"},{"drive","satMix"},{"room","damping","preDelay","reverbMix"},{"delayTime","feedback","delayTone","delayMix"},{"modRate","modDepth","modMix"}};
        enabled.setButtonText(mouth::moduleNames()[m]);addAndMakeVisible(enabled);addAndMakeVisible(up);addAndMakeVisible(down);
        enableAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,enabledIds[m],enabled);
        up.onClick=[this]{p.moveModule(slot,-1);};down.onClick=[this]{p.moveModule(slot,1);};
        for(auto id:ids[size_t(m)]){auto c=std::make_unique<Control>(p,id);addAndMakeVisible(*c);controls.push_back(std::move(c));}
        if(m==6){sync.setButtonText("Sync to Logic");addAndMakeVisible(sync);syncAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"delaySync",sync);choice.addItemList({"1/16","1/8","1/8 dotted","1/4","1/4 dotted","1/2"},1);}
        if(m==7)choice.addItemList({"Chorus","Flanger","Tremolo"},1);
        if(m==6||m==7){addAndMakeVisible(choice);choiceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,m==6?"division":"modStyle",choice);}
    }
    void paint(juce::Graphics& g) override {g.setColour(card);g.fillRoundedRectangle(getLocalBounds().toFloat(),12);g.setColour(accent.withAlpha(.45f));g.fillRoundedRectangle(12,12,3,24,1);}
    void resized() override {enabled.setBounds(22,10,190,28);up.setBounds(getWidth()-160,12,70,24);down.setBounds(getWidth()-82,12,70,24);int y=50;for(size_t i=0;i<controls.size();++i)controls[i]->setBounds(16+(int(i)%2)*(getWidth()-32)/2,y+(int(i)/2)*64,(getWidth()-40)/2,58);choice.setBounds(16,250,190,26);sync.setBounds(224,250,170,26);up.setEnabled(slot>0);down.setEnabled(slot<7);}
};
struct MusicToMyMouthEditor::PianoRoll : juce::Component {
    MusicToMyMouthEditor& editor;std::shared_ptr<const mouth::Take> take;int selected=-1;double pixelsPerSecond=90;int low=36,high=84;
    explicit PianoRoll(MusicToMyMouthEditor& e):editor(e) {}
    juce::Rectangle<float> noteRect(const mouth::VocalNote& n) const {float row=float(getHeight()-30)/float(high-low+1);return {float(50+(n.start+n.offset)*pixelsPerSecond),float(30+(high-n.midi-n.shift)*row),float(std::max(6.,n.duration*n.stretch*pixelsPerSecond)),std::max(5.f,row*.8f)};}
    void paint(juce::Graphics& g) override {
        g.fillAll(background);float row=float(getHeight()-30)/float(high-low+1);
        for(int midi=low;midi<=high;++midi){float y=30+(high-midi)*row;bool black=(midi%12==1||midi%12==3||midi%12==6||midi%12==8||midi%12==10);g.setColour(black?juce::Colour(0xff191e21):juce::Colour(0xff22292d));g.fillRect(0.f,y,float(getWidth()),row);if(midi%12==0){g.setColour(muted);g.setFont(11);g.drawText(juce::MidiMessage::getMidiNoteName(midi,true,true,4),4,int(y),44,int(row),juce::Justification::centredLeft);}}
        for(int second=0;50+second*pixelsPerSecond<getWidth();++second){int x=int(50+second*pixelsPerSecond);g.setColour(juce::Colour(0xff354044));g.drawVerticalLine(x,30,float(getHeight()));g.setColour(muted);g.drawText(juce::String(second)+"s",x+3,2,50,22,juce::Justification::left);}
        if(!take){g.setColour(muted);g.drawText("Capture a vocal section to see its notes.",getLocalBounds().reduced(70),juce::Justification::centred);return;}
        for(size_t i=0;i<take->notes.size();++i){auto r=noteRect(take->notes[i]);g.setColour(int(i)==selected?accent:juce::Colour(0xff6aada7));g.fillRoundedRectangle(r,3);if(r.getWidth()>34){g.setColour(background);g.setFont(11);g.drawText(juce::MidiMessage::getMidiNoteName(int(std::round(take->notes[i].midi+take->notes[i].shift)),true,true,4),r.toNearestInt(),juce::Justification::centred);}}
    }
    void mouseDown(const juce::MouseEvent& e) override {if(!take)return;for(size_t i=0;i<take->notes.size();++i)if(noteRect(take->notes[i]).expanded(0,3).contains(e.position)){editor.selectNote(int(i));return;}}
};
void MusicToMyMouthEditor::addControl(juce::Component& page,const char* id,std::vector<std::unique_ptr<Control>>& list) {auto c=std::make_unique<Control>(processor,id);page.addAndMakeVisible(*c);list.push_back(std::move(c));}
void MusicToMyMouthEditor::addToggle(juce::Component& page,juce::ToggleButton& button,const char* id,const juce::String& name) {button.setButtonText(name);page.addAndMakeVisible(button);buttons.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,id,button));}
MusicToMyMouthEditor::MusicToMyMouthEditor(MusicToMyMouthProcessor& p):AudioProcessorEditor(p),processor(p),theme(std::make_unique<Theme>()) {
    setLookAndFeel(theme.get());setSize(1120,800);setResizable(true,true);setResizeLimits(960,720,1440,1100);
    addAndMakeVisible(tabs);tabs.addTab("TUNE",background,&tunePage,false);tabs.addTab("VOCAL CHAIN",background,&rackPage,false);tabs.addTab("CAPTURE & EDIT",background,&editPage,false);
    addAndMakeVisible(presets);presets.addItemList([&]{juce::StringArray names;for(auto& preset:mouth::factoryPresets())names.add(preset.name);return names;}(),1);presets.setSelectedId(p.getCurrentProgram()+1,juce::dontSendNotification);presets.onChange=[this]{processor.setCurrentProgram(presets.getSelectedId()-1);};
    addAndMakeVisible(savePreset);addAndMakeVisible(loadPreset);savePreset.onClick=[this]{presetDialog(true);};loadPreset.onClick=[this]{presetDialog(false);};
    tunePage.addAndMakeVisible(key);key.addItemList(mouth::keyNames(),1);combos.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"key",key));
    tunePage.addAndMakeVisible(scale);scale.addItemList(mouth::scaleNames(),1);combos.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"scale",scale));
    addToggle(tunePage,tuneOn,"tuneOn","Automatic tuning");addToggle(tunePage,preserveFormant,"preserveFormant","Preserve vocal formants");
    for(auto id:{"character","strength","retune","humanize","transpose","formant","input","output","mix"})addControl(tunePage,id,tuneControls);
    tunePage.addAndMakeVisible(pitchStatus);
    rackPage.addAndMakeVisible(rackViewport);rackViewport.setViewedComponent(&rackContent,false);rackViewport.setScrollBarsShown(true,false);
    for(int i=0;i<8;++i){auto m=std::make_unique<ModuleCard>(p,i);rackContent.addAndMakeVisible(*m);modules.push_back(std::move(m));}
    for(auto* button:{&captureButton,&stopCapture,&clearCapture,&undoButton,&applyButton})editPage.addAndMakeVisible(*button);
    addToggle(editPage,editedOn,"editedOn","Play edited Capture");editPage.addAndMakeVisible(captureStatus);editPage.addAndMakeVisible(noteDetails);
    captureButton.onClick=[this]{processor.capture.arm(int(mouth::value(processor.parameters,"key")),int(mouth::value(processor.parameters,"scale")));};stopCapture.onClick=[this]{processor.capture.finish();};clearCapture.onClick=[this]{processor.capture.clear();};undoButton.onClick=[this]{processor.capture.undo();};applyButton.onClick=[this]{applyNote();};
    piano=std::make_unique<PianoRoll>(*this);editPage.addAndMakeVisible(noteViewport);noteViewport.setViewedComponent(piano.get(),false);noteViewport.setScrollBarsShown(false,true);
    auto setup=[&](juce::Slider& s,juce::Label& l,const juce::String& name,double min,double max,double interval,const juce::String& suffix){s.setRange(min,max,interval);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,24);s.setTextValueSuffix(suffix);editPage.addAndMakeVisible(s);label(l,name);editPage.addAndMakeVisible(l);};
    setup(noteShift,shiftLabel,"Pitch shift",-12,12,.01," st");setup(noteOffset,offsetLabel,"Start offset",-2000,2000,1," ms");setup(noteLength,lengthLabel,"Duration",50,200,.1," %");setup(noteCorrection,correctionLabel,"Snap toward selected scale",0,100,.1," %");setup(zoom,zoomLabel,"Timeline zoom",20,400,1," px/s");zoom.setValue(90,juce::dontSendNotification);zoom.onValueChange=[this]{resized();piano->repaint();};
    startTimerHz(12);timerCallback();
}
MusicToMyMouthEditor::~MusicToMyMouthEditor() {stopTimer();noteViewport.setViewedComponent(nullptr,false);rackViewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr);}
void MusicToMyMouthEditor::paint(juce::Graphics& g) {
    g.fillAll(background);g.setColour(text);g.setFont(juce::FontOptions(29,juce::Font::bold));g.drawText("MusicToMyMouth",24,16,390,42,juce::Justification::left);g.setColour(muted);g.setFont(13);g.drawText("A voice worth keeping.   /   Native vocal processor",26,60,440,24,juce::Justification::left);
    g.setFont(12);g.drawText("INPUT",24,getHeight()-34,50,20,juce::Justification::left);g.drawText("OUTPUT",300,getHeight()-34,60,20,juce::Justification::left);
    auto meter=[&](float level,int x){g.setColour(card);g.fillRoundedRectangle(float(x),float(getHeight()-28),190,8,4);g.setColour(level>=1?juce::Colours::orangered:accent);float width=juce::jlimit(0.f,1.f,(juce::Decibels::gainToDecibels(level,-60.f)+60)/60)*190;g.fillRoundedRectangle(float(x),float(getHeight()-28),width,8,4);};meter(processor.inputLevel.load(),85);meter(processor.outputLevel.load(),370);
    g.setColour(muted);g.drawText("Clean isolated vocals  •  v0.1 development",getWidth()-390,getHeight()-36,365,24,juce::Justification::right);
}
void MusicToMyMouthEditor::resized() {
    presets.setBounds(getWidth()-570,24,310,30);savePreset.setBounds(getWidth()-250,24,105,30);loadPreset.setBounds(getWidth()-135,24,110,30);
    tabs.setBounds(20,100,getWidth()-40,getHeight()-148);
    int w=tunePage.getWidth();tuneOn.setBounds(24,26,180,28);key.setBounds(230,26,90,28);scale.setBounds(334,26,200,28);preserveFormant.setBounds(560,26,220,28);
    for(size_t i=0;i<tuneControls.size();++i)tuneControls[i]->setBounds(30+(int(i)%3)*(w-60)/3,110+(int(i)/3)*120,(w-90)/3,80);
    pitchStatus.setBounds(30,490,w-60,36);
    rackViewport.setBounds(rackPage.getLocalBounds().reduced(12));rackContent.setSize(std::max(1,rackViewport.getWidth()-16),4*300+12);
    auto order=processor.moduleOrder();int cw=(rackContent.getWidth()-36)/2;
    for(int slot=0;slot<8;++slot){auto& m=*modules[size_t((order>>(slot*3))&7)];m.slot=slot;m.setBounds(12+(slot%2)*(cw+12),12+(slot/2)*300,cw,288);m.resized();}
    int ew=editPage.getWidth(),eh=editPage.getHeight();captureButton.setBounds(16,16,140,30);stopCapture.setBounds(166,16,78,30);clearCapture.setBounds(254,16,78,30);undoButton.setBounds(342,16,78,30);editedOn.setBounds(445,16,200,30);captureStatus.setBounds(16,53,ew-32,42);
    noteViewport.setBounds(16,105,std::max(1,ew-326),std::max(1,eh-164));double duration=displayedTake?displayedTake->audio->getNumSamples()/displayedTake->rate:8;int pw=std::max(noteViewport.getWidth()-4,int(60+duration*zoom.getValue()));piano->pixelsPerSecond=zoom.getValue();piano->setSize(pw,std::max(1,noteViewport.getHeight()-18));
    int x=ew-294;noteDetails.setBounds(x,105,278,54);int y=172;juce::Slider* sliders[]{&noteShift,&noteOffset,&noteLength,&noteCorrection};juce::Label* labels[]{&shiftLabel,&offsetLabel,&lengthLabel,&correctionLabel};for(int i=0;i<4;++i){labels[i]->setBounds(x,y,278,22);sliders[i]->setBounds(x,y+25,278,32);y+=75;}applyButton.setBounds(x,y+8,278,32);zoomLabel.setBounds(16,eh-50,110,22);zoom.setBounds(134,eh-54,std::max(1,ew-470),32);
}
void MusicToMyMouthEditor::timerCallback() {
    auto stage=processor.capture.stage();bool busy=stage==mouth::CaptureManager::Working||stage==mouth::CaptureManager::Queued;
    captureButton.setEnabled(!busy&&stage!=mouth::CaptureManager::Armed&&stage!=mouth::CaptureManager::Recording);stopCapture.setEnabled(stage==mouth::CaptureManager::Armed||stage==mouth::CaptureManager::Recording);clearCapture.setEnabled(!busy);undoButton.setEnabled(stage==mouth::CaptureManager::Available);
    juce::String status=processor.capture.message();if(stage==mouth::CaptureManager::Recording)status="Capturing: "+juce::String(processor.capture.capturedSeconds(),1)+"s / "+juce::String(processor.capture.limitSeconds(),0)+"s. Finish or stop Logic playback.";if(stage==mouth::CaptureManager::Armed&&!processor.hostTimelineAvailable.load())status="Armed. Waiting for Logic playback and a host timeline.";captureStatus.setText(status,juce::dontSendNotification);
    auto take=processor.capture.document();if(take!=displayedTake){displayedTake=take;piano->take=take;if(take&&!take->notes.empty()){double lowest=100,highest=0;for(auto& n:take->notes){lowest=std::min(lowest,n.midi+n.shift);highest=std::max(highest,n.midi+n.shift);}piano->low=juce::jlimit(12,100,int(lowest)-8);piano->high=juce::jlimit(piano->low+12,120,int(highest)+8);}if(selected>=0)selectNote(selected);resized();piano->repaint();}
    for(auto* s:{&noteShift,&noteOffset,&noteLength,&noteCorrection})s->setEnabled(!busy&&selected>=0&&bool(displayedTake));applyButton.setEnabled(!busy&&selected>=0&&bool(displayedTake));
    float midi=processor.pitch.detectedMidi.load();pitchStatus.setText(midi>0?"Detected "+juce::MidiMessage::getMidiNoteName(int(std::round(midi)),true,true,4)+"    •    correction "+juce::String(processor.pitch.correctionCents.load(),0)+" cents    •    latency "+juce::String(1000.*processor.getLatencySamples()/std::max(1.,processor.getSampleRate()),1)+" ms":"Play a clean vocal to see pitch detection. Set the song key and scale above.",juce::dontSendNotification);
    resized();repaint();
}
void MusicToMyMouthEditor::selectNote(int index) {if(!displayedTake||index<0||index>=int(displayedTake->notes.size())){selected=-1;piano->selected=-1;return;}selected=index;piano->selected=index;auto& n=displayedTake->notes[size_t(index)];noteShift.setValue(n.shift,juce::dontSendNotification);noteOffset.setValue(n.offset*1000,juce::dontSendNotification);noteLength.setValue(n.stretch*100,juce::dontSendNotification);noteCorrection.setValue(n.correction*100,juce::dontSendNotification);label(noteDetails,"Note "+juce::String(index+1)+"  •  "+juce::MidiMessage::getMidiNoteName(int(std::round(n.midi)),true,true,4)+"\n"+juce::String(n.start,2)+"s  /  "+juce::String(n.duration,2)+"s");piano->repaint();}
void MusicToMyMouthEditor::applyNote() {if(!displayedTake||selected<0)return;auto n=displayedTake->notes[size_t(selected)];n.shift=noteShift.getValue();n.offset=noteOffset.getValue()*.001;n.stretch=noteLength.getValue()*.01;n.correction=noteCorrection.getValue()*.01;if(!processor.capture.editNote(size_t(selected),n))captureStatus.setText("Edit rejected: keep the note within the take and clear of its neighbors.",juce::dontSendNotification);}
void MusicToMyMouthEditor::presetDialog(bool saving) {
    auto directory=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("MusicToMyMouth/Presets");directory.createDirectory();chooser=std::make_unique<juce::FileChooser>(saving?"Save MusicToMyMouth sound":"Load MusicToMyMouth sound",directory,"*.mtmm");auto safe=juce::Component::SafePointer<MusicToMyMouthEditor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|(saving?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode),[safe,saving](const juce::FileChooser& c){if(!safe)return;auto file=c.getResult();if(file==juce::File{})return;bool ok=saving?safe->processor.savePreset(file.withFileExtension("mtmm")):safe->processor.loadPreset(file);juce::AlertWindow::showMessageBoxAsync(ok?juce::MessageBoxIconType::InfoIcon:juce::MessageBoxIconType::WarningIcon,"MusicToMyMouth",ok?(saving?"Sound preset saved.":"Sound preset loaded."):"Could not save or load this preset.");});
}
