# Milestones

## Specification

- `7fc9e96`: separate project and agreed vocal-processing scope, private GitHub repository created and pushed.
- `9c684cb`: approved native Logic Capture integration, retaining manual pitch/timing editing and downloaded-stem support.

## First native AU implementation — 0.1.0 development

Implemented:
- Unique native AU effect identity `aufx/Mtmm/Sbcd`, mono and stereo matched layouts, Apple Silicon build.
- Monophonic YIN pitch detection, key/scale quantization, retune/strength/character/humanize controls, transpose, Rubber Band R3 pitch/formant engine.
- Oversampled chain with reorderable EQ, linked soft-knee compression, split-band de-esser, gate, saturation, pre-delayed reverb, tempo-synced/manual delay, chorus/flanger/tremolo.
- 12 factory sounds across natural, hard tune, robot/formant, and spacious families; user sound preset save/load.
- Host-automated sound parameters; serialized chain order, captured media, and note edits.
- Timeline-anchored Capture with background analysis, a scrollable note display, individual pitch/start/duration/correction edits, background rendering, and undo.
- Preallocated audio buffers, bounded Capture size, fixed-block adaptation, delayed dry mixing, floating-point WAV project media, and original-media sharing across undo states.

Build evidence:
- CMake Release configuration and native `arm64` AU compilation completed on this Mac with AppleClang 17 and pinned JUCE commit `b5df4524c0f01ba570a2949b625fecc426e7b8cc` (8.0.14).
- Rubber Band 4.0.0 archive SHA-256: `24300f48a8014b7c863b573a9647e61b1b19b37875e2cdd92005e64c6424d266`.
- Generated bundle: `build/MusicToMyMouth_artefacts/Release/AU/MusicToMyMouth.component`.

Remaining work before a production-quality claim:
- Actual Logic host validation, save/reopen and offline-bounce checks, latency alignment and bypass checks across host buffer/sample-rate changes.
- Pitch accuracy, unvoiced transitions, vibrato handling, manual splice/timing quality, CPU headroom, and real-vocal listening comparisons against established plugins.
- Pitch/formant processing currently precedes the reorderable effect chain. Some module parameters and bypass/order changes are applied at block boundaries; smooth transitions need further refinement.
- Manual editing currently uses detected note slices and per-note controls. It has no continuous pitch-curve drawing, note split/merge, or arbitrary overlapping notes. Automatic detection is not guaranteed to identify every note correctly.
- Capture stays anchored in absolute host seconds; DAW region/tempo changes require recapture. One take per instance; analysis/rendering is asynchronous.
- No automated tests or AU/Logic validation were run in this milestone.

This milestone provides a runnable development artifact and a foundation for quality work. It does not establish equivalence to Auto-Tune, Melodyne, Nectar, or other production tools.
