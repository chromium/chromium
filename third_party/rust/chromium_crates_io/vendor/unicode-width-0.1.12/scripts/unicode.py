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
#
# - DerivedCoreProperties.txt
# - EastAsianWidth.txt
# - HangulSyllableType.txt
# - PropList.txt
# - ReadMe.txt
# - emoji/emoji-variation-sequences.txt
#
# Since this should not require frequent updates, we just store this
# out-of-line and check the generated module into git.

import enum
import math
import os
import re
import sys
from collections import defaultdict
from itertools import batched

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
    fetches it from `http://www.unicode.org/Public/UNIDATA/`. Exits with code 1 on failure.
    """
    basename = os.path.basename(filename)
    if not os.path.exists(basename):
        os.system(f"curl -O http://www.unicode.org/Public/UNIDATA/{filename}")
    try:
        return open(basename, encoding="utf-8")
    except OSError:
        sys.stderr.write(f"cannot load {basename}")
        sys.exit(1)


def load_unicode_version() -> "tuple[int, int, int]":
    """Returns the current Unicode version by fetching and processing `ReadMe.txt`."""
    with fetch_open("ReadMe.txt") as readme:
        pattern = r"for Version (\d+)\.(\d+)\.(\d+) of the Unicode"
        return tuple(map(int, re.search(pattern, readme.read()).groups()))


class EffectiveWidth(enum.IntEnum):
    """Represents the width of a Unicode character. All East Asian Width classes resolve into
    either `EffectiveWidth.NARROW`, `EffectiveWidth.WIDE`, or `EffectiveWidth.AMBIGUOUS`.
    """

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
    character. `c` is considered a zero-width character if

    - it is a control character,
    - or if it has the `Default_Ignorable_Code_Point` property (determined from `DerivedCoreProperties.txt`),
    - or if it has the `Grapheme_Extend` property (determined from `DerivedCoreProperties.txt`),
    - or if it one of eight characters that should be `Grapheme_Extend` but aren't due to a Unicode spec bug,
    - or if it has a `Hangul_Syllable_Type` of `Vowel_Jamo` or `Trailing_Jamo` (determined from `HangulSyllableType.txt`).
    """

    zw_map = [False] * NUM_CODEPOINTS

    # Control characters have width 0
    for c in range(0x00, 0x20):
        zw_map[c] = True
    for c in range(0x7F, 0xA0):
        zw_map[c] = True

    # `Default_Ignorable_Code_Point`s also have 0 width:
    # https://www.unicode.org/faq/unsup_char.html#3
    # https://www.unicode.org/versions/Unicode15.1.0/ch05.pdf#G40095
    #
    # `Grapheme_Extend` includes characters with general category `Mn` or `Me`,
    # as well as a few `Mc` characters that need to be included so that
    # canonically equivalent sequences have the same width.
    with fetch_open("DerivedCoreProperties.txt") as properties:
        single = re.compile(
            r"^([0-9A-F]+)\s+;\s+(?:Default_Ignorable_Code_Point|Grapheme_Extend)\s+"
        )
        multiple = re.compile(
            r"^([0-9A-F]+)\.\.([0-9A-F]+)\s+;\s+(?:Default_Ignorable_Code_Point|Grapheme_Extend)\s+"
        )

        for line in properties.readlines():
            raw_data = None  # (low, high)
            if match := single.match(line):
                raw_data = (match.group(1), match.group(1))
            elif match := multiple.match(line):
                raw_data = (match.group(1), match.group(2))
            else:
                continue
            low = int(raw_data[0], 16)
            high = int(raw_data[1], 16)
            for cp in range(low, high + 1):
                zw_map[cp] = True

    # Unicode spec bug: these should be `Grapheme_Cluster_Break=Extend`,
    # as they canonically decompose to two characters with this property,
    # but they aren't.
    for c in [0x0CC0, 0x0CC7, 0x0CC8, 0x0CCA, 0x0CCB, 0x1B3B, 0x1B3D, 0x1B43]:
        zw_map[c] = True

    # Treat `Hangul_Syllable_Type`s of `Vowel_Jamo` and `Trailing_Jamo`
    # as zero-width. This matches the behavior of glibc `wcwidth`.
    #
    # Decomposed Hangul characters consist of 3 parts: a `Leading_Jamo`,
    # a `Vowel_Jamo`, and an optional `Trailing_Jamo`. Together these combine
    # into a single wide grapheme. So we treat vowel and trailing jamo as
    # 0-width, such that only the width of the leading jamo is counted
    # and the resulting grapheme has width 2.
    #
    # (See the Unicode Standard sections 3.12 and 18.6 for more on Hangul)
    with fetch_open("HangulSyllableType.txt") as categories:
        single = re.compile(r"^([0-9A-F]+)\s+;\s+(V|T)\s+")
        multiple = re.compile(r"^([0-9A-F]+)\.\.([0-9A-F]+)\s+;\s+(V|T)\s+")

        for line in categories.readlines():
            raw_data = None  # (low, high)
            if match := single.match(line):
                raw_data = (match.group(1), match.group(1))
            elif match := multiple.match(line):
                raw_data = (match.group(1), match.group(2))
            else:
                continue
            low = int(raw_data[0], 16)
            high = int(raw_data[1], 16)
            for cp in range(low, high + 1):
                zw_map[cp] = True

    # Special case: U+115F HANGUL CHOSEONG FILLER.
    # U+115F is a `Default_Ignorable_Code_Point`, and therefore would normally have
    # zero width. However, the expected usage is to combine it with vowel or trailing jamo
    # (which are considered 0-width on their own) to form a composed Hangul syllable with
    # width 2. Therefore, we treat it as having width 2.
    zw_map[0x115F] = False

    return zw_map


class Bucket:
    """A bucket contains a group of codepoints and an ordered width list. If one bucket's width
    list overlaps with another's width list, those buckets can be merged via `try_extend`.
    """

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

    def width(self) -> "EffectiveWidth | None":
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
    buckets = [Bucket() for _ in range(0, 2**num_bits)]
    mask = (1 << num_bits) - 1
    for codepoint, width in entries:
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
            for i, existing in enumerate(self.indexed):
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
        their buckets. Assumes that no bucket contains codepoints with different widths.
        """
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
    for low_bit, cap_bit, offset_type in table_cfgs:
        table = Table(entry_groups, low_bit, cap_bit, offset_type)
        entry_groups = map(lambda bucket: bucket.entries(), table.buckets())
        tables.append(table)
    return tables


def load_variation_sequences() -> "list[int]":
    """Outputs a list of character ranages, corresponding to all the valid characters for starting
    an emoji presentation sequence."""

    with fetch_open("emoji/emoji-variation-sequences.txt") as sequences:
        # Match all emoji presentation sequences
        # (one codepoint followed by U+FE0F, and labeled "emoji style")
        sequence = re.compile(r"^([0-9A-F]+)\s+FE0F\s*;\s+emoji style")
        codepoints = []
        for line in sequences.readlines():
            if match := sequence.match(line):
                cp = int(match.group(1), 16)
                codepoints.append(cp)
    return codepoints


def make_variation_sequence_table(
    seqs: "list[int]",
    width_map: "list[EffectiveWidth]",
) -> "tuple[list[int], list[list[int]]]":
    """Generates 2-level lookup table for whether a codepoint might start an emoji presentation sequence.
    (Characters that are always wide may be excluded.)
    The first level is a match on all but the 10 LSB, the second level is a 1024-bit bitmap for those 10 LSB.
    """

    prefixes_dict = defaultdict(set)
    for cp in seqs:
        prefixes_dict[cp >> 10].add(cp & 0x3FF)

    # We don't strictly need to keep track of characters that are always wide,
    # because being in an emoji variation seq won't affect their width.
    # So store their info only when it wouldn't inflate the size of the tables.
    for k in list(prefixes_dict.keys()):
        if all(
            map(
                lambda cp: width_map[(k << 10) | cp] == EffectiveWidth.WIDE,
                prefixes_dict[k],
            )
        ):
            del prefixes_dict[k]

    indexes = list(prefixes_dict.keys())

    # Similarly, we can spuriously return `true` for always-wide characters
    # even if not part of a presentation seq; this saves an additional lookup,
    # so we should do it where there is no size cost.
    for cp, width in enumerate(width_map):
        if width == EffectiveWidth.WIDE and (cp >> 10) in indexes:
            prefixes_dict[cp >> 10].add(cp & 0x3FF)

    leaves = []
    for cps in prefixes_dict.values():
        leaf = [0] * 128
        for cp in cps:
            idx_in_leaf, bit_shift = divmod(cp, 8)
            leaf[idx_in_leaf] |= 1 << bit_shift
        leaves.append(leaf)
    return (indexes, leaves)


def emit_module(
    out_name: str,
    unicode_version: "tuple[int, int, int]",
    tables: "list[Table]",
    variation_table: "tuple[list[int], list[list[int]]]",
):
    """Outputs a Rust module to `out_name` using table data from `tables`.
    If `TABLE_CFGS` is edited, you may need to edit the included code for `lookup_width`.
    """
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

        variation_idx, variation_leaves = variation_table

        module.write(
            """
    /// Whether this character forms an [emoji presentation sequence]
    /// (https://www.unicode.org/reports/tr51/#def_emoji_presentation_sequence)
    /// when followed by `'\\u{FEOF}'`.
    /// Emoji presentation sequences are considered to have width 2.
    /// This may spuriously return `true` or `false` for characters that are always wide.
    #[inline]
    pub fn starts_emoji_presentation_seq(c: char) -> bool {
        let cp: u32 = c.into();
        // First level of lookup uses all but 10 LSB
        let top_bits = cp >> 10;
        let idx_of_leaf: usize = match top_bits {
"""
        )

        for i, msbs in enumerate(variation_idx):
            module.write(f"            {msbs} => {i},\n")

        module.write(
            """            _ => return false,
        };
        // Extract the 3-9th (0-indexed) least significant bits of `cp`,
        // and use them to index into `leaf_row`.
        let idx_within_leaf = usize::try_from((cp >> 3) & 0x7F).unwrap();
        let leaf_byte = EMOJI_PRESENTATION_LEAVES.0[idx_of_leaf][idx_within_leaf];
        // Use the 3 LSB of `cp` to index into `leaf_byte`.
        ((leaf_byte >> (cp & 7)) & 1) == 1
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
        for i, table in enumerate(tables):
            new_subtable_count = len(table.buckets())
            if i == len(tables) - 1:
                table.indices_to_widths()  # for the last table, indices == widths
            byte_array = table.to_bytes()
            module.write(
                f"""
    /// Autogenerated. {subtable_count} sub-table(s). Consult [`lookup_width`] for layout info.
    static TABLES_{i}: [u8; {len(byte_array)}] = ["""
            )
            for j, byte in enumerate(byte_array):
                # Add line breaks for every 15th entry (chosen to match what rustfmt does)
                if j % 15 == 0:
                    module.write("\n       ")
                module.write(f" 0x{byte:02X},")
            module.write("\n    ];\n")
            subtable_count = new_subtable_count

        # emoji table

        module.write(
            f"""
    #[repr(align(128))]
    struct Align128<T>(T);
    /// Array of 1024-bit bitmaps. Index into the correct (obtained from `EMOJI_PRESENTATION_INDEX`)
    /// bitmap with the 10 LSB of your codepoint to get whether it can start an emoji presentation seq.
    static EMOJI_PRESENTATION_LEAVES: Align128<[[u8; 128]; {len(variation_leaves)}]> = Align128([
"""
        )
        for leaf in variation_leaves:
            module.write("        [\n")
            for row in batched(leaf, 14):
                module.write("           ")
                for entry in row:
                    module.write(f" 0x{entry:02X},")
                module.write("\n")
            module.write("        ],\n")

        module.write("    ]);\n")

        module.write("}\n")


def main(module_filename: str):
    """Obtain character data from the latest version of Unicode, transform it into a multi-level
    lookup table for character width, and write a Rust module utilizing that table to
    `module_filename`.

    We obey the following rules, in decreasing order of importance:

    - Emoji presentation sequences are double-width.
    - The soft hyphen (`U+00AD`) is single-width. (https://archive.is/fCT3c)
    - Hangul jamo medial vowels & final consonants are zero-width.
    - `Default_Ignorable_Code_Point`s are zero-width, except for U+115F HANGUL CHOSEONG FILLER.
    - Control characters are zero-width.
    - `Grapheme_Extend` chracters, as well as eight characters that NFD decompose to `Grapheme_Extend` chracters,
      are zero-width.
    - Codepoints with an East Asian Width of `Ambigous` are ambiguous-width.
    - Codepoints with an East Asian Width of `Wide` or `Fullwidth` are double-width.
    - All other codepoints (including unassigned codepoints and codepoints with an East Asian Width
      of `Neutral`, `Narrow`, or `Halfwidth`) are single-width.

    These rules are based off of UAX11, other Unicode standards, and various `wcwidth()` implementations.
    """
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

    tables = make_tables(TABLE_CFGS, enumerate(width_map))

    emoji_variations = load_variation_sequences()
    variation_table = make_variation_sequence_table(emoji_variations, width_map)

    print("------------------------")
    total_size = 0
    for i, table in enumerate(tables):
        size_bytes = len(table.to_bytes())
        print(f"Table {i} size: {size_bytes} bytes")
        total_size += size_bytes
    emoji_index_size = len(variation_table[0]) * 4
    print(f"Emoji presentation index size: {emoji_index_size} bytes")
    total_size += emoji_index_size
    emoji_leaves_size = len(variation_table[1]) * len(variation_table[1][0])
    print(f"Emoji presentation leaves size: {emoji_leaves_size} bytes")
    total_size += emoji_leaves_size
    print("------------------------")
    print(f"  Total size: {total_size} bytes")

    emit_module(module_filename, version, tables, variation_table)
    print(f'Wrote to "{module_filename}"')


if __name__ == "__main__":
    main(MODULE_FILENAME)
