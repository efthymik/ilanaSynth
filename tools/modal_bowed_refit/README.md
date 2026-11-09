# Modal Bowed refit (2026-11-08)

Fits the Modal Bowed exciter (src/dsp/ModalBowed.h) over time to renders of
`render-midi/<violin|viola|cello>-arco-full-range.mid` (ilana's Spitfire
String Quartet, Long Normale, dynamics 90, reverb off). Run from a working
folder; the scripts read and write there.

1. `split.py RENDER.flac MIDI.mid arco/<inst> 80 0.8` cuts the velocity-80
   notes out, one WAV per note, half a second of silence before each.
   Spitfire's pack is ONE combined player (pitch picks the instrument): the
   "cello" render is a cello only below G3 and violins above, so cello fits
   use C2 to F#3 only.
2. `measure.py` prints Spitfire's vibrato, noise floor, rise and release.
3. `sim.py` is a Python twin of ModalBowed.h (a plugin render and the twin
   score the same on the picture score). `look.py` is the picture score:
   1/6-octave spectrogram terms plus partial-by-partial ones (level profile,
   how deep each partial moves at the vibrato's rate, how much it drifts
   slowly), what the eye reads in a spectrogram. `look_score.py '{params}'`
   scores the twin over 8 violin and 8 cello notes.
4. `fit_look.py [start.json]` fits the engine's constants jointly to the
   picture score (`FIT=rel_s,fade_db,ring,ring_s fit_look.py ...` fits
   only those); `fit_body2.py <inst> params.json` then refits a body's
   gains through the whole engine (each note's partial shape and loudness),
   `write_bodies.py violin cello` and `write_engine.py look_fit.json` write
   them into ModalBowedBodies.h and ModalBowed.h. (`fit_att.py`,
   `fit_noise.py`, `score.py` are the first pass's fits, kept for the record.)
5. `wobble.py '{params}'` measures the bow's fine randomness partial by
   partial (band-passed, analytic signal: each partial's skirt and flutter up
   to 40 Hz, the pitch's drift, the vibrato cycle to cycle, the brightness's
   wobble, the noise under the partials), Spitfire's against the twin's;
   `fit_wobble.py` fits the flutter, drift and vibrato wander on the picture
   score plus those (instrument medians: one random take can't match note
   for note).
6. `spec_sim.py out.png '{params}'` draws Spitfire against the twin;
   `spec.py PREFIX out.png` and `score_render.py PREFIX` against plugin renders
   of demo-violin.mid / demo-cello.mid (commands in score_render.py);
   `levels.py params.json` compares each note's loudness.

Lessons (2026-11-08): a per-block step in a partial's gain zips (broadband
noise that faked a release tail): glide gains over the block. One-pole noise
for slow wander still jitters at the block rate: use two poles. Keep a
body's loudness offset when refitting its gains. Measure a release on
high-passed audio: the bow's push leaves a slow offset on the string that
hid a cliff (the bow slowing to a stop braked the note 0.35 s after the
note-off; the bow now lifts off at speed). A spectrogram's window hides
fast flutter: measure it on band-passed partials. Spitfire's partials
flutter about 2 dB at 10-40 Hz on their own (the violin's quicker than
the cello's); a pitch jitter on the string's modes instead broke the bow's
stick-slip into noise on low notes.

Needs numpy, scipy, soundfile, numba, mido, matplotlib.
