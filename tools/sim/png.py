#!/usr/bin/env python3
"""Turn the simulator's raw RGB dumps into PNGs.

The panel is a 368x448 rectangle (Waveshare AMOLED-1.8), so unlike the earlier
round-panel version there is no circular mask -- every pixel is real estate.
Dimensions come from size.txt, written by the simulator.
"""
import sys, os, glob, re
from PIL import Image, ImageDraw

src = sys.argv[1] if len(sys.argv) > 1 else "."
with open(os.path.join(src, "size.txt")) as f:
    W, H = (int(v) for v in f.read().split())

# The glass has rounded corners; mask them so a render matches what the panel
# actually shows. Radius is read from config.h so the two cannot drift.
cfg = open(os.path.join(os.path.dirname(__file__),
                        "../../firmware/include/config.h")).read()
R = int(re.search(r"#define\s+SCREEN_CORNER_R\s+(\d+)", cfg).group(1))

mask = Image.new("L", (W, H), 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, W - 1, H - 1), radius=R, fill=255)

for path in sorted(glob.glob(os.path.join(src, "*.rgb"))):
    img = Image.frombytes("RGB", (W, H), open(path, "rb").read())
    out = Image.new("RGB", (W, H), (0, 0, 0))
    out.paste(img, (0, 0), mask)
    dst = path[:-4] + ".png"
    out.save(dst)
    print(dst)
