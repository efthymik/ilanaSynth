#!/usr/bin/env python3
"""Software vs Vulkan picture parity (the editor's Vulkan renderer, src/gpu).

Compares pictures pairwise and reports, per picture: size match, max and mean
absolute channel difference, the share of pixels with any channel off by more
than 2, 8 and 32 levels, and the bounding boxes of clusters of big (> 32 by
default) differences. Writes an amplified diff heatmap per picture, a
side-by-side crop (software | vulkan | diff) of the worst region, a JSON
summary and a Markdown table. Exits 1 when any picture fails.

Pairs come from:
  --dir D          every D/<name>.vulkan.png with its D/<name>.png (one
                   `ILANA_SNAPSHOT_RENDERER=both` run: same UI state). Pixels
                   where D/<name>.software2.png (software again, after the
                   Vulkan capture) differs from D/<name>.png moved on their
                   own (playhead dots, readouts): they are left out of the
                   numbers and reported as "unstable".
  --pair A B       every A/<name>.png with B/<name>.png (two runs; A is the
                   reference, "software", B the candidate); --unstable DIR
                   (another run of A) masks what moves between runs
Both may be given more than once.

Pass/fail (all thresholds are arguments):
  --within 5 --min-within 99.9   at least 99.9 % of stable pixels have every
                                 channel within 5 levels (GPU blending rounds
                                 where JUCE's PixelARGB::blend truncates: a
                                 level per stacked translucent layer)
  --cluster-level 32 --max-cluster 4
                                 no 8-connected cluster of pixels off by more
                                 than 32 whose bounding box is wider or taller
                                 than 4 px
  --exact                        every pixel identical (baseline checks)

Example (her 175 %):
  ILANA_SNAPSHOT_SCALE=1.75 ILANA_SNAPSHOT_RENDERER=both \\
    xvfb-run -a -s "-screen 0 2560x1600x24" build/ilanaSnapshot_artefacts/Release/ilanaSnapshot shots
  python3 tools/vulkan_parity.py --dir shots --out shots/parity
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def load(path):
    image = Image.open(path)
    return np.asarray(image.convert("RGBA"), dtype=np.int16)


def label_clusters(mask):
    """8-connected components of a boolean mask; returns a list of
    (x0, y0, x1, y1, pixel_count) with x1/y1 exclusive. Run-length union-find,
    so a picture full of differences stays fast."""
    parent = []

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        a, b = find(a), find(b)
        if a != b:
            parent[max(a, b)] = min(a, b)

    runs = []  # (y, x0, x1, label)
    prev = []
    for y in range(mask.shape[0]):
        row = mask[y]
        if not row.any():
            prev = []
            continue
        padded = np.concatenate(([False], row, [False]))
        edges = np.flatnonzero(padded[1:] != padded[:-1])
        cur = []
        j = 0
        for x0, x1 in zip(edges[0::2], edges[1::2]):
            label = len(parent)
            parent.append(label)
            # previous row's runs touching [x0-1, x1] (8-connectivity)
            while j < len(prev) and prev[j][1] <= x0:
                j += 1
            k = j
            while k < len(prev) and prev[k][0] < x1:
                union(label, prev[k][2])
                k += 1
            cur.append((x0 - 1, x1 + 1, label))  # widened for diagonal contact
            runs.append((y, x0, x1, label))
        prev = cur
    boxes = {}
    for y, x0, x1, label in runs:
        root = find(label)
        b = boxes.get(root)
        if b is None:
            boxes[root] = [x0, y, x1, y + 1, x1 - x0]
        else:
            b[0] = min(b[0], x0)
            b[1] = min(b[1], y)
            b[2] = max(b[2], x1)
            b[3] = max(b[3], y + 1)
            b[4] += x1 - x0
    return [tuple(int(v) for v in b) for b in boxes.values()]


def grow(mask, r):
    """Dilates a boolean mask by r px (square)."""
    out = mask.copy()
    h, w = mask.shape
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            ys, yd = (slice(0, h - dy), slice(dy, h)) if dy >= 0 else (slice(-dy, h), slice(0, h + dy))
            xs, xd = (slice(0, w - dx), slice(dx, w)) if dx >= 0 else (slice(-dx, w), slice(0, w + dx))
            out[yd, xd] |= mask[ys, xs]
    return out


def heatmap(diff_max, amplify):
    """Black where equal; dim blue for small, yellow/red for big differences."""
    d = np.clip(diff_max.astype(np.float32) * amplify, 0, 255) / 255.0
    rgb = np.zeros(diff_max.shape + (3,), dtype=np.float32)
    rgb[..., 0] = np.clip(d * 3 - 1, 0, 1)
    rgb[..., 1] = np.clip(d * 3 - 2, 0, 1) + np.clip(d * 3, 0, 1) * 0.15
    rgb[..., 2] = np.clip(d * 3, 0, 1) * (1 - np.clip(d * 3 - 1, 0, 1))
    rgb[diff_max > 0] = np.maximum(rgb[diff_max > 0], 0.25 * np.array([0, 0.4, 1.0]))
    return Image.fromarray((rgb * 255).astype(np.uint8), "RGB")


def side_by_side(ref, cand, diff_max, box, zoom, amplify):
    x0, y0, x1, y1 = box
    crops = [Image.fromarray(ref[y0:y1, x0:x1].astype(np.uint8), "RGBA").convert("RGB"),
             Image.fromarray(cand[y0:y1, x0:x1].astype(np.uint8), "RGBA").convert("RGB"),
             heatmap(diff_max[y0:y1, x0:x1], amplify)]
    w, h = (x1 - x0) * zoom, (y1 - y0) * zoom
    sheet = Image.new("RGB", (w * 3 + 8, h), (255, 0, 255))
    for i, crop in enumerate(crops):
        sheet.paste(crop.resize((w, h), Image.NEAREST), (i * (w + 4), 0))
    return sheet


def compare(name, ref_path, cand_path, unstable_paths, args, out):
    result = {"name": name, "reference": str(ref_path), "candidate": str(cand_path)}
    ref, cand = load(ref_path), load(cand_path)
    result["size_reference"] = [ref.shape[1], ref.shape[0]]
    result["size_candidate"] = [cand.shape[1], cand.shape[0]]
    result["size_match"] = ref.shape == cand.shape
    if not result["size_match"]:
        result["pass"] = False
        result["why"] = "size differs"
        return result

    diff = np.abs(ref - cand)
    # --unstable: pixels where another run of the reference differs from it
    # (live views, timers) don't count, grown by --unstable-grow px.
    unstable = np.zeros(ref.shape[:2], dtype=bool)
    for other in unstable_paths:
        if other.exists():
            o = load(other)
            if o.shape == ref.shape:
                unstable |= np.abs(o - ref).max(axis=2) > 0
            else:
                unstable[:] = True
    if unstable.any() and args.unstable_grow > 0:
        unstable = grow(unstable, args.unstable_grow)
    result["unstable_px"] = int(unstable.sum())
    result["pct_unstable"] = float(unstable.mean() * 100.0)
    diff[unstable] = 0
    diff_max = diff.max(axis=2)
    total = diff_max.size - int(unstable.sum()) if not unstable.all() else 1
    result["max_abs"] = int(diff.max())
    result["mean_abs"] = float(diff.sum() / (total * diff.shape[2]))
    for level in (2, 8, 32):
        result[f"pct_over_{level}"] = float((diff_max > level).sum() * 100.0 / total)
    result["pct_within"] = float(((diff_max <= args.within) & ~unstable).sum() * 100.0 / total)
    result["identical"] = bool(result["max_abs"] == 0)
    # Stable pixels by their largest channel difference: 0, 1, 2, 3, 4, 5+ levels (percent).
    stable = diff_max[~unstable]
    result["hist_pct"] = [float((stable == v).sum() * 100.0 / total) for v in range(5)] + [float((stable >= 5).sum() * 100.0 / total)]

    clusters = label_clusters(diff_max > args.cluster_level)
    clusters.sort(key=lambda c: (-c[4], c[1], c[0]))
    big = [c for c in clusters if (c[2] - c[0]) > args.max_cluster or (c[3] - c[1]) > args.max_cluster]
    result["clusters_total"] = len(clusters)
    result["clusters_big"] = len(big)
    result["clusters"] = [{"x": c[0], "y": c[1], "w": c[2] - c[0], "h": c[3] - c[1], "pixels": c[4],
                           "big": c in big} for c in clusters[:args.list_clusters]]

    why = []
    if args.exact:
        if not result["identical"]:
            why.append(f"not identical (max {result['max_abs']}, {int((diff_max > 0).sum())} px differ)")
    else:
        if result["pct_within"] < args.min_within:
            why.append(f"{result['pct_within']:.3f}% within {args.within} < {args.min_within}%")
        if big:
            why.append(f"{len(big)} cluster(s) > {args.cluster_level} levels bigger than {args.max_cluster}x{args.max_cluster} px")
    result["pass"] = not why
    result["why"] = "; ".join(why)

    if out is not None and not result["identical"]:
        stem = name.replace("/", "_")
        heatmap(diff_max, args.amplify).save(out / f"{stem}.heat.png")
        result["heatmap"] = str(out / f"{stem}.heat.png")
        # Worst region: the biggest cluster of big differences, else the
        # largest difference, padded so its neighbourhood shows.
        if clusters:
            cx0, cy0, cx1, cy1, _ = clusters[0]
        else:
            yy, xx = np.unravel_index(int(diff_max.argmax()), diff_max.shape)
            cx0, cy0, cx1, cy1 = int(xx), int(yy), int(xx) + 1, int(yy) + 1
        pad = args.crop_pad
        h, w = diff_max.shape
        box = (max(0, cx0 - pad), max(0, cy0 - pad), min(w, cx1 + pad), min(h, cy1 + pad))
        # Keep crops readable: at most 400 px of source each way.
        box = (box[0], box[1], min(box[2], box[0] + 400), min(box[3], box[1] + 400))
        zoom = max(1, min(6, 600 // max(box[2] - box[0], box[3] - box[1])))
        side_by_side(ref, cand, diff_max, box, zoom, args.amplify).save(out / f"{stem}.worst.png")
        result["worst_box"] = list(box)
        result["worst_crop"] = str(out / f"{stem}.worst.png")
    return result


def collect_pairs(args):
    pairs = []
    for d in args.dir or []:
        d = Path(d)
        for cand in sorted(d.glob("*.vulkan.png")):
            ref = cand.with_name(cand.name[: -len(".vulkan.png")] + ".png")
            prefix = (d.name + "/") if len(args.dir) + len(args.pair or []) > 1 else ""
            pairs.append((prefix + ref.stem, ref, cand, [ref.with_name(ref.stem + ".software2.png")]))
    for a, b in args.pair or []:
        a, b = Path(a), Path(b)
        prefix = (a.name + "/") if len(args.dir or []) + len(args.pair) > 1 else ""
        derived = (".vulkan.png", ".software2.png")
        for ref in sorted(a.glob("*.png")):
            if ref.name.endswith(derived):
                continue
            cand = b / ref.name
            if cand.exists():
                pairs.append((prefix + ref.stem, ref, cand, [Path(d) / ref.name for d in args.unstable or []]))
            else:
                print(f"missing in {b}: {ref.name}", file=sys.stderr)
                pairs.append((prefix + ref.stem, ref, None, []))
        for cand in sorted(b.glob("*.png")):
            if not (a / cand.name).exists() and not cand.name.endswith(derived):
                print(f"missing in {a}: {cand.name}", file=sys.stderr)
                pairs.append((prefix + cand.stem, None, cand, []))
    return pairs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", action="append", help="a directory from one ILANA_SNAPSHOT_RENDERER=both run")
    ap.add_argument("--pair", nargs=2, action="append", metavar=("REFERENCE", "CANDIDATE"),
                    help="two directories of same-named PNGs")
    ap.add_argument("--out", help="where heatmaps, crops, summary.json and summary.md go")
    ap.add_argument("--within", type=int, default=5, help="levels a pixel's channels may differ and still count as equal")
    ap.add_argument("--min-within", type=float, default=99.9, help="percent of pixels that must be within --within")
    ap.add_argument("--cluster-level", type=int, default=32, help="a 'big' difference, in levels")
    ap.add_argument("--max-cluster", type=int, default=4, help="largest allowed bounding box side of a cluster of big differences")
    ap.add_argument("--unstable", action="append", metavar="DIR",
                    help="another run of the reference: pixels where it differs from the reference are left out (repeatable)")
    ap.add_argument("--unstable-grow", type=int, default=1, help="px the --unstable mask is grown by")
    ap.add_argument("--exact", action="store_true", help="require identical pixels (software baseline check)")
    ap.add_argument("--amplify", type=float, default=8.0, help="heatmap gain")
    ap.add_argument("--crop-pad", type=int, default=24, help="context around the worst region in the side-by-side crop")
    ap.add_argument("--list-clusters", type=int, default=12, help="clusters listed per picture in the JSON")
    args = ap.parse_args()
    if not args.dir and not args.pair:
        ap.error("give --dir and/or --pair")

    out = Path(args.out) if args.out else None
    if out is not None:
        out.mkdir(parents=True, exist_ok=True)

    results = []
    for name, ref, cand, unstable in collect_pairs(args):
        if ref is None or cand is None:
            results.append({"name": name, "pass": False, "why": "missing pair", "size_match": False})
            continue
        results.append(compare(name, ref, cand, unstable, args, out))

    if not results:
        print("no picture pairs found", file=sys.stderr)
        return 2

    header = ("| picture | size | max | mean | >2 % | >8 % | >32 % | 0 / 1 / 2 / 3 / 4 / 5+ levels (% px) | big clusters | "
              "worst clusters (x,y wxh) | unstable px (%) | result |")
    lines = [header, "|---" * (header.count("|") - 1) + "|"]
    for r in results:
        if not r.get("size_match"):
            lines.append(f"| {r['name']} | {r.get('size_reference')} vs {r.get('size_candidate')} | | | | | | | | | | FAIL: {r['why']} |")
            continue
        worst = ", ".join(f"{c['x']},{c['y']} {c['w']}x{c['h']}" for c in r["clusters"][:3])
        lines.append(f"| {r['name']} | {r['size_reference'][0]}x{r['size_reference'][1]} | {r['max_abs']} | {r['mean_abs']:.4f} | "
                     f"{r['pct_over_2']:.3f} | {r['pct_over_8']:.3f} | {r['pct_over_32']:.3f} | {' / '.join(f'{v:.2f}' for v in r['hist_pct'])} | {r['clusters_big']}/{r['clusters_total']} | "
                     f"{worst} | {r['unstable_px']} ({r['pct_unstable']:.2f}) | {'pass' if r['pass'] else 'FAIL: ' + r['why']} |")
    failed = [r for r in results if not r["pass"]]
    lines.append("")
    lines.append(f"{len(results) - len(failed)}/{len(results)} pass"
                 + (" (exact)" if args.exact else f" (>= {args.min_within}% within {args.within}; no >{args.cluster_level} cluster over {args.max_cluster}x{args.max_cluster} px)"))
    table = "\n".join(lines)
    print(table)
    if out is not None:
        thresholds = {k: getattr(args, k) for k in ("within", "min_within", "cluster_level", "max_cluster", "exact", "unstable_grow")}
        (out / "summary.json").write_text(json.dumps({"thresholds": thresholds, "pass": not failed,
                                                       "pictures": results}, indent=1))
        (out / "summary.md").write_text(table + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
