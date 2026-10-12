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
        rawDiff[size_t(lag)] = float(d);running += d; diff[size_t(lag)] = running > 0 ? float(d*lag/running) : 1;
    }
    int best = -1;
    for(int lag=minLag;lag<maxLag;++lag) if(diff[size_t(lag)]<.15f) {
        while(lag+1<maxLag && diff[size_t(lag+1)]<diff[size_t(lag)]) ++lag;
        best=lag; break;
    }
    if(best<0) return {};
    // Use the normalized curve to select the trough, then the raw difference
    // to refine its period without the normalization slope bias.
    double a=rawDiff[size_t(best-1)], b=rawDiff[size_t(best)], c=rawDiff[size_t(best+1)];
    double lag = best + (std::abs(a-2*b+c)>1e-9 ? .5*(a-c)/(a-2*b+c) : 0);
    double hz = rate/lag;
    return {69+12*std::log2(hz/440), 1-diff[size_t(best)], hz>=65 && hz<=1100};
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
void PitchEngine::Diagnostics::count(const char* message) {
    // Library diagnostics stay allocation-free and do not write to stderr on the audio thread.
    if (std::strstr(message, "ERROR")) ++errors;
    else if (std::strstr(message, "WARNING")) ++warnings;
}
void PitchEngine::prepare(double sr, int channels) {
    sampleRate = sr;
    detector.prepare(sr);
    using R = RubberBand::RubberBandStretcher;
    shifter = std::make_unique<R>(size_t(sr), size_t(channels), diagnostics,
        R::OptionProcessRealTime | R::OptionEngineFiner | R::OptionChannelsTogether
        | R::OptionPitchHighConsistency | R::OptionFormantPreserved | R::OptionThreadingNever);
    shifter->setDebugLevel(0);
    shifter->setMaxProcessSize(size_t(block));
    // Output production varies across blocks even at time ratio 1. Keep a bounded
    // reserve, report its delay to the host, and remove the engine's startup delay.
    delay = int(shifter->getPreferredStartPad() + shifter->getStartDelay()) + block * 4;
    input.setSize(channels, block);
    staging.setSize(channels, block);
    fifo.setSize(channels, delay * 4 + block * 16);
    reset();
}
void PitchEngine::reset() {
    if (!shifter) return;
    shifter->reset();
    detector.reset();input.clear();staging.clear();fifo.clear();
    cursor = read = 0;write = delay;queued = delay;
    discard = int(shifter->getStartDelay());shiftSemitones = 0;
    detectedMidi = confidence = correctionCents = 0;
    underruns = overflows = 0;diagnostics->errors = diagnostics->warnings = 0;
    for (int pad = int(shifter->getPreferredStartPad()); pad > 0; pad -= block) {
        shifter->process(input.getArrayOfReadPointers(), size_t(std::min(block, pad)), false);
        drain();
    }
}
void PitchEngine::drain() {
    while (shifter->available() > 0) {
        int count = int(shifter->retrieve(staging.getArrayOfWritePointers(),
            size_t(std::min(block, shifter->available()))));
        for (int i = 0; i < count; ++i) {
            if (discard > 0) { --discard; continue; }
            if (queued >= fifo.getNumSamples()) { ++overflows; continue; }
            for (int ch = 0; ch < fifo.getNumChannels(); ++ch)
                fifo.setSample(ch, write, staging.getSample(ch, i));
            write = (write + 1) % fifo.getNumSamples();++queued;
        }
    }
}
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
            input.setSample(ch,cursor,x);buffer.setSample(ch,i,queued>0?fifo.getSample(ch,read):0.f);
        }
        if (queued > 0) { read = (read + 1) % fifo.getNumSamples();--queued; }
        else ++underruns;
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
            shifter->setFormantOption(value(params,"preserveFormant")>.5f ? RubberBand::RubberBandStretcher::OptionFormantPreserved : RubberBand::RubberBandStretcher::OptionFormantShifted);
            auto formant=value(params,"formant");
            // Explicit envelope ratios are relative to the uncorrected envelope.
            shifter->setFormantScale(std::abs(formant)<.001f ? 0 : std::pow(2.,formant/12.)/shifter->getPitchScale());
            shifter->process(input.getArrayOfReadPointers(),size_t(block),false);drain();cursor=0;
            detectedMidi.store(reading.voiced?float(reading.midi):0);
            confidence.store(float(reading.confidence));correctionCents.store(float(shiftSemitones*100));
        }
    }
}
}
