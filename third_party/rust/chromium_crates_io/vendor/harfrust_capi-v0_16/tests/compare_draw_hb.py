"""Compare outline callbacks with HarfBuzz, including state and cubic fallback.

Usage: python3 compare_draw_hb.py HR_LIBRARY HB_LIBRARY [EXTRA_FONT ...]
Extra fonts allow testing local VARC fixtures without copying their licenses.
"""

import ctypes as c
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
P, U, F = c.c_void_p, c.c_uint, c.c_float


class State(c.Structure):
    _fields_ = [("open", c.c_int), ("start_x", F), ("start_y", F),
                ("x", F), ("y", F), ("reserved", U * 7)]


def bind(path, prefix):
    lib = c.CDLL(str(Path(path).resolve()))

    def fn(name, result, *args):
        func = getattr(lib, prefix + name)
        func.restype, func.argtypes = result, args
        return func

    return {
        "blob": fn("blob_create_from_file", P, c.c_char_p),
        "drop_blob": fn("blob_destroy", None, P),
        "face": fn("face_create", P, P, U),
        "drop_face": fn("face_destroy", None, P),
        "count": fn("face_get_glyph_count", U, P),
        "upem": fn("face_get_upem", U, P),
        "font": fn("font_create", P, P),
        "drop_font": fn("font_destroy", None, P),
        "scale": fn("font_set_scale", None, P, c.c_int, c.c_int),
        "coords": fn("font_set_var_coords_normalized", None, P, c.POINTER(c.c_int), U),
        "draw": fn("font_draw_glyph_or_fail", c.c_int, P, U, P, P),
        "funcs": fn("draw_funcs_create", P),
        "drop_funcs": fn("draw_funcs_destroy", None, P),
        "set": {name: fn("draw_funcs_set_" + name + "_func", None, P, P, P, P)
                for name in ("move_to", "line_to", "quadratic_to", "cubic_to", "close_path")},
    }


def record(api, font, glyph, cubic_only):
    funcs, events, keep = api["funcs"](), [], []
    for name, argc in (("move_to", 2), ("line_to", 2), ("quadratic_to", 4),
                       ("cubic_to", 6), ("close_path", 0)):
        if cubic_only and name == "quadratic_to":
            continue

        def callback(_funcs, _data, state, *args, name=name):
            s = state.contents
            # CFF's native HarfBuzz interpreter leaves the pre-closing point
            # in close state; glyf and Skrifa use the contour start instead.
            current = (s.start_x, s.start_y) if name == "close_path" else (s.x, s.y)
            events.append((name, s.open, s.start_x, s.start_y, *current, *args[:-1]))

        cb = c.CFUNCTYPE(None, P, P, c.POINTER(State), *([F] * argc), P)(callback)
        keep.append(cb)
        api["set"][name](funcs, c.cast(cb, P), None, None)
    ok = api["draw"](font, glyph, funcs, None)
    api["drop_funcs"](funcs)
    # Native HarfBuzz and Skrifa can emit different numbers of zero-length
    # closing lines. They have the same geometry and do not change state.
    events = [e for e in events if not (e[0] == "line_to" and e[4:6] == e[6:8])]
    # CFF seac interpreters can visit base and accent in different order.
    contours, contour = [], []
    for event in events:
        contour.append(event)
        if event[0] == "close_path":
            contours.append(contour)
            contour = []
    assert not contour, "unclosed contour"
    events = [event for contour in sorted(contours) for event in contour]
    return bool(ok), events


def compare(a, b, context):
    assert a[0] == b[0], (context, "success", a[0], b[0])
    assert len(a[1]) == len(b[1]), (context, "events", len(a[1]), len(b[1]))
    for left, right in zip(a[1], b[1]):
        assert left[:2] == right[:2], (context, left, right)
        assert all(math.isclose(x, y, abs_tol=0.04 if "varc" in context[0] else 0.004, rel_tol=2e-6)
                   for x, y in zip(left[2:], right[2:])), (context, left, right)


def main():
    apis = [bind(sys.argv[1], "hr_"), bind(sys.argv[2], "hb_")]
    paths = [ROOT / "harfrust/tests/fonts" / name for name in (
        "rb_custom/LaBelleAurore.ttf", "rb_custom/PT_Sans-Caption-Web-Regular.ttf",
        "rb_custom/Linefont.ttf", "text-rendering-tests/TestCFFThree.otf",
        "text-rendering-tests/AdobeVFPrototype-Subset.otf")]
    paths.extend(map(Path, sys.argv[3:]))
    probes = 0
    for path in paths:
        objects = []
        for api in apis:
            blob = api["blob"](str(path).encode())
            face = api["face"](blob, 0)
            font = api["font"](face)
            objects.append((blob, face, font))
        count = apis[0]["count"](objects[0][1])
        assert count > 0, path
        upem = apis[0]["upem"](objects[0][1])
        for coords in ([], [4096, -8192], [16384, 16384]):
            coord_array = (c.c_int * len(coords))(*coords)
            for scale in ((upem, upem), (upem * 2, -upem // 2), (0, upem)):
                for api, (_, _, font) in zip(apis, objects):
                    api["scale"](font, *scale)
                    api["coords"](font, coord_array, len(coords))
                for glyph in list(range(min(count, 40))) + [count - 1, count, 0xFFFFFFFF]:
                    for cubic_only in (False, True):
                        traces = [record(api, obj[2], glyph, cubic_only)
                                  for api, obj in zip(apis, objects)]
                        compare(*traces, (path.name, glyph, coords, scale, cubic_only))
                        probes += 1
        for api, (blob, face, font) in zip(apis, objects):
            api["drop_font"](font)
            api["drop_face"](face)
            api["drop_blob"](blob)
    print(f"{probes} outline/state comparisons passed")


if __name__ == "__main__":
    main()
