#!/usr/bin/env python3
#
#  make-arrival-images.py
#  tools
#
#  Draws the two images another person's avatar shows while it loads (interface/src/avatar/OtherAvatar.cpp):
#  a see-through silhouette with a cyan-to-violet rim, which pulses until the avatar arrives, and the soft
#  sparkle its arrival shimmer is made of. Darkride's colours, as on darkride.ai (--dr-cyan, --dr-violet).
#
#  Needs Pillow. Run from anywhere:  python3 tools/make-arrival-images.py
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#

import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter

CYAN = (0, 175, 255)
VIOLET = (165, 72, 255)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "interface", "resources", "images")

# the silhouette is 0.4 as wide as it is tall; OtherAvatar sizes the image to the avatar's height
W, H = 512, 1280


def silhouette():
    mask = Image.new("L", (W, H), 0)
    draw = ImageDraw.Draw(mask)
    draw.ellipse([W * 0.35, H * 0.035, W * 0.65, H * 0.185], fill=255)  # head
    draw.rounded_rectangle([W * 0.17, H * 0.215, W * 0.83, H * 1.2], radius=int(W * 0.3), fill=255)  # body

    soft = mask.filter(ImageFilter.GaussianBlur(2))
    rim = ImageChops.subtract(soft, soft.filter(ImageFilter.MinFilter(11)))
    rim = rim.point(lambda v: min(255, v * 3))
    glow = rim.filter(ImageFilter.GaussianBlur(14)).point(lambda v: min(255, int(v * 1.6)))
    fill = soft.point(lambda v: int(v * 0.12))
    alpha = ImageChops.lighter(ImageChops.lighter(fill, glow), rim)

    # fades out toward the floor, so it stands rather than sits on a hard edge
    fade = Image.new("L", (1, H))
    for y in range(H):
        t = (y - H * 0.7) / (H * 0.3)
        fade.putpixel((0, y), int(255 * (1.0 - 0.85 * min(1.0, max(0.0, t)))))
    alpha = ImageChops.multiply(alpha, fade.resize((W, H)))

    colour = Image.new("RGB", (1, H))
    for y in range(H):
        t = y / (H - 1)
        colour.putpixel((0, y), tuple(int(a * (1 - t) + b * t) for a, b in zip(CYAN, VIOLET)))
    image = colour.resize((W, H))
    image.putalpha(alpha)
    return image


def sparkle(size=64):
    image = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    centre = (size - 1) / 2
    for y in range(size):
        for x in range(size):
            r = (((x - centre) ** 2 + (y - centre) ** 2) ** 0.5) / centre
            core = max(0.0, 1.0 - r * 3.0)
            halo = max(0.0, 1.0 - r) ** 2.2
            image.putpixel((x, y), (255, 255, 255, int(255 * min(1.0, core + 0.75 * halo))))
    return image


if __name__ == "__main__":
    silhouette().save(os.path.join(OUT, "avatar-arrival-silhouette.png"), optimize=True)
    sparkle().save(os.path.join(OUT, "avatar-arrival-sparkle.png"), optimize=True)
    print("wrote avatar-arrival-silhouette.png and avatar-arrival-sparkle.png in", os.path.normpath(OUT))
