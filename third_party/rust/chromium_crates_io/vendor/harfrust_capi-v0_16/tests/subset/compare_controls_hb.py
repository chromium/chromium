"""Differential tests for input controls, flags, and preprocessing.

Uses the same three library arguments as compare_skia_hb.py.
"""
import argparse
import ctypes as c
import io
import tempfile
from pathlib import Path

from fontTools.ttLib import TTFont, newTable
from compare_skia_hb import ROOT, PTR, UINT, bind, new_face, font_bytes


def api(core, subset, prefix):
    result = bind(core, subset, prefix)
    library = c.CDLL(str(subset.resolve()))
    for name, return_type, arguments in [
        ("subset_input_set", PTR, [PTR, UINT]),
        ("subset_input_keep_everything", None, [PTR]),
        ("subset_preprocess", PTR, [PTR]),
    ]:
        f = getattr(library, prefix + name)
        f.restype, f.argtypes = return_type, arguments
        result[name] = f
    library = c.CDLL(str(core.resolve()))
    for name, return_type, arguments in [
        ("set_clear", None, [PTR]), ("set_invert", None, [PTR]),
        ("set_next_range", c.c_int, [PTR, c.POINTER(UINT), c.POINTER(UINT)]),
    ]:
        f = getattr(library, prefix + name)
        f.restype, f.argtypes = return_type, arguments
        result[name] = f
    return result


def tag(value):
    return int.from_bytes(value.encode("ascii"), "big")


def set_ranges(api, selection):
    first, last = UINT(0xFFFFFFFF), UINT(0xFFFFFFFF)
    result = []
    while api["set_next_range"](selection, c.byref(first), c.byref(last)):
        result.append((first.value, last.value))
        if last.value == 0xFFFFFFFF:
            break
    return result


def set_values(api, input_obj, selector, values):
    selection = api["subset_input_set"](input_obj, selector)
    api["set_clear"](selection)
    for value in values:
        api["set_add"](selection, value)


def run(api, path, configure):
    face = new_face(api, path)
    input_obj = api["subset_input_create_or_fail"]()
    set_values(api, input_obj, 1, map(ord, "fiac("))
    configure(api, input_obj)
    result = api["subset_or_fail"](face, input_obj)
    api["face_destroy"](face)
    api["subset_input_destroy"](input_obj)
    assert result
    data = font_bytes(api, result)
    api["face_destroy"](result)
    return TTFont(io.BytesIO(data))


def normalized(font):
    order = font.getGlyphOrder()
    names = sorted((r.platformID, r.platEncID, r.langID, r.nameID, r.string)
                   for r in font["name"].names) if "name" in font else []
    cmap = sorted(font.getBestCmap().items()) if "cmap" in font else []
    cmap = [(cp, order.index(name)) for cp, name in cmap]
    features, scripts = [], []
    if "GSUB" in font:
        gsub = font["GSUB"].table
        features = [r.FeatureTag for r in gsub.FeatureList.FeatureRecord] if gsub.FeatureList else []
        scripts = [r.ScriptTag for r in gsub.ScriptList.ScriptRecord] if gsub.ScriptList else []
    return {"tables": set(font.keys()), "glyphs": font["maxp"].numGlyphs,
            "cmap": cmap, "names": names, "features": features, "scripts": scripts}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["hr", "hb_core", "hb_subset"]:
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    hr = api(args.hr, args.hr, "hr_")
    hb = api(args.hb_core, args.hb_subset, "hb_")
    hi, bi = hr["subset_input_create_or_fail"](), hb["subset_input_create_or_fail"]()
    for selector in range(8):
        assert set_ranges(hr, hr["subset_input_set"](hi, selector)) == set_ranges(hb, hb["subset_input_set"](bi, selector)), selector
    assert hr["subset_input_set"](hi, 99) is None
    assert hr["subset_input_set"](None, 0) is None
    assert hr["subset_input_set"](hi, 0) == hr["subset_input_glyph_set"](hi)
    assert hr["subset_input_set"](hi, 1) == hr["subset_input_unicode_set"](hi)
    hr["subset_input_keep_everything"](hi)
    hb["subset_input_keep_everything"](bi)
    for selector in range(8):
        assert set_ranges(hr, hr["subset_input_set"](hi, selector)) == set_ranges(hb, hb["subset_input_set"](bi, selector)), selector
    assert hr["subset_input_get_flags"](hi) == hb["subset_input_get_flags"](bi)
    hr["subset_input_destroy"](hi)
    hb["subset_input_destroy"](bi)

    fixture = ROOT / "harfrust/tests/fonts/rb_custom/PT_Sans-Caption-Web-Regular.ttf"
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "controls.ttf"
        font = TTFont(fixture)
        table = newTable("TEST")
        table.data = b"opaque test table"
        font["TEST"] = table
        font.save(path)
        cases = [
            lambda a, i: a["subset_input_set_flags"](i, 2 | 8),
            lambda a, i: a["subset_input_set_flags"](i, 2 | 16),
            lambda a, i: a["subset_input_set_flags"](i, 2 | 32),
            lambda a, i: a["subset_input_set_flags"](i, 2 | 128),
            lambda a, i: a["subset_input_set_flags"](i, 2 | 256),
            lambda a, i: a["subset_input_set_flags"](i, 2 | 512),
            lambda a, i: a["subset_input_set_flags"](i, 2 | 2048),
            lambda a, i: set_values(a, i, 3, [tag("GSUB")]),
            lambda a, i: set_values(a, i, 4, [1, 2]),
            lambda a, i: set_values(a, i, 5, []),
            lambda a, i: set_values(a, i, 6, [tag("liga")]),
            lambda a, i: set_values(a, i, 6, []),
            lambda a, i: set_values(a, i, 7, [tag("latn")]),
            lambda a, i: set_values(a, i, 7, []),
            lambda a, i: a["subset_input_keep_everything"](i),
        ]
        for index, configure in enumerate(cases):
            actual, expected = run(hr, path, configure), run(hb, path, configure)
            assert normalized(actual) == normalized(expected), (index, normalized(actual), normalized(expected))
            if index == 1:
                for name in actual.getGlyphOrder():
                    glyph = actual["glyf"][name]
                    if glyph.numberOfContours > 0:
                        assert glyph.flags[0] & 0x40
            if index == 2:
                assert actual.getTableData("TEST") == b"opaque test table"
            if index == 3:
                assert actual["post"].formatType == expected["post"].formatType
                assert actual.getGlyphOrder() == expected.getGlyphOrder()
            if index == 4:
                for field in ["ulUnicodeRange1", "ulUnicodeRange2", "ulUnicodeRange3", "ulUnicodeRange4"]:
                    assert getattr(actual["OS/2"], field) == getattr(expected["OS/2"], field)
            if index == 14:
                assert actual["maxp"].numGlyphs == font["maxp"].numGlyphs
                assert actual.getTableData("TEST") == font.getTableData("TEST")

        source = new_face(hr, path)
        processed = hr["subset_preprocess"](source)
        hr["face_destroy"](source)
        assert processed
        copied = TTFont(io.BytesIO(font_bytes(hr, processed)))
        serialized = TTFont(path)
        for record in serialized.reader.tables:
            if record != "head":
                assert copied.reader[record] == serialized.reader[record], record
        hr["face_destroy"](processed)
        assert not hr["subset_preprocess"](None)
    print(f"{len(cases)} differential input/flag cases, defaults, keep-everything and preprocessing passed")


if __name__ == "__main__":
    main()
