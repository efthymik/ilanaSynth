"""Write fitted constants (a fit_look.py json) into src/dsp/ModalBowed.h."""
import json
import os
import re
import sys

import sim

ENGINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "dsp", "ModalBowed.h")
p = dict(sim.DEFAULT)
p.update({k: v for k, v in json.load(open(sys.argv[1])).items() if k in sim.DEFAULT})
s = open(ENGINE).read()


def number(v):
    text = ("%.1f" % v) if abs(v) >= 1000 else ("%.4g" % v) if abs(v) >= 1e-3 else ("%.3e" % v)
    return text if ("." in text or "e" in text) else text + ".0"


def put(name, value, suffix="f"):
    global s
    pattern = r"(\b%s = )[-0-9.e+]+%s" % (name, suffix)
    assert re.search(pattern, s), name
    s = re.sub(pattern, lambda m: m.group(1) + number(value) + suffix, s)


for name, key in (("rosinJitter", "jitter"), ("hissLevel", "hiss"), ("brightness", "bright"), ("topHz", "top"), ("tiltHz", "lp_hz"),
                  ("directDb", "direct_db"), ("rippleDb", "rip_db"), ("rippleDensity", "rip_c"), ("wanderDb", "slow_db"), ("bowDriftDb", "loud_db")):
    put(name, p[key])
put("bodyNoise", p["rosin"] * 1e-4)
put("releaseSeconds", p["rel_s"], "")
put("releaseFadeDb", p["fade_db"])
put("releaseRing", p["ring"])
put("releaseRingSeconds", p["ring_s"], "")
put("flutterDb", p["fl_db"])
put("flutterSlope", p["fl_slope"])
put("flutterHzSlope", p["fl_hzslope"])
put("pitchDriftCents", p["pdrift_ct"])
put("flutterHz", p["fl_hz"], "")
put("rateWander", p["vib_rate_dev"])
put("depthWander", p["vib_depth_dev"])
put("flutterKnee", p["fl_knee"])
put("flutterFreqSlope", p["fl_fslope"])
put("rippleKnee", p["rip_knee"])
put("rippleFreqSlope", p["rip_fslope"])
put("directHp", p["direct_hp"])
put("flutterClip", p["fl_clip"])
assert re.search(r"(pizz \? 3\.0e-5f : )[-0-9.e+]+f", s)
s = re.sub(r"(pizz \? 3\.0e-5f : )[-0-9.e+]+f", lambda m: m.group(1) + number(p["loss1"]) + "f", s)
open(ENGINE, "w").write(s)
print({k: p[k] for k in ("loss1", "jitter", "hiss", "rosin", "bright", "top", "lp_hz", "rip_db", "rip_c", "slow_db", "loud_db", "direct_db", "rel_s", "fade_db", "ring", "ring_s", "fl_db", "fl_hz", "fl_slope", "fl_hzslope", "pdrift_ct", "vib_rate_dev", "vib_depth_dev", "fl_knee", "fl_fslope", "rip_knee", "rip_fslope", "direct_hp", "fl_clip")})
