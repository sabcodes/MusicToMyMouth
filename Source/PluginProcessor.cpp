#include "PluginProcessor.h"
#include "PluginEditor.h"
MusicToMyMouthProcessor::MusicToMyMouthProcessor():AudioProcessor(BusesProperties().withInput("Vocal",juce::AudioChannelSet::stereo(),true).withOutput("Processed vocal",juce::AudioChannelSet::stereo(),true)),parameters(*this,nullptr,"Parameters",mouth::makeParameters()) {setCurrentProgram(0);}
bool MusicToMyMouthProcessor::isBusesLayoutSupported(const BusesLayout& b) const {auto in=b.getMainInputChannelSet();return in==b.getMainOutputChannelSet()&&(in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo());}
void MusicToMyMouthProcessor::prepareToPlay(double sr,int maxBlock) {
    rate=sr;blockCapacity=std::max(512,maxBlock);int channels=getTotalNumInputChannels();
    pitch.prepare(sr,channels);chain.prepare(sr,blockCapacity,channels);capture.prepare(sr,channels);
    setLatencySamples(pitch.latency()+chain.latency());
    dryDelay.setSize(channels,getLatencySamples()+1);dryDelay.clear();dryScratch.setSize(channels,blockCapacity);dryCursor=0;manualMask=std::make_unique<bool[]>(size_t(blockCapacity));
    inputGain.reset(sr,.02);outputGain.reset(sr,.02);mix.reset(sr,.02);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(mouth::value(parameters,"input")));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(mouth::value(parameters,"output")));
    mix.setCurrentAndTargetValue(mouth::value(parameters,"mix")*.01f);
}
void MusicToMyMouthProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if(!manualMask){b.clear();return;}
    bool playing=false,valid=false;double seconds=0,bpm=120;
    if(auto* head=getPlayHead())if(auto position=head->getPosition()) {
        playing=position->getIsPlaying();
        if(auto time=position->getTimeInSeconds()){seconds=*time;valid=true;}
        else if(auto samples=position->getTimeInSamples()){seconds=*samples/rate;valid=true;}
        if(auto tempo=position->getBpm())if(std::isfinite(*tempo)&&*tempo>0)bpm=*tempo;
    }
    valid=valid&&std::isfinite(seconds);hostTimelineAvailable=valid;
    float in=0,out=0;for(int ch=0;ch<b.getNumChannels();++ch)in=std::max(in,b.getMagnitude(ch,0,b.getNumSamples()));inputLevel=in;
    if(valid)capture.capture(b,seconds,playing);else capture.capture(b,0,false);
    for(int offset=0;offset<b.getNumSamples();offset+=blockCapacity) {
        int count=std::min(blockCapacity,b.getNumSamples()-offset);
        juce::AudioBuffer<float> view(b.getArrayOfWritePointers(),b.getNumChannels(),offset,count);
        processSection(view,seconds+offset/rate,valid&&playing,bpm);
    }
    for(int ch=0;ch<b.getNumChannels();++ch)out=std::max(out,b.getMagnitude(ch,0,b.getNumSamples()));outputLevel=out;
}
void MusicToMyMouthProcessor::processSection(juce::AudioBuffer<float>& b,double seconds,bool timeline,double bpm) {
    capture.replace(b,manualMask.get(),seconds,timeline&&mouth::value(parameters,"editedOn")>.5f,rate);
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(mouth::value(parameters,"input")));
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(mouth::value(parameters,"output")));
    mix.setTargetValue(mouth::value(parameters,"mix")*.01f);
    // Read the delay per frame before later input can overwrite the ring.
    for(int i=0;i<b.getNumSamples();++i) {
        float g=inputGain.getNextValue();
        for(int ch=0;ch<b.getNumChannels();++ch){float x=b.getSample(ch,i)*g;b.setSample(ch,i,x);dryDelay.setSample(ch,dryCursor,x);dryScratch.setSample(ch,i,dryDelay.getSample(ch,(dryCursor+1)%dryDelay.getNumSamples()));}
        dryCursor=(dryCursor+1)%dryDelay.getNumSamples();
    }
    pitch.process(b,parameters,manualMask.get());chain.process(b,parameters,order.load(),bpm);
    for(int i=0;i<b.getNumSamples();++i) {
        float blend=mix.getNextValue(),g=outputGain.getNextValue();
        for(int ch=0;ch<b.getNumChannels();++ch){float x=(b.getSample(ch,i)*blend+dryScratch.getSample(ch,i)*(1-blend))*g;b.setSample(ch,i,std::isfinite(x)?x:0.f);}
    }
}
void MusicToMyMouthProcessor::setParameterValue(const juce::String& id,float raw) {if(auto* p=parameters.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(raw));p->endChangeGesture();}}
void MusicToMyMouthProcessor::setCurrentProgram(int index) {
    if(index<0||index>=getNumPrograms())return;
    for(auto* p:getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(p)) {
        auto id=parameter->getParameterID();if(id=="key"||id=="scale"||id=="editedOn")continue;
        parameter->setValueNotifyingHost(parameter->getDefaultValue());
    }
    for(auto& [id,v]:mouth::factoryPresets()[size_t(index)].values)setParameterValue(id,v);
    order=mouth::VocalChain::defaultOrder;currentPreset=index;
}
const juce::String MusicToMyMouthProcessor::getProgramName(int index) {return juce::isPositiveAndBelow(index,getNumPrograms())?mouth::factoryPresets()[size_t(index)].name:juce::String{};}
void MusicToMyMouthProcessor::moveModule(int slot,int delta) {int other=slot+delta;if(slot<0||slot>=8||other<0||other>=8)return;uint32_t o=order.load(),a=(o>>(3*slot))&7,b=(o>>(3*other))&7;o&=~((7u<<(3*slot))|(7u<<(3*other)));o|=a<<(3*other);o|=b<<(3*slot);order=o;}
juce::ValueTree MusicToMyMouthProcessor::soundState() {juce::ValueTree s("MusicToMyMouthPreset");s.setProperty("version",1,nullptr);s.setProperty("order",int(order.load()),nullptr);for(auto* p:getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(p)){auto id=parameter->getParameterID();if(id!="key"&&id!="scale"&&id!="editedOn")s.setProperty(id,parameter->convertFrom0to1(parameter->getValue()),nullptr);}return s;}
bool MusicToMyMouthProcessor::applySoundState(const juce::ValueTree& tree) {if(tree.getType()!=juce::Identifier("MusicToMyMouthPreset")||int(tree["version"])!=1||!mouth::VocalChain::validOrder(uint32_t(int(tree["order"]))))return false;for(auto* p:getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(p)){auto id=parameter->getParameterID();if(tree.hasProperty(id)){float raw=float(tree[id]);if(!std::isfinite(raw))return false;}}
    for(auto* p:getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(p)){auto id=parameter->getParameterID();if(id!="key"&&id!="scale"&&id!="editedOn"&&tree.hasProperty(id))setParameterValue(id,float(tree[id]));}order=uint32_t(int(tree["order"]));return true;}
bool MusicToMyMouthProcessor::savePreset(const juce::File& file) {auto xml=soundState().createXml();return xml&&xml->writeTo(file);}
bool MusicToMyMouthProcessor::loadPreset(const juce::File& file) {if(file.getSize()>1024*1024)return false;auto xml=juce::XmlDocument::parse(file);return xml&&applySoundState(juce::ValueTree::fromXml(*xml));}
void MusicToMyMouthProcessor::getStateInformation(juce::MemoryBlock& dest) {juce::ValueTree s("MusicToMyMouth");s.setProperty("version",1,nullptr);s.setProperty("order",int(order.load()),nullptr);s.setProperty("preset",currentPreset.load(),nullptr);s.addChild(parameters.copyState(),-1,nullptr);s.addChild(capture.save(),-1,nullptr);juce::MemoryOutputStream stream(dest,false);s.writeToStream(stream);}
void MusicToMyMouthProcessor::setStateInformation(const void* data,int size) {if(size<=0||size>300000000)return;auto s=juce::ValueTree::readFromData(data,size);if(s.getType()!=juce::Identifier("MusicToMyMouth")||int(s["version"])!=1)return;auto p=s.getChildWithName("Parameters");if(!p.isValid())return;uint32_t o=uint32_t(int(s["order"]));if(!mouth::VocalChain::validOrder(o))return;if(!capture.restore(s.getChildWithName("Capture")))return;parameters.replaceState(p);order=o;currentPreset=juce::jlimit(0,getNumPrograms()-1,int(s["preset"]));}
juce::AudioProcessorEditor* MusicToMyMouthProcessor::createEditor() {return new MusicToMyMouthEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new MusicToMyMouthProcessor();}
