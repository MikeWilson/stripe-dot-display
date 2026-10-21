#!/usr/bin/env python3
"""Turn the simulator's raw RGB dumps into PNGs.

The panel is a 368x448 rectangle (Waveshare AMOLED-1.8), so unlike the earlier
round-panel version there is no circular mask -- every pixel is real estate.
Dimensions come from size.txt, written by the simulator.
"""
import sys, os, glob
from PIL import Image

src = sys.argv[1] if len(sys.argv) > 1 else "."
with open(os.path.join(src, "size.txt")) as f:
    W, H = (int(v) for v in f.read().split())

for path in sorted(glob.glob(os.path.join(src, "*.rgb"))):
    img = Image.frombytes("RGB", (W, H), open(path, "rb").read())
    dst = path[:-4] + ".png"
    img.save(dst)
    print(dst)
