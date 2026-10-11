# MusicToMyMouth

A dedicated macOS Audio Unit vocal processor for Logic Pro, developed for personal use with production quality as the goal.

MusicToMyMouth processes clean, isolated, prerecorded vocal tracks. It combines automatic pitch correction, manual pitch and timing editing, a configurable vocal effect chain, and factory and user presets.

## Project status

The first milestone records the agreed product specification. Plugin implementation has not started. See [SPEC.md](SPEC.md) for scope, research sources, and the approved native Logic Capture workflow.

## Planned capabilities

- Manually selected key and scale, natural-to-robotic pitch character, independent correction strength and retune speed.
- An advanced editor for individual note pitch and timing.
- Reorderable and bypassable EQ, compression, de-essing, gate, saturation, reverb, tempo-synced delay, formant, and creative effects.
- Preset families for natural polished leads, hard tuning, robotic/formant effects, and wide vocals with delay and reverb.
- DAW automation, project state recall, and user preset saving.

Harmonies and doubling are backlog items. Comping and audio cleanup are handled in other tools.

## Development milestones

Major milestones must be committed and pushed to the GitHub repository after the appropriate checks. Commit messages should identify the completed capability, and milestone notes should describe validation and any remaining limitations. Do not present unverified sound quality as established.

Planned milestones include the AU host foundation, automatic tuning, configurable vocal effects, presets and controls, manual note editing, and Logic integration and audio quality validation. The editor uses Capture: arm it and play the vocal section once in native Logic, then edit pitch and timing. Downloaded vocal stems are supported.

## Project boundaries

This project has its own source tree, build outputs, plugin identity, and AU bundle. SerumSynth is a separate plugin. Existing SerumSynth and VocalSuiteX directories are outside this repository.
