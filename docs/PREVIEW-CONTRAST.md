# Preview contrast investigation

The owner reported that the natural and hard-tune audio previews sounded the same. This report investigates preview selection; it does not establish a perceptual grade or claim the complaint is resolved.

## Measured finding

The original example was selected for high RMS energy at 78 seconds. In its first eight seconds:
- 24.78% of blocks were recognized as voiced.
- Natural correction was 0.86 cents RMS; hard correction was 9.11 cents RMS, weighted by input energy across all blocks.
- Only 12.78% of signal energy corresponded to blocks with hard correction exceeding ten cents.

Waveform comparison confirmed distinct exported audio, but numerical waveform differences do not establish an obvious stylistic difference. The existing validation suite proved pitch accuracy on controlled signals and stable rendering; it did not test whether the chosen preview demonstrated a convincing change.

## New comparison

The preview helper scans for an eight-second window with substantial voiced energy and estimated correction need in the selected C-minor scale. Its chosen section begins at 0.8475 seconds. On that section, the processor recognizes voiced sound in 62.32% of blocks, hard correction is 45.06 cents RMS, and 93.82% of signal energy corresponds to corrections greater than ten cents.

These measurements are detector estimates. They do not prove the song key, correct voice segmentation, audibility, or sound quality. The selection gate (at least 25 cents correction RMS and at least 50% of energy receiving corrections over ten cents) is a demo-selection heuristic.

Local comparison: `rendered/too-hotty-contrast-v2.mp3`. Same phrase repeated in one player, gain-matched where peak headroom permits, with short edge fades and half-second gaps:

| Start | Segment |
| --- | --- |
| 0:00 | Original |
| 0:08.5 | Natural / Studio lead |
| 0:17 | Hard tune / Instant snap |
| 0:25.5 | Robot / Deep formant |
| 0:34 | +12-semitone shift, effects disabled, automatic correction disabled |

The last segment is a deliberate signal-path control, not a natural-tuning preset. The owner judges the listening result.

## Reproduce

Enable `MOUTH_BUILD_VALIDATION`, then build `MusicToMyMouthPreviewContrast`. Run it with an input WAV and a local output directory. A third optional argument forces a start time in seconds. The original 78-second excerpt fails the new selection gate; automatic selection passes it. No plugin DSP or factory preset settings were changed for this investigation.

Audio-free measurements are in [preview-engagement.json](validation/preview-engagement.json). Audio remains in ignored local folders.

## Testing responsibility

The owner performs all Logic UI and listening tests. This workflow uses local offline processing and a native AU test host; access to Logic is not part of it.
