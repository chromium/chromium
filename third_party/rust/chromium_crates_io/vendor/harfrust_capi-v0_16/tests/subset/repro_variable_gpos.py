"""Print the fractional GPOS advance mismatch without Skia or subsetting.

Usage: python3 repro_variable_gpos.py HR_CORE HB_CORE
"""
import ctypes as c
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
P, U, I = c.c_void_p, c.c_uint, c.c_int
class Var(c.Structure):
    _fields_ = [('tag', U), ('value', c.c_float)]
class Pos(c.Structure):
    _fields_ = [('x_advance', I), ('y_advance', I), ('x_offset', I), ('y_offset', I), ('private', U)]
ADV = c.CFUNCTYPE(I, P, P, U, P)
@ADV
def zero(*_): return 0
for path, prefix in [(sys.argv[1], 'hr_'), (sys.argv[2], 'hb_')]:
    lib = c.CDLL(path)
    def call(name, result, args, *values):
        fn = getattr(lib, prefix + name)
        fn.restype, fn.argtypes = result, args
        return fn(*values)
    blob = call('blob_create_from_file', P, [c.c_char_p], str(ROOT / 'harfrust/tests/fonts/rb_custom/Linefont.ttf').encode())
    face = call('face_create', P, [P, U], blob, 0)
    parent = call('font_create', P, [P], face)
    settings = (Var * 2)(Var(int.from_bytes(b'wdth', 'big'), 150), Var(int.from_bytes(b'wght', 'big'), 800))
    call('font_set_variations', None, [P, c.POINTER(Var), U], parent, settings, 2)
    font = call('font_create_sub_font', P, [P], parent)
    funcs = call('font_funcs_create', P, [])
    call('font_funcs_set_glyph_h_advance_func', None, [P, ADV, P, P], funcs, zero, None, None)
    call('font_set_funcs', None, [P, P, P, P], font, funcs, None, None)
    coords_n = U()
    coords = call('font_get_var_coords_normalized', c.POINTER(I), [P, c.POINTER(U)], font, c.byref(coords_n))
    print(prefix, 'coords', list(coords[:coords_n.value]))
    for size in [12, 36]:
        call('font_set_scale', None, [P, I, I], font, size * 65536, size * 65536)
        buf = call('buffer_create', P, [])
        call('buffer_add_utf8', None, [P, c.c_char_p, I, U, I], buf, b'office', 6, 0, 6)
        call('buffer_guess_segment_properties', None, [P], buf)
        call('shape', None, [P, P, P, U], font, buf, None, 0)
        length = U()
        positions = call('buffer_get_glyph_positions', c.POINTER(Pos), [P, c.POINTER(U)], buf, c.byref(length))
        print(size, [(p.x_advance, p.x_offset, p.y_offset) for p in positions[:length.value]])
        call('buffer_destroy', None, [P], buf)
    for name, obj in [('font_funcs', funcs), ('font', font), ('font', parent), ('face', face), ('blob', blob)]:
        call(name + '_destroy', None, [P], obj)
