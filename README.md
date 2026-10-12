# MusicToMyMouth

A dedicated native macOS Audio Unit vocal effect for personal use in Logic Pro. It processes clean, isolated prerecorded vocals and downloaded stems.

## Current milestone

The first AU implementation is built for Apple Silicon. It includes automatic pitch/formant processing, eight reorderable effect modules, 12 factory sounds, user sound presets, host parameters, and a Capture editor for per-note pitch and timing changes. Project state embeds captured audio and note edits.

This is a **development build**, with production quality still the goal. Apple AU validation and automated vocal/DSP checks have passed. Direct Logic UI playback and listening comparisons remain outstanding; see [vocal validation](docs/VOCAL-VALIDATION.md) for the tested scope and measurements. See [SPEC.md](SPEC.md), [architecture](docs/ARCHITECTURE.md), and [milestone notes](docs/MILESTONES.md).

The editor-open crash reported in Logic has been fixed and the rebuilt AU installed. Native Cocoa editor/audio lifecycle checks pass; owner retesting in Logic is pending. See [editor crash fix](docs/LOGIC-EDITOR-CRASH.md).

The owner handles Logic UI and listening tests. This development workflow uses local offline tests and a native AU test host. For the investigation of the initially weak listening previews, see [preview contrast](docs/PREVIEW-CONTRAST.md).

## Use in Logic

1. Import a clean isolated vocal stem onto a mono or stereo audio track.
2. Insert **Audio FX → Audio Units → Sabarno Islam → MusicToMyMouth**.
3. Choose the song key/scale and a sound. Tune controls range from natural correction to fast hard tuning; transpose and formant controls are independent.
4. Open **Vocal Chain** to adjust, bypass, or move EQ, compression, de-essing, gate, saturation, reverb, tempo-synced delay, and modulation.
5. For manual edits, open **Capture & Edit**, arm Capture, and play the section once. Press **Finish**, or stop Logic. Select a detected note, set pitch/start/duration/correction, and press **Apply note edit**. Replay that timeline section to hear it.

Downloaded stems work through this playback transfer. Logic runs natively; ARA and Rosetta are not required. Capture is limited to one contiguous take per instance, up to three minutes or 128 MiB of raw audio. Loops or seeks end the capture. Finish capture/analysis before saving the project. Moving, replacing, or time-stretching the Logic source region requires recapture. **Play edited Capture** switches between the captured take and current incoming track audio; automatic tuning still applies to untouched notes.

Sound preset files (`.mtmm`) include effect settings and module order. They preserve the current song key/scale and exclude captured audio. Logic project saves include key/scale, effect settings, order, Capture media, and note edits. Embedded media increases project size. Undo keeps up to 20 note edits and shares the original media.

Harmonies and doubling are backlog items. Comping and audio cleanup are handled elsewhere.

## Build and install

Requires macOS, CMake 3.24+, Apple command line developer tools, and network access for pinned dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release -j 6
./scripts/install-au.sh
```

To run the authorized vocal checks on another local stem:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DMOUTH_BUILD_VALIDATION=ON
cmake --build build --target MusicToMyMouthValidation MusicToMyMouth_AU -j 6
./scripts/install-au.sh
./scripts/validate-vocals.sh /absolute/path/to/vocals.mp3 --full
```

Editor lifecycle regressions can be run separately (the native host deliberately does not link JUCE):

```sh
cmake --build build --target MusicToMyMouthEditorValidation MusicToMyMouthNativeEditorValidation -j 6
./build/MusicToMyMouthEditorValidation
./build/MusicToMyMouthNativeEditorValidation
```

The runner tests the installed AU as well as its offline processor. Audio and result files stay inside ignored build folders. `--full` adds complete-stem checks for the natural lead and hard-tune sounds; all factory sounds use a vocal-rich excerpt. This requires ffmpeg.

The default build downloads JUCE at a pinned commit and Rubber Band 4.0.0 with a pinned archive checksum. `-DJUCE_SOURCE_DIR=/absolute/path/to/JUCE` is an optional local dependency override. Build outputs stay in this repository's ignored `build/` directory. Installation copies only `MusicToMyMouth.component` into the user's AU Components folder and signs it locally. Reopen Logic after installation.

## Project boundaries and milestone commits

Source folder: `/Users/sabarnoislam/MusicToMyMouth`.

Private repository: https://github.com/sabcodes/MusicToMyMouth

SerumSynth and VocalSuiteX are separate projects and are not modified. Major milestones are committed and pushed, with capability, build evidence, and remaining limitations recorded in the milestone notes.
