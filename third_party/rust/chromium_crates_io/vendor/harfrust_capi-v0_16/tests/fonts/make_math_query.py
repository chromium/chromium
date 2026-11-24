"""Build a small MATH query fixture from the in-house two-glyph test font."""

from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.ttLib.tables import otTables


ROOT = Path(__file__).resolve().parents[3]
source = ROOT / "harfrust/tests/fonts/in-house/8d9c4b193808b8bde94389ba7831c1fc6f9e794e.ttf"
target = Path(__file__).with_name("MathQuery.ttf")
font = TTFont(source)
math = font["MATH"].table

def device(ppem, delta):
    result = otTables.Device()
    result.StartSize = ppem
    result.EndSize = ppem
    result.DeltaFormat = 3
    result.DeltaValue = [delta]
    return result


math.MathConstants.AxisHeight.DeviceTable = device(12, 1)


def coverage():
    result = otTables.Coverage()
    result.glyphs = ["space"]
    return result


def value(n, hinting=None):
    result = otTables.MathValueRecord()
    result.Value = n
    result.DeviceTable = hinting
    return result


info = math.MathGlyphInfo
italics = otTables.MathItalicsCorrectionInfo()
italics.Coverage = coverage()
italics.ItalicsCorrectionCount = 1
italics.ItalicsCorrection = [value(120)]
info.MathItalicsCorrectionInfo = italics

accent = otTables.MathTopAccentAttachment()
accent.TopAccentCoverage = coverage()
accent.TopAccentAttachmentCount = 1
accent.TopAccentAttachment = [value(300)]
info.MathTopAccentAttachment = accent
info.ExtendedShapeCoverage = coverage()

kern = otTables.MathKern()
kern.HeightCount = 1
kern.CorrectionHeight = [value(400, device(12, 1))]
kern.KernValue = [value(25, device(13, 2)), value(50)]
record = otTables.MathKernInfoRecord()
record.TopRightMathKern = kern
record.TopLeftMathKern = None
record.BottomRightMathKern = None
record.BottomLeftMathKern = None
kern_info = otTables.MathKernInfo()
kern_info.MathKernCoverage = coverage()
kern_info.MathKernCount = 1
kern_info.MathKernInfoRecords = [record]
info.MathKernInfo = kern_info


def construction(advance):
    variant = otTables.MathGlyphVariantRecord()
    variant.VariantGlyph = "space"
    variant.AdvanceMeasurement = advance
    part = otTables.GlyphPartRecord()
    part.glyph = "space"
    part.StartConnectorLength = 20
    part.EndConnectorLength = 30
    part.FullAdvance = advance
    part.PartFlags = 1
    assembly = otTables.GlyphAssembly()
    assembly.ItalicsCorrection = value(70)
    assembly.PartCount = 1
    assembly.PartRecords = [part]
    result = otTables.MathGlyphConstruction()
    result.GlyphAssembly = assembly
    result.VariantCount = 1
    result.MathGlyphVariantRecord = [variant]
    return result


variants = math.MathVariants
variants.VertGlyphCoverage = coverage()
variants.VertGlyphCount = 1
variants.VertGlyphConstruction = [construction(700)]
variants.HorizGlyphCoverage = coverage()
variants.HorizGlyphCount = 1
variants.HorizGlyphConstruction = [construction(900)]

font.save(target)
print(target)
