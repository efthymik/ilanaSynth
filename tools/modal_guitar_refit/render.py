"""Render render-midi/guitar-full-range.mid (E2..E6, 7 s slots, velocities
40/80/120) through the plugin (ilanaMidiRender, Init + osc 1 Modal Guitar with
the "as recorded" knobs, clean gain staging) and cut it into out/<tag>/<note>-<pp|mf|ff>.wav
exactly like refs.py cuts a reference render.
    render.py MIDI TAG [extra ILANA_SET pairs]
"""
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
# The Init patch's tanh stages (F1 DRIVE 1.5, the master soft clip) are off,
# and osc 1 and the master low, so an ff note is not distorted (at trim 12.5
# the voice still squashed ff peaks by ~4 dB at OSC 1 LEVEL 0.8; at 0.2 the
# score does not move with the level).
KNOBS = "master_clip=0,f1_drive=1,master=-18,osc1_level=0.2,osc1_excite=12,osc1_string_damp=0,osc1_string_decay=0.5,osc1_string_stiffness=0.5,osc1_hammer_hard=0.5"


def main():
    mid, tag = os.path.abspath(sys.argv[1]), sys.argv[2]
    extra = sys.argv[3] if len(sys.argv) > 3 else ""
    os.makedirs("out/" + tag, exist_ok=True)
    wav = os.path.abspath("out/%s/all.wav" % tag)
    env = dict(os.environ, ILANA_SET=KNOBS + ("," + extra if extra else ""))
    exe = os.environ.get("ILANA_RENDER", os.path.join(ROOT, "build/ilanaMidiRender_artefacts/Release/ilanaMidiRender"))
    subprocess.run([exe, mid, wav, "init-modal", "48000", "512"], env=env, check=True, stdout=subprocess.DEVNULL)
    here = os.path.dirname(os.path.abspath(__file__))
    subprocess.run([sys.executable, os.path.join(here, "refs.py"), wav, mid, "out/" + tag], check=True)


if __name__ == "__main__":
    main()
