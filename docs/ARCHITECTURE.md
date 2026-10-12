# Implementation boundaries

- Standard native AU, unique `Sbcd/Mtmm` identity. No source or build changes in SerumSynth or VocalSuiteX.
- JUCE is pinned by commit. Rubber Band 4.0.0 supplies R3 streamed pitch/formant shifting and offline pitch/time rendering; its single compilation unit uses Apple's Accelerate FFT.
- Monophonic YIN detection feeds key/scale quantization and smoothed correction. Unvoiced frames return toward unity. The R3 streaming interface uses the high-consistency option for changing pitch ratios, explicit startup pad/discard handling, and a bounded output reserve. The reserve is reported as host latency. Delayed dry mixing and host bypass follow that latency; the wet impulse was measured within one sample of it. The 44.1 kHz build reports 6,148 samples (139.41 ms).
- Parameters live in AudioProcessorValueTreeState. Module order and Capture document are separately versioned project data.
- Capture writes raw incoming audio into a preallocated buffer. It requires a running, continuous host timeline, stops on loops/seeks, and is bounded to three minutes or 128 MiB of raw media. Analysis and manual rendering run off the audio callback.
- Manual edits are per-note transposition, start offset, duration, and correction strength. The rendered take replaces source only inside the captured timeline interval, then passes through the vocal chain. Changes to host region position or source content require recapture.
- Project state embeds 32-bit floating-point WAV Capture media and note edits. Sound presets exclude captured media and song key/scale. Capture audio is personal material and is never committed.
- This is an initial implementation. Production equivalence requires listening comparisons on actual vocal stems and Logic host checks; compilation cannot establish sonic quality.

## Dependencies

JUCE's upstream licensing applies (AGPLv3/commercial options). Rubber Band is GPL-2.0-or-later/commercial. This repository is for the owner's personal use; distribution terms have not been selected. Preserve upstream copyright and license files in dependency downloads.

Primary implementation references:
- https://breakfastquay.com/rubberband/code-doc/classRubberBand_1_1RubberBandStretcher.html
- https://breakfastquay.com/rubberband/integration.html
- https://helpcenter.celemony.com/M5/doc/melodyneEssential5/en/M5tour_TransferARA?env=live

YIN period refinement uses the raw difference trough after normalized trough selection, following Step 5 of [de Cheveigné and Kawahara (2002)](https://www.ee.columbia.edu/~dpwe/papers/deChevK02-yin.pdf). This avoids the normalization slope bias observed by the controlled high-note tests.
