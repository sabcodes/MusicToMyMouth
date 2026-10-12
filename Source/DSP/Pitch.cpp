#include "Pitch.h"
namespace mouth {
void YinDetector::prepare(double sr) { decimation = std::max(1, int(sr / 12000)); rate = sr / decimation; reset(); }
void YinDetector::reset() { ring.fill(0); write = hop = filled = decimationCount = 0; sum = 0; last = {}; }
PitchReading YinDetector::push(float sample) {
    sum += sample;
    if (++decimationCount < decimation) return last;
    ring[size_t(write)] = sum / float(decimation); sum = 0; decimationCount = 0;
    write = (write+1) % int(ring.size()); filled = std::min(filled+1, int(ring.size()));
    if (++hop >= 128 && filled == int(ring.size())) { hop = 0; last = detect(); }
    return last;
}
PitchReading YinDetector::analyse(const float* samples, int count) { reset(); for(int i=0;i<count;++i) push(samples[i]); return last; }
PitchReading YinDetector::detect() {
    double energy = 0;
    for(int i=0;i<1024;++i) { frame[size_t(i)] = ring[size_t((write+i)%1024)]; energy += frame[size_t(i)]*frame[size_t(i)]; }
    if (energy / 1024 < 1.0e-7) return {};
    const int minLag = std::max(2, int(rate/1100)), maxLag = std::min(510, int(rate/65));
    double running = 0; diff[0] = 1;
    for(int lag=1;lag<=maxLag;++lag) {
        double d=0;
        for(int i=0;i<512;++i) { double x = frame[size_t(i)] - frame[size_t(i+lag)]; d += x*x; }
        running += d; diff[size_t(lag)] = running > 0 ? float(d*lag/running) : 1;
    }
    int best = -1;
    for(int lag=minLag;lag<maxLag;++lag) if(diff[size_t(lag)]<.15f) {
        while(lag+1<maxLag && diff[size_t(lag+1)]<diff[size_t(lag)]) ++lag;
        best=lag; break;
    }
    if(best<0) return {};
    double a=diff[size_t(best-1)], b=diff[size_t(best)], c=diff[size_t(best+1)];
    double lag = best + (std::abs(a-2*b+c)>1e-9 ? .5*(a-c)/(a-2*b+c) : 0);
    double hz = rate/lag;
    return {69+12*std::log2(hz/440), 1-b, hz>=65 && hz<=1100};
}
double nearestNote(double midi, int key, int scale) {
    static const std::vector<std::vector<int>> scales {{0,1,2,3,4,5,6,7,8,9,10,11},{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,8,11},{0,2,4,7,9},{0,3,5,7,10}};
    auto& notes=scales[size_t(juce::jlimit(0,5,scale))];
    int centre=int(std::floor(midi)); double best=centre, distance=100;
    for(int n=centre-12;n<=centre+12;++n) {
        int pc=((n-key)%12+12)%12;
        if(std::find(notes.begin(),notes.end(),pc)!=notes.end() && std::abs(n-midi)<distance) { best=n; distance=std::abs(n-midi); }
    } return best;
}
void PitchEngine::prepare(double sr,int channels) {
    sampleRate=sr; detector.prepare(sr);
    using R=RubberBand::RubberBandLiveShifter;
    shifter=std::make_unique<R>(size_t(sr),size_t(channels),R::OptionWindowMedium|R::OptionFormantPreserved|R::OptionChannelsTogether);
    shifter->setDebugLevel(0); block=int(shifter->getBlockSize());
    input.setSize(channels,block); output.setSize(channels,block);
    delay=block+int(shifter->getStartDelay()); reset();
}
void PitchEngine::reset() { if(shifter)shifter->reset(); detector.reset(); input.clear();output.clear();cursor=0;shiftSemitones=0;detectedMidi=0;confidence=0;correctionCents=0; }
void PitchEngine::process(juce::AudioBuffer<float>& buffer,const juce::AudioProcessorValueTreeState& params,const bool* manualMask) {
    if(!shifter) return;
    bool on=value(params,"tuneOn")>.5f;
    double character=value(params,"character")/100.;
    double strength=value(params,"strength")/100.;
    double retune=value(params,"retune")*(1-.95*character);
    double alpha=retune<.01 ? 1 : 1-std::exp(-block/(sampleRate*retune*.001));
    auto reading=PitchReading{};
    for(int i=0;i<buffer.getNumSamples();++i) {
        float mono=0;
        for(int ch=0;ch<buffer.getNumChannels();++ch) {
            float x=buffer.getSample(ch,i);mono+=x;
            input.setSample(ch,cursor,x);buffer.setSample(ch,i,output.getSample(ch,cursor));
        }
        reading=detector.push(mono/float(buffer.getNumChannels()));
        if(++cursor==block) {
            double correction=0;
            if(on&&reading.voiced&&!(manualMask&&manualMask[i])) {
                correction=(nearestNote(reading.midi,int(value(params,"key")),int(value(params,"scale")))-reading.midi)*strength;
                double deadZone=value(params,"humanize")*.0025*(1-character);
                correction=std::copysign(std::max(0.,std::abs(correction)-deadZone),correction);
            }
            if(manualMask&&manualMask[i])shiftSemitones=0;else shiftSemitones+=alpha*(correction-shiftSemitones);
            shifter->setPitchScale(std::pow(2.,(shiftSemitones+value(params,"transpose"))/12.));
            shifter->setFormantOption(value(params,"preserveFormant")>.5f ? RubberBand::RubberBandLiveShifter::OptionFormantPreserved : RubberBand::RubberBandLiveShifter::OptionFormantShifted);
            auto formant=value(params,"formant");
            // Explicit envelope ratios are relative to the uncorrected envelope.
            shifter->setFormantScale(std::abs(formant)<.001f ? 0 : std::pow(2.,formant/12.)/shifter->getPitchScale());
            shifter->shift(input.getArrayOfReadPointers(),output.getArrayOfWritePointers());cursor=0;
            detectedMidi.store(reading.voiced?float(reading.midi):0);
            confidence.store(float(reading.confidence));correctionCents.store(float(shiftSemitones*100));
        }
    }
}
}
