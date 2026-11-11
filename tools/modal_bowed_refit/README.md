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

Round 12 (2026-11-11), the cello's: its own noise layers. Nothing reaches a violin's size (the violin renders bit
for bit as round 11's) nor a pluck. New:
- The cello's table gains `rs0_db`, `rs1_db`, `rs2_db` (19 notes): residual layers exactly as the violin's (same
  generator and loops, built with the violin's on the same background thread: +7.1 MB, 22.7 MB in all, about 1 s).
  `note_rows(..., layer=True)` weighs a table's layers w (1 - |t - u_table|): w (1 - t) for the violin's (as round 11)
  and w t for the cello's. Played, they replace `rsc_cflr` of the low floor, `rsc_hf` of the string's top and `rsc_nfl`
  of the release floor (all 1, by their level; those terms are by t already); `rsc_hiss` (0) is the twin's only.
- Each table's own layer envelope (`rs_env` in the table's json over DEFAULT's: the cello's `rs0_rdb` 20, `rs0_att`
  0.4): `ModalBowedNotes::Table::envelope`.
- Per-note `sn_db` (the sine wander's depth) and `am_db` (the level vibrato's) in the cello's table, replacing the global.
- `kn_on` / `kn_rl` refitted (a second frozen-bow pass with the layers in), and `lp_hz` 5590 -> 3000 at the cello's
  size (blended by t; the layers come after the roll-off). The pizzicato keeps 5590 (`tiltHzPizz`).
- The engine picks a note's layers once the tables' weights at its size are known (`pickLayers`: the residual table
  weighing most, the first on a tie, as round 11 took the first); a note whose loops aren't built yet plays round 11's
  floor, top and release floor.
Twin, demo notes, 3 seeds: cello 11.39 -> 10.07 (sustain 1.75 -> 0.96); the engine alone, 3 takes: 11.83 -> 10.62, each
term within 0.1 of the twin's change; about 1.5 dB louder on the demo notes, as the twin. Lessons:
- What the cello's round 10 noise paths (low floor, string top, release floor) left was again each note's own noise
  spectrum: the violin's layers carry over unchanged, with a slower, deeper let-go step on the sustain layer.
- The plugin's demo render gains about 0.8 less than the engine alone: the plugin's Init-patch render is 1 to 10 dB
  darker than the bare engine from about 9 kHz up (the same before and after); the layers, matched to Spitfire up
  there, show it where the round 11 cello's too-bright top hid it. Judge engine changes on the engine alone (or the
  twin) as well as on the demo.

Round 12 (2026-11-11), the violin's: each partial's and each layer's level over time. All in the violin's table, by its
weight (nothing reaches the cello: rendered alone it plays bit for bit as its round 12 above), never on a pluck. New:
- `kv_su` [8 octaves][4 knots at `su_t` 1.0, 1.9, 2.8, 3.7 s]: dB on each partial's gain over the held note (its mean over
  1.5-3.5 s 0), spread over the partials as the cello's knots, held before the first knot and after the last (through
  the release too). Engine: `ModalBowedNotes::Row::knotSustain`, `knotSustainSeconds`.
- `kv_on` [8][6] and `kv_rl` [8][5]: the cello's `kn_on` / `kn_rl` mechanism and knot times, in the same engine columns
  (`knotOn`, `knotRelease`), now played without the cello's other terms (`levels<cello, knots>`).
- `rk_on` [3 layers][6] and `rk_rl` [3][5]: the same knots (dB, amplitude) on each residual layer, from the note-on (0 from
  0.9 s) and after the let-go (0 at the let-go, the last held). Engine: `Row::layerOn`, `layerRelease`.
- The table's `slow_db` at 0.35 of round 11's.
- In the twin only: `rk_su` (a hook, unused) and the frozen-bow fitting masks (`_mmask`, `_ymask`, `_rsmask`).
Twin, demo notes, 3 seeds: violin 10.01 -> 9.29 (attack 4.57 -> 4.16, release 4.71 -> 4.13); the engine alone, 3 takes:
10.30 -> 9.56, each term within 0.2 of the twin's change. CPU per held violin voice about 1.5 % more (the knots each block).
All 42 notes (the fit's run): -0.66 (better on 40 of 42; held-out notes -0.77), from the attack (-0.36, all 42) and the release
(-0.55, all 42). Lesson: as on the cello in round 11, what was left on the violin was the shape of the levels over time, of
the partials and of the noise layers alike; knots fitted with the bow frozen carry it, held-out notes as much as fitted ones.

Round 13 (2026-11-11), both instruments: every linear output part refitted at once. All in the tables, by their weight,
never on a pluck. New:
- A joint frozen-bow refit (two passes, 6 + 6 takes, validated on seeds 4-6) of each note's linear columns together on
  the whole picture score: `gain_db` (one per partial now, up to 64), the partials' onset / release knots (`kv_on`,
  `kv_rl`; the cello's `kn_on`, `kn_rl`), the held note's knots `kv_su` on 10 knots (`su_t` every 0.3 s from 1.0 to
  3.7 s, was 4 every 0.9 s), the three layers' 45 band levels (`rs0_db`, `rs1_db`, `rs2_db`) and their knots `rk_on`,
  `rk_rl` and the new `rk_su` [3 layers][10] (dB on each layer at `su_t`, held before the first knot and after the last,
  through the release). Engine: `Row::layerSustain`, played in `layerKnots` beside the onset and release knots.
- The cello's table has `kv_su`, `rk_on`, `rk_rl`, `rk_su` too: the twin now plays them from any table by its weight,
  as the engine already did (round 12 kept them to the violin's size in the twin only).
- `out_db` per note: dB on the note's whole output (`ModalBowedNotes::outputDb`; the engine's `outputNow` and the hiss,
  floors, rumble and layers at note-on), keeping each note's loudness at round 12's: the picture score is blind to
  level, so the joint fit moved it (violin -8.4 to +2.8 dB, cello -5.4 to +1.8).
- The released bowed note's fade (`releaseFadeAtSize`) goes a sample at a time (`levelFall`), as the twin's: a step
  every 32-sample block amplitude-modulated the ringing string at the block rate, a buzz of lines at 1.5 kHz and its
  multiples around each partial in a cello's tail. A pluck still steps a block (pizzicato bit for bit).
Twin, all notes, seeds 4-6: violin 9.75 -> 8.68 (better on 41 of 42), cello 9.20 -> 7.88 (19 of 19). Demo notes:
violin 9.29 -> 8.32, cello 10.07 -> 8.08 (3 seeds). The engine alone, 3 takes: violin 9.56 -> 8.42, cello 10.62 ->
8.78, each term within 0.1 of the twin's change but the violin's plevel (-0.39 against the twin's -0.13); loudness
within 0.5 dB of round 12's on the demo notes. CPU per held voice +0.3 to +0.4 % (violin) and +0.7 % (cello) in
callgrind; the tables +19 KB (Row +79 floats), each voice +1.8 KB (`knotSustain` 10 columns), the layer loops
unchanged (22.7 MB). Lessons:
- The separate rounds' fits (each part fitted with the others fixed) left gains in their interplay: fitting every
  linear part at once with the bow frozen, by exact gradient on the whole score, improved 41 of 42 violin notes and all
  19 cello notes, every term better on average.
- A fit on a level-blind score drifts the level: keep a per-note output gain (`out_db`) to put the loudness back.
- The engine's block-rate steps show once the rest matches: the release fade's 32-sample step was a visible buzz in the
  cello's tail spectrum; anything that ramps a level over the note should go per sample.
- The plugin's Init patch is not the engine: its F1 (12 kHz, 12 dB low-pass, reso 0.25) is what makes the plugin's
  render 1 to 10 dB darker than the engine from about 9 kHz up (round 12's lesson), and its AMP (sustain 0.8, release
  0.8 s in `score_render.py`, 2.5 s in the demo renders) reshapes the release. For scoring a plugin render against the
  twin, render with `ILANA_SET="osc1_excite=Modal Bowed,amp_release=5,amp_sustain=1,f1_cutoff=20000"` (and
  `osc1_modal_size=2.1` for the cello): the demo with the Init patch scores violin 10.26 -> 9.64, cello 11.44 -> 9.77; rendered so, violin 9.69 -> 8.76, cello 10.53 -> 8.75 (both
  rounds better by 0.6 to 1.0 there, the change itself close to the engine alone's).

Needs numpy, scipy, soundfile, numba, mido, matplotlib.
