#!/usr/bin/env bash
# Render the default page set, "Shimmer Pad" and "FM E-Piano" at scale 1.75
# with two ilanaSnapshot builds and compare the pictures. The baseline is
# rendered twice: pixels that differ between those two runs are animation noise
# (meter, keyboard strip, FM scope) and are masked; only differences outside
# that mask are reported (exit 1).
#
#   tools/compare_pictures.sh BASE_SNAPSHOT [NEW_SNAPSHOT] [WORKDIR]
#
# NEW_SNAPSHOT defaults to build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.
# To compare against an older commit, build that commit's ilanaSnapshot,
# copy the binary away, and pass the copy as BASE_SNAPSHOT. The baseline
# renders are reused from WORKDIR (default build/pictures) when they exist, so
# delete the directory when BASE_SNAPSHOT changes. Needs python3 with PIL and
# numpy, and a display (xvfb-run is used when DISPLAY is unset).
set -euo pipefail
cd "$(dirname "$0")/.."
base="${1:?usage: tools/compare_pictures.sh BASE_SNAPSHOT [NEW_SNAPSHOT] [WORKDIR]}"
new="${2:-build/ilanaSnapshot_artefacts/Release/ilanaSnapshot}"
work="$(realpath -m "${3:-build/pictures}")"
base="$(realpath "$base")"; new="$(realpath "$new")"
mkdir -p "$work"
pages="MAIN,VECTOR,MATRIX,FM,ARP/SEQ,FX,PATCH"

render() { # binary outdir [preset]
    [ -d "$2" ] && return 0
    local tmp; tmp="$(mktemp -d)"
    ( cd "$tmp"
      if command -v xvfb-run > /dev/null && [ -z "${DISPLAY:-}" ]; then run=(xvfb-run -a); else run=(); fi
      ILANA_SNAPSHOT_SCALE=1.75 ILANA_SNAPSHOT_PAGES="$pages" "${run[@]}" "$1" out ${3:+"$3"} > /dev/null )
    mv "$tmp/out" "$2"; rm -rf "$tmp"
}

args=()
for set in default "Shimmer Pad" "FM E-Piano"; do
    key="${set// /_}"
    preset=""; [ "$set" = default ] || preset="$set"
    echo "rendering: $set"
    render "$base" "$work/base1-$key" ${preset:+"$preset"}
    render "$base" "$work/base2-$key" ${preset:+"$preset"}
    rm -rf "$work/new-$key"
    render "$new"  "$work/new-$key"   ${preset:+"$preset"}
    args+=("$work/base1-$key" "$work/base2-$key" "$work/new-$key")
done
python3 tools/compare_pictures.py "${args[@]}"
