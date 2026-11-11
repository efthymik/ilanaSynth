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

Round 9 (2026-11-10): per-note tables, the violin's hall floor, the cello's release floor.
`sim.py` plays both instruments' round 9 by default (`DEFAULT` the violin's, `CELLO` what each
blended term is at the cello's size, so a cello keeps round 8's values for everything the
violin retuned, and the violin round 8's for everything the cello retuned). New:
- The per-note tables `notes_violin.json` (size 1, MIDI 55..96: 24 partial gains and nine
  scalars per note) and `notes_cello.json` (size 2.1, MIDI 36..54, held below, faded out over 4
  semitones above: 64 partial gains, onset darkening, released loss and let-go drop, and six
  scalars). `write_engine.py` writes them to `src/dsp/ModalBowedNotes.h` (a `Row` array and a
  `Table` entry per instrument: another instrument's table is one more of each). A table weighs
  1 at its own size and nothing a quarter of the way (log size) to the other; its scalars replace
  the value at that size before the size powers. `play(..., tables=[])` plays without them (a
  fit of a table passes its own values in kw).
- The violin's hall floor (`flr_*`): a pink noise about 40 dB under the note after the let-go's
  gain, rising with the note and fading 24 dB a second once let go; the hiss grows with the
  register (`hiss_k`). Faded out by 1 - t in level toward the cello.
- The cello's release floor (`nfloor_db`): once let go, the bow's noise falls no lower than
  11.7 dB under its level at the let-go, fading 20 dB a second (by t, none on a violin).
- The cello's gains are 1.36 dB down (ModalBowedBodies.h): under its round 9 terms (mostly the
  direct path, `dir_sz` -12 -> -7.7) it was that much louder than round 8.
Twin, demo notes, 3 seeds: violin 16.6 -> 13.1, cello 17.1 -> 13.5. Lessons:
- The tables carry most of the gain on the level-by-partial term (violin plevel 3.4 -> 1.5,
  cello 3.1 -> 1.4) and they hold on held-out notes: Spitfire's partial levels are each
  recording's own (the body, the player, the microphone), not a smooth function of pitch.
- Interpolating a table between neighbouring notes scored worse than no table at all: a
  note's row only fits its own recording (and its pitch-shifted pair). Pick the nearest note.
- A table fitted with one body is wrong on another: keep it to the size it was fitted at.
- The violin's release was too slow and too bright up high: a faster let-go (rel_s 1.19 ->
  0.30 s) under the hall's own floor, which keeps the tail from falling into silence, halved
  the release term (8.7 -> 5.2).
- A global retune made for one instrument hurts the other: blend every retuned term back to the
  other instrument's value by size (the cello twin scored worse under the violin's round 9).

Round 10 (2026-11-10), the violin's: the noise and the body between the partials. Every new
term fades out toward the cello by size (`CELLO` gains `slow_hz` 0.6, `hs_depth` 0, `lp_k` 0),
so none of it reaches a cello (rendered alone, the cello played bit for bit as round 9's), and
the pizzicato is as before. New:
- The body's air mode (`a0_*`, about 277 Hz, 9.6 Hz wide) rung by the hair's noise and a share
  of the hall floor, by note (`a0_db` in the table), and the body's own 300-500 Hz modes rung
  by the hair's noise too (`b1_*`).
- The hall floor high-passed twice (`flr_ord` 2) with a low rumble resonance of its own
  (`rb_*`, about 70 Hz).
- The hiss about 38 dB louder at A4 (`hiss`, with a lower high-pass), rising with the note up to
  `hiss_cap` (about B5) and per note (`hiss_db`), and with the rosin's noise building over the
  first `hs_s` (0.3 s) of a bowed note from `1 - hs_depth` (`hs_rn` 1).
- A faster wander (`slow_hz` 0.6 -> 3 Hz), a little more hair noise (`nz_amt`), a higher hum
  corner (`hum_k`), and the table's onset darkening by partial (`dark_db`, 48 partials).
- `rnb*` and `lp_k` stay in the twin for fitting, at 0: the engine has neither.
- The VIBRATO knob plays 30 v ^ log2(30 / 14.65) cents, so its default 0.5 is the fitted
  14.65 (it was 30 v ^ 1.737, 9 cents at 0.5): `vib_ct` is the knob at 0.5.
- The tables are written to 6 digits (round 9's 5 rounded a few of the json's values).
Twin, 42 notes: 14.0 -> 12.5 (better on 41 of 42, held-out notes as much as fitted ones);
demo notes 13.5 -> 12.0. Lessons:
- Most of it is the sustain and attack terms, the noise between and under the partials: the
  body's lowest modes rung by the bow's noise and a hiss that builds as the bow bites in.
- The wander is faster than round 6's 0.6 Hz: 1.2, 2, 3 and 4.5 Hz scored 0.13, 0.22, 0.23 and
  0.20 better (alone, all 42 notes), so 3. Its depth (`slow_db`), the pitch drift and the
  loudness wander didn't help.

Round 10, the cello's: its movement, floor and top. Every new term comes in by t (nothing at a
violin's size, nor on a pluck); the `CELLO` dict gains `vib_rate`, `vib_delay`, `vib_rise`,
`vib_depth_dev` and `fl_hz` (the violin keeps its own) and retunes `slow_db`, `loud_db`,
`rip_db` and `fl_db`. New:
- A sine wander per partial (`sn_*`, about 0.74 Hz, the rate spread +-37 % across the partials
  by the golden ratio, the phase from the seed) in place of most of the noise wander (`slow_db`
  1.50 -> 0.09): it moves each partial as much in every take and sums to nothing over a second
  or two, where the noise left a random tilt on a note's partials.
- The flutter less its slow part (`fl_hpz`, a one-pole at 1.2 Hz on the block rate).
- The low partials' level vibrato (`am_*`, 1.9 dB under about 430 Hz, 1.19 rad ahead of the
  pitch's).
- The low floor (`cflr_*`): the violin's hall-floor path with the cello's own level, band (30 to
  206 Hz, one high-pass: the violin's second, `flr_ord`, goes by 1 - t), let-go step and fade,
  per note `cflr_add`; the release floor under the bow's noise higher and quicker (`nfloor_db`
  -11.7 -> -5, `nfloor_dbs` 20 -> 47).
- The string's top past its last mode (`hf_*`): its own noise, as loud as the stroke, four
  poles high-passed at the last mode and four low-passed at 6.5 kHz, on notes 36-40 and 45-46
  only (per note `hf_add`; -100 is off, and the engine skips the filters).
- The release and room size powers retuned (`fade_k`, `ring_k`, `rloss_k`, `rpow_k`, `rg_k`,
  `room_lk`, `room_hk`, `room_sz`) and the table's `gain_db` refitted.
The cello's vibrato in its table is played directly (`vib_ct`), the knob's 0.5.
Twin, demo notes, 3 seeds: cello 13.5 -> 12.1 (about 0.85 dB louder); the violin as above.

Round 11 (2026-11-10), the cello's: each partial's level over time. Nothing reaches a violin's size
(the violin renders bit for bit as round 10's) nor a pluck. New:
- The low partials ring on once let go (`lr_*`, by t): each partial's gain rises `lr_dbs` (9.8) dB a
  second for `lr_cap` (0.59) s after the let-go, as 1 / (1 + (hz / `lr_knee`) ^ `lr_slope`) by partial
  (260 Hz, 3.3), against the fade.
- A gain envelope by octave and time knot in the cello's table: `kn_on` [8 octaves, centres 110 Hz x
  2 ^ k][knots at 0, 0.05, 0.1, 0.2, 0.35, 0.6 s from the note-on; 0 at 0.9 s and after] and `kn_rl`
  [8][knots at 0.05 to 0.6 s after the let-go; 0 at the let-go, the last held], dB on each partial's
  gain, interpolated over the partials in log frequency (held past the end centres), in the exponent
  the block's gain glide already takes. Fitted note by note with the bow frozen (each take's own
  bow noise held, so the knots fit the level, not the take). The engine weighs the knots by the
  cello table's weight (nothing at the viola's size) and the ring-on by t.
Twin, demo notes: cello 12.22 -> 11.58 (+0.17 dB). Lessons:
- What was left on the cello was the shape of each partial's level over the first second and the
  release, not its sustain: a few knots per octave carry it, and a frozen bow keeps the fit from
  chasing one take's noise.

Round 11 (2026-11-10), the violin's: the noise between and under the partials as the note's own.
Each violin note's table gains `rs0_db`, `rs1_db`, `rs2_db`: the band energy (dB, look.py's 45
1/6-octave bands with STFT bins, as the score's STFT reads them, at an envelope of 1) of three
residual layers, the sustain's, the onset's and the release's. Each layer plays two noise loops of
coprime lengths (`RS_L1`, `RS_L2` = 16384 and 11 ^ 4 = 14641 samples), synthesised from those band
levels with a random phase per bin (`_rs_noise`, calibrated per loop by `_rs_cal`); nothing but the
table is stored. Envelopes (amplitude, per block, glided): the sustain 1 - exp(-t / 0.25), once let go
10 dB down over 0.05 s and fading 60 dB a second; the onset (1 - exp(-t / 0.005)) exp(-t / 0.25); the
release nothing while held, rising over 0.15 s once let go and fading 40 dB a second. With `rs_replace`
the hall floor and its rumble, the air mode and the hair's noise into the 300-500 Hz modes go by keep
= 1 - the layers' table weight (none on a violin note, all on a cello), and the layers by that weight
and 1 - t. The engine (ModalBowedLayers.h) builds every note's loops once per process on a background
thread (0.7 s, 15.6 MB as floats at 48 kHz) and plays a note without them, as round 10, until its
loops are built; offline renders wait for them. Their level follows the bow's speed (SPEED) against the
fitted one (the string's level does, about as much; GRIP moves it under 1 dB); velocity is the voice's.
Twin, all 42 notes: 12.53 -> 10.40 (16384/14641; 65536/59049 scored 10.38, 8192/6561 10.43). Lessons:
- What round 10's noise paths could not reach was each note's own noise spectrum: the bands between
  and under the partials, which no smooth model of the bow's noise matched note for note. A table of
  45 band levels per layer and note, played as shaped noise, carries it, as the partial gains did.
- Replace, don't add: the round 10 floor, air mode and body noise under the layers scored worse
  (11.57 -> 10.45 with them off and the layers refitted); a two-band sustain layer gained nothing.
- The onset and release are layers of their own (their spectra differ from the sustain's); the
  release's step down and fade (10 dB, 60 dB a second) and the release layer's 40 dB a second were
  the biggest gains after the layers themselves.
- A loop that simply wraps repeats: half of a layer's energy came back every 0.31-0.34 s (its
  autocorrelation at the loop lengths was about 0.5), a pattern the ear can catch in noise. Each loop
  now jumps to a random place once a length, crossfading over 15 ms with equal power (the two places
  are uncorrelated): the autocorrelation there is near 0, the spectrum and the score unchanged.

Needs numpy, scipy, soundfile, numba, mido, matplotlib.
