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


def number(v, digits=4):
    text = ("%.1f" % v) if abs(v) >= 1000 else ("%%.%dg" % digits % v) if abs(v) >= 1e-3 or v == 0 else ("%.3e" % v)
    return text if ("." in text or "e" in text) else text + ".0"


def put(name, value, suffix="f", digits=4):
    global s
    pattern = r"(\b%s = )[-0-9.e+]+%s" % (name, suffix)
    assert re.search(pattern, s), name
    s = re.sub(pattern, lambda m: m.group(1) + number(value, digits) + suffix, s)


for name, key in (("rosinJitter", "jitter"), ("hissLevel", "hiss"), ("brightness", "bright"), ("topHz", "top"), ("tiltHz", "lp_hz"),
                  ("directDb", "direct_db"), ("rippleDb", "rip_db"), ("rippleDensity", "rip_c"), ("wanderDb", "slow_db"), ("bowDriftDb", "loud_db")):
    put(name, p[key])
put("bodyNoise", p["rosin"] * 1e-4)
put("releaseSeconds", p["rel_s"], "")
put("releaseFadeDb", p["fade_db"], "")
put("releaseRing", p["ring"], "")
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
# (round 6: the onset, the box's darker top, the room, the release's losses)
put("swellDepth", p["swell_amt"], "")
put("swellSeconds", p["swell_s"], "")
put("onsetDarkDb", p["ob_db"], "")
put("onsetDarkKnee", p["ob_knee"], "")
put("onsetDarkSlope", p["ob_slope"], "")
put("onsetDarkSeconds", p["ob_s"], "")
put("directLp", p["direct_lp"])
put("releaseLossHz", p["rloss"], "")
put("releaseLossSlope", p["rpow"])
# (the engine has room_sz in two parts, roomSizeDb + roomSizeDbMore: the second stays)
put("roomSizeDb", p["room_sz"] - float(re.search(r"roomSizeDbMore = ([-0-9.e+]+)", s).group(1)), "")
for name, key in (("roomDb", "room_db"), ("roomMs", "room_ms"), ("roomLowDecay", "room_lo"), ("roomHighDecay", "room_hi")):
    put(name, p[key], "")
# (round 7: the cello's size powers; roomSizeDb and roomSizeDbMore together are room_sz)
put("onsetDarkKneeSize", p["ob_ksz"])
put("swellSize", p["sw_sz"], "")
put("releaseLossSizeSlope", p["rloss_k"], digits=5)
put("roomLowSizeSlope", p["room_lk"], "")
put("roomHighSizeSlope", p["room_hk"], "")
# (round 8: the hum's high-pass, the second swell stage, the noise's swell, the let-go's two
# stages; the cello's size powers; sim.CELLO, the values the violin's terms blend to at the
# cello's size: the engine's ...Cello constants)
for name, key in (("humRatio", "hum_k"),):
    put(name, p[key])
for name, key in (("swellSecondStage", "sw2"), ("noiseSwellDepth", "nz_amt"), ("letGoDropDb", "rdrop_db"), ("letGoDropSeconds", "rdrop_s"),
                  ("tailDb", "tail_db"), ("tailSeconds", "tail_s"), ("tailDelay", "tail_t0"), ("tailNoise", "ntail"),
                  ("releaseRingSizeSlope", "ring_k"), ("releaseLossSlopeSizeSlope", "rpow_k"), ("releaseRampSizeSlope", "rg_k"),
                  ("releaseFadeSizeSlope", "fade_k"), ("directSizeDb", "dir_sz"), ("directHpSizeSlope", "dhp_k"),
                  ("vibratoSizeSlope", "vib_k"), ("onsetDarkSecondsSizeSlope", "obs_k")):
    put(name, p[key], "")
for name, key in (("swellSecondsCello", "swell_s"), ("swellDepthCello", "swell_amt"), ("onsetDarkDbCello", "ob_db"), ("onsetDarkKneeCello", "ob_knee"),
                  ("onsetDarkSlopeCello", "ob_slope"), ("onsetDarkSecondsCello", "ob_s"), ("releaseSecondsCello", "rel_s"),
                  ("releaseFadeDbCello", "fade_db"), ("releaseRingCello", "ring"), ("releaseLossHzCello", "rloss")):
    put(name, sim.CELLO[key], "")
# (the engine has the cello's second stage, let-go stages and tail noise off, and its noise on the stroke's own swell)
assert sim.CELLO["sw2"] == 0 and sim.CELLO["rdrop_db"] == 0 and sim.CELLO["tail_db"] == 0 and sim.CELLO["ntail"] == 0 and sim.CELLO["nz_amt"] == sim.CELLO["swell_amt"]
assert p["hum_all"] == 1 and p["lift"] == 1
assert re.search(r"(pizz \? 3\.0e-5f : )[-0-9.e+]+f", s)
s = re.sub(r"(pizz \? 3\.0e-5f : )[-0-9.e+]+f", lambda m: m.group(1) + number(p["loss1"]) + "f", s)
open(ENGINE, "w").write(s)
print({k: p[k] for k in ("loss1", "jitter", "hiss", "rosin", "bright", "top", "lp_hz", "rip_db", "rip_c", "slow_db", "loud_db", "direct_db", "rel_s", "fade_db", "ring", "ring_s", "fl_db", "fl_hz", "fl_slope", "fl_hzslope", "pdrift_ct", "vib_rate_dev", "vib_depth_dev", "fl_knee", "fl_fslope", "rip_knee", "rip_fslope", "direct_hp", "fl_clip", "swell_amt", "swell_s", "ob_db", "ob_knee", "ob_slope", "ob_s", "direct_lp", "rloss", "rpow", "room_db", "room_sz", "room_ms", "room_lo", "room_hi")})
