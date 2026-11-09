"""Write body2-<inst>.json gains (absolute dB) into src/dsp/ModalBowedBodies.h.

write_bodies.py violin cello [viola]: the instruments to replace; the rest stay."""
import json
import os
import re
import sys

HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "dsp", "ModalBowedBodies.h")
NAME = {"violin": "violin", "viola": "violas", "cello": "cellos"}
s = open(HEADER).read()
for inst in sys.argv[1:]:
    j = json.load(open(f"body2-{inst}.json"))
    pat = r"float %s\[count\] \{[^}]*\}" % NAME[inst]
    assert re.search(pat, s)
    s = re.sub(pat, "float %s[count] { %s }" % (NAME[inst], ", ".join("%.2ff" % v for v in j["gains"])), s)
    s = re.sub(r"(%s) [\d.]+ dB" % inst, r"\g<1> %.1f dB" % j["err"], s)
open(HEADER, "w").write(s)
