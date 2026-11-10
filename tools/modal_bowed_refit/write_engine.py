"""Write fitted constants (a fit_look.py json) into src/dsp/ModalBowed.h, and the per-note
tables (notes_violin.json, notes_cello.json) into src/dsp/ModalBowedNotes.h."""
import json
import os
import re
import sys

import sim

ENGINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "dsp", "ModalBowed.h")
p = dict(sim.DEFAULT)
p.update({k: v for k, v in json.load(open(sys.argv[1])).items() if k in sim.DEFAULT})
s = open(ENGINE).read()


def number(v, digits=5):
    text = ("%.1f" % v) if abs(v) >= 1000 else ("%%.%dg" % digits % v) if abs(v) >= 1e-3 or v == 0 else ("%%.%de" % (digits - 1) % v)
    return text if ("." in text or "e" in text) else text + ".0"


def put(name, value, suffix="f", digits=5):
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
put("stringLossHigh", p["loss1"])
# (round 9: the violin's retuned globals and hall floor, the cello's flutter, wander, ripple, hiss
# and release floor; sim.CELLO's values for the globals the violin retuned are the engine's ...Cello)
for name, key in (("attackScale", "attack_scale"), ("hissHp", "hiss_hp"), ("hissSlope", "hiss_k"), ("hallFloorDb", "flr_db"),
                  ("hallFloorSlope", "flr_k"), ("hallFloorLowHz", "flr_lp"), ("hallFloorHighHz", "flr_hp"), ("hallFloorRise", "flr_att"),
                  ("hallFloorLetGoDb", "flr_rel_db"), ("hallFloorLetGoSeconds", "flr_rs"), ("hallFloorFade", "flr_fade"),
                  ("hallFloorWhite", "flr_w"), ("noiseFloorDb", "nfloor_db"), ("noiseFloorFade", "nfloor_dbs"),
                  ("partialDropSeconds", "rdk_s")):
    put(name, p[key], "", digits=5)
for name, key, scale in (("brightnessCello", "bright", 1.0), ("stringLossHighCello", "loss1", 1.0), ("directDbCello", "direct_db", 1.0),
                         ("directHpCello", "direct_hp", 1.0), ("tiltHzCello", "lp_hz", 1.0), ("bodyNoiseCello", "rosin", 1e-4),
                         ("humRatioCello", "hum_k", 1.0), ("attackScaleCello", "attack_scale", 1.0), ("hissHpCello", "hiss_hp", 1.0),
                         ("hissLevelCello", "hiss", 1.0), ("flutterDbCello", "fl_db", 1.0), ("wanderDbCello", "slow_db", 1.0),
                         ("bowDriftDbCello", "loud_db", 1.0), ("rippleDbCello", "rip_db", 1.0)):
    put(name, sim.CELLO[key] * scale, "", digits=5)
assert sim.CELLO["hiss_k"] == 0
put("vibratoFitted", p["vib_ct"], "")
open(ENGINE, "w").write(s)

# (round 9) The per-note tables (notes_<inst>.json) as ModalBowedNotes.h.
NOTES_H = os.path.join(os.path.dirname(ENGINE), "ModalBowedNotes.h")
SCALARS = (("floorDb", "flr_db"), ("tailDb", "tail_db"), ("letGoDropDb", "rdrop_db"), ("fadeDb", "fade_db"), ("onsetDarkDb", "ob_db"),
           ("swellSeconds", "swell_s"), ("vibratoCents", "vib_ct"), ("wanderDb", "slow_db"), ("rippleDb", "rip_db"),
           ("releaseSeconds", "rel_s"), ("releaseRing", "ring"))
assert tuple(k for _, k in SCALARS) == sim.TABLE_SCALARS
ARRAYS = (("gainDb", "gain_db"), ("darkDb", "dark_db"), ("lossAdd", "loss_add"), ("dropDb", "drop_db"))


def floats(v):
    v = list(v)
    while v and v[-1] == 0:
        v.pop()
    return "{ " + ", ".join(number(x, 5) + "f" for x in v) + " }" if v else "{}"


out = ["#pragma once", "",
       "// The Modal Bowed exciter's per-note tables (ModalBowed.h, round 9), each fitted at one body size",
       "// to ilana's Spitfire String Quartet renders through the engine's Python twin. Per MIDI note:",
       "// gainDb (dB on each partial's share of the bridge force, index 0 the fundamental), darkDb (dB",
       "// more onset darkening), lossAdd (1/s more loss once let go), dropDb (each partial's own drop at",
       "// the let-go), and scalars that replace the engine's value at that size for that note (scalars:",
       "// a bit for each one the table sets). A table is read at note-on by the nearest MIDI note (never",
       "// between notes), all there at its own size and gone a quarter of the way (in log size) toward",
       "// the next; below first it is held (holdBelow) or absent, above last it fades over fadeAbove",
       "// semitones (0: absent). Another instrument's table is one more Row array and Table entry.",
       "// Generated (tools/modal_bowed_refit/write_engine.py from notes_<inst>.json): edit the tools, not this file.",
       "struct ModalBowedNotes",
       "{",
       "    static constexpr int partials = 64;",
       "    enum Scalar { " + ", ".join(n for n, _ in SCALARS) + ", scalarCount };",
       "    struct Row",
       "    {",
       "        float " + " {}, ".join("%s[partials]" % n for n, _ in ARRAYS) + " {};",
       "        float scalar[scalarCount] {};",
       "    };",
       "    struct Table",
       "    {",
       "        double size;          // the body size it was fitted at (a violin's 1)",
       "        int first, last;      // MIDI notes",
       "        bool holdBelow;",
       "        int fadeAbove;        // semitones",
       "        unsigned scalars;     // 1 << Scalar for each one the table sets",
       "        const Row* rows;      // first..last",
       "    };"]
tables = []
for inst in ("violin", "cello"):
    tab = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "notes_%s.json" % inst)))
    assert sorted(map(int, tab["notes"])) == list(range(tab["first"], tab["last"] + 1))
    mask = 0
    rows = []
    for n in range(tab["first"], tab["last"] + 1):
        row = tab["notes"][str(n)]
        for i, (_, key) in enumerate(SCALARS):
            if key in row:
                mask |= 1 << i
        rows.append(row)
    for row in rows:
        assert all((key in row) == bool(mask >> i & 1) for i, (_, key) in enumerate(SCALARS))
        assert all(len(row.get(key, [])) <= 64 for _, key in ARRAYS)
    out.append("    static constexpr Row %sRows[%d] {" % (inst, len(rows)))
    for n, row in zip(range(tab["first"], tab["last"] + 1), rows):
        out.append("        { %s, // %d" % (", ".join(floats(row.get(key, [])) for _, key in ARRAYS), n))
        out.append("          %s }," % floats([row.get(key, 0.0) for _, key in SCALARS] if mask else []))
    out.append("    };")
    tables.append("{ %s, %d, %d, %s, %d, 0x%xu, %sRows }" % (number(tab["size"]), tab["first"], tab["last"],
                                                             "true" if tab["hold_below"] else "false", tab["fade_above"], mask, inst))
out += ["    static constexpr int tableCount = %d;" % len(tables),
        "    static constexpr Table tables[tableCount] { %s };" % (", ".join(tables)),
        "};", ""]
open(NOTES_H, "w").write("\n".join(out))
print({k: p[k] for k in ("loss1", "jitter", "hiss", "rosin", "bright", "top", "lp_hz", "rip_db", "rip_c", "slow_db", "loud_db", "direct_db", "rel_s", "fade_db", "ring", "ring_s", "fl_db", "fl_hz", "fl_slope", "fl_hzslope", "pdrift_ct", "vib_rate_dev", "vib_depth_dev", "fl_knee", "fl_fslope", "rip_knee", "rip_fslope", "direct_hp", "fl_clip", "swell_amt", "swell_s", "ob_db", "ob_knee", "ob_slope", "ob_s", "direct_lp", "rloss", "rpow", "room_db", "room_sz", "room_ms", "room_lo", "room_hi")})
