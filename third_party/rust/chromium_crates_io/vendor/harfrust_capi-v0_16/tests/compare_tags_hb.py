"""Compare OpenType tag conversion against HarfBuzz.

Usage: python harfrust_capi/tests/compare_tags_hb.py HR_LIBRARY HB_LIBRARY

The checked-in forward language table contains newer mappings than the local
HarfBuzz reference DLL, so forward probes use representative languages and
all exposed script constants. Reverse probes cover every known tag.
"""

import ctypes as c
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def bind(path, prefix):
    library = c.CDLL(str(Path(path).resolve()))

    def fn(name, result, *args):
        function = getattr(library, prefix + name)
        function.restype = result
        function.argtypes = args
        return function

    uint_ptr = c.POINTER(c.c_uint)
    return {
        "from_string": fn("language_from_string", c.c_void_p, c.c_char_p, c.c_int),
        "to_string": fn("language_to_string", c.c_char_p, c.c_void_p),
        "to_language": fn("ot_tag_to_language", c.c_void_p, c.c_uint),
        "from_script_language": fn("ot_tags_from_script_and_language", None,
                                   c.c_uint, c.c_void_p, uint_ptr, uint_ptr,
                                   uint_ptr, uint_ptr),
    }


def language_from_tag(api, tag):
    language = api["to_language"](tag)
    return api["to_string"](language) if language else None


def tags_from_script_language(api, script, language, script_capacity, language_capacity,
                              script_output=True, language_output=True):
    language = api["from_string"](language, -1) if language else None
    script_count = c.c_uint(script_capacity)
    language_count = c.c_uint(language_capacity)
    scripts = (c.c_uint * max(script_capacity, 1))()
    languages = (c.c_uint * max(language_capacity, 1))()
    api["from_script_language"](
        script, language, c.byref(script_count), scripts if script_output else None,
        c.byref(language_count), languages if language_output else None)
    return (script_count.value, tuple(scripts[:script_count.value]) if script_output else (),
            language_count.value, tuple(languages[:language_count.value]) if language_output else ())


def main():
    hr, hb = bind(sys.argv[1], "hr_"), bind(sys.argv[2], "hb_")
    source = (ROOT / "harfrust/src/tag_table.rs").read_text(encoding="utf-8")
    known = set(re.findall(r'tag: Tag::new\(b"(.{4})"\)', source))
    source = (ROOT / "harfrust/src/tag_reverse.rs").read_text(encoding="utf-8")
    known.update(re.findall(r'\*b"(.{4})"', source))
    known.update(("dflt", "ABCD", "abc ", "1234", "    "))
    reverse_count = 0
    for value in sorted(known):
        tag = int.from_bytes(value.encode("ascii"), "big")
        ours, reference = language_from_tag(hr, tag), language_from_tag(hb, tag)
        if ours != reference:
            raise AssertionError(("to language", value, ours, reference))
        reverse_count += 1

    scripts = (0, *[int.from_bytes(value, "big") for value in
                    (b"Latn", b"Deva", b"Beng", b"Mymr", b"Hira", b"Hrkt", b"Zzzz")])
    languages = (None, b"en", b"fr", b"ar", b"zh-Hans", b"zh-Hant", b"zh-HK",
                 b"ga-Latg", b"und-fonipa", b"x-hbot-41424344", b"x-hbsc-6d796d32",
                 b"en-x-hbot-41424344", b"en-x-hbsc-64657661", b"abc")
    forward_count = 0
    for script in scripts:
        for language in languages:
            for script_capacity, language_capacity, script_output, language_output in (
                (0, 0, True, True), (1, 1, True, True), (2, 2, True, True),
                (3, 3, True, True), (4, 4, True, True),
                (3, 3, False, True), (3, 3, True, False),
            ):
                args = (script, language, script_capacity, language_capacity,
                        script_output, language_output)
                ours = tags_from_script_language(hr, *args)
                reference = tags_from_script_language(hb, *args)
                if ours != reference:
                    raise AssertionError(("from script/language", args, ours, reference))
                forward_count += 1
    source = (ROOT / "harfrust_capi/src/common.rs").read_text(encoding="utf-8")
    for value in set(re.findall(r'HR_SCRIPT_[A-Z0-9_]+: hr_script_t = 0x([0-9A-Fa-f_]+)',
                                source)):
        args = (int(value.replace("_", ""), 16), None, 3, 0)
        ours = tags_from_script_language(hr, *args)
        reference = tags_from_script_language(hb, *args)
        if ours != reference:
            raise AssertionError(("from script constant", args, ours, reference))
        forward_count += 1
    print(f"{reverse_count} reverse and {forward_count} forward queries match HarfBuzz")


if __name__ == "__main__":
    main()
