# Modal Bowed refit (2026-11-08)

Fits the Modal Bowed exciter (src/dsp/ModalBowed.h) over time to renders of
`render-midi/<violin|viola|cello>-arco-full-range.mid` (ilana's Spitfire
String Quartet, Long Normale, dynamics 90, reverb off). Run from a working
folder; the scripts read and write there.

1. `split.py RENDER.flac MIDI.mid arco/<inst> 80 0.8` cuts the velocity-80
   notes out, one WAV per note, half a second of silence before each.
2. `measure.py` prints Spitfire's vibrato, noise floor, rise and release.
3. `sim.py` is a Python twin of ModalBowed.h (matches a C++ render within
   about 1 dB per octave band); `score.py '{json params}'` scores it.
4. `fit_att.py` (rise: contact time, swell time and depth), `fit_noise.py`
   (rosin jitter, hair hiss), then `fit_body2.py <inst>` (body gains through
   the whole engine) give the constants in ModalBowed.h and the gains in
   ModalBowedBodies.h.
5. `spec.py <prefix>` draws spectrograms of Spitfire against a render.

Needs numpy, scipy, soundfile, numba, mido, matplotlib.
