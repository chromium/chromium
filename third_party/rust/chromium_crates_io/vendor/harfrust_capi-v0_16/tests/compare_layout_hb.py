"""Compare HarfRust layout glyph collection with HarfBuzz.

Usage: python harfrust_capi/tests/compare_layout_hb.py HR_LIBRARY HB_LIBRARY FONT [FONT ...]
The libraries must be built for the host Python architecture. This is an
optional differential test; it does not add a HarfBuzz build dependency.
"""

import argparse
import ctypes as c
from pathlib import Path

TAGS = {"GSUB": 0x47535542, "GPOS": 0x47504F53}
KINDS = ("before", "input", "after", "output")
INVALID = 0xFFFFFFFF


def bind(library, prefix):
    def function(name, result, *args):
        fn = getattr(library, prefix + name)
        fn.restype = result
        fn.argtypes = args
        return fn

    ptr = c.c_void_p
    uint = c.c_uint
    return {
        "blob_file": function("blob_create_from_file", ptr, c.c_char_p),
        "blob_destroy": function("blob_destroy", None, ptr),
        "face_create": function("face_create", ptr, ptr, uint),
        "face_destroy": function("face_destroy", None, ptr),
        "set_create": function("set_create", ptr),
        "set_destroy": function("set_destroy", None, ptr),
        "set_has": function("set_has", c.c_int, ptr, uint),
        "count": function("ot_layout_table_get_lookup_count", uint, ptr, uint),
        "collect": function("ot_layout_lookup_collect_glyphs", None, ptr, uint, uint, ptr, ptr, ptr, ptr),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("harfrust", type=Path)
    parser.add_argument("harfbuzz", type=Path)
    parser.add_argument("fonts", nargs="+", type=Path)
    args = parser.parse_args()
    hr = bind(c.CDLL(str(args.harfrust.resolve())), "hr_")
    hb_lib = c.CDLL(str(args.harfbuzz.resolve()))
    hb = bind(hb_lib, "hb_")
    hb_next = hb_lib.hb_set_next
    hb_next.restype = c.c_int
    hb_next.argtypes = [c.c_void_p, c.POINTER(c.c_uint)]
    mismatches = 0
    checked = 0

    for path in args.fonts:
        blobs = [api["blob_file"](str(path.resolve()).encode()) for api in (hr, hb)]
        faces = [api["face_create"](blob, 0) for api, blob in zip((hr, hb), blobs)]
        try:
            for name, tag in TAGS.items():
                counts = [api["count"](face, tag) for api, face in zip((hr, hb), faces)]
                if counts[0] != counts[1]:
                    print(f"{path.name} {name}: lookup counts differ: {counts}")
                    mismatches += 1
                for index in range(min(counts)):
                    sets = [[api["set_create"]() for _ in KINDS] for api in (hr, hb)]
                    try:
                        for api, face, row in zip((hr, hb), faces, sets):
                            api["collect"](face, tag, index, *row)
                        checked += 1
                        for kind, ours, reference in zip(KINDS, *sets):
                            missing = []
                            value = c.c_uint(INVALID)
                            while hb_next(reference, c.byref(value)):
                                if not hr["set_has"](ours, value.value):
                                    missing.append(value.value)
                                    if len(missing) == 5:
                                        break
                            extra = []
                            for glyph in range(65536):
                                if hr["set_has"](ours, glyph) and not hb["set_has"](reference, glyph):
                                    extra.append(glyph)
                                    if len(extra) == 5:
                                        break
                            if missing or extra:
                                print(f"{path.name} {name}[{index}] {kind}: missing={missing} extra={extra}")
                                mismatches += 1
                    finally:
                        for api, row in zip((hr, hb), sets):
                            for item in row:
                                api["set_destroy"](item)
        finally:
            for api, face, blob in zip((hr, hb), faces, blobs):
                api["face_destroy"](face)
                api["blob_destroy"](blob)

    print(f"Compared {checked} lookups; {mismatches} set mismatches")
    raise SystemExit(bool(mismatches))


if __name__ == "__main__":
    main()
