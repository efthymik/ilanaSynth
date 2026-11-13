"""Cut a reference render of render-midi/guitar-full-range.mid (E2..E6 one
note at a time in 7 s slots, velocities 40/80/120 = pp/mf/ff) into
refs/<note>-<pp|mf|ff>.wav, mono, 48 kHz, 32-bit float (16-bit cuts put the
noise term on the quantisation floor), from the pluck, 5 s long.
    refs.py RENDER.(wav|flac) MIDI.mid [refs]
"""
import os
import sys

import mido
import numpy as np
import soundfile as sf

x, rate = sf.read(sys.argv[1], always_2d=True)
assert rate == 48000
x = x.mean(axis=1)
out = sys.argv[3] if len(sys.argv) > 3 else "refs"
os.makedirs(out, exist_ok=True)
t, n = 0.0, 0
level = {40: "pp", 80: "mf", 120: "ff"}
for msg in mido.MidiFile(sys.argv[2]):
    t += msg.time
    if msg.type == "note_on" and msg.velocity > 0:
        a = int(t * rate)
        seg = x[a:a + int(6.5 * rate)]
        env = np.convolve(np.abs(seg), np.ones(240) / 240, "same")
        start = max(0, int(np.argmax(env > 0.15 * env.max())) - int(0.004 * rate))
        sf.write("%s/%d-%s.wav" % (out, msg.note, level[msg.velocity]), seg[start:start + int(5 * rate)].astype(np.float32), rate, subtype="FLOAT")
        n += 1
print(n, "notes")
