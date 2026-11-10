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
6. `wood.py '{params}'` measures what sits between and under the partials
   (the floor per octave with each partial's vibrato skirt masked out, and
   all of it below the fundamental) and each partial's fast flutter by the
   octave it sits in; `fit_wood.py` (FIT=names) fits on the picture score,
   the wobble measures and those together, each note rendered once.
7. `spec_sim.py out.png '{params}'` draws Spitfire against the twin;
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

Lessons (2026-11-09, round 5, from difference spectrograms: Spitfire minus
the engine over time): the flutter goes by where a partial sits, not by the
note (Spitfire's partials under 500 Hz hold within about half a dB, those
over 1 kHz shake 2 to 3 dB, violin and cello alike). Flutter on each mode's
output gain also shakes that mode's off-resonance share of the other
partials, which smeared noise onto the fundamental (the floor next to it
rose 10 dB): put it on each mode's loss instead (only the mode's own ring
moves), with the hair on the string, and bounded (an unbounded log-normal
swing choked a partial for a moment, a dropout). The body's fine ripple
swept by the vibrato filled the space under a low cello note: thin it out
low down. The box's direct path needs a high-pass at its lowest resonance
or a low note booms.

Round 8 (2026-11-10): the violin's hum high-pass, S-shaped stroke and
two-stage let-go, the cello's wood (direct path down, refitted body,
shallower vibrato) and release by partial. In `sim.py` the violin's new terms
blend to their round 7 values at the cello's size (`CELLO`, by
log(size) / log(2.1)) and the cello's are powers of size on top (all 1 at
size 1); `write_engine.py` knows both sets. Lessons:
- Spitfire is sampled every whole tone: each odd MIDI note and the note
  above it are one recording, pitch-shifted. Hold out whole pairs, or the
  "held-out" notes are the fit notes again.
- Two takes of the twin (seeds) already score about 9.5 apart on the picture
  score, so a single note or seed says little: judge on all the notes with 2
  or 3 seeds, and compare an engine render with the twin term by term.
- Match each partial's level over time (a heterodyne track per harmonic,
  against its own sustain level; attack and release errors apart): the band
  terms cannot tell a partial from the noise around it. Over half of the
  band release term is cells more than 40 dB under the note's peak; mask
  them before judging a release (the two-stage let-go reads worse unmasked
  and better masked and by partial).
- "Brassy, not woody" was a too-smooth spectrum: too much of the bridge
  straight through (the direct path) against the body's resonances, and a
  too-deep vibrato. Measure the spectrum's jaggedness and formant contrast
  against the recording's, not only the level profile.
- Refitting a body under a quieter direct path drops the instrument's
  loudness (the picture score is blind to level): check it. The cello's
  gains carry + 4 dB to keep its mean loudness at round 7's.
- A size power on a loss must be bounded after any ramp on it, not before
  (bounding the cello's released loss first delayed its upper partials'
  fall).
- The hum high-pass on the whole bowed bridge follows the note (0.33 x f0)
  but is capped at 0.7 x f0, so a low cello fundamental stays under the
  corner of the direct path's own high-pass and keeps its level.

Needs numpy, scipy, soundfile, numba, mido, matplotlib.
