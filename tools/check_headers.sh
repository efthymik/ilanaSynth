#!/usr/bin/env bash
# Header self-containment check (ARCH-8 step 1): compiles each GUI header on
# its own (a generated one-line .cpp that includes only it, -fsyntax-only),
# so a header that leans on what an earlier include happened to bring in fails
# here instead of when someone includes it somewhere new.
#
#   tools/check_headers.sh [HEADER...]     (default: src/gui/*.h src/gui/pages/*.h)
#
# Borrows the compiler and flags of ilanaSnapshot's src/PluginEditor.cpp from
# the build in build/ (configure it first; BUILD_DIR overrides). JOBS sets the
# parallelism (default 4). Exit 1 if any header fails; its first errors are printed.
set -euo pipefail
cd "$(dirname "$0")/.."
build="${BUILD_DIR:-build}"
jobs="${JOBS:-4}"
cmd="$(ninja -C "$build" -t commands ilanaSnapshot 2> /dev/null | grep -m1 ' -c [^ ]*src/PluginEditor\.cpp$' || true)"
[ -n "$cmd" ] || { echo "check_headers: no PluginEditor.cpp compile in $build (configure the build first)" >&2; exit 2; }
# The compiler and its flags, without ccache, the output, the dependency file and the source.
flags="$(printf '%s\n' "$cmd" | sed -E 's/^(ccache )?//; s/ -o [^ ]+//; s/ -MD//; s/ -MT [^ ]+//; s/ -MF [^ ]+//; s/ -c [^ ]+$//')"
if [ $# -gt 0 ]; then headers=("$@"); else headers=(src/gui/*.h src/gui/pages/*.h); fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
export flags work
check_one() {
    local header="$1" name; name="$(printf '%s' "$header" | tr '/.' '__')"
    # A fragment (a piece of another header, included mid-file) says so.
    if grep -q 'header-check: fragment' "$header"; then echo "skip $header (a fragment)" > "$work/$name.res"; return; fi
    printf '#include "%s"\n' "$(realpath "$header")" > "$work/$name.cpp"
    if eval "$flags -fsyntax-only -Wno-unused-function $work/$name.cpp" > "$work/$name.log" 2>&1; then
        echo "ok   $header" > "$work/$name.res"
    else
        { echo "FAIL $header"; grep -m6 'error' "$work/$name.log" | sed 's/^/    /' | cut -c1-240; } > "$work/$name.res"
    fi
}
export -f check_one
printf '%s\n' "${headers[@]}" | xargs -P "$jobs" -I{} bash -c 'check_one "$@"' _ {}
for header in "${headers[@]}"; do cat "$work/$(printf '%s' "$header" | tr '/.' '__').res"; done > "$work/result"
cat "$work/result"
fails="$(grep -c '^FAIL' "$work/result" || true)"
skips="$(grep -c '^skip' "$work/result" || true)"
echo "$((${#headers[@]} - fails - skips)) of $((${#headers[@]} - skips)) headers compile on their own ($skips fragments skipped)"
[ "$fails" -eq 0 ]
