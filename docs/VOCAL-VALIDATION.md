# Too Hotty vocal validation

## Result

**76 distinct automated checks passed, plus Apple's Audio Unit validator.** Evidence combines the complete-stem run (`build/validation/final/results.json`) and expanded manual/native-bypass regression run (`build/validation/release-check/results.json`) against the same DSP build.

Input: the owner's locally supplied `Too Hotty vocals.mp3`, decoded to 32-bit floating-point WAV without modifying the original. Duration: **297.029 seconds**, stereo, **44.1 kHz**. No audio or captures are committed or uploaded.

The supplied analysis estimates C minor and 129.2 BPM. Those settings were used for the vocal runs; the estimate is not musical ground truth. The vocal stem's E-flat major estimate uses the same pitch classes.

## Coverage and measurements

- All 12 factory sounds rendered the strongest 12-second RMS excerpt, **78–90 seconds**. Outputs were finite, non-silent, below sample clipping, and had zero engine diagnostics, FIFO underruns, or overflows.
- Natural lead and instant hard-tune sounds processed the **entire 4:57 stem** with zero engine warnings/errors and zero output-buffer underruns/overflows.
- The installed native AU rendered the excerpt bit-identically to its offline processor; both reported **6,148 samples / 139.41 ms** of processing latency at 44.1 kHz.
- Native AU host bypass returned the raw source with exactly matched timing, including through the AU bypass property.
- Delayed dry processing matched the original samples exactly at 44.1, 48, and 96 kHz, mono/stereo, with 64, 137, 512, and 4,096-frame buffers.
- Controlled detuned tones at MIDI 45.37, 57.37, 69.37, and 81.37 passed a **two-cent** correction limit at those three sample rates. Maximum measured error: **0.504 cents**. The independent measurement uses zero crossings on steady sine-wave output.
- Transposition checks covered -12, -3, +3, and +12 semitones. The unity wet impulse peak landed within one sample of the reported latency.
- Native Capture detected 23 candidate notes in the excerpt. Manual pitch/duration rendering, undo, media preservation, serialized note edits, and recalled playback across 137/512-frame buffers passed. Captured media survived save/reopen with **zero sample error**; recalled renders were identical.
- A separate synthetic captured note shifted +3 semitones with **0.020 cents** measured error, and contracting duration removed audio from the old trailing span. This checks the rendered audio, beyond edit metadata.
- Stopped transport did not retrigger the captured take. Sound preset files retained effect/order settings and preserved the receiving instance's song key/scale and Capture playback choice.

### Full-stem processing on this Mac

512-frame blocks provide an 11.61 ms callback budget at 44.1 kHz. These are isolated offline callback timings, not a guarantee of live DAW performance under other workloads.

| Sound | Full render wall time | Callback p99 | Sample peak | Buffer underruns |
| --- | ---: | ---: | ---: | ---: |
| Natural / Studio lead | 40.09 s | 2.54 ms | 0.9583 | 0 |
| Hard tune / Instant snap | 26.54 s | 2.46 ms | 0.9472 | 0 |

## Fixes made during testing

1. **Host bypass timing:** the inherited bypass path returned undelayed audio. Added a continuously warmed raw delay and an explicit bypass callback; verified through both processor and installed AU APIs. DSP remains warm during bypass, so bypass does not reduce processing CPU.
2. **Pitch output shortage:** the original fixed-block live-shifter integration emitted 511/512-sample shortage diagnostics on this stem. Replaced it with the R3 streaming interface, high-consistency pitch mode, explicit startup padding/discard handling, and bounded output buffering. Diagnostic callbacks use atomic counters without audio-thread console writes. The reported latency increased from about 71 ms to 139 ms at 44.1 kHz.
3. **High-note estimation bias:** normalized-trough interpolation missed the stricter two-cent target. Period refinement now uses the raw difference curve, following Step 5 of the [YIN paper](https://www.ee.columbia.edu/~dpwe/papers/deChevK02-yin.pdf). The high-note cases then passed.

Buffered R3 integration follows the [upstream integration guidance](https://breakfastquay.com/rubberband/integration.html). Apple `auval` passed after installing the updated build, including its render, format, parameter, preset, and class-state checks.

Machine-readable, audio-free evidence is committed in [too-hotty-results.json](validation/too-hotty-results.json), including hashes of the tested DSP source.

## Listening clips

Local exports under the ignored `rendered/` folder:
- `too-hotty-original.wav`
- `too-hotty-natural.wav`
- `too-hotty-hard-tune.wav`
- `too-hotty-deep-formant.wav`
- `too-hotty-wide.wav`
- `too-hotty-manual-edit.wav`

They use the same 78–90-second excerpt, with reported latency trimmed. They have not been loudness normalized. Sample peaks were checked; this report does not establish true-peak compliance or perceptual quality.

## Remaining limits

The owner has chosen to perform Logic UI playback, project reopening, and Logic offline bounce personally. This workflow uses offline processing and a native AU test host; Logic access is not part of it. Those owner-run checks remain outstanding.

The numerical tests establish the behaviors listed above. They do not establish that the result sounds better, that detected note boundaries are all correct, or that this processor replaces industry tools. Real-vocal pitch ground truth, artifact listening, automation/order transitions, tempo/region edits, and comparative listening remain outstanding. This is still a development build.
