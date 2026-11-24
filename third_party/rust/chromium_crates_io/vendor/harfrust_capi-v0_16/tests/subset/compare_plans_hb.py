"""Compare subset plans, snapshots, maps, and table passthrough with HarfBuzz."""
import argparse
import ctypes as c
import io
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from fontTools.ttLib import TTFont, newTable
from compare_skia_hb import ROOT, PTR, UINT, OUT, new_face, font_bytes, glyph
from compare_controls_hb import api as control_api, tag, set_values


DESTROY = c.CFUNCTYPE(None, PTR)


def bind(core, subset, prefix):
    result = control_api(core, subset, prefix)
    library = c.CDLL(str(subset.resolve()))
    for name, return_type, arguments in [
        ("subset_plan_create_or_fail", PTR, [PTR, PTR]),
        ("subset_plan_execute_or_fail", PTR, [PTR]),
        ("subset_plan_reference", PTR, [PTR]), ("subset_plan_destroy", None, [PTR]),
        ("subset_plan_old_to_new_glyph_mapping", PTR, [PTR]),
        ("subset_plan_new_to_old_glyph_mapping", PTR, [PTR]),
        ("subset_plan_unicode_to_old_glyph_mapping", PTR, [PTR]),
    ]:
        f = getattr(library, prefix + name)
        f.restype, f.argtypes = return_type, arguments
        result[name] = f
    for kind in ["input", "plan"]:
        name = "subset_" + kind + "_set_user_data"
        f = getattr(library, prefix + name)
        f.restype, f.argtypes = c.c_int, [PTR, PTR, PTR, DESTROY, c.c_int]
        result[name] = f
        name = "subset_" + kind + "_get_user_data"
        f = getattr(library, prefix + name)
        f.restype, f.argtypes = PTR, [PTR, PTR]
        result[name] = f
    library = c.CDLL(str(core.resolve()))
    for name, return_type, arguments in [
        ("map_reference", PTR, [PTR]), ("map_destroy", None, [PTR]),
        ("map_next", c.c_int, [PTR, c.POINTER(c.c_int), OUT, OUT]),
    ]:
        f = getattr(library, prefix + name)
        f.restype, f.argtypes = return_type, arguments
        result[name] = f
    return result


def items(api, mapping):
    index, key, value = c.c_int(-1), UINT(), UINT()
    result = {}
    while api["map_next"](mapping, c.byref(index), c.byref(key), c.byref(value)):
        result[key.value] = value.value
    return result


def run(api, path, flags, mode):
    source = new_face(api, path)
    font = api["font_create"](source)
    gids = [glyph(api, font, char) for char in "fia("]
    api["font_destroy"](font)
    input_obj = api["subset_input_create_or_fail"]()
    set_values(api, input_obj, 0 if mode == "glyphs" else 1,
               gids if mode == "glyphs" else map(ord, "fia("))
    api["subset_input_set_flags"](input_obj, flags)
    if mode == "no_layout":
        set_values(api, input_obj, 6, [])
    if mode == "all":
        api["subset_input_keep_everything"](input_obj)
    api["face_set_index"](source, 1234)
    plan = api["subset_plan_create_or_fail"](source, input_obj)
    assert plan
    assert api["subset_plan_reference"](plan) == plan
    api["subset_plan_destroy"](plan)
    maps = [api[name](plan) for name in ["subset_plan_old_to_new_glyph_mapping",
            "subset_plan_new_to_old_glyph_mapping", "subset_plan_unicode_to_old_glyph_mapping"]]
    mappings = [items(api, mapping) for mapping in maps]
    assert {new: old for old, new in mappings[0].items()} == mappings[1]
    if flags & 2 and mode != "all":
        assert all(old == new for old, new in mappings[0].items())
    # Planning must snapshot input sets and table bytes, not borrow the input.
    for selector in range(8):
        api["set_clear"](api["subset_input_set"](input_obj, selector))
    api["subset_input_set_flags"](input_obj, 0xFFFFFFFF)
    api["subset_input_destroy"](input_obj)
    api["face_destroy"](source)
    with ThreadPoolExecutor(max_workers=2) as pool:
        copies = list(pool.map(lambda _: api["subset_plan_execute_or_fail"](plan), range(2)))
    assert all(copies)
    borrowed = api["map_reference"](maps[0])
    api["subset_plan_destroy"](plan)
    assert items(api, borrowed) == mappings[0]
    api["map_destroy"](borrowed)
    bytes_out = [font_bytes(api, face) for face in copies]
    assert bytes_out[0] == bytes_out[1]
    for face in copies:
        font = api["font_create"](face)
        for char in "fia(":
            old = mappings[2][ord(char)]
            assert glyph(api, font, char) == mappings[0][old]
        api["font_destroy"](font)
        api["face_destroy"](face)
    return mappings, TTFont(io.BytesIO(bytes_out[0]))["maxp"].numGlyphs


def metadata(api, path):
    for kind in ["input", "plan"]:
        source = new_face(api, path)
        input_obj = api["subset_input_create_or_fail"]()
        obj = input_obj if kind == "input" else api["subset_plan_create_or_fail"](source, input_obj)
        assert obj
        setter, getter = [api["subset_" + kind + suffix] for suffix in ["_set_user_data", "_get_user_data"]]
        key = c.c_int(0)
        key_ptr = c.cast(c.byref(key), PTR)
        drops, observations = [], []

        @DESTROY
        def destroy(value):
            drops.append(value)
            if value != 3:  # Reentry is tested during replacement/removal, not final destruction.
                observations.append((value, getter(obj, key_ptr)))

        assert getter(None, key_ptr) is None
        assert not setter(None, key_ptr, 99, destroy, 1)
        assert not setter(obj, None, 99, destroy, 1)
        assert setter(obj, key_ptr, 1, destroy, 0)
        assert getter(obj, key_ptr) == 1
        assert not setter(obj, key_ptr, 99, destroy, 0)
        assert not drops
        assert setter(obj, key_ptr, 2, destroy, 1)
        assert drops == [1]
        assert setter(obj, key_ptr, None, DESTROY(), 1)
        assert drops == [1, 2]
        assert setter(obj, key_ptr, 3, destroy, 0)
        api["subset_" + kind + "_reference"](obj)
        api["subset_" + kind + "_destroy"](obj)
        assert drops == [1, 2]
        api["subset_" + kind + "_destroy"](obj)
        assert drops == [1, 2, 3]
        assert observations == [(1, 2), (2, None)]
        if kind == "plan":
            api["subset_input_destroy"](input_obj)
        api["face_destroy"](source)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["hr", "hb_core", "hb_subset"]:
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    hr = bind(args.hr, args.hr, "hr_")
    hb = bind(args.hb_core, args.hb_subset, "hb_")
    cases = 0
    for name in ["PT_Sans-Caption-Web-Regular.ttf", "LaBelleAurore.ttf"]:
        path = ROOT / "harfrust/tests/fonts/rb_custom" / name
        for flags in [0, 2]:
            for mode in ["glyphs", "unicodes", "no_layout", "all"]:
                actual, expected = run(hr, path, flags, mode), run(hb, path, flags, mode)
                assert actual == expected, (name, flags, mode, actual, expected)
                cases += 1

    fixture = ROOT / "harfrust/tests/fonts/rb_custom/PT_Sans-Caption-Web-Regular.ttf"
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "passthrough.ttf"
        font = TTFont(fixture)
        table = newTable("TEST")
        table.data = b"opaque passthrough"
        font["TEST"] = table
        font.save(path)
        for api in [hr, hb]:
            source = new_face(api, path)
            input_obj = api["subset_input_create_or_fail"]()
            set_values(api, input_obj, 1, map(ord, "ac"))
            api["set_add"](api["subset_input_set"](input_obj, 2), tag("TEST"))
            result = api["subset_or_fail"](source, input_obj)
            assert result
            copied = TTFont(io.BytesIO(font_bytes(api, result)))
            assert copied.getTableData("TEST") == b"opaque passthrough"
            api["face_destroy"](result)
            # Drop-table rules override passthrough.
            api["set_add"](api["subset_input_set"](input_obj, 3), tag("TEST"))
            result = api["subset_or_fail"](source, input_obj)
            assert result and "TEST" not in TTFont(io.BytesIO(font_bytes(api, result)))
            api["face_destroy"](result)
            api["subset_input_destroy"](input_obj)
            api["face_destroy"](source)
    for api in [hr, hb]:
        metadata(api, fixture)
    assert not hr["subset_plan_create_or_fail"](None, None)
    assert not hr["subset_plan_execute_or_fail"](None)
    assert not hr["subset_plan_old_to_new_glyph_mapping"](None)
    hr["subset_plan_destroy"](None)
    print(f"{cases} differential plan/map cases, snapshot/lifetime/metadata checks and passthrough passed")


if __name__ == "__main__":
    main()
