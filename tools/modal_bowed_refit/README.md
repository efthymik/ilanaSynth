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

Round 14 (2026-11-11), both instruments: the take to take.
What two takes of the twin differ by (its seed against its seed, "t2t") the score counts as error. Measured stream by stream
(the twin's kw `_sd` gives each random stream its own seed: 0 the bow's noise, 1 the slow wander (each partial's, the
loudness's, the vibrato's rate and depth), 2 the flutter and pitch drift, 3 the floor, 4 the sine wander's phases, 5 the
string top's noise, 6 the layers' start, 7 their jumps), and Spitfire's own steadiness measured where one take allows
(wobble.py's measures on all notes, the whole note's level drift, each partial's roughness in the attack and release):
- About 60 % of the violin's attack and release cells (27 % of the cello's) hold no partial: there two takes differ by
  4.2-4.6 dB, and the twin differs from Spitfire by 4.7-4.8: noise against noise. Spitfire's noise wobbles over time as
  much as the twin's (sustain wobble by band within 0.3 dB), so that part of the grey is honest; it stays.
- The bow's chaotic onset is not where the attack's take to take comes from: the rosin's jitter brought in over 0.3 s
  (the string's onset then noise-free) left the attack's scatter in the partials' bands unchanged (2.18 -> 2.20 dB) and
  the score with it. The slow wander (1.5 dB of it alone) and the flutter (1.3) are.
- Spitfire's whole note is steadier in level than the twin's (its level's drift under 2 Hz: violin 1.72 against 1.94 dB,
  cello 1.45 against 2.01) and its partials drift less together (their slow correlation 0.10 against 0.21); its vibrato
  depth varies cycle to cycle at least as much as the twin's (violin 0.44 against 0.38, cello 0.58 against 0.32), and its
  pitch drifts as much (violin 3.6 cents against 3.6).
So, in sim.py's DEFAULT and CELLO (zero CPU: four constants):
- The vibrato's depth wander half as deep (vib_depth_dev 0.4561 -> 0.22805, the cello's 0.31309 -> 0.156545) and the
  vibrato's wanders three times quicker (vib_drift_hz 0.8 -> 2.4 Hz, both sizes): the depth still varies cycle to cycle
  (violin 0.41 against Spitfire's 0.35 on 9 notes), but no longer drifts a whole note's partial levels over a take; the
  depth's spread take to take halved (1.9 -> 1.0 cents).
- The cello's loudness wander half as deep (CELLO loud_db 0.77633 -> 0.388165).
- Each note's partial gains moved back to round 13's mean levels (the wanders' depth shifts them), then round 13's joint
  frozen-bow refit run once more on top (fit seeds 1, 2, 3, 7, 8, 9; validated on 4-6), out_db relevelled (each note's
  loudness round 13's).
Twin, all notes, seeds 4-6 (paired by note, se over notes):
  violin 8.520 -> 8.034 (-0.486 se 0.041, better on 41 of 42; held-out pairs -0.423, 16 of 16); globals alone -0.242
  (39/42), the refit on top -0.245 (39/42). Take to take (8 seeds, 28 pairs) 6.58 -> 5.92.
  cello 7.798 -> 7.491 (-0.306 se 0.035, 19 of 19; held-out pairs -0.374, 6 of 6); globals alone -0.128 (17/19), the refit
  on top -0.178 (19/19). Take to take 5.68 -> 5.27.
  Demo notes, seeds 1-3: violin 8.32 -> 7.78, cello 8.08 -> 7.68.
  Control (cello): the same refit pass on round 13's cello alone gains -0.177 (18/19, take to take 5.66 -> 5.63): the refit's
  share is another pass's, not the calming's. Against that control round 14's cello is -0.129 (15/19, held-out 5/6), the
  globals' own share, and its take to take 0.39 lower. (No violin control: the refit's -0.245 there is likely the same kind.)
Tried and left out: no loudness wander on the violin (attack and release better, wobble and vibam worse: even), the pitch
drift off (Spitfire drifts as much; worse on the cello), the rate wander off, the violin's per-partial noise wander as the
cello's sines (-0.016, 28/42: not worth the cello path's CPU on every violin voice), the flutter halved (+0.43 / +0.41:
Spitfire's upper partials flutter), the ripple halved (+0.24 violin), the rosin's jitter halved (even / +0.05), the rosin's
jitter faded in at the onset (even).
Engine: `depthWander`, `vibratoWanderHz`, `depthWanderCello`, `bowDriftDbCello` and the tables; `write_engine.py` now writes
`vibratoWanderHz` too (from `vib_drift_hz`, one value at every size). The engine alone, 3 takes: violin 8.42 -> 8.10, cello
8.78 -> 8.42, each term within 0.1 of the twin's change but the violin's plevel (-0.03 against the twin's -0.33 on seeds 1-3:
take luck, round 13's twin was 0.31 worse there than on other seeds; on 6 more takes / seeds the engine moves 8.43 -> 7.99,
plevel -0.09, the twin 8.56 -> 8.02, plevel -0.13). The plugin (demo, Init patch) violin 9.64 -> 9.21, cello 9.77 -> 9.23;
rendered with the AMP open and F1 at 20 kHz, violin 8.76 -> 8.28, cello 8.75 -> 8.23. Loudness of the held part within
0.4 dB of round 13's; the cello's G2 (43) starts about 2 dB softer and ends 0.7 dB louder in the twin and the engine
alike (the refit's onset and release knots; the relevel holds 1-4 s), -0.8 dB over the whole note. Pizzicato bit for bit,
CPU per held voice unchanged (callgrind within 0.04 %), memory unchanged.
Lessons:
- Measure what two takes differ by, stream by stream, before calming anything: most of it is noise cells the recording
  shares, and the rest (wander, flutter) is where the twin is less steady than one take of Spitfire, not the bow's onset.
- A random process whose slow part moves a whole take (a 0.8 Hz vibrato-depth drift over a 2.8 s window) costs the score
  through its spread; the same randomness moved quicker (cycle to cycle) keeps the character and averages out.
- Judge a global that moves the partials' mean levels after moving the gains back (a recentred table, seeds 1-3): the level
  shift otherwise hides or fakes its effect on the movement terms.

SIZE's loudness (2026-11-11). Between the violin (size 1, u 0) and the cello (2.1, u 1) the two fits blend and neither
is whole: the tables weigh nothing from u 0.25 to 0.75, the layers by w (1 - |u - u_table|), and the body, direct path
and room move with SIZE. A held bowed note fell 5-14 dB under the line (in dB, by u) between its levels at the two ends,
deepest near u 0.75-0.85 (size 1.75-1.85), and the cello's fit is itself 4-18 dB under the violin's on the same note:
turning SIZE toward the cello only ever got quieter. `size_level.cpp` measures the engine (without the correction,
`setSizeLevel (false)`) note by note, MIDI 24-108, at 25 sizes (u = k / 24, three seeds, 1.0-3.5 s of a held note at
the card's defaults) and writes `src/dsp/ModalBowedSizeLevel.h`: the dB at each knot that keeps the note at one level,
its anchor. The anchor is the violin's level from MIDI 55 up (its table's notes); under that, where size 1 plays notes
no violin has (and is up to 10 dB louder than its lowest real notes), it is the cello's level plus the violin-cello gap
smoothed over 9 notes, so the cello keeps its own note-to-note levels, eased in from the violin's own level over u
0-0.25. The engine adds it, linear between knots and held past the cello, to the note's `outputDb` (so on everything
`outputScale` scales), set with the fit: at note-on and once a block only when SIZE moves, no per-sample cost. It is 0
at size 1 and under, so the violin (and the pizzicato) play bit for bit as before; the cello is its fit raised by a
constant per note (4-18 dB): the score is taken against each note's own sustain, so that is free on it. Rerun
`size_level.cpp` after any round that moves a note's level.

SIZE past the fitted bodies (2026-11-12; ilana: "make it more realistic", "push all modal instruments in this manner ...
to create novel timbres"). The fitted span 1..2.1 is untouched (bit for bit at 1, 1.17 and 2.1, bowed and pizz; 0 of
769 fingerprints). Past either end (`beyondFit`, `slotPower` in ModalBowed.h) each body slot goes on from that end
(q = SIZE / the end's) by its family's law (Schelleng 1963; Hutchins's octet; Bissinger 2003, 2008; Jansson, Woodhouse
2005 for the bridge hill; the shared plan in /mnt/project-files/ilanasynth/modal-size-physics/): A0 (slots 0, 1)
q^-1.25, the corpus and every slot under 700 Hz q^-1.5 easing to q^-1 at 2 kHz, the bridge hill (2.3-3.5 kHz) q^-0.5,
above it q^-0.75; past the cello Q rises as sqrt q, under the violin it holds; gains the end's, A0 +3 dB per doubling;
the direct path's low-pass q^-0.5 and, under the violin, +3 dB per halving. Past the cello the air mode the cello's fit
leaves silent (slot 0: 88 Hz on the cello, a real cello's A0 is about 100) is voiced again toward a double bass (3.1: a
3/4 bass's 111 cm body against a violin's 35.5) at the loudest slot - 4 dB: A0 54 Hz (a bass's sits near its open A,
55 Hz), corpus 105, 114, 146, 177, 186, 206, 229 Hz (Brown 2004: T1 about 110 Hz). Slot 1 by the plan's law lands at 77 Hz
there, 30 % high, which is why slot 0 carries the bass's A0. "What if": past the bass (3.1 to 4) a cistern, Q x8 (body
gain +9 dB for the energy), the bow 3x slower to bite, the room 4x longer and 6 dB louder; under 0.5 (to 0.25) glass, Q
up to x41 (the air mode x301 and +12 dB) and the hair's noise ringing the air mode and the first corpus modes (the hair-
body path, three resonators, only there): a bell at a fixed pitch over the string. Slots under 30 Hz or past a quarter of
the rate fade out over an octave (a low one keeps its place, silent). Held at the ends instead of extrapolating their fitted
powers: the per-note tables (their own size's weight past their end; they faded out a quarter of the way before), the
released partials' loss (size^10.9 took every partial at the let-go past the cello) and the room (its decay powers
gave the tiny a 13 s hall and the giant none). `ModalBowedSizeLevel.h` gains `above` and `below` knots (12 a unit of u,
to u 2 and -2, held past) from `size_level.cpp --beyond` / `--merge` (the span's rows kept as they were), each knot's
level the mean of it and its neighbours (a partial landing on a narrow mode at a knot is luck), within +-18 dB: held
notes stay within about +-4 dB of the violin's level from 0.25 to 4 (MIDI 28-93). No new per-sample work but the
glass's three resonators under 0.5; the WOLF drive is not scaled (its resonance moves with its slot).

Chords' noise (2026-11-11; ilana: "the bowed modal noise is a bit much when playing chords"). Each bowed note makes its own
noise (the bow hair's, so its hiss, body and air-mode noise; the cello's string top; the hall's floor and rumble; the
residual layers), so four notes stacked four independent noise beds, 6 dB over one note's, while the tone adds as notes
do. `IlanaSynth::updateBowedNoiseShare`, once a sub-block on the audio thread before the voices render (the MULTI-CORE
workers only read it), sets every bowed voice's noise to 1 / sqrt (n), n = sum (w / w max)^2 over the sounding voices,
w = the voice's loudest Modal Bowed string's bow envelope by its amp level and velocity scale (`Voice::bowedNoiseWeight`):
a releasing tail or a quiet note counts less, unison and twin strings and several bowed oscillators are one note, a pluck
counts 0. Smoothed with a one-pole of 80 ms on the sub-block; each voice glides to it over its next 32-sample block,
sample by sample (`ModalBowed::setNoiseShare`); a new note starts at the current share. One note (or none) is exactly 1,
so a single note plays bit for bit as before (the scores are unchanged); the pizzicato has none of these noises. The
string and body's tone (and the rosin's jitter, which drives the string) are untouched. Init patch, held 1-4 s, noise =
the render less a render with the share 0 (the noise paths are linear, their random streams unmoved): cello C3 E3 G3 C4
noise to tone -16.4 -> -22.4 dB (its noise -32.1 -> -38.1 dB, one note's -34.8 to -46.6), violin G4 B4 D5 G5 -12.1 ->
-18.0 dB (-28.6 -> -34.6, one note's -31.4 to -43.2). Staggered entries and exits glide (largest 20 ms step 0.4 dB).

Round 15 (2026-11-11), both instruments: each partial's own movement.

Scores: twin, all notes, seeds 4-6 against Spitfire, paired by note (se over notes); t2t = the twin against itself over all
pairs of seeds 1-8 (28 pairs); ES = raw - 0.5 t2t; held = the held-out note pairs (Spitfire samples every whole tone).
  violin  raw 8.034 -> 7.455 (-0.579 se 0.047, better on 40 of 42; held -0.625 se 0.074, 16 of 16)
          ES 5.072 -> 4.520 (-0.552 se 0.042, 41 of 42); t2t 5.924 -> 5.869 (falls)
  cello   raw 7.491 -> 6.741 (-0.750 se 0.091, 19 of 19; held -0.587 se 0.112, 6 of 6)
          ES 4.854 -> 4.111 (-0.743 se 0.065, 19 of 19); t2t 5.274 -> 5.260 (flat; +0.015 on 6 seeds)
  Mean take (8 seeds): violin 6.515 -> 5.834, cello 6.208 -> 5.268.
  Demo notes, seeds 1-3: violin 7.78 -> 7.05, cello 7.68 -> 6.88.
  By term (raw / ES; t2t on seeds 1-6):
    violin    raw before -> after   ES before -> after   (d raw, notes better)
    sustain   0.835 -> 0.826        0.593 -> 0.592       (-0.009, 19/42)
    wobble    0.697 -> 0.686        0.537 -> 0.525       (-0.012, 25/42)
    attack    4.181 -> 4.109        2.493 -> 2.486       (-0.072, 30/42)
    release   4.098 -> 4.063        2.332 -> 2.326       (-0.035, 32/42)
    plevel    0.723 -> 0.724        0.368 -> 0.381       (+0.001, 20/42)
    vibam     0.521 -> 0.329        0.396 -> 0.182       (-0.193, 42/42)
    slow      0.479 -> 0.419        0.263 -> 0.217       (-0.060, 36/42)
    cello
    sustain   0.476 -> 0.474        0.294 -> 0.298       (-0.001, 10/19)
    wobble    0.647 -> 0.545        0.510 -> 0.408       (-0.102, 18/19)
    attack    3.374 -> 3.333        1.864 -> 1.856       (-0.041, 11/19)
    release   3.560 -> 3.487        2.016 -> 1.963       (-0.072, 16/19)
    plevel    0.869 -> 0.879        0.647 -> 0.662       (+0.010,  9/19)
    vibam     0.603 -> 0.432        0.463 -> 0.274       (-0.170, 19/19)
    slow      0.631 -> 0.504        0.426 -> 0.296       (-0.127, 17/19)
  Per-partial corr(Spitfire, twin's mean take) over the partials within 40 dB: vibam violin 0.09 -> 0.73, cello 0.33 -> 0.65;
  slow violin 0.74 -> 0.82, cello 0.64 -> 0.81 (the twin's own take against its other takes: 0.84 / 0.80, the ceiling).

What was found (scratch scripts vib.py, an1-an3.py):
- The vibrato-rate movement (vibam) was bias: the twin's per-partial depth correlated 0.09 (violin) / 0.34 (cello) with
  Spitfire's, its own takes 0.84 with each other. But Spitfire's partial movement is mostly NOT locked to its vibrato: a
  regression of each partial's 3-10 Hz dB track on cos/sin of the vibrato's phase (from its f0, the partials' instantaneous
  frequency) explains 10-30 % of the variance (the twin's: 15-70 %, its body ripple). Replacing the twin's locked part by
  Spitfire's locked phasors would leave vibam no better (mean take, violin 0.49 -> 0.52, cello 0.58 -> 0.52), so the
  signed vibrato-AM table was not built: the movement Spitfire has is random, partial by partial.
- Spitfire's vibrato is shallower than the twin's: violin 8.1 cents against 12.9, cello 6.1 against 15.6 (open strings near
  0); its rate 5.4 / 5.3 Hz against 5.26 / 5.04. Its per-note kinematics do not help (below).
- In Spitfire the loudest partials move most (vibam 2.4 dB at the top, 1.9 at -40); in the twin the reverse (1.7 / 2.0).
- The violin's 140-190 Hz excess sits on notes 63-73 only (4-9 dB, sustain and attack), with the residual layers already
  off there: it is the rosin's jitter on the fundamental's low skirt (jitter 0 removes it), under the hum high-pass's corner.
- The cello's plevel gap is mostly its weak partials: those below -40 dB carry 0.44 of its 0.87 (twin t2t there 0.22).

So, in sim.py and the tables:
- Per note and partial, fa_k: each partial's flutter (its fast part, flhp) through a two-pole band-pass at 5.5 Hz (4 Hz wide,
  updated every fourth block and glided) into its level, dB per dB, signed (negative calms the flutter's own pull on the
  ring), in from 0.6 s over 0.4 s, gone over 0.06 s once let go; and sl_db, dB on its slow wander (on the cello its sine
  wander too). Fitted in closed form: per note four renders (fa 0, +0.25, -0.25; the wander off) on the fit seeds (1-3,
  7-15), each partial's mean vibam^2 and slow^2 a quadratic in fa and the wander's gain; the |vibam| + |slow| error to
  Spitfire minimised per partial (fa within +-1, the wander 0.25-2.8 times). Then the gains moved back to round 14's
  partial levels.
- No loudness wander (loud_db 0, both sizes): with each partial's own movement fitted, the common wander only added take to
  take (violin, refit each way: 1.742 -0.230 t2t +0.24; half -0.298 t2t +0.09; none -0.335 t2t +0.02).
- The violin's hum corner per note (hum_k in the table, 13 notes; gains moved by the corner's own change at each partial):
  -0.043 (13 of 42 better: those 13; held -0.062).
- The round 13/14 joint frozen-bow refit once more (fit 1,2,3,7,8,9, val 4-6): violin -0.194 (40/42), cello -0.280 (19/19);
  its share is another pass's (round 14's control: one pass on its own -0.18 on the cello).
- out_db relevelled: each note's loudness is round 14's (violin and cello: 0.00 dB). The viola (no table of its own) moves
  -0.11 dB on average (-0.50 .. +0.17 over 48-74): regenerate ModalBowedSizeLevel.h in the port.
Steps on the way (seeds 4-6, against round 14): fa/sl alone (with loudness wander) violin -0.230 t2t +0.24, cello -0.500
t2t +0.23; + no loudness wander violin -0.335 (t2t +0.02), cello -0.454 (t2t +0.19); + the 0.6 s onset violin -0.342
(t2t 0.00), cello -0.470 (t2t +0.15); + hum violin -0.385; + refit violin -0.579 (t2t -0.09), cello -0.750 (t2t +0.02; 8 seeds -0.01).

Tried and left out:
- The flutter straight into the level (no band-pass): vibam -0.16 but sustain +1.1, plevel +0.65, attack +0.54 (most of its
  power above 10 Hz and under 3: the 85 ms STFT's power average lifts the level); a one-pole band-pass: sustain +0.5.
- Spitfire's per-note vibrato (depth by its f0, rate): rate alone violin +0.050 (13/42), cello -0.021 (held +0.046);
  depth alone +0.75 / +0.23 (plevel and sustain: the twin's partial gains were fitted under the deeper vibrato); depth and
  rate with the fa/sl refit on top: violin -0.030 against -0.230 without them, cello -0.486 against -0.500 (even). The onset
  phase and the vibrato's delay/rise were not fitted (Spitfire's onset envelope is too irregular to read reliably).
- The locked vibrato-AM table (in-phase / quadrature dB per partial): see above, predicted no gain; not built.
- AM onset at the vibrato's own delay/rise: 0.6 s / 0.4 s was -0.007 (violin) / -0.016 (cello) better, t2t -0.02 / -0.04.

CPU: about +1 % on a bowed voice (the band-pass: 5 multiply-adds a mode every 128 samples, one add a mode a block); sl_db and
hum_k are note-on only; loud_db 0 changes no code. Old presets: with round 14's loud_db and tables the sim is bit for bit
round 14's (chk2/).

Lessons:
- Before fitting a phase-locked model, measure how much of the target is locked: Spitfire's partial movement at the
  vibrato's rates is 70-90 % random, so its depth by partial is the thing to match, not a phasor.
- A per-partial random modulation has to sit in the measured band: power outside it (below 3 Hz, above 10 Hz) costs the
  level and spectrum terms more than it buys.
- Fit the per-partial movement first, then drop a global randomness it makes redundant (the loudness wander): the take to
  take falls back and the raw score gains.
Engine (round 15): the tables' new columns are `ModalBowedNotes::Row::fastLevel` / `slowDb` (64 each) and the scalar
`humCorner` (a row without one holds 0, which the engine skips; `write_engine.py`'s `PARTIAL`); the globals are
`fastLevelHz`, `fastLevelWidth`, `fastLevelOnset`, `fastLevelRise`, `fastLevelRelease` (one value at every size) and
`bowDriftDb` / `bowDriftDbCello` 0. At the fit (`updateModes`, `updateFit`): `slowBy[n]` (the tables' slowDb by their weights,
as a factor) on each partial's wander gain (`wanderGainOf`) and on the cello's `sineAmp`; `fastBy[n]` = the tables'
fastLevel / `flutterShape[n]` (the engine's flutter carries flutterShape, the twin's does not); `humRatioNow` takes the
table's humCorner by its weight where the row has one. In `updateVibrato`, after the flutter: every fourth block the
band-pass on `swaying[n]` (the flutter, the cello's less its slow part; `fastQ0..2` from 4 blocks / rate), its target
`fastBy * y * envelope * onset ramp * let-go fade`, glided over the four blocks (`fastNow`, `fastStep`) and added to the
level's exponent. Skipped (and 0) when no partial has a fastLevel (a pluck, a size no table reaches); with slowDb 0 and
no humCorner the code is round 14's to the bit. The pizzicato renders bit for bit as before.
The engine alone (bench, 3 takes on the demo notes; the twin seeds 1-3 beside it): violin 8.10 -> 7.32 (twin 7.78 ->
7.05), cello 8.42 -> 7.49 (twin 7.68 -> 6.88); by term (engine / twin change) violin sustain +0.02 / -0.01, wobble -0.07 /
-0.07, attack -0.12 / -0.09, release -0.06 / -0.04, plevel -0.02 / -0.13, vibam -0.26 / -0.22, slow -0.06 / -0.03; cello
sustain 0.00 / -0.03, wobble -0.13 / -0.09, attack -0.12 / -0.13, release -0.16 / -0.06, plevel -0.02 / 0.00, vibam -0.14 /
-0.13, slow -0.18 / -0.17. On 6 more takes: violin 7.99 -> 7.45 (twin, seeds 4-9, 8.02 -> 7.36), cello 8.45 -> 7.59
(7.88 -> 6.98). `ModalBowedSizeLevel.h` regenerated with `size_level.cpp` on the round 15 engine (entries move up to
2.4 dB, mean +0.04; still 0 at size 1, bit for bit). Loudness of the held demo notes (1-4 s) against round 14: violin 62
0.0, 69 -0.6, 76 -0.5, 81 +0.3 dB, cello 36 -0.1, 43 -0.2, 48 -0.5, 53 -0.4 (the twin's own change on 9 seeds: 69 -0.60,
48 -0.34). The plugin (demo, Init patch) violin 9.21 -> 8.47, cello 9.52 -> 8.74 (round 14 rendered at e9ed60a, with
SIZE's loudness and the chords' noise); with the AMP open and F1 at 20 kHz violin 8.28 -> 7.61, cello 8.56 -> 7.76.
CPU per held voice (callgrind, 2 s held) +0.3 % (violin 81) to +0.7 % (cello 36, 48); memory 2 x 64 floats a table row,
8 x 64 floats a voice.

Needs numpy, scipy, soundfile, numba, mido, matplotlib.
