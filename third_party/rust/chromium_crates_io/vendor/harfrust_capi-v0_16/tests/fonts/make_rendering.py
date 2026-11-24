"""Build a small, original font for C drawing and painting regression tests.

Run with fontTools installed. All glyphs, palettes, SVG and PNG data are
created here; the resulting Rendering.ttf has the repository's MIT license.
"""

import struct
import zlib
from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.colorLib.builder import buildCOLR, buildCPAL
from fontTools.ttLib.tables.otTables import PaintFormat as P
from fontTools.ttLib.tables.sbixStrike import Strike
from fontTools.ttLib.tables.sbixGlyph import Glyph


def png_chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))


def main():
    names = [".notdef", "triangle", "layers", "linear", "cycle", "svg", "bitmap", "radial", "sweep",
             "bitmap_negative"]
    builder = FontBuilder(1000, isTTF=True)
    builder.setupGlyphOrder(names)
    builder.setupCharacterMap({0x41 + i: name for i, name in enumerate(names[1:])})
    glyphs = {}
    for name in names:
        pen = TTGlyphPen(None)
        if name == "triangle":
            pen.moveTo((100, 0))
            pen.lineTo((500, 800))
            pen.lineTo((900, 0))
            pen.closePath()
        glyphs[name] = pen.glyph()
    builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics({name: (1000, 100 if name == "triangle" else 0) for name in names})
    builder.setupHorizontalHeader(ascent=800, descent=-200)
    builder.setupNameTable({"familyName": "Rendering Test", "styleName": "Regular"})
    builder.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
    builder.setupPost()
    line = {"Extend": 0, "ColorStop": [
        {"StopOffset": 0, "PaletteIndex": 0, "Alpha": 1},
        {"StopOffset": 1, "PaletteIndex": 0xFFFF, "Alpha": 0.5}]}
    gradients = {
        "linear": {"Format": P.PaintLinearGradient, "ColorLine": line,
                   "x0": 100, "y0": 0, "x1": 900, "y1": 0, "x2": 100, "y2": 800},
        "radial": {"Format": P.PaintRadialGradient, "ColorLine": line,
                   "x0": 100, "y0": 100, "r0": 20, "x1": 500, "y1": 500, "r1": 400},
        "sweep": {"Format": P.PaintSweepGradient, "ColorLine": line,
                  "centerX": 500, "centerY": 400, "startAngle": 30, "endAngle": 270},
    }
    colors = {"layers": [("triangle", 0), ("triangle", 0xFFFF)],
              "cycle": {"Format": P.PaintTranslate, "dx": 10, "dy": 20,
                        "Paint": {"Format": P.PaintColrGlyph, "Glyph": "cycle"}}}
    colors.update({name: {"Format": P.PaintGlyph, "Glyph": "triangle", "Paint": paint}
                   for name, paint in gradients.items()})
    builder.font["COLR"] = buildCOLR(colors, glyphMap=builder.font.getReverseGlyphMap())
    builder.font["CPAL"] = buildCPAL([[(1, 0, 0, 1)], [(0, 0, 1, 0.5)]])
    svg = newTable("SVG ")
    svg.docList = [('<svg xmlns="http://www.w3.org/2000/svg"><path d="M0 0h10v10z"/></svg>', 5, 5)]
    builder.font["SVG "] = svg
    image = (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
             + png_chunk(b"IDAT", zlib.compress(b"\0\xff\0\0\xff")) + png_chunk(b"IEND", b""))
    sbix = newTable("sbix")
    strike = Strike(ppem=16, resolution=72)
    strike.glyphs["bitmap"] = Glyph(glyphName="bitmap", originOffsetX=1, originOffsetY=2,
                                    graphicType="png ", imageData=image)
    strike.glyphs["bitmap_negative"] = Glyph(glyphName="bitmap_negative", originOffsetX=-1,
                                             originOffsetY=-2, graphicType="png ", imageData=image)
    sbix.strikes = {16: strike}
    builder.font["sbix"] = sbix
    builder.font["head"].created = builder.font["head"].modified = 3400000000
    builder.font.recalcTimestamp = False
    builder.font.save(Path(__file__).with_name("Rendering.ttf"))


if __name__ == "__main__":
    main()
