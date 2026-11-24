"""Compare the shared-handle Skia subset workflow against HarfBuzz.

Build harfrust_capi with --features subset, then pass HR HB_CORE HB_SUBSET.
Requires fontTools. Fonts are existing HarfRust regression fixtures.
"""

import argparse
import ctypes as c
import io
import tempfile
from pathlib import Path

from fontTools.ttLib import TTCollection, TTFont

ROOT = Path(__file__).resolve().parents[3]
PTR, UINT = c.c_void_p, c.c_uint
OUT = c.POINTER(UINT)
TABLE = c.CFUNCTYPE(PTR, PTR, UINT, PTR)
TAGS = c.CFUNCTYPE(UINT, PTR, UINT, OUT, OUT, PTR)


def bind(core, subset, prefix):
    core = c.CDLL(str(core.resolve()), mode=c.RTLD_GLOBAL)
    subset = c.CDLL(str(subset.resolve()))
    signatures = {
        "blob_create_from_file": (PTR, c.c_char_p),
        "blob_get_data": (PTR, PTR, OUT), "blob_destroy": (None, PTR),
        "face_create": (PTR, PTR, UINT), "face_destroy": (None, PTR),
        "face_get_glyph_count": (UINT, PTR), "face_set_index": (None, PTR, UINT),
        "face_reference_blob": (PTR, PTR), "face_reference_table": (PTR, PTR, UINT),
        "face_get_table_tags": (UINT, PTR, UINT, OUT, OUT),
        "face_create_for_tables": (PTR, TABLE, PTR, PTR),
        "face_set_get_table_tags_func": (None, PTR, TAGS, PTR, PTR),
        "font_create": (PTR, PTR), "font_destroy": (None, PTR),
        "font_get_nominal_glyph": (c.c_int, PTR, UINT, OUT),
        "font_set_variations": (None, PTR, PTR, UINT),
        "font_get_glyph_h_advance": (c.c_int, PTR, UINT),
        "set_add": (None, PTR, UINT), "set_reference": (PTR, PTR),
        "set_destroy": (None, PTR), "set_has": (c.c_int, PTR, UINT),
        "subset_input_create_or_fail": (PTR,), "subset_input_reference": (PTR, PTR),
        "subset_input_destroy": (None, PTR), "subset_input_glyph_set": (PTR, PTR),
        "subset_input_unicode_set": (PTR, PTR),
        "subset_input_set_flags": (None, PTR, UINT),
        "subset_input_get_flags": (UINT, PTR), "subset_or_fail": (PTR, PTR, PTR),
    }
    api = {}
    for name, (result, *args) in signatures.items():
        library = subset if name.startswith("subset_") else core
        function = getattr(library, prefix + name)
        function.restype, function.argtypes = result, args
        api[name] = function
    return api


def new_face(api, path, face_index=0):
    blob = api["blob_create_from_file"](str(path).encode())
    face = api["face_create"](blob, face_index)
    api["blob_destroy"](blob)
    return face


def glyph(api, font, char):
    output = UINT()
    assert api["font_get_nominal_glyph"](font, ord(char), c.byref(output))
    return output.value


def font_bytes(api, face):
    blob = api["face_reference_blob"](face)
    count = UINT()
    data = api["blob_get_data"](blob, c.byref(count))
    assert data and count.value
    result = c.string_at(data, count.value)
    api["blob_destroy"](blob)
    return result


def subset_font(api, path, flags, callback=False, unicode=False, face_index=0):
    original = new_face(api, path, face_index)
    font = api["font_create"](original)
    gids = [glyph(api, font, char) for char in "ac"]
    api["font_destroy"](font)
    input_obj = api["subset_input_create_or_fail"]()
    selection = api["subset_input_unicode_set" if unicode else "subset_input_glyph_set"](input_obj)
    for value in map(ord, "ac") if unicode else gids:
        api["set_add"](selection, value)
    api["subset_input_set_flags"](input_obj, flags)
    assert api["subset_input_get_flags"](input_obj) == flags
    assert api["subset_input_reference"](input_obj) == input_obj
    api["subset_input_destroy"](input_obj)

    source = original
    if callback:
        @TABLE
        def table(_face, tag, data):
            return api["face_reference_table"](data, tag)

        @TAGS
        def tags(_face, start, count, output, data):
            return api["face_get_table_tags"](data, start, count, output)

        source = api["face_create_for_tables"](table, original, None)
        api["face_set_get_table_tags_func"](source, tags, original, None)
        # Both blob and callback faces must retain their table source when
        # index metadata changes to a value that is not a valid TTC index.
    api["face_set_index"](source, 1234)
    result = api["subset_or_fail"](source, input_obj)
    assert result, (path.name, flags, callback, unicode)
    borrowed = api["set_reference"](selection)
    api["subset_input_destroy"](input_obj)
    assert api["set_has"](borrowed, ord("a") if unicode else gids[0])
    api["set_destroy"](borrowed)
    if callback:
        api["face_destroy"](source)
    api["face_destroy"](original)
    # All source data and inputs have gone away before accessing the result.
    bytes_out = font_bytes(api, result)
    font = api["font_create"](result)
    mappings = [glyph(api, font, char) for char in "ac"]
    advances = [api["font_get_glyph_h_advance"](font, gid) for gid in mappings]
    api["font_destroy"](font)
    api["face_destroy"](result)
    if flags & 2:
        assert mappings == gids
    return TTFont(io.BytesIO(bytes_out)), mappings, advances


def compare(actual, expected, original, flags):
    afont, agids, advances = actual
    bfont, bgids, badvances = expected
    assert agids == bgids
    assert advances == badvances
    assert afont["maxp"].numGlyphs == bfont["maxp"].numGlyphs
    for agid, bgid in zip([0, *agids], [0, *bgids]):
        a = afont["glyf"][afont.getGlyphName(agid)]
        b = bfont["glyf"][bfont.getGlyphName(bgid)]
        assert a.numberOfContours == b.numberOfContours
        assert a.getCoordinates(afont["glyf"])[0] == b.getCoordinates(bfont["glyf"])[0]
        aprogram = bytes(a.program.getBytecode()) if hasattr(a, "program") else b""
        bprogram = bytes(b.program.getBytecode()) if hasattr(b, "program") else b""
        assert aprogram == bprogram
        if flags & 1:
            assert not aprogram
    if flags & 2:
        selected = set(agids)
        # Closure may retain more glyphs; choose a glyph neither subset retains
        # and confirm that it is represented by an empty hole.
        holes = [gid for gid in range(1, afont["maxp"].numGlyphs)
                 if gid not in selected and bfont["glyf"][bfont.getGlyphName(gid)].numberOfContours == 0]
        assert holes
        assert afont["glyf"][afont.getGlyphName(holes[0])].numberOfContours == 0
    for tag in ["cvt ", "fpgm", "prep"]:
        if flags & 1:
            assert tag not in afont
        elif tag in original:
            assert tag in afont
            assert afont.getTableData(tag) == original.getTableData(tag)


def failure_cases(api):
    input_obj = api["subset_input_create_or_fail"]()
    assert not api["subset_or_fail"](None, input_obj)
    assert not api["subset_or_fail"](None, None)
    path = ROOT / "harfrust/tests/fonts/rb_custom/PT_Sans-Caption-Web-Regular.ttf"
    original = new_face(api, path)
    api["subset_input_set_flags"](input_obj, 0x80000000)
    assert not api["subset_or_fail"](original, input_obj)
    api["subset_input_set_flags"](input_obj, 2)

    @TABLE
    def table(_face, tag, data):
        return api["face_reference_table"](data, tag)

    source = api["face_create_for_tables"](table, original, None)
    assert not api["subset_or_fail"](source, input_obj)  # No enumeration.
    api["face_destroy"](source)
    api["face_destroy"](original)
    # Unsupported outline formats must fail rather than lose outlines.
    for name, tag in [("TestCFFThree.otf", "CFF "), ("AdobeVFPrototype-Subset.otf", "CFF2")]:
        path = ROOT / "harfrust/tests/fonts/text-rendering-tests" / name
        assert tag in TTFont(path)
        source = new_face(api, path)
        assert not api["subset_or_fail"](source, input_obj)
        api["face_destroy"](source)
    api["subset_input_destroy"](input_obj)
    api["subset_input_destroy"](None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["hr", "hb_core", "hb_subset"]:
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    hr = bind(args.hr, args.hr, "hr_")
    hb = bind(args.hb_core, args.hb_subset, "hb_")
    cases = 0
    for name in ["PT_Sans-Caption-Web-Regular.ttf", "LaBelleAurore.ttf", "Linefont.ttf"]:
        path = ROOT / "harfrust/tests/fonts/rb_custom" / name
        original = TTFont(path)
        for flags in [0, 2, 2 | 64, 2 | 1, 2 | 64 | 1]:
            expected = subset_font(hb, path, flags)
            for callback, unicode in [(False, False), (True, False), (False, True)]:
                actual = subset_font(hr, path, flags, callback, unicode)
                compare(actual, expected, original, flags)
                cases += 1
    # Reconstruct the selected TTC face even after metadata index assignment.
    collection = TTCollection()
    collection.fonts = [TTFont(ROOT / "harfrust/tests/fonts/rb_custom" / name)
                        for name in ["PT_Sans-Caption-Web-Regular.ttf", "LaBelleAurore.ttf"]]
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "two-fonts.ttc"
        collection.save(path)
        expected = subset_font(hb, path, 2 | 64, face_index=1)
        for callback, unicode in [(False, False), (True, False), (False, True)]:
            actual = subset_font(hr, path, 2 | 64, callback, unicode, face_index=1)
            compare(actual, expected, collection.fonts[1], 2 | 64)
            cases += 1
    failure_cases(hr)
    print(f"{cases} differential subset cases and failure/lifetime checks passed")


if __name__ == "__main__":
    main()
