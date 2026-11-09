# Review plan: state and what's next

Updated 2026-09-30, at the move from the Windows PC to cloud sessions.
Review: claude.ai/artifact/LAbrHT1KnLVEiv8Yd3SjHS. Projected "potential" review
(what the synth should be once this plan ships): claude.ai/artifact/XecerytfKfBKdepvZLxKZt.

## The user's decisions (standing)
- Personal use only. Commercial work (licensing, store, installer polish) is parked and forgotten.
- Downloads of samples, wavetables, reference presets: allowed.
- Presets are made by automated pipelines only (no hired people). The user A/B-tests by ear.
- Positioning: "physical hybrid" synth.
- Old presets may change in sound, but must never break level-wise (no silent, clipping or wildly quieter presets).
- Filters: replace the old ones, improve them massively, make warps and the other spectral stuff modulatable.
  The user also suggests reusing Airwindows filters where they fit (there are good ones: e.g. Air, Baxandall,
  Capacitor, Isolator, Pop, Coils, and the uncoded Airwindows filter set).
- Everything from the review except scripting, plus SoundFont (SF2/SFZ) support.
- AI agents maintain the synth for years: keep docs short, current and machine-readable.
- 32 voices, 8 macros. Keep Grand Piano but rename it (it's "not a piano").
- The user has no host automation in sets (choice params made non-automatable).
- At most 2 agents in parallel (weekly usage).

## Next
All steps 1-51 of the old "Next, in order" list are done; the full record is in [REVIEW-PLAN-HISTORY.md](REVIEW-PLAN-HISTORY.md) (Done table, gate state, content, steps). Live state and open issues: [HANDOFF.md](../HANDOFF.md). Exit criteria for RC 1.0: [RC-1.0-CHECKLIST.md](RC-1.0-CHECKLIST.md).
Still open (details in the history, step numbers as there):
- Step 45 Granular controls (grain position vs sample start, scan speed, freeze, stereo spread destination): in progress in its own thread.
- Physical modelling overhaul: Phase 1 (audible knobs) built 2026-10-19; Phase 2 (fit to real recordings, her blind A/B votes) and Phase 3 (missing real-instrument behaviour) wait for her answers to the plan's four questions.
- Listening rounds by ilana are tracked in ROADMAP.md.
