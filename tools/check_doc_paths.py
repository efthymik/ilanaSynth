#!/usr/bin/env python3
"""Fail if docs/ARCHITECTURE.md or HANDOFF.md mention a repo path that does not exist.

Looks at every `backticked` token. A token counts as a path when it holds a "/"
and starts with a top-level repo folder (src/, tools/, docs/, tests/, ...), or
is a bare file name with a source/doc extension (MainPage.h). Brace lists
(`src/PluginProcessor.{h,cpp}`) and globs (`src/gui/pages/*.h`) are expanded;
bare names match a file of that name anywhere in the repo. Paths under build/,
absolute paths and URLs are not checked (outputs, other machines).

    python3 tools/check_doc_paths.py [doc ...]
"""
import glob
import itertools
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DOCS = ["docs/ARCHITECTURE.md", "HANDOFF.md"]
BARE_EXT = (".h", ".cpp", ".inc", ".py", ".sh", ".cmake", ".md", ".ps1", ".yml", ".csv")
TOP = {d for d in os.listdir(ROOT) if os.path.isdir(os.path.join(ROOT, d)) and not d.startswith(".") and d != "build"}
TOP |= {".github"}


def expand_braces(token):
    m = re.search(r"\{([^{}]*)\}", token)
    if not m:
        return [token]
    return list(itertools.chain.from_iterable(
        expand_braces(token[:m.start()] + alt + token[m.end():]) for alt in m.group(1).split(",")))


def repo_basenames():
    try:
        files = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.split("\n")
    except Exception:
        files = [os.path.relpath(os.path.join(d, f), ROOT) for d, _, fs in os.walk(ROOT) for f in fs]
    return {os.path.basename(f) for f in files if f}


def exists(path, names, has_dir):
    if not has_dir:
        return path in names
    if any(c in path for c in "*?["):
        return bool(glob.glob(os.path.join(ROOT, path)))
    return os.path.exists(os.path.join(ROOT, path))


def main():
    docs = sys.argv[1:] or DOCS
    names = repo_basenames()
    bad = []
    for doc in docs:
        with open(os.path.join(ROOT, doc), encoding="utf-8", errors="replace") as f:
            for n, line in enumerate(f, 1):
                for token in re.findall(r"`([^`\n]+)`", line):
                    token = token.strip().rstrip(".,;:)")
                    if not token or re.search(r"[\s<>=$%()]|^https?:|^[/~]|^\.\.|^build/", token):
                        continue
                    first = token.split("/")[0]
                    if "/" in token:
                        if first not in TOP:
                            continue
                    elif not token.endswith(BARE_EXT) or token.startswith(".") or "*" in token:
                        continue
                    for path in expand_braces(token):
                        path = path.rstrip("/")
                        if not exists(path, names, "/" in token):
                            bad.append(f"{doc}:{n}: {path}")
    if bad:
        print("Paths named in the docs that do not exist:")
        print("\n".join(sorted(set(bad), key=bad.index)))
        return 1
    print("doc paths ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
