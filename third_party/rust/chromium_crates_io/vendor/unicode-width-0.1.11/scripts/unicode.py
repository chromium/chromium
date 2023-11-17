#!/usr/bin/env python3
#
# Copyright 2011-2022 The Rust Project Developers. See the COPYRIGHT
# file at the top-level directory of this distribution and at
# http://rust-lang.org/COPYRIGHT.
#
# Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
# http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
# <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
# option. This file may not be copied, modified, or distributed
# except according to those terms.

# This script uses the following Unicode tables:
# - EastAsianWidth.txt
# - ReadMe.txt
# - UnicodeData.txt
#
# Since this should not require frequent updates, we just store this
# out-of-line and check the generated module into git.

import enum
import math
import os
import re
import sys

NUM_CODEPOINTS = 0x110000
"""An upper bound for which `range(0, NUM_CODEPOINTS)` contains Unicode's codespace."""

MAX_CODEPOINT_BITS = math.ceil(math.log2(NUM_CODEPOINTS - 1))
"""The maximum number of bits required to represent a Unicode codepoint."""


class OffsetType(enum.IntEnum):
    """Represents the data type of a lookup table's offsets. Each variant's value represents the
    number of bits required to represent that variant's type."""

    U2 = 2
    """Offsets are 2-bit unsigned integers, packed four-per-byte."""
    U4 = 4
    """Offsets are 4-bit unsigned integers, packed two-per-byte."""
    U8 = 8
    """Each offset is a single byte (u8)."""


TABLE_CFGS = [
    (13, MAX_CODEPOINT_BITS, OffsetType.U8),
    (6, 13, OffsetType.U8),
    (0, 6, OffsetType.U2),
]
"""Represents the format of each level of the multi-level lookup table.
A level's entry is of the form `(low_bit, cap_bit, offset_type)`.
This means that every sub-table in that level is indexed by bits `low_bit..cap_bit` of the
codepoint and those tables offsets are stored according to `offset_type`.

If this is edited, you must ensure that `emit_module` reflects your changes."""

MODULE_FILENAME = "tables.rs"
"""The filename of the emitted Rust module (will be created in the working directory)"""

Codepoint = int
BitPos = int


def fetch_open(filename: str):
    """Opens `filename` and return its corresponding file object. If `filename` isn't on disk,
    fetches it from `http://www.unicode.org/Public/UNIDATA/`. Exits with code 1 on failure."""
    if not os.path.exists(os.path.basename(filename)):
        os.system(f"curl -O http://www.unicode.org/Public/UNIDATA/{filename}")
    try:
        return open(filename, encoding="utf-8")
    except OSError:
        sys.stderr.write(f"cannot load {filename}")
        sys.exit(1)


def load_unicode_version() -> "tuple[int, int, int]":
    """Returns the current Unicode version by fetching and processing `ReadMe.txt`."""
    with fetch_open("ReadMe.txt") as readme:
        pattern = r"for Version (\d+)\.(\d+)\.(\d+) of the Unicode"
        return tuple(map(int, re.search(pattern, readme.read()).groups()))


class EffectiveWidth(enum.IntEnum):
    """Represents the width of a Unicode character. All East Asian Width classes resolve into
    either `EffectiveWidth.NARROW`, `EffectiveWidth.WIDE`, or `EffectiveWidth.AMBIGUOUS`."""

    ZERO = 0
    """ Zero columns wide. """
    NARROW = 1
    """ One column wide. """
    WIDE = 2
    """ Two columns wide. """
    AMBIGUOUS = 3
    """ Two columns wide in a CJK context. One column wide in all other contexts. """


def load_east_asian_widths() -> "list[EffectiveWidth]":
    """Return a list of effective widths, indexed by codepoint.
    Widths are determined by fetching and parsing `EastAsianWidth.txt`.

    `Neutral`, `Narrow`, and `Halfwidth` characters are assigned `EffectiveWidth.NARROW`.

    `Wide` and `Fullwidth` characters are assigned `EffectiveWidth.WIDE`.

    `Ambiguous` chracters are assigned `EffectiveWidth.AMBIGUOUS`."""
    with fetch_open("EastAsianWidth.txt") as eaw:
        # matches a width assignment for a single codepoint, i.e. "1F336;N  # ..."
        single = re.compile(r"^([0-9A-F]+)\s+;\s+(\w+) +# (\w+)")
        # matches a width assignment for a range of codepoints, i.e. "3001..3003;W  # ..."
        multiple = re.compile(r"^([0-9A-F]+)\.\.([0-9A-F]+)\s+;\s+(\w+) +# (\w+)")
        # map between width category code and condensed width
        width_codes = {
            **{c: EffectiveWidth.NARROW for c in ["N", "Na", "H"]},
            **{c: EffectiveWidth.WIDE for c in ["W", "F"]},
            "A": EffectiveWidth.AMBIGUOUS,
        }

        width_map = []
        current = 0
        for line in eaw.readlines():
            raw_data = None  # (low, high, width)
            if match := single.match(line):
                raw_data = (match.group(1), match.group(1), match.group(2))
            elif match := multiple.match(line):
                raw_data = (match.group(1), match.group(2), match.group(3))
            else:
                continue
            low = int(raw_data[0], 16)
            high = int(raw_data[1], 16)
            width = width_codes[raw_data[2]]

            assert current <= high
            while current <= high:
                # Some codepoints don't fall into any of the ranges in EastAsianWidth.txt.
                # All such codepoints are implicitly given Neural width (resolves to narrow)
                width_map.append(EffectiveWidth.NARROW if current < low else width)
                current += 1

        while len(width_map) < NUM_CODEPOINTS:
            # Catch any leftover codepoints and assign them implicit Neutral/narrow width.
            width_map.append(EffectiveWidth.NARROW)

        return width_map


def load_zero_widths() -> "list[bool]":
    """Returns a list `l` where `l[c]` is true if codepoint `c` is considered a zero-width
    character. `c` is considered a zero-width character if `c` is in general categories
    `Cc`, `Cf`, `Mn`, or `Me` (determined by fetching and processing `UnicodeData.txt`)."""
    with fetch_open("UnicodeData.txt") as categories:
        zw_map = []
        current = 0
        for line in categories.readlines():
            if len(raw_data := line.split(";")) != 15:
                continue
            [codepoint, name, cat_code] = [
                int(raw_data[0], 16),
                raw_data[1],
                raw_data[2],
            ]
            zero_width = cat_code in ["Cc", "Cf", "Mn", "Me"]

            assert current <= codepoint
            while current <= codepoint:
                if name.endswith(", Last>") or current == codepoint:
                    # if name ends with Last, we backfill the width value to all codepoints since
                    # the previous codepoint (aka the start of the range)
                    zw_map.append(zero_width)
                else:
                    # unassigned characters are implicitly given Neutral width, which is nonzero
                    zw_map.append(False)
                current += 1

        while len(zw_map) < NUM_CODEPOINTS:
            # Catch any leftover codepoints. They must be unassigned (so nonzero width)
            zw_map.append(False)

        return zw_map


class Bucket:
    """A bucket contains a group of codepoints and an ordered width list. If one bucket's width
    list overlaps with another's width list, those buckets can be merged via `try_extend`."""

    def __init__(self):
        """Creates an empty bucket."""
        self.entry_set = set()
        self.widths = []

    def append(self, codepoint: Codepoint, width: EffectiveWidth):
        """Adds a codepoint/width pair to the bucket, and appends `width` to the width list."""
        self.entry_set.add((codepoint, width))
        self.widths.append(width)

    def try_extend(self, attempt: "Bucket") -> bool:
        """If either `self` or `attempt`'s width list starts with the other bucket's width list,
        set `self`'s width list to the longer of the two, add all of `attempt`'s codepoints
        into `self`, and return `True`. Otherwise, return `False`."""
        (less, more) = (self.widths, attempt.widths)
        if len(self.widths) > len(attempt.widths):
            (less, more) = (attempt.widths, self.widths)
        if less != more[: len(less)]:
            return False
        self.entry_set |= attempt.entry_set
        self.widths = more
        return True

    def entries(self) -> "list[tuple[Codepoint, EffectiveWidth]]":
        """Return a list of the codepoint/width pairs in this bucket, sorted by codepoint."""
        result = list(self.entry_set)
        result.sort()
        return result

    def width(self) -> "EffectiveWidth":
        """If all codepoints in this bucket have the same width, return that width; otherwise,
        return `None`."""
        if len(self.widths) == 0:
            return None
        potential_width = self.widths[0]
        for width in self.widths[1:]:
            if potential_width != width:
                return None
        return potential_width


def make_buckets(entries, low_bit: BitPos, cap_bit: BitPos) -> "list[Bucket]":
    """Partitions the `(Codepoint, EffectiveWidth)` tuples in `entries` into `Bucket`s. All
    codepoints with identical bits from `low_bit` to `cap_bit` (exclusive) are placed in the
    same bucket. Returns a list of the buckets in increasing order of those bits."""
    num_bits = cap_bit - low_bit
    assert num_bits > 0
    buckets = [Bucket() for _ in range(0, 2 ** num_bits)]
    mask = (1 << num_bits) - 1
    for (codepoint, width) in entries:
        buckets[(codepoint >> low_bit) & mask].append(codepoint, width)
    return buckets


class Table:
    """Represents a lookup table. Each table contains a certain number of subtables; each
    subtable is indexed by a contiguous bit range of the codepoint and contains a list
    of `2**(number of bits in bit range)` entries. (The bit range is the same for all subtables.)

    Typically, tables contain a list of buckets of codepoints. Bucket `i`'s codepoints should
    be indexed by sub-table `i` in the next-level lookup table. The entries of this table are
    indexes into the bucket list (~= indexes into the sub-tables of the next-level table.) The
    key to compression is that two different buckets in two different sub-tables may have the
    same width list, which means that they can be merged into the same bucket.

    If no bucket contains two codepoints with different widths, calling `indices_to_widths` will
    discard the buckets and convert the entries into `EffectiveWidth` values."""

    def __init__(
        self, entry_groups, low_bit: BitPos, cap_bit: BitPos, offset_type: OffsetType
    ):
        """Create a lookup table with a sub-table for each `(Codepoint, EffectiveWidth)` iterator
        in `entry_groups`. Each sub-table is indexed by codepoint bits in `low_bit..cap_bit`,
        and each table entry is represented in the format specified by  `offset_type`. Asserts
        that this table is actually representable with `offset_type`."""
        self.low_bit = low_bit
        self.cap_bit = cap_bit
        self.offset_type = offset_type
        self.entries = []
        self.indexed = []

        buckets = []
        for entries in entry_groups:
            buckets.extend(make_buckets(entries, self.low_bit, self.cap_bit))

        for bucket in buckets:
            for (i, existing) in enumerate(self.indexed):
                if existing.try_extend(bucket):
                    self.entries.append(i)
                    break
            else:
                self.entries.append(len(self.indexed))
                self.indexed.append(bucket)

        # Validate offset type
        for index in self.entries:
            assert index < (1 << int(self.offset_type))

    def indices_to_widths(self):
        """Destructively converts the indices in this table to the `EffectiveWidth` values of
        their buckets. Assumes that no bucket contains codepoints with different widths."""
        self.entries = list(map(lambda i: int(self.indexed[i].width()), self.entries))
        del self.indexed

    def buckets(self):
        """Returns an iterator over this table's buckets."""
        return self.indexed

    def to_bytes(self) -> "list[int]":
        """Returns this table's entries as a list of bytes. The bytes are formatted according to
        the `OffsetType` which the table was created with, converting any `EffectiveWidth` entries
        to their enum variant's integer value. For example, with `OffsetType.U2`, each byte will
        contain four packed 2-bit entries."""
        entries_per_byte = 8 // int(self.offset_type)
        byte_array = []
        for i in range(0, len(self.entries), entries_per_byte):
            byte = 0
            for j in range(0, entries_per_byte):
                byte |= self.entries[i + j] << (j * int(self.offset_type))
            byte_array.append(byte)
        return byte_array


def make_tables(
    table_cfgs: "list[tuple[BitPos, BitPos, OffsetType]]", entries
) -> "list[Table]":
    """Creates a table for each configuration in `table_cfgs`, with the first config corresponding
    to the top-level lookup table, the second config corresponding to the second-level lookup
    table, and so forth. `entries` is an iterator over the `(Codepoint, EffectiveWidth)` pairs
    to include in the top-level table."""
    tables = []
    entry_groups = [entries]
    for (low_bit, cap_bit, offset_type) in table_cfgs:
        table = Table(entry_groups, low_bit, cap_bit, offset_type)
        entry_groups = map(lambda bucket: bucket.entries(), table.buckets())
        tables.append(table)
    return tables


def emit_module(
    out_name: str, unicode_version: "tuple[int, int, int]", tables: "list[Table]"
):
    """Outputs a Rust module to `out_name` using table data from `tables`.
    If `TABLE_CFGS` is edited, you may need to edit the included code for `lookup_width`."""
    if os.path.exists(out_name):
        os.remove(out_name)
    with open(out_name, "w", newline="\n", encoding="utf-8") as module:
        module.write(
            """// Copyright 2012-2022 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// NOTE: The following code was generated by "scripts/unicode.py", do not edit directly
"""
        )
        module.write(
            f"""
/// The version of [Unicode](http://www.unicode.org/)
/// that this version of unicode-width is based on.
pub const UNICODE_VERSION: (u8, u8, u8) = {unicode_version};
"""
        )

        module.write(
            """
pub mod charwidth {
    use core::option::Option::{self, None, Some};

    /// Returns the [UAX #11](https://www.unicode.org/reports/tr11/) based width of `c` by
    /// consulting a multi-level lookup table.
    /// If `is_cjk == true`, ambiguous width characters are treated as double width; otherwise,
    /// they're treated as single width.
    ///
    /// # Maintenance
    /// The tables themselves are autogenerated but this function is hardcoded. You should have
    /// nothing to worry about if you re-run `unicode.py` (for example, when updating Unicode.)
    /// However, if you change the *actual structure* of the lookup tables (perhaps by editing the
    /// `TABLE_CFGS` global in `unicode.py`) you must ensure that this code reflects those changes.
    #[inline]
    fn lookup_width(c: char, is_cjk: bool) -> usize {
        let cp = c as usize;

        let t1_offset = TABLES_0[cp >> 13 & 0xFF];

        // Each sub-table in TABLES_1 is 7 bits, and each stored entry is a byte,
        // so each sub-table is 128 bytes in size.
        // (Sub-tables are selected using the computed offset from the previous table.)
        let t2_offset = TABLES_1[128 * usize::from(t1_offset) + (cp >> 6 & 0x7F)];

        // Each sub-table in TABLES_2 is 6 bits, but each stored entry is 2 bits.
        // This is accomplished by packing four stored entries into one byte.
        // So each sub-table is 2**(6-2) == 16 bytes in size.
        // Since this is the last table, each entry represents an encoded width.
        let packed_widths = TABLES_2[16 * usize::from(t2_offset) + (cp >> 2 & 0xF)];

        // Extract the packed width
        let width = packed_widths >> (2 * (cp & 0b11)) & 0b11;

        // A width of 3 signifies that the codepoint is ambiguous width.
        if width == 3 {
            if is_cjk {
                2
            } else {
                1
            }
        } else {
            width.into()
        }
    }
"""
        )

        module.write(
            """
    /// Returns the [UAX #11](https://www.unicode.org/reports/tr11/) based width of `c`, or
    /// `None` if `c` is a control character other than `'\\x00'`.
    /// If `is_cjk == true`, ambiguous width characters are treated as double width; otherwise,
    /// they're treated as single width.
    #[inline]
    pub fn width(c: char, is_cjk: bool) -> Option<usize> {
        if c < '\\u{7F}' {
            if c >= '\\u{20}' {
                // U+0020 to U+007F (exclusive) are single-width ASCII codepoints
                Some(1)
            } else if c == '\\0' {
                // U+0000 *is* a control code, but it's special-cased
                Some(0)
            } else {
                // U+0001 to U+0020 (exclusive) are control codes
                None
            }
        } else if c >= '\\u{A0}' {
            // No characters >= U+00A0 are control codes, so we can consult the lookup tables
            Some(lookup_width(c, is_cjk))
        } else {
            // U+007F to U+00A0 (exclusive) are control codes
            None
        }
    }
"""
        )

        subtable_count = 1
        for (i, table) in enumerate(tables):
            new_subtable_count = len(table.buckets())
            if i == len(tables) - 1:
                table.indices_to_widths()  # for the last table, indices == widths
            byte_array = table.to_bytes()
            module.write(
                f"""
    /// Autogenerated. {subtable_count} sub-table(s). Consult [`lookup_width`] for layout info.
    static TABLES_{i}: [u8; {len(byte_array)}] = ["""
            )
            for (j, byte) in enumerate(byte_array):
                # Add line breaks for every 15th entry (chosen to match what rustfmt does)
                if j % 15 == 0:
                    module.write("\n       ")
                module.write(f" 0x{byte:02X},")
            module.write("\n    ];\n")
            subtable_count = new_subtable_count
        module.write("}\n")


def main(module_filename: str):
    """Obtain character data from the latest version of Unicode, transform it into a multi-level
    lookup table for character width, and write a Rust module utilizing that table to
    `module_filename`.

    We obey the following rules in decreasing order of importance:
    - The soft hyphen (`U+00AD`) is single-width.
    - Hangul Jamo medial vowels & final consonants (`U+1160..=U+11FF`) are zero-width.
    - All codepoints in general categories `Cc`, `Cf`, `Mn`, and `Me` are zero-width.
    - All codepoints with an East Asian Width of `Ambigous` are ambiguous-width.
    - All codepoints with an East Asian Width of `Wide` or `Fullwidth` are double-width.
    - All other codepoints (including unassigned codepoints and codepoints with an East Asian Width
    of `Neutral`, `Narrow`, or `Halfwidth`) are single-width.

    These rules are based off of Markus Kuhn's free `wcwidth()` implementation:
    http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c"""
    version = load_unicode_version()
    print(f"Generating module for Unicode {version[0]}.{version[1]}.{version[2]}")

    eaw_map = load_east_asian_widths()
    zw_map = load_zero_widths()

    # Characters marked as zero-width in zw_map should be zero-width in the final map
    width_map = list(
        map(lambda x: EffectiveWidth.ZERO if x[1] else x[0], zip(eaw_map, zw_map))
    )

    # Override for soft hyphen
    width_map[0x00AD] = EffectiveWidth.NARROW

    # Override for Hangul Jamo medial vowels & final consonants
    for i in range(0x1160, 0x11FF + 1):
        width_map[i] = EffectiveWidth.ZERO

    tables = make_tables(TABLE_CFGS, enumerate(width_map))

    print("------------------------")
    total_size = 0
    for (i, table) in enumerate(tables):
        size_bytes = len(table.to_bytes())
        print(f"Table {i} Size: {size_bytes} bytes")
        total_size += size_bytes
    print("------------------------")
    print(f"  Total Size: {total_size} bytes")

    emit_module(module_filename, version, tables)
    print(f'Wrote to "{module_filename}"')


if __name__ == "__main__":
    main(MODULE_FILENAME)
