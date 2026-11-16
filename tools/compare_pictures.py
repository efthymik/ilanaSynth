#!/usr/bin/env python3
"""Compare two directories of ilanaSnapshot pictures, ignoring run-to-run noise.

    compare_pictures.py NOISE_A NOISE_B NEW [NOISE_A2 NOISE_B2 NEW2 ...]

NOISE_A and NOISE_B are two renders of the same (baseline) build: every pixel
that differs between them is animation noise (meter, keyboard strip, FM scope),
dilated a few pixels to form the mask. NEW is a render of the build under test;
a pixel is reported only when it differs from both baseline renders and lies
outside the mask. Exit 1 when there are any.
"""
import sys, os
import numpy as np
from PIL import Image


def binary_dilation(m, iterations=3):
    r = iterations
    out = m.copy()
    h, w = m.shape
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            sy0, sy1 = max(0, -dy), min(h, h - dy)
            sx0, sx1 = max(0, -dx), min(w, w - dx)
            out[sy0 + dy:sy1 + dy, sx0 + dx:sx1 + dx] |= m[sy0:sy1, sx0:sx1]
    return out


def load(path):
    return np.asarray(Image.open(path).convert("RGBA")).astype(np.int16)


def main(argv):
    if len(argv) < 3 or len(argv) % 3:
        print(__doc__)
        return 2
    bad = 0
    total = 0
    for i in range(0, len(argv), 3):
        a, b, new = argv[i:i + 3]
        for name in sorted(os.listdir(a)):
            if not name.endswith(".png"):
                continue
            total += 1
            pa, pb, pn = (os.path.join(d, name) for d in (a, b, new))
            if not os.path.exists(pn) or not os.path.exists(pb):
                print(f"MISSING {new}/{name}")
                bad += 1
                continue
            ia, ib, inn = load(pa), load(pb), load(pn)
            if ia.shape != inn.shape or ia.shape != ib.shape:
                print(f"SIZE {new}/{name}: {ia.shape} vs {inn.shape}")
                bad += 1
                continue
            noise = (ia != ib).any(axis=2)
            if noise.any():
                noise = binary_dilation(noise, iterations=3)
            diff = (ia != inn).any(axis=2) & (ib != inn).any(axis=2) & ~noise
            if diff.any():
                ys, xs = np.nonzero(diff)
                print(f"DIFF {new}/{name}: {int(diff.sum())} px, bbox x {xs.min()}..{xs.max()} y {ys.min()}..{ys.max()}")
                bad += 1
    print(f"{total} pictures compared, {bad} differ outside the noise mask")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
