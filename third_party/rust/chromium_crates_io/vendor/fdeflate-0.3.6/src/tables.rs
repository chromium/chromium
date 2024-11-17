use crate::decompress::{EXCEPTIONAL_ENTRY, LITERAL_ENTRY};

/// Hard-coded Huffman codes used regardless of the input.
///
/// These values work well for PNGs with some form of filtering enabled, but will likely make most
/// other inputs worse.
pub(crate) const HUFFMAN_LENGTHS: [u8; 286] = [
    2, 3, 4, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 10, 11, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 8, 9, 8, 8, 8, 8, 8, 7,
    7, 7, 6, 6, 6, 5, 4, 3, 12, 12, 12, 9, 9, 11, 10, 11, 11, 10, 11, 11, 11, 11, 11, 11, 12, 11,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 9,
];

pub(crate) const HUFFMAN_CODES: [u16; 286] = match crate::compute_codes(&HUFFMAN_LENGTHS) {
    Some(codes) => codes,
    None => panic!("HUFFMAN_LENGTHS is invalid"),
};

/// Length code for length values (derived from deflate spec).
pub(crate) const LENGTH_TO_SYMBOL: [u16; 256] = [
    257, 258, 259, 260, 261, 262, 263, 264, 265, 265, 266, 266, 267, 267, 268, 268, 269, 269, 269,
    269, 270, 270, 270, 270, 271, 271, 271, 271, 272, 272, 272, 272, 273, 273, 273, 273, 273, 273,
    273, 273, 274, 274, 274, 274, 274, 274, 274, 274, 275, 275, 275, 275, 275, 275, 275, 275, 276,
    276, 276, 276, 276, 276, 276, 276, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277,
    277, 277, 277, 277, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278,
    278, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 280, 280,
    280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 281, 281, 281, 281, 281,
    281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281,
    281, 281, 281, 281, 281, 281, 281, 281, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283,
    283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 284, 284, 284, 284,
    284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284,
    284, 284, 284, 284, 284, 284, 284, 284, 285,
];

/// Number of extra bits for length values (derived from deflate spec).
pub(crate) const LENGTH_TO_LEN_EXTRA: [u8; 256] = [
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0,
];

pub(crate) const BITMASKS: [u32; 17] = [
    0x0000, 0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F, 0x00FF, 0x01FF, 0x03FF, 0x07FF,
    0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
];

/// Order of the length code length alphabet (derived from deflate spec).
pub(crate) const CLCL_ORDER: [usize; 19] = [
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
];

/// Number of extra bits for each length code (derived from deflate spec).
pub(crate) const LEN_SYM_TO_LEN_EXTRA: [u8; 29] = [
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
];

/// The base length for each length code (derived from deflate spec).
pub(crate) const LEN_SYM_TO_LEN_BASE: [usize; 29] = [
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131,
    163, 195, 227, 258,
];

/// Number of extra bits for each distance code (derived from deflate spec.)
pub(crate) const DIST_SYM_TO_DIST_EXTRA: [u8; 30] = [
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13,
    13,
];

/// The base distance for each distance code (derived from deflate spec).
pub(crate) const DIST_SYM_TO_DIST_BASE: [u16; 30] = [
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
    2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
];

/// The main litlen_table uses a 12-bit input to lookup the meaning of the symbol. The table is
/// split into 4 sections:
///
///   aaaaaaaa_bbbbbbbb_1000yyyy_0000xxxx  x = input_advance_bits, y = output_advance_bytes (literal)
///   0000000z_zzzzzzzz_00000yyy_0000xxxx  x = input_advance_bits, y = extra_bits, z = distance_base (length)
///   00000000_00000000_01000000_0000xxxx  x = input_advance_bits (EOF)
///   0000xxxx_xxxxxxxx_01100000_00000000  x = secondary_table_index
///   00000000_00000000_01000000_00000000  invalid code
pub(crate) const LITLEN_TABLE_ENTRIES: [u32; 288] = {
    let mut entries = [EXCEPTIONAL_ENTRY; 288];
    let mut i = 0;
    while i < 256 {
        entries[i] = (i as u32) << 16 | LITERAL_ENTRY | (1 << 8);
        i += 1;
    }

    let mut i = 257;
    while i < 286 {
        entries[i] = (LEN_SYM_TO_LEN_BASE[i - 257] as u32) << 16
            | (LEN_SYM_TO_LEN_EXTRA[i - 257] as u32) << 8;
        i += 1;
    }
    entries
};

/// The distance table is a 512-entry table that maps 9 bits of distance symbols to their meaning.
///
///   00000000_00000000_00000000_00000000     symbol is more than 9 bits
///   zzzzzzzz_zzzzzzzz_0000yyyy_0000xxxx     x = input_advance_bits, y = extra_bits, z = distance_base
pub(crate) const DISTANCE_TABLE_ENTRIES: [u32; 32] = {
    let mut entries = [0; 32];
    let mut i = 0;
    while i < 30 {
        entries[i] = (DIST_SYM_TO_DIST_BASE[i] as u32) << 16
            | (DIST_SYM_TO_DIST_EXTRA[i] as u32) << 8
            | LITERAL_ENTRY;
        i += 1;
    }
    entries
};

pub(crate) const FIXED_CODE_LENGTHS: [u8; 320] = make_fixed_code_lengths();
const fn make_fixed_code_lengths() -> [u8; 320] {
    let mut i = 0;
    let mut lengths = [0; 320];
    while i < 144 {
        lengths[i] = 8;
        i += 1;
    }
    while i < 256 {
        lengths[i] = 9;
        i += 1;
    }
    while i < 280 {
        lengths[i] = 7;
        i += 1;
    }
    while i < 288 {
        lengths[i] = 8;
        i += 1;
    }
    while i < 320 {
        lengths[i] = 5;
        i += 1;
    }
    lengths
}
