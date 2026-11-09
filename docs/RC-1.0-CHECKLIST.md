# RC 1.0 exit criteria

Features are frozen until RC 1.0 (HANDOFF.md). RC 1.0 is reached when every box below is ticked, on one commit. Owner in brackets.

- [ ] Gates green on the RC commit: Linux `tools/verify.sh`; Windows `tools/verify.ps1`; unit tests, FX tests, `--uitest`, pluginval. Fingerprints: 0 unintended changes against the Linux baseline `tests/fingerprints-linux.csv` and the Windows baseline (compare only within a platform). [agent]
- [ ] Installed on ilana's PC with `build-and-install.cmd`, loads and plays in Ableton Live 12 (VST3 instrument and FX). [ilana]
- [ ] An old Live set (saved with an earlier install) opens, sounds as before and re-saves. Includes any set using Toy Grand or Dense Board Piano with BOARD macro or MIX routes (see the Dense Board note in HANDOFF). [ilana]
- [ ] No known HIGH bug in HANDOFF "Open issues" (fixed, or written down as accepted by ilana). [agent, ilana]
- [ ] Build label visible: the short git hash and date shown in the plugin and printed by `build-and-install.cmd` (`build\build-label.txt`), so she can tell which build runs. [agent]
- [ ] Presets level check: no factory preset silent, clipping or wildly quieter than its category (`ILANA_LIBRARY_TEST`, `tools/tune_presets.py`). [agent]
- [ ] CPU test: 8 instances in Live playing a dense preset, MULTI-CORE on and off, no dropouts at her usual buffer size. Note the CPU % for both. [ilana, agent]
- [ ] Mac build by Leo: builds, passes unit tests and pluginval, loads in Live on macOS. [Leo]
- [ ] Listening rounds in ROADMAP.md closed or waived by ilana. [ilana]
- [ ] Version bump in every place (`src/PluginEditor.h` `appVersion`, `TutorialOverlay.h` `whatsNewVersion`, `CMakeLists.txt`, `installer/ilanaSynth.iss`, README badges) and a git tag. [agent]
