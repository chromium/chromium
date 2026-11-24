"""Differential checks for HarfRust's set and map C APIs.

Usage: python harfrust_capi/tests/compare_containers_hb.py HR_LIBRARY HB_LIBRARY
No HarfBuzz build dependency is added to the Rust tests. Hash values and map
iteration order are implementation dependent and are not compared. Older
HarfBuzz versions have bugs in mixed-inversion equality, self assignment, and
iteration across missing pages; those can cause differential failures.
"""

import argparse
import ctypes as c
from pathlib import Path
import random

INVALID = 0xFFFFFFFF
VALUES = [0, 1, 2, 255, 256, 511, 512, 513, 65535, 65536,
          INVALID - 3, INVALID - 2, INVALID - 1, INVALID]


def bind(path, prefix):
    library = c.CDLL(str(path.resolve()))
    ptr, uint, boolean = c.c_void_p, c.c_uint, c.c_int
    uout, iout = c.POINTER(uint), c.POINTER(c.c_int)
    signatures = {
        "set_create": (ptr,), "set_destroy": (None, ptr),
        "set_copy": (ptr, ptr), "set_clear": (None, ptr),
        "set_add": (None, ptr, uint), "set_del": (None, ptr, uint),
        "set_add_range": (None, ptr, uint, uint),
        "set_del_range": (None, ptr, uint, uint),
        "set_add_sorted_array": (None, ptr, uout, uint),
        "set_invert": (None, ptr), "set_is_inverted": (boolean, ptr),
        "set_has": (boolean, ptr, uint), "set_is_empty": (boolean, ptr),
        "set_get_population": (uint, ptr), "set_get_min": (uint, ptr),
        "set_get_max": (uint, ptr), "set_is_equal": (boolean, ptr, ptr),
        "set_is_subset": (boolean, ptr, ptr),
        "set_set": (None, ptr, ptr), "set_union": (None, ptr, ptr),
        "set_intersect": (None, ptr, ptr), "set_subtract": (None, ptr, ptr),
        "set_symmetric_difference": (None, ptr, ptr),
        "set_next": (boolean, ptr, uout), "set_previous": (boolean, ptr, uout),
        "set_next_range": (boolean, ptr, uout, uout),
        "set_previous_range": (boolean, ptr, uout, uout),
        "set_next_many": (uint, ptr, uint, uout, uint),
        "map_create": (ptr,), "map_destroy": (None, ptr),
        "map_copy": (ptr, ptr), "map_clear": (None, ptr),
        "map_set": (None, ptr, uint, uint), "map_del": (None, ptr, uint),
        "map_get": (uint, ptr, uint), "map_has": (boolean, ptr, uint),
        "map_get_population": (uint, ptr), "map_is_empty": (boolean, ptr),
        "map_is_equal": (boolean, ptr, ptr), "map_update": (None, ptr, ptr),
        "map_next": (boolean, ptr, iout, uout, uout),
        "map_keys": (None, ptr, ptr), "map_values": (None, ptr, ptr),
    }
    if hasattr(library, prefix + "set_intersects"):
        signatures["set_intersects"] = (boolean, ptr, ptr)
    functions = {}
    for name, (result, *args) in signatures.items():
        function = getattr(library, prefix + name)
        function.restype, function.argtypes = result, args
        functions[name] = function
    return functions


def check_equal(name, actual, expected):
    assert actual == expected, f"{name}: HarfRust={actual!r}, HarfBuzz={expected!r}"


def set_ranges(api, obj, backwards=False):
    first, last = c.c_uint(INVALID), c.c_uint(INVALID)
    result = []
    function = api["set_previous_range" if backwards else "set_next_range"]
    while function(obj, c.byref(first), c.byref(last)):
        result.append((first.value, last.value))
    check_equal("range exhaustion", (first.value, last.value), (INVALID, INVALID))
    return result


def map_entries(api, obj):
    idx, key, value = c.c_int(-1), c.c_uint(123), c.c_uint(456)
    result = {}
    while api["map_next"](obj, c.byref(idx), c.byref(key), c.byref(value)):
        assert key.value not in result, "duplicate map entry"
        result[key.value] = value.value
    check_equal("map exhaustion", idx.value, -1)
    return result


def check_sets(apis, pair):
    for name in ["is_empty", "is_inverted", "get_population", "get_min", "get_max"]:
        check_equal("set_" + name, *(api["set_" + name](obj) for api, obj in zip(apis, pair)))
    for backwards in [False, True]:
        check_equal("set ranges", *(set_ranges(api, obj, backwards) for api, obj in zip(apis, pair)))
    for value in VALUES:
        check_equal("set_has", *(api["set_has"](obj, value) for api, obj in zip(apis, pair)))
        for direction in ["next", "previous"]:
            answers = []
            for api, obj in zip(apis, pair):
                cursor = c.c_uint(value)
                found = api["set_" + direction](obj, c.byref(cursor))
                answers.append((found, cursor.value))
            check_equal("set_" + direction, *answers)
        for direction in ["next_range", "previous_range"]:
            answers = []
            for api, obj in zip(apis, pair):
                first, last = c.c_uint(value), c.c_uint(value)
                found = api["set_" + direction](obj, c.byref(first), c.byref(last))
                answers.append((found, first.value, last.value))
            check_equal("set_" + direction, *answers)
        answers = []
        for api, obj in zip(apis, pair):
            out = (c.c_uint * 8)(*[77] * 8)
            count = api["set_next_many"](obj, value, out, len(out))
            answers.append((count, list(out)))
        check_equal("set_next_many", *answers)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("harfrust", type=Path)
    parser.add_argument("harfbuzz", type=Path)
    parser.add_argument("--steps", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=504)
    args = parser.parse_args()
    apis = [bind(args.harfrust, "hr_"), bind(args.harfbuzz, "hb_")]
    rng = random.Random(args.seed)
    sets = [[api["set_create"]() for api in apis] for _ in range(4)]
    maps = [[api["map_create"]() for api in apis] for _ in range(4)]
    set_ops = ["add", "del", "add_range", "del_range", "add_sorted_array",
               "invert", "clear", "set", "union", "intersect", "subtract",
               "symmetric_difference", "copy"]
    try:
        for step in range(args.steps):
            pair, other = rng.choice(sets), rng.choice(sets)
            operation = rng.choice(set_ops)
            value = rng.choice(VALUES + [rng.randrange(2048)])
            # Keep finite ranges small: storing billions of members/exclusions
            # would exercise allocator limits instead of the container API.
            last = min(INVALID, value + rng.randrange(8))
            array = (c.c_uint * 5)(*sorted(rng.choices(VALUES, k=5)))
            for index, (api, obj) in enumerate(zip(apis, pair)):
                function = api["set_" + operation]
                if operation in ["add", "del"]:
                    function(obj, value)
                elif operation in ["add_range", "del_range"]:
                    function(obj, value, last)
                elif operation == "add_sorted_array":
                    function(obj, array, len(array))
                elif operation in ["invert", "clear"]:
                    function(obj)
                elif operation == "copy":
                    copy = function(obj)
                    api["set_destroy"](obj)
                    pair[index] = copy
                else:
                    function(obj, other[index])
            check_sets(apis, pair)
            for name in ["is_equal", "is_subset", "intersects"]:
                if "set_" + name not in apis[1]:
                    continue
                answers = [api["set_" + name](a, b) for api, a, b in zip(apis, pair, other)]
                check_equal("set_" + name, *answers)

            pair, other = rng.choice(maps), rng.choice(maps)
            operation = rng.choice(["set", "del", "clear", "update", "copy"])
            key, value = rng.choice(VALUES), rng.choice(VALUES)
            for index, (api, obj) in enumerate(zip(apis, pair)):
                function = api["map_" + operation]
                if operation == "set":
                    function(obj, key, value)
                elif operation == "del":
                    function(obj, key)
                elif operation == "clear":
                    function(obj)
                elif operation == "update":
                    function(obj, other[index])
                else:
                    copy = function(obj)
                    api["map_destroy"](obj)
                    pair[index] = copy
            check_equal("map entries", *(map_entries(api, obj) for api, obj in zip(apis, pair)))
            for name in ["get_population", "is_empty"]:
                check_equal("map_" + name, *(api["map_" + name](obj) for api, obj in zip(apis, pair)))
            for key in VALUES:
                for name in ["has", "get"]:
                    check_equal("map_" + name, *(api["map_" + name](obj, key) for api, obj in zip(apis, pair)))
            check_equal("map_is_equal", *(api["map_is_equal"](a, b) for api, a, b in zip(apis, pair, other)))
            for name in ["keys", "values"]:
                collected = []
                for api, obj in zip(apis, pair):
                    dest = api["set_create"]()
                    try:
                        api["set_add"](dest, 42)
                        api["map_" + name](obj, dest)
                        collected.append(set_ranges(api, dest))
                    finally:
                        api["set_destroy"](dest)
                check_equal("map_" + name, *collected)
    except AssertionError as error:
        raise AssertionError(f"seed {args.seed}, step {step}: {error}") from error
    finally:
        for kind, objects in [("set", sets), ("map", maps)]:
            for pair in objects:
                for api, obj in zip(apis, pair):
                    api[kind + "_destroy"](obj)
    print(f"Compared {args.steps} set and map mutations (seed {args.seed}); no differences")


if __name__ == "__main__":
    main()
