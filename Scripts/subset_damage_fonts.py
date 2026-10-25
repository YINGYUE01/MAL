"""Subset the damage-number fonts down to printable ASCII and stage them for UE.

Bebas Neue and Archivo Black ship with the full Latin set, so each file carries
130-390 KB of glyphs a floating damage number never draws. Keeping only
U+0020-007E leaves every digit, separator and K/M suffix while cutting the
payload by an order of magnitude.

Run:  python Scripts/subset_damage_fonts.py
"""

import os
import shutil

from fontTools import subset

_HERE = os.path.dirname(os.path.abspath(__file__))
_PROJECT = os.path.dirname(_HERE)
_CACHE = os.path.join(_PROJECT, "Saved", "_fonts")
_OUT_ROOT = os.path.join(_PROJECT, "Content", "UI", "Fonts")
_SRC_DIR = os.path.join(_OUT_ROOT, "Source")

# (display name, ttf as downloaded from google/fonts, subset asset name)
FONTS = (
    ("Bebas Neue", "ofl_bebasneue_BebasNeue-Regular.ttf", "BebasNeue-Regular.ttf"),
    ("Archivo Black", "ofl_archivoblack_ArchivoBlack-Regular.ttf", "ArchivoBlack-Regular.ttf"),
)


def subset_font(src, dst):
    options = subset.Options()
    options.name_IDs = ["*"]          # keep copyright / licence name records
    options.name_legacy = True
    options.name_languages = ["*"]
    options.layout_features = ["*"]
    options.notdef_outline = True
    options.hinting = True            # keep grid-fitting: the numbers render small
    options.drop_tables += ["DSIG"]

    font = subset.load_font(src, options)
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=range(0x20, 0x7F))
    subsetter.subset(font)
    subset.save_font(font, dst, options)
    font.close()


def main():
    os.makedirs(_SRC_DIR, exist_ok=True)

    for label, cache_name, out_name in FONTS:
        src = os.path.join(_CACHE, cache_name)
        if not os.path.exists(src):
            raise SystemExit("missing cached font: " + src + "\nrun Saved/_font_probe.py first")

        dst = os.path.join(_SRC_DIR, out_name)
        subset_font(src, dst)

        before = os.path.getsize(src) / 1024.0
        after = os.path.getsize(dst) / 1024.0
        print("%-16s %7.1f KB -> %5.1f KB  (%s)" % (label, before, after, out_name))

        # Keep the licence text next to the font: OFL asks that it travel with
        # the font file, and it costs a few KB.
        lic = os.path.join(_CACHE, cache_name.replace("-Regular.ttf", "-OFL.txt"))
        if os.path.exists(lic):
            shutil.copyfile(lic, os.path.join(_SRC_DIR, out_name.replace(".ttf", "-OFL.txt")))


if __name__ == "__main__":
    main()
