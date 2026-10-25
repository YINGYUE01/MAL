#!/usr/bin/env python3
"""
MAL skill and status icons.

Builds the whole icon set out of game-icons.net silhouettes: downloads the 512px
PNG for every entry in SKILLS / STATUSES, re-colours it to the ability's element
tint, and composites it onto a dark-gold frame. Everything comes from a table, so
adding a skill later is one tuple rather than an art request.

Run with the *system* Python (needs Pillow + network), NOT the Unreal editor's
embedded Python:

    python Scripts/generate_skill_icons.py
    python Scripts/generate_skill_icons.py --refresh     # ignore download cache

Output:
    Content/UI/Icons/Skills/<Name>.png        framed, same shape as the existing
                                              GuiParts/RPGIcons/skill_icon_0X
    Content/UI/Icons/Skills/<Name>_nobg.png   icon only, for slots that draw their
                                              own frame (WBP_AbilitySlot already
                                              binds a FrameImage)
    Content/UI/Icons/Status/<Name>.png        same two variants per status
    Content/UI/Icons/CREDITS.md               CC BY 3.0 attribution

Licensing: the silhouettes are CC BY 3.0 by Lorc and Delapouite, so
CREDITS.md has to ship with the game -- that is the only condition. The framed
variants are derivative works and keep the same licence.

Why PNG and not SVG: game-icons.net serves the same artwork rasterised at
/icons/<fg>/<bg>/1x1/<author>/<name>.png, so the pipeline needs no SVG renderer
(cairo / Inkscape / resvg) and therefore no native dependency on Windows. If the
PNG endpoint ever disappears, swap SOURCE_URL for the raw GitHub SVG and rasterise
it yourself -- the rest of the script is unaffected.
"""

import argparse
import os
import sys
import time
import urllib.error
import urllib.request

from PIL import Image, ImageDraw, ImageFilter, ImageOps

# --------------------------------------------------------------------------- #
# layout
# --------------------------------------------------------------------------- #

# Work at 512 and downsample on the way out. The frame's outer ring is only a few
# pixels wide at the final size, so it has to be drawn larger to stay smooth.
SIZE = 512
OUT_SIZE = 256

# Fraction of the canvas the glyph may occupy. Leaves room for the frame ring and
# keeps every icon in the set at a similar optical weight regardless of how much
# empty space its source artwork carries.
CONTENT = 0.76

RADIUS = 92          # corner radius of the frame, in the 512 space
RING_WIDTH = 13      # thickness of the gold ring

FRAME_INNER = (0x34, 0x2B, 0x1E)
FRAME_OUTER = (0x0D, 0x09, 0x06)
GOLD = (0xC9, 0xA2, 0x27)
GOLD_HIGHLIGHT = (0xF3, 0xDD, 0x9B)

RESAMPLE = getattr(Image, "Resampling", Image).LANCZOS

_RADIAL = None

_HERE = os.path.dirname(os.path.abspath(__file__))
_CONTENT = os.path.normpath(os.path.join(_HERE, "..", "Content"))
OUT_ROOT = os.path.join(_CONTENT, "UI", "Icons")
CACHE_DIR = os.path.join(_HERE, ".iconcache")

SOURCE_URL = "https://game-icons.net/icons/ffffff/000000/1x1/{author}/{name}.png"
USER_AGENT = "MAL-icon-build/1.0 (+local single-player project)"

# --------------------------------------------------------------------------- #
# icon table
# --------------------------------------------------------------------------- #
# (file stem, author, icon name, tint, label)
#
# Names are checked against the game-icons repository tree, not guessed: a typo
# here is a silent 404 at build time, which is why main() reports misses loudly
# instead of skipping them.

SKILLS = (
    ("Skill_Combo_Attack", "lorc", "broadsword", "#E6DFCB", "Combo Attack"),
    ("Skill_Heavy_Slash", "lorc", "blade-fall", "#E6DFCB", "Heavy Slash"),
    ("Skill_Whirlwind", "lorc", "whirlwind", "#E6DFCB", "Whirlwind"),
    ("Skill_Dash", "lorc", "sprint", "#D9AE52", "Dash"),
    ("Skill_Shield_Bash", "delapouite", "shield-bash", "#D9AE52", "Shield Bash"),
    ("Skill_Parry", "lorc", "shield-reflect", "#D9AE52", "Parry"),
    ("Skill_Execute", "lorc", "crossed-slashes", "#C0392B", "Execute"),
    ("Skill_Fireball", "lorc", "fireball", "#E2571E", "Fireball"),
    ("Skill_Flame_Wave", "lorc", "fire-wave", "#E2571E", "Flame Wave"),
    ("Skill_Lightning", "lorc", "heavy-lightning", "#EFC64B", "Lightning"),
    ("Skill_Frost_Nova", "lorc", "snowflake-1", "#4FA8D8", "Frost Nova"),
    ("Skill_Poison_Cloud", "lorc", "poison-gas", "#7FA53F", "Poison Cloud"),
    ("Skill_Heal", "delapouite", "healing", "#79C97A", "Heal"),
    ("Skill_Buff_Attack", "delapouite", "upgrade", "#C0392B", "Attack Up"),
    ("Skill_Stone_Skin", "lorc", "stone-block", "#A08662", "Stone Skin"),
    ("Skill_Summon_Golem", "delapouite", "rock-golem", "#A08662", "Summon Golem"),
)

STATUSES = (
    ("Status_Stun", "delapouite", "knocked-out-stars", "#EFC64B", "Stun"),
    ("Status_Bleed", "lorc", "bleeding-wound", "#C0392B", "Bleed"),
    ("Status_Burn", "lorc", "burning-dot", "#E2571E", "Burn"),
    ("Status_Freeze", "lorc", "frozen-block", "#4FA8D8", "Freeze"),
    ("Status_Poison", "lorc", "poison-bottle", "#7FA53F", "Poison"),
    ("Status_Slow", "lorc", "snail", "#8FA6B8", "Slow"),
    ("Status_Rage", "delapouite", "enrage", "#C0392B", "Rage"),
    ("Status_Shielded", "lorc", "energy-shield", "#D9AE52", "Shielded"),
)


# --------------------------------------------------------------------------- #
# colour helpers
# --------------------------------------------------------------------------- #

def hex_rgb(value):
    value = value.lstrip("#")
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def lighten(rgb, amount):
    return mix(rgb, (255, 255, 255), amount)


def darken(rgb, amount):
    return mix(rgb, (0, 0, 0), amount)


# --------------------------------------------------------------------------- #
# download
# --------------------------------------------------------------------------- #

def fetch_icon(author, name, refresh=False):
    """Download one 512px PNG, cached on disk.

    The cache matters more than it looks: a rerun after tweaking the frame
    geometry should not depend on the network, and game-icons stays reachable
    long after a build has stopped being interesting.
    """
    cache_path = os.path.join(CACHE_DIR, "{}--{}.png".format(author, name))
    if not refresh and os.path.exists(cache_path) and os.path.getsize(cache_path) > 512:
        return cache_path

    url = SOURCE_URL.format(author=author, name=name)
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})

    last_error = None
    for attempt in range(3):
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                payload = response.read()
            if not payload.startswith(b"\x89PNG"):
                raise ValueError("not a PNG: {} bytes".format(len(payload)))
            os.makedirs(CACHE_DIR, exist_ok=True)
            with open(cache_path, "wb") as handle:
                handle.write(payload)
            return cache_path
        except (urllib.error.URLError, ValueError, OSError) as error:
            last_error = error
            time.sleep(1.0 + attempt)

    raise RuntimeError("{} ({})".format(last_error, url))


# --------------------------------------------------------------------------- #
# glyph preparation
# --------------------------------------------------------------------------- #

def glyph_mask(path):
    """Source PNG -> hard alpha mask, cropped to the artwork.

    game-icons ships white-on-black, so the luminance *is* the coverage. Corners
    are sampled rather than assumed: a few authors invert the palette, and getting
    that wrong yields a solid square instead of an icon.
    """
    image = Image.open(path).convert("L")
    width, height = image.size

    corners = (
        image.getpixel((2, 2)),
        image.getpixel((width - 3, 2)),
        image.getpixel((2, height - 3)),
        image.getpixel((width - 3, height - 3)),
    )
    if sum(corners) / 4.0 > 127:
        image = ImageOps.invert(image)

    box = image.getbbox()
    if box is None:
        raise ValueError("empty artwork")
    return image.crop(box)


def fit_to_canvas(mask):
    """Scale the cropped glyph into the content area, centred on a square canvas."""
    target = int(SIZE * CONTENT)
    scale = min(float(target) / mask.width, float(target) / mask.height)
    new_size = (max(1, int(mask.width * scale)), max(1, int(mask.height * scale)))
    mask = mask.resize(new_size, RESAMPLE)

    canvas = Image.new("L", (SIZE, SIZE), 0)
    canvas.paste(mask, ((SIZE - mask.width) // 2, (SIZE - mask.height) // 2))
    return canvas


def tint_glyph(mask, rgb):
    """Fill the mask with a vertical gradient.

    A flat fill reads as a sticker; fading the bottom toward the tint's shadow
    gives the silhouette enough depth to sit on the frame without a separate
    bevel pass.
    """
    top = lighten(rgb, 0.55)
    bottom = darken(rgb, 0.18)

    strip = Image.new("RGB", (1, SIZE))
    for y in range(SIZE):
        strip.putpixel((0, y), mix(top, bottom, y / float(SIZE - 1)))

    glyph = strip.resize((SIZE, SIZE), RESAMPLE).convert("RGBA")
    glyph.putalpha(mask)
    return glyph


def tint_glow(mask, rgb, strength=0.55, radius=10):
    """Soft halo behind the glyph, in the same tint.

    This is what keeps the coloured glyphs legible on the dark frame and gives
    each ability an element colour readable at a glance in the ability bar.
    """
    halo = mask.filter(ImageFilter.GaussianBlur(radius))
    halo = halo.point(lambda v: int(v * strength))

    glow = Image.new("RGBA", (SIZE, SIZE), rgb + (0,))
    glow.putalpha(halo)
    return glow


# --------------------------------------------------------------------------- #
# frame
# --------------------------------------------------------------------------- #

def radial_mask():
    """Centre-weighted ramp: 255 in the middle, 0 at the rim.

    Cached because every icon rebuilds its frame and re-deriving a 512px
    gradient 24 times is pure waste.
    """
    global _RADIAL
    if _RADIAL is None:
        _RADIAL = ImageOps.invert(
            Image.radial_gradient("L").resize((SIZE, SIZE), RESAMPLE)
        )
    return _RADIAL


def build_frame(tint):
    """Dark radial plate with a gold ring, washed with the ability's tint.

    Deliberately not a plain circle: a rounded square matches the slot widgets and
    survives being scaled down to 64px in the bar far better than a thin annulus.
    """
    centre = radial_mask()

    plate_rgb = Image.composite(
        Image.new("RGB", (SIZE, SIZE), FRAME_INNER),
        Image.new("RGB", (SIZE, SIZE), FRAME_OUTER),
        centre,
    )

    # Element wash: a whisper of the ability colour in the plate. At 13% it never
    # competes with the gold ring, but it makes a fire skill read warm and a frost
    # skill read cold at a glance -- which is the whole job of a slot icon.
    wash = centre.point(lambda v: int(v * 0.13))
    plate_rgb.paste(Image.new("RGB", (SIZE, SIZE), tint), (0, 0), wash)

    # Bottom shade. Without it the plate reads as a flat disc and the glyph looks
    # pasted on rather than seated in the slot.
    shade = Image.new("L", (1, SIZE))
    for y in range(SIZE):
        t = (y / float(SIZE - 1) - 0.42) / 0.58
        shade.putpixel((0, y), int(max(0.0, min(1.0, t)) * 70))
    plate_rgb.paste((0, 0, 0), (0, 0), shade.resize((SIZE, SIZE), RESAMPLE))

    plate = plate_rgb.convert("RGBA")

    # Knock the corners out so the plate is a rounded square, not a rectangle.
    corners = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(corners).rounded_rectangle(
        [0, 0, SIZE - 1, SIZE - 1], radius=RADIUS, fill=255
    )
    plate.putalpha(corners)

    # Gold ring: draw the outer rounded rect, then punch the middle back out.
    # ImageDraw overwrites pixels in RGBA rather than blending, so the second
    # call leaves a clean transparent hole.
    ring = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(ring)
    draw.rounded_rectangle(
        [0, 0, SIZE - 1, SIZE - 1], radius=RADIUS, fill=GOLD + (255,)
    )
    draw.rounded_rectangle(
        [RING_WIDTH, RING_WIDTH, SIZE - 1 - RING_WIDTH, SIZE - 1 - RING_WIDTH],
        radius=RADIUS - RING_WIDTH,
        fill=(0, 0, 0, 0),
    )

    # Inner hairline: reads as a metal lip catching light along the top edge.
    lip = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ImageDraw.Draw(lip).rounded_rectangle(
        [RING_WIDTH + 4, RING_WIDTH + 4,
         SIZE - 1 - RING_WIDTH - 4, SIZE - 1 - RING_WIDTH - 4],
        radius=RADIUS - RING_WIDTH - 4,
        outline=GOLD_HIGHLIGHT + (105,),
        width=2,
    )

    # Top sheen, clipped to the plate so it never floats over the transparent
    # corners.
    sheen = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ImageDraw.Draw(sheen).ellipse(
        [-SIZE * 0.20, -SIZE * 0.58, SIZE * 1.20, SIZE * 0.30],
        fill=(255, 240, 200, 30),
    )
    sheen = sheen.filter(ImageFilter.GaussianBlur(20))

    plate.alpha_composite(ring)
    plate.alpha_composite(lip)

    clipped_sheen = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    clipped_sheen.paste(sheen, (0, 0), corners)
    plate.alpha_composite(clipped_sheen)

    return plate


# --------------------------------------------------------------------------- #
# assembly
# --------------------------------------------------------------------------- #

def build_variants(mask, tint):
    glyph = tint_glyph(mask, tint)
    glow = tint_glow(mask, tint)

    # Framed: frame -> glow -> glyph.
    framed = build_frame(tint)
    framed.alpha_composite(glow)
    framed.alpha_composite(glyph)

    # Bare: the frame already exists in WBP_AbilitySlot, so ship the glyph alone
    # and let the widget supply the plate. Same glow, so the two variants read
    # identically side by side.
    bare = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    bare.alpha_composite(glow)
    bare.alpha_composite(glyph)

    return (
        framed.resize((OUT_SIZE, OUT_SIZE), RESAMPLE),
        bare.resize((OUT_SIZE, OUT_SIZE), RESAMPLE),
    )


def emit(entries, folder, refresh):
    out_dir = os.path.join(OUT_ROOT, folder)
    os.makedirs(out_dir, exist_ok=True)

    written = []
    failed = []

    for stem, author, name, tint, label in entries:
        try:
            path = fetch_icon(author, name, refresh=refresh)
            mask = fit_to_canvas(glyph_mask(path))
        except Exception as error:  # noqa: BLE001 - one bad icon must not kill the run
            failed.append((stem, name, str(error)))
            print("  FAIL  {:<24} {}".format(stem, error))
            continue

        framed, bare = build_variants(mask, hex_rgb(tint))
        framed.save(os.path.join(out_dir, stem + ".png"))
        bare.save(os.path.join(out_dir, stem + "_nobg.png"))

        written.append((stem, author, name, label))
        print("  ok    {:<24} {:<10} {}".format(stem, name, label))

    return written, failed


# --------------------------------------------------------------------------- #
# credits
# --------------------------------------------------------------------------- #

CREDITS_HEADER = """# Icon credits

The skill and status icons in this folder are recoloured derivatives of
[game-icons.net](https://game-icons.net) artwork, licensed under
[Creative Commons BY 3.0](https://creativecommons.org/licenses/by/3.0/).

That licence allows commercial use and modification but **requires attribution**.
Keep this file with the build and surface the credit line below somewhere the
player can reach it -- a credits screen, an about box, a README.

> Game icons by Lorc and Delapouite - https://game-icons.net (CC BY 3.0)

Regenerate with `python Scripts/generate_skill_icons.py`.
"""


def write_credits(rows):
    lines = [CREDITS_HEADER, ""]
    lines.append("| File | Icon | Author | Source |")
    lines.append("| --- | --- | --- | --- |")

    for stem, author, name, label in rows:
        url = "https://game-icons.net/1x1/{}/{}.html".format(author, name)
        lines.append(
            "| `{}` | {} (`{}`) | {} | {} |".format(stem, label, name, author, url)
        )

    path = os.path.join(OUT_ROOT, "CREDITS.md")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


# --------------------------------------------------------------------------- #
# entry point
# --------------------------------------------------------------------------- #

def main():
    parser = argparse.ArgumentParser(description="Build MAL skill and status icons.")
    parser.add_argument(
        "--refresh",
        action="store_true",
        help="re-download every source icon instead of reusing the cache",
    )
    args = parser.parse_args()

    if not os.path.isdir(_CONTENT):
        sys.exit("Content directory not found next to Scripts/: " + _CONTENT)

    print("output: " + OUT_ROOT)

    print("\nskills ({}):".format(len(SKILLS)))
    skill_rows, skill_fail = emit(SKILLS, "Skills", args.refresh)

    print("\nstatus ({}):".format(len(STATUSES)))
    status_rows, status_fail = emit(STATUSES, "Status", args.refresh)

    rows = skill_rows + status_rows
    if rows:
        credits = write_credits(rows)
        print("\ncredits: " + credits)

    failures = skill_fail + status_fail
    print("\n{} icons written, {} failed".format(len(rows), len(failures)))

    if failures:
        print("\nFailed sources (check the name against game-icons.net):")
        for stem, name, error in failures:
            print("  {:<24} {:<20} {}".format(stem, name, error))
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
