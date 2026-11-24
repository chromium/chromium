"""Run Skia's real shaper and PDF subsetter against HarfBuzz and HarfRust.

Requires a matching Linux GN static Skia build with FreeType, ICU, HarfBuzz,
and PDF enabled, plus fontTools and Poppler's pdffonts/pdftotext/pdftoppm. The Skia
checkout must include removal of hb_face_set_upem (Skia change 1387436).
This script builds consumers in its output directory without modifying Skia.
"""

import argparse
from collections import Counter
import math
import re
import shlex
import subprocess
import unicodedata
from pathlib import Path

from fontTools.ttLib import TTFont

from compare_skia_hb import compare

ROOT = Path(__file__).resolve().parents[3]


def run(command):
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        print(result.stderr, end="")
        result.check_returncode()
    return result.stdout


def build(args, backend):
    includes = [args.hb_build / "src", args.hb_source / "src"]
    if backend == "hr":
        includes = [args.output / "include", ROOT / "harfrust_capi/include"]
    shaper = args.skia / "modules/skshaper/src/SkShaper_harfbuzz.cpp"
    if backend == "hb-upem":
        shaper = args.hb_upem_shaper
    objects = []
    for name, source, ninja in [
        ("shaper", shaper,
         args.skia_build / "obj/modules/skshaper/skshaper.ninja"),
        ("pdf", args.skia / "src/pdf/SkPDFSubsetFont.cpp", args.skia_build / "obj/pdf.ninja"),
        ("consumer", Path(__file__).with_name("skia_runtime.cc"),
         args.skia_build / "obj/modules/skshaper/skshaper.ninja"),
    ]:
        defines = next(line.split("=", 1)[1] for line in ninja.read_text().splitlines()
                       if line.startswith("defines ="))
        obj = args.output / f"{backend}-{name}.o"
        run([args.compiler, "-std=c++20", "-O1", "-fno-rtti", "-fno-exceptions",
             "-Wno-attributes", *shlex.split(defines),
             *["-I" + str(p) for p in includes], "-I" + str(args.skia),
             "-c", str(source), "-o", str(obj)])
        objects.append(str(obj))
    if backend != "hr":
        libraries = [str(args.hb_build / "src/libharfbuzz-subset.so"),
                     str(args.hb_build / "src/libharfbuzz.so")]
    else:
        libraries = [str(args.hr)]
    rpaths = sorted({str(Path(lib).parent) for lib in libraries})
    executable = args.output / ("skia-" + backend)
    run([args.compiler, "-Wl,--gc-sections", "-Wl,--start-group", *objects,
         *[str(args.skia_build / name) for name in ["libskia.a", "libskshaper.a",
                                                  "libskunicode_core.a", "libskunicode_icu.a"]],
         "-Wl,--end-group", *libraries, *["-Wl,-rpath," + p for p in rpaths],
         "-lpthread", "-lfontconfig", "-ldl", "-lfreetype", "-licuuc", "-licui18n",
         "-lz", "-o", str(executable)])
    print(f"Built actual Skia {backend} consumer", flush=True)
    return executable


def compare_shaping(actual, expected, variable=False):
    actual, expected = actual.splitlines(), expected.splitlines()
    assert actual and len(actual) == len(expected)
    tolerance = 1e-5
    for a, b in zip(actual, expected):
        a, b = a.split(), b.split()
        assert len(a) == len(b)
        integer_fields = 4 if a[0] == "run" else 3
        if a[0] == "run":
            # At wdth=150/wght=800, fractional GPOS variation deltas differ
            # by a 16.16 position unit. Bound accumulated drift to one unit
            # per glyph, including the largest horizontal scale tested.
            tolerance = int(a[3]) * 1.75 / 65536 + 1e-5 if variable else 1e-5
        assert a[:integer_fields] == b[:integer_fields], (a, b)
        for av, bv in zip(a[integer_fields:], b[integer_fields:]):
            assert math.isclose(float(av), float(bv), rel_tol=0, abs_tol=tolerance), (a, b)


def font_result(path):
    font = TTFont(path)
    names = [font.getBestCmap()[ord(c)] for c in "ac"]
    return font, [font.getGlyphID(name) for name in names], [font["hmtx"][name][0] for name in names]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["skia", "skia-build", "hb-source", "hb-build", "hr", "output"]:
        parser.add_argument("--" + name, type=lambda p: Path(p).resolve(), required=True)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--allow-variable-pixel-differences", action="store_true",
                        help="Acknowledge the known fractional GPOS variation raster difference")
    parser.add_argument("--hb-upem-shaper", type=lambda p: Path(p).resolve(),
                        help="Optional original Skia shaper source to validate UPEM removal")
    args = parser.parse_args()
    shaper_source = args.skia / "modules/skshaper/src/SkShaper_harfbuzz.cpp"
    assert "hb_face_set_upem(" not in shaper_source.read_text(), "Apply Skia change 1387436 first"
    args.output.mkdir(parents=True, exist_ok=True)
    include = args.output / "include"
    include.mkdir(exist_ok=True)
    for name, target in [("hb.h", "hr-hb.h"), ("hb-ot.h", "hr-hb-ot.h"),
                         ("hb-subset.h", "hr-hb-subset.h")]:
        (include / name).write_text('#include "' + target + '"\n')
    backends = ["hb", "hr"]
    if args.hb_upem_shaper:
        assert "hb_face_set_upem(" in args.hb_upem_shaper.read_text()
        backends.append("hb-upem")
    executables = {backend: build(args, backend) for backend in backends}
    fixtures = ROOT / "harfrust/tests/fonts/rb_custom"
    cases = 0
    pixel_differences = []
    for name in ["PT_Sans-Caption-Web-Regular.ttf", "LaBelleAurore.ttf", "Linefont.ttf"]:
        path = fixtures / name
        for mode in ["stream", "callback"]:
            for instance in (["default", "variable"] if name == "Linefont.ttf" else ["default"]):
                label = f"{path.stem}-{mode}-{instance}"
                outputs = {}
                for backend, executable in executables.items():
                    prefix = args.output / f"{label}-{backend}"
                    outputs[backend] = (prefix, run([str(executable), str(path), str(prefix), mode, instance]))
                compare_shaping(outputs["hr"][1], outputs["hb"][1], instance == "variable")
                if "hb-upem" in outputs:
                    compare_shaping(outputs["hb"][1], outputs["hb-upem"][1])
                for suffix, flags in [(".ttf", 2), (".zero.ttf", 66)]:
                    compare(font_result(str(outputs["hr"][0]) + suffix),
                            font_result(str(outputs["hb"][0]) + suffix), TTFont(path), flags)
                extracted, embeddings, images = [], [], {}
                for backend, (prefix, _) in outputs.items():
                    pdf = str(prefix) + ".pdf"
                    fonts = run(["pdffonts", pdf]).splitlines()[2:]
                    assert fonts
                    statuses = [re.search(r"(yes|no)\s+(yes|no)\s+(yes|no)\s+\d+\s+\d+$", row)
                                for row in fonts]
                    assert all(statuses), fonts
                    expected = ("yes", "yes", "yes")
                    if name == "Linefont.ttf":
                        assert all("Type 3" in row for row in fonts), fonts
                    assert all(status.groups() == expected for status in statuses), fonts
                    embeddings.append([status.groups() for status in statuses])
                    extracted.append(run(["pdftotext", pdf, "-"]))
                    image = str(prefix) + "-render"
                    run(["pdftoppm", "-singlefile", "-r", "144", pdf, image])
                    images[backend] = Path(image + ".ppm").read_bytes()
                assert all(text == extracted[0] for text in extracted), extracted
                text = unicodedata.normalize("NFC", "".join(c for c in extracted[0] if not c.isspace()))
                expected_text = unicodedata.normalize("NFC", "officecaf\u00e9a\u0301ac")
                assert Counter(text) == Counter(expected_text), extracted
                assert all(status == embeddings[0] for status in embeddings), embeddings
                if "hb-upem" in images:
                    assert images["hb-upem"] == images["hb"], "UPEM removal changed pixels: " + label
                if images["hr"] != images["hb"]:
                    assert instance == "variable", "Unexpected raster difference: " + label
                    hb_pixels = images["hb"].split(b"\n", 3)[3]
                    hr_pixels = images["hr"].split(b"\n", 3)[3]
                    assert len(hb_pixels) == len(hr_pixels)
                    changed = sum(hb_pixels[i:i + 3] != hr_pixels[i:i + 3]
                                  for i in range(0, len(hb_pixels), 3))
                    pixel_differences.append(label)
                    print(f"Known variable GPOS raster difference: {label} ({changed} pixels)", flush=True)
                cases += 1
                print(f"Checked shaping, glyph-0 subsets and PDF: {label}", flush=True)
    print(f"{cases} actual Skia cases checked (four shaping sizes/scales, two subsets and a PDF each)")
    print(f"HarfRust PDF pixel mismatches: {len(pixel_differences)}")
    assert not pixel_differences or args.allow_variable_pixel_differences, pixel_differences


if __name__ == "__main__":
    main()
