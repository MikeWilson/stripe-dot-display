#!/usr/bin/env python3
"""Turn the simulator's raw RGB dumps into PNGs, with the round panel masked."""
import sys, os, glob
from PIL import Image, ImageDraw

W = H = 466
src = sys.argv[1] if len(sys.argv) > 1 else "."
for path in sorted(glob.glob(os.path.join(src, "*.rgb"))):
    img = Image.frombytes("RGB", (W, H), open(path, "rb").read())
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, W - 1, H - 1), fill=255)
    out = Image.new("RGB", (W, H), (0, 0, 0))
    out.paste(img, (0, 0), mask)
    dst = path[:-4] + ".png"
    out.save(dst)
    print(dst)
