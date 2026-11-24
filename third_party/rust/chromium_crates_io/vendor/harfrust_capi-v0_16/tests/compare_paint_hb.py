"""Compare semantic paint callbacks against HarfBuzz.

Usage: python3 compare_paint_hb.py HR_LIBRARY HB_LIBRARY
Transforms around glyph clips may differ; compare their accumulated effect.
Gradient geometry and stops are normalized to equivalent representations.
"""
import ctypes as c
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
P, U, F, I = c.c_void_p, c.c_uint, c.c_float, c.c_int
IDENTITY = (1, 0, 0, 1, 0, 0)


class Stop(c.Structure):
    _fields_ = [("offset", F), ("foreground", I), ("color", U)]


class Extents(c.Structure):
    _fields_ = [(name, I) for name in ("x", "y", "width", "height")]


def multiply(a, b):
    xx, yx, xy, yy, dx, dy = a
    u, v, w, z, e, f = b
    return (xx*u + xy*v, yx*u + yy*v, xx*w + xy*z, yx*w + yy*z,
            xx*e + xy*f + dx, yx*e + yy*f + dy)


def bind(path, prefix):
    lib = c.CDLL(str(Path(path).resolve()))

    def fn(name, result, *args):
        func = getattr(lib, prefix + name)
        func.restype, func.argtypes = result, args
        return func

    return {
        "fn": fn,
        "blob": fn("blob_create_from_file", P, c.c_char_p),
        "drop_blob": fn("blob_destroy", None, P),
        "face": fn("face_create", P, P, U),
        "drop_face": fn("face_destroy", None, P),
        "count": fn("face_get_glyph_count", U, P),
        "font": fn("font_create", P, P),
        "drop_font": fn("font_destroy", None, P),
        "scale": fn("font_set_scale", None, P, I, I),
        "paint": fn("font_paint_glyph_or_fail", I, P, U, P, P, U, U),
        "funcs": fn("paint_funcs_create", P),
        "drop_funcs": fn("paint_funcs_destroy", None, P),
        "stops": fn("color_line_get_color_stops", U, P, U, c.POINTER(U), c.POINTER(Stop)),
        "extend": fn("color_line_get_extend", U, P),
        "bytes": fn("blob_get_data", P, P, c.POINTER(U)),
    }


def record(api, font, glyph, palette):
    funcs, events, keep, errors = api["funcs"](), [], [], []
    transforms, clips, scopes = [IDENTITY], [], []

    def register(name, result, args, callback):
        # ctypes cannot propagate Python exceptions through a C callback.
        def checked(_f, _d, *values):
            try:
                return callback(*values[:-1])
            except Exception as error:
                errors.append(error)
                return 0 if result else None

        cb = c.CFUNCTYPE(result, P, P, *args, P)(checked)
        keep.append(cb)
        api["fn"]("paint_funcs_set_" + name + "_func", None, P, P, P, P)(
            funcs, c.cast(cb, P), None, None)

    def push_transform(*matrix):
        transforms.append(multiply(transforms[-1], matrix))
        scopes.append("transform")

    def pop_transform():
        assert scopes.pop() == "transform"
        transforms.pop()

    def clip_glyph(glyph, _font):
        clips.append(("glyph", glyph, transforms[-1]))
        scopes.append("clip")

    def clip_rectangle(*box):
        clips.append(("rectangle", box, transforms[-1]))
        scopes.append("clip")

    def pop_clip():
        assert scopes.pop() == "clip"
        clips.pop()

    def gradient(kind, line, *geometry):
        count = U(api["stops"](line, 0, None, None))
        stops = (Stop * count.value)()
        api["stops"](line, 0, c.byref(count), stops)
        stops = [(s.offset, s.foreground, s.color) for s in stops]
        geometry = list(geometry)
        if kind == "linear":
            x, y, u, v, w, z = geometry
            # Replace the three anchors with the projected gradient axis.
            nx, ny = z-y, x-w
            norm = nx*nx + ny*ny
            projection = ((u-x)*nx + (v-y)*ny) / norm if norm else 0
            geometry = [x, y, nx*projection, ny*projection]
        if kind == "sweep" and geometry[2] > geometry[3]:
            geometry[2], geometry[3] = geometry[3], geometry[2]
            stops = [(1-offset, foreground, color) for offset, foreground, color in reversed(stops)]
        events.append((kind, tuple(clips), transforms[-1], tuple(geometry),
                       api["extend"](line), tuple(stops)))

    def image(blob, width, height, fmt, slant, extents):
        count = U()
        data = api["bytes"](blob, c.byref(count))
        box = tuple(getattr(extents.contents, n) for n in ("x", "y", "width", "height")) if extents else None
        events.append(("image", width, height, fmt, slant, box, c.string_at(data, count.value)))
        return 1

    def push_group():
        scopes.append("group")
        events.append(("push_group",))

    def pop_group(mode):
        assert scopes.pop() == "group"
        events.append(("pop_group", mode))

    register("push_transform", None, [F]*6, push_transform)
    register("pop_transform", None, [], pop_transform)
    register("push_clip_glyph", None, [U, P], clip_glyph)
    register("push_clip_rectangle", None, [F]*4, clip_rectangle)
    register("pop_clip", None, [], pop_clip)
    register("color", None, [I, U], lambda fg, color: events.append(("color", tuple(clips), fg, color)))
    register("linear_gradient", None, [P]+[F]*6, lambda *args: gradient("linear", *args))
    register("radial_gradient", None, [P]+[F]*6, lambda *args: gradient("radial", *args))
    register("sweep_gradient", None, [P]+[F]*4, lambda *args: gradient("sweep", *args))
    register("image", I, [P, U, U, U, F, c.POINTER(Extents)], image)
    register("push_group", None, [], push_group)
    register("pop_group", None, [U], pop_group)
    ok = api["paint"](font, glyph, funcs, None, palette, 0x112233ff)
    api["drop_funcs"](funcs)
    assert not errors, errors
    assert not scopes and not clips and transforms == [IDENTITY], (glyph, scopes)
    return bool(ok), events


def equal(a, b):
    if isinstance(a, (list, tuple)):
        if len(a) == 7 and a[0] == "image" and isinstance(b, (list, tuple)) and b and b[0] == "image":
            # Bitmap half-unit rounding differs between the native reference
            # and the Skrifa adapter, by at most one font unit before scaling.
            boxes = a[5] == b[5] if a[5] is None or b[5] is None else all(abs(x-y) <= 2 for x, y in zip(a[5], b[5]))
            return equal(a[:5], b[:5]) and boxes and a[6] == b[6]
        return isinstance(b, (list, tuple)) and len(a) == len(b) and all(equal(x, y) for x, y in zip(a, b))
    if isinstance(a, float) or isinstance(b, float):
        return math.isclose(a, b, abs_tol=0.02, rel_tol=2e-5)
    return a == b


def main():
    apis = [bind(sys.argv[1], "hr_"), bind(sys.argv[2], "hb_")]
    paths = [ROOT / "harfrust_capi/tests/fonts/Rendering.ttf"]
    probes = 0
    for path in paths:
        objects = []
        for api in apis:
            blob = api["blob"](str(path).encode())
            face = api["face"](blob, 0)
            font = api["font"](face)
            objects.append((blob, face, font))
        count = apis[0]["count"](objects[0][1])
        for scale in ((1000, 1000), (2000, -500)):
            for palette in (0, 1, 99):
                for api, obj in zip(apis, objects):
                    api["scale"](obj[2], *scale)
                for glyph in range(count):
                    if glyph == 4:
                        # HarfBuzz reports success for this cyclic graph;
                        # Skrifa reports failure. Error cleanup is tested in Rust.
                        continue
                    traces = [record(api, obj[2], glyph, palette) for api, obj in zip(apis, objects)]
                    assert equal(*traces), (path.name, glyph, scale, palette, traces)
                    probes += 1
        for api, (blob, face, font) in zip(apis, objects):
            api["drop_font"](font)
            api["drop_face"](face)
            api["drop_blob"](blob)
    print(f"{probes} paint comparisons passed")


if __name__ == "__main__":
    main()
