# MusicToMyMouth

A separate macOS Audio Unit vocal effect for personal use in Logic Pro.

## Agreed scope

- Process clean, isolated, prerecorded vocal clips and stems on Logic audio tracks.
- Automatic pitch correction with manually selected key and scale.
- Natural-to-robotic pitch character control plus independent correction strength and retune speed.
- Advanced manual editing of note pitch and timing.
- Editable, reorderable, bypassable vocal processing modules: EQ, compression, de-essing, gate, saturation, reverb, tempo-synced delay, formant and creative effects.
- Factory presets in four families: natural polished lead, hard tuning, robotic/formant effects, and wide vocals with delay and reverb.
- Full parameter controls, user preset saving, DAW automation, and project state recall.
- Aim for production quality suitable for finished music; quality must be demonstrated rather than inferred from feature count.

## Deferred or external workflows

Harmonies and vocal doubling are backlog items. Take comping, audio cleanup, noise removal, and breath removal are handled elsewhere.

## Project isolation

MusicToMyMouth has its own source folder, build outputs, plugin identity, and Audio Unit bundle. SerumSynth is a separate product and is not modified by this project. VocalSuiteX is an existing prototype available for read-only reference; reuse has not been assumed.

## Manual editor integration decision

ARA2 offers direct access to host audio regions and tighter synchronization. Current Antares Logic support documentation requires Logic to run in Rosetta on Apple Silicon for its ARA mode. A standard native AU can also offer manual pitch and timing editing through an explicit playback-transfer/capture workflow, as documented by Celemony. Manual editing does not inherently require ARA2.

The user has requested an explanation of these workflows before selecting the integration approach. Keep both automatic tuning and manual pitch/timing editing in scope.

## Research sources

- https://voloco.com/plugin.html
- https://voloco.com/plugin-manual.html
- https://www.antarestech.com/documentation/auto-tune-pro-11
- https://help.antarestech.com/hc/en-us/articles/41115595800084-How-to-Use-Auto-Tune-Pro-as-an-ARA2-Plug-In-in-Logic-Pro
- https://helpcenter.celemony.com/M5/doc/melodyneEssential5/en/M5tour_TransferARA?env=live
- https://www.waves.com/1lib/pdf/plugins/tune-real-time.pdf
- https://www.izotope.com/products/nectar-advanced
- https://www.soundonsound.com/techniques/inside-track-lil-uzi-vert
- https://www.soundonsound.com/techniques/inside-track-kendrick-lamar-black-panther-all-stars
