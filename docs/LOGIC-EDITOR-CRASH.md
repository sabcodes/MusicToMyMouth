# Logic editor-open crash

## Report and cause

The owner reported Logic's Audio Unit instability warning and lost sound on inserting MusicToMyMouth. Both local AUHostingService crash reports from October 11, 2026 at 22:28 showed a null-address EXC_BAD_ACCESS in `MusicToMyMouthEditor::resized()`, called synchronously by the editor constructor's initial `setSize()`.

That call preceded construction of the eight module cards and piano roll. The layout indexed the empty module vector and would also dereference the uninitialized piano roll. An editor crash takes down Logic's AU hosting process, explaining the warning and interrupted audio.

The fix moves resizing setup, resize limits and initial size until after all layout children have been created, before starting the UI timer. DSP and preset behavior are unchanged by this fix.

## Reproduction and checks

- Original linked editor: standalone reproduction exited 139 (SIGSEGV), with the same `resized()` crash stack.
- Fixed linked editor plus old installed AU: local editor checks passed; opening the old installed Cocoa editor exited 139, with the same stack. This confirmed that the installed editor was also affected.
- Fixed direct editor: three construction/destruction cycles, all three tabs, minimum/default/maximum window sizes, message-loop timer activity and page painting passed before audio preparation.
- Fixed installed native AU: a plain Cocoa/Core Audio host opened its editor before initialization, reopened it twice, resized it, and rendered two seconds of finite, non-silent audio with the editor alive. Mono/stereo at 44.1/48 kHz all passed.
- Apple `auval -v aufx Mtmm Sbcd`: AU VALIDATION SUCCEEDED.
- Existing authorized Too Hotty validation runner: 72 checks passed, zero failures (without `--full`). Includes installed-AU stem rendering and raw host bypass timing.
- Installed bundle signature verification and build/installed Mach-O UUID agreement passed.

An initial combined validation host linked its own JUCE runtime and loaded the AU's second runtime; it aborted on a heap operation after the local editor tests. That host design was discarded. The committed tests separate direct JUCE editor tests from the plain native Cocoa host. The latter mirrors a host that does not embed JUCE and passes against the installed bundle. No Logic controls were accessed.

Local raw logs and generated audio remain ignored under `build/validation/logic-editor-crash/`. Numeric results are recorded in `validation/logic-editor-crash.json`. User audio and full crash reports are not committed.

## Owner retest

The fixed component is installed in the user's Audio Unit Components folder. Fully quit and reopen Logic so it loads the rebuilt binary. Remove and reinsert MusicToMyMouth if the previous instance remains failed. Check that the plugin opens, the vocal continues playing, and the input/output meters respond. Owner Logic testing and feedback remain pending; standalone host results do not establish successful playback in the actual Logic session.
