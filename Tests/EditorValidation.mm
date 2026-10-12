#import <Cocoa/Cocoa.h>
#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
void require(bool okay,const char* message) {
    if(!okay)throw std::runtime_error(message);
}
void pump() {
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.1]];
}
juce::TabbedComponent* findTabs(juce::Component& c) {
    if(auto* tabs=dynamic_cast<juce::TabbedComponent*>(&c))return tabs;
    for(auto* child:c.getChildren())if(auto* tabs=findTabs(*child))return tabs;
    return nullptr;
}
void exercise(juce::AudioProcessorEditor& editor) {
    require(editor.getWidth()==1120&&editor.getHeight()==800,"initial editor size");
    auto* tabs=findTabs(editor);require(tabs&&tabs->getNumTabs()==3,"all editor pages created");
    for(auto size:{juce::Point<int>{960,720},{1440,1100},{1120,800}}) {
        editor.setSize(size.x,size.y);
        for(int page=0;page<3;++page) {
            tabs->setCurrentTabIndex(page);editor.resized();pump();
            auto image=editor.createComponentSnapshot(editor.getLocalBounds());
            require(image.isValid(),"editor page paint");
        }
    }
}

}
int main() {
    @autoreleasepool {
        juce::ScopedJuceInitialiser_GUI juce;
        try {
            MusicToMyMouthProcessor processor;
            // Covers Logic opening the editor before it starts the audio engine.
            for(int i=0;i<3;++i) {
                std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
                exercise(*editor);
            }
            std::cout<<"PASS direct editor open/reopen, all tabs, resizing and paint before prepare"<<std::endl;
            std::cout<<"FAILURES 0"<<std::endl;return 0;
        } catch(const std::exception& error) {
            std::cerr<<"FAIL "<<error.what()<<std::endl;return 1;
        }
    }
}
