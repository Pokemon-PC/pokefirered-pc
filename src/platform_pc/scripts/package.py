#!/usr/bin/env python3
"""Builds the distributable zip of the PC version.

Contents: pokefirered and/or pokeleafgreen (debug symbols stripped: smaller;
crash addresses stay valid against the unstripped exe of the build
directory, the base is fixed), the README, and a version note. Everything
the games need is built into the executables.

Never packaged: saves (firered.sav, leafgreen.sav...), config.ini, ROMs, logs.

Usage: python src/platform_pc/scripts/package.py [--dist DIST] [BUILD_DIR...]
  default build directories: build-pc (FireRed) and build-lg (LeafGreen),
  each used if it holds a built game.
"""
import argparse
import datetime
import os
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
FORBIDDEN = (".bin", ".rtc", ".sav", ".gba", ".ini", ".log", ".wav", ".bmp")
EXT = ".exe" if os.name == "nt" else ""
GAMES = ("pokefirered", "pokeleafgreen")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dist", default=os.path.join(ROOT, "dist"))
    ap.add_argument("builds", nargs="*")
    a = ap.parse_args()
    builds = [os.path.abspath(b) for b in (a.builds or [os.path.join(ROOT, "build-pc"), os.path.join(ROOT, "build-lg")])]
    exes = []
    for b in builds:
        for g in GAMES:
            p = os.path.join(b, g + EXT)
            if os.path.exists(p) and all(os.path.basename(p) != os.path.basename(e) for e in exes):
                exes.append(p)
    if not exes:
        sys.exit(f"no built game in {builds}: build first")

    dist = os.path.abspath(a.dist)
    stage = os.path.join(dist, "pokefirered-pc")
    shutil.rmtree(stage, ignore_errors=True)
    os.makedirs(stage)
    strip = shutil.which("llvm-strip") or shutil.which("strip")
    for exe in exes:
        out_exe = os.path.join(stage, os.path.basename(exe))
        if strip:
            subprocess.run([strip, "--strip-debug", "-o", out_exe, exe], check=True)
        else:
            shutil.copy2(exe, out_exe)
        print("+", os.path.basename(exe))
    shutil.copy2(os.path.join(ROOT, "README_PC.md"), os.path.join(stage, "README_PC.md"))
    commit = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                            capture_output=True, text=True).stdout.strip() or "unknown"
    with open(os.path.join(stage, "VERSION.txt"), "w", encoding="utf-8") as f:
        f.write(f"pokefirered PC port ({', '.join(os.path.basename(e) for e in exes)})\n"
                f"commit {commit}\nbuilt {datetime.date.today()}\n"
                "Crash addresses: symbolize with the matching unstripped exe of the build directory.\n")

    for dirpath, _, files in os.walk(stage):
        for name in files:
            if name.lower().endswith(FORBIDDEN):
                sys.exit(f"refusing to package {name}: saves, ROMs and configs stay private")

    zip_path = os.path.join(dist, f"pokefirered-pc-{commit}.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for dirpath, _, files in os.walk(stage):
            for name in files:
                full = os.path.join(dirpath, name)
                z.write(full, os.path.relpath(full, dist))
    size = os.path.getsize(zip_path) / (1 << 20)
    print(f"{zip_path} ({size:.1f} MiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
