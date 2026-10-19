#!/usr/bin/env python3
# Makes every logo file the client and the domain server show, from two masters:
#   the mark      a transparent PNG of the neon dR, square (1024x1024 or larger)
#   the wordmark  a transparent PNG of "DARKRIDE.AI" and its tagline, 10:3 (1000x300 or larger)
#
# usage: python3 make-icons.py <mark.png> <wordmark.png> <repository root>
# needs Pillow; on macOS the .icns is written with iconutil, elsewhere with Pillow.
#
# Written files (relative to the repository root):
#   interface/icon/interface.icns, interface-beta.icns     macOS app icon: navy rounded square, the mark on it
#   interface/icon/interface.ico, interface-beta.ico       Windows app icon, the same picture, 256 down to 16 px
#   interface/resources/images/darkride-logo.png           the app icon picture at 256 px: the update dialog (white), the Linux window icon
#   interface/resources/images/darkride-banner.png         the wordmark, 1000x300: the three login dialogs
#   domain-server/resources/web/favicon.png                the mark on a plain navy square, 512 px
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFilter

NAVY = (9, 16, 35, 255)  # #091023, the background of darkride.ai's artwork

mark_path, wordmark_path, root = sys.argv[1:4]
mark = Image.open(mark_path).convert("RGBA")
wordmark = Image.open(wordmark_path).convert("RGBA")


def write(rel, image):
	path = os.path.join(root, rel)
	os.makedirs(os.path.dirname(path), exist_ok=True)
	image.save(path)
	print(f"{rel}: {image.size[0]}x{image.size[1]}")


def app_icon(size=1024):
	# the macOS shape: a rounded square on a transparent margin (the old icon's geometry: 824 of
	# 1024, corner radius 185), a faint shadow under it, the mark at 92% of the square's width (its body is 71% of the mark picture)
	page = Image.new("RGBA", (size, size), (0, 0, 0, 0))
	margin, radius = round(size * 100 / 1024), round(size * 185 / 1024)
	shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
	ImageDraw.Draw(shadow).rounded_rectangle(
		(margin, margin + size // 64, size - margin, size - margin + size // 64), radius, fill=(0, 0, 0, 90)
	)
	page.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(size / 48)))
	ImageDraw.Draw(page).rounded_rectangle((margin, margin, size - margin, size - margin), radius, fill=NAVY)
	side = round((size - 2 * margin) * 0.92)
	m = mark.resize((side, side), Image.LANCZOS)
	page.alpha_composite(m, ((size - side) // 2, (size - side) // 2))
	return page


def icns(image, rel):
	path = os.path.join(root, rel)
	os.makedirs(os.path.dirname(path), exist_ok=True)
	if shutil.which("iconutil"):
		with tempfile.TemporaryDirectory(dir=os.path.dirname(path)) as tmp:
			iconset = os.path.join(tmp, "icon.iconset")
			os.mkdir(iconset)
			for px in (16, 32, 128, 256, 512):
				image.resize((px, px), Image.LANCZOS).save(f"{iconset}/icon_{px}x{px}.png")
				image.resize((px * 2, px * 2), Image.LANCZOS).save(f"{iconset}/icon_{px}x{px}@2x.png")
			subprocess.run(["iconutil", "-c", "icns", iconset, "-o", path], check=True)
	else:
		image.save(path, sizes=[(s, s) for s in (16, 32, 64, 128, 256, 512, 1024)])
	print(f"{rel}: {os.path.getsize(path)} bytes")


icon = app_icon()
icns(icon, "interface/icon/interface.icns")
icns(icon, "interface/icon/interface-beta.icns")
ico_sizes = [(s, s) for s in (256, 128, 64, 48, 32, 16)]
for rel in ("interface/icon/interface.ico", "interface/icon/interface-beta.ico"):
	path = os.path.join(root, rel)
	icon.resize((256, 256), Image.LANCZOS).save(path, sizes=ico_sizes)
	print(f"{rel}: {os.path.getsize(path)} bytes")

write("interface/resources/images/darkride-logo.png", icon.resize((256, 256), Image.LANCZOS))
write("interface/resources/images/darkride-banner.png", wordmark.resize((1000, 300), Image.LANCZOS))

favicon = Image.new("RGBA", (512, 512), NAVY)
favicon.alpha_composite(mark.resize((480, 480), Image.LANCZOS), (16, 16))
write("domain-server/resources/web/favicon.png", favicon)
