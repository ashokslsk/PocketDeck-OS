#!/usr/bin/env python3
"""Drive the X3 simulator through named sections and collect screenshots.

usage: walk.py <section> [<section> ...]   (sections defined in SECTIONS)
Each step is (key, label-or-None, wait_ms). A label takes a screenshot
`wait_ms` after the key press. Keys: UP DOWN LEFT RIGHT ENTER BACK, or
"HOLD:<KEY>:<ms>" for long presses, or "WAIT" for pure delay.
"""
import os, subprocess, sys
from PIL import Image

SP = "/private/tmp/claude-501/-Users-ashok-Library-Application-Support-Claude-scratch-workspaces-9fbb4d3d-2663-4dcd-b22b-654bf7582786-e533bd95-93c8-45a5-94c8-6fab1c9482d0-scratch-2026-09-26-a24ec2/edfb3f92-85f7-4810-8e22-67e88b4e7394/scratchpad"
HERE = os.path.dirname(os.path.abspath(__file__))
SIMRUN = os.path.join(HERE, "simrun.sh")
REUSE = {}
START = {}
exec(open(os.path.join(HERE, "walk_sections.py")).read())  # defines SECTIONS, PREP, REUSE


def build(steps, start=1500, gap=900):
    t = start; ev = []; shots = []
    for key, label, wait in steps:
        if key.startswith("HOLD:"):
            _, k, ms = key.split(":")
            ev.append(f"{t}:{k}:{ms}"); t += int(ms)
        elif key != "WAIT":
            ev.append(f"{t}:{key}")
        if label:
            shots.append(f"{t + wait}:SHOTS/{label}.bmp"); t += wait + 300
        else:
            t += wait or gap
    ev.append(f"{t + 500}:QUIT")
    ev.append(f"{t + 1500}:ENTER")  # wakes a sleeping simulator so it can quit
    return ";".join(ev), ";".join(shots)


for name in sys.argv[1:]:
    ev, shots = build(SECTIONS[name])
    reuse = REUSE.get(name, "")
    env = dict(os.environ, SIM_ENV="simulator-X3", PREP=PREP.get(name, "" if reuse else PREP.get("*", "")), REUSE_FS=("walk_" + reuse) if reuse else "", CROSSINK_SIMULATOR_START_SCREEN=START.get(name, ""))
    out = subprocess.run([SIMRUN, "walk_" + name, ev, shots], env=env, capture_output=True, text=True).stdout
    d = f"{SP}/simrun/walk_{name}/shots"
    fs = sorted(f for f in os.listdir(d) if f.endswith(".bmp"))
    for f in fs:
        Image.open(os.path.join(d, f)).convert("RGB").save(os.path.join(d, f[:-4] + ".png"))
    print(name, out.split()[0:2], len(fs), "shots of", shots.count(".bmp"))
