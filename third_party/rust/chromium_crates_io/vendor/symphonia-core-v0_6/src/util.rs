// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `util` module provides a repository of commonly used utility functions sorted into distinct
//! categories.
//!
//! If a function is used all-over the codebase, and does not belong to specific top-level module,
//! it should be placed here.

pub mod bits {
    //! Utilities for bit manipulation.

    /// Sign extends an arbitrary, 8-bit or less, signed two's complement integer stored within an
    /// u8 to a full width i8.
    #[inline(always)]
    pub fn sign_extend_leq8_to_i8(value: u8, width: u32) -> i8 {
        // Rust uses an arithmetic shift right (the original sign bit is repeatedly shifted on) for
        // signed integer types. Therefore, shift the value to the right-hand side of the integer,
        // then shift it back to extend the sign bit.
        (value.wrapping_shl(8 - width) as i8).wrapping_shr(8 - width)
    }

    /// Sign extends an arbitrary, 16-bit or less, signed two's complement integer stored within an
    /// u16 to a full width i16.
    #[inline(always)]
    pub fn sign_extend_leq16_to_i16(value: u16, width: u32) -> i16 {
        (value.wrapping_shl(16 - width) as i16).wrapping_shr(16 - width)
    }

    /// Sign extends an arbitrary, 32-bit or less, signed two's complement integer stored within an
    /// u32 to a full width i32.
    #[inline(always)]
    pub fn sign_extend_leq32_to_i32(value: u32, width: u32) -> i32 {
        (value.wrapping_shl(32 - width) as i32).wrapping_shr(32 - width)
    }

    /// Sign extends an arbitrary, 64-bit or less, signed two's complement integer stored within an
    /// u64 to a full width i64.
    #[inline(always)]
    pub fn sign_extend_leq64_to_i64(value: u64, width: u32) -> i64 {
        (value.wrapping_shl(64 - width) as i64).wrapping_shr(64 - width)
    }

    /// Masks the bit at the specified bit index.
    #[inline(always)]
    pub fn mask_at(idx: u32) -> u8 {
        debug_assert!(idx <= 7);
        1 << idx
    }

    /// Masks all bits with an index greater than or equal to idx.
    #[inline(always)]
    pub fn mask_upper_eq(idx: u32) -> u8 {
        debug_assert!(idx <= 7);
        !((1 << idx) - 1)
    }

    /// Masks all bits with an index greater than idx.
    #[inline(always)]
    pub fn mask_upper(idx: u32) -> u8 {
        debug_assert!(idx <= 7);
        !((1 << idx) - 1) ^ (1 << idx)
    }

    /// Masks all bits with an index less than or equal to idx.
    #[inline(always)]
    pub fn mask_lower_eq(idx: u32) -> u8 {
        debug_assert!(idx <= 7);
        ((1 << idx) - 1) ^ (1 << idx)
    }

    /// Masks all bits with an index less than idx.
    #[inline(always)]
    pub fn mask_lower(idx: u32) -> u8 {
        debug_assert!(idx <= 7);
        (1 << idx) - 1
    }

    /// Masks out all bits in positions less than upper, but greater than or equal to lower
    /// (upper < bit <= lower)
    #[inline(always)]
    pub fn mask_range(upper: u32, lower: u32) -> u8 {
        debug_assert!(upper <= 8);
        debug_assert!(lower <= 8);
        ((0xff_u32 << upper) ^ (0xff_u32 << lower)) as u8
    }

    /// Returns true if the unsigned 16-bit integer contains one or more bytes which have all bits
    /// set.
    #[inline(always)]
    pub fn contains_ones_byte_u16(value: u16) -> bool {
        ((value & !value.wrapping_add(0x0101)) & 0x8080) != 0
    }

    /// Returns true if the unsigned 32-bit integer contains one or more bytes which have all bits
    /// set.
    #[inline(always)]
    pub fn contains_ones_byte_u32(value: u32) -> bool {
        ((value & !value.wrapping_add(0x0101_0101)) & 0x8080_8080) != 0
    }

    /// Returns true if the unsigned 64-bit integer contains one or more bytes which have all bits
    /// set.
    #[inline(always)]
    pub fn contains_ones_byte_u64(value: u64) -> bool {
        ((value & !value.wrapping_add(0x0101_0101_0101_0101)) & 0x8080_8080_8080_8080) != 0
    }

    #[test]
    fn verify_masks() {
        assert_eq!(mask_at(0), 0b0000_0001);
        assert_eq!(mask_at(1), 0b0000_0010);
        assert_eq!(mask_at(2), 0b0000_0100);
        assert_eq!(mask_at(3), 0b0000_1000);
        assert_eq!(mask_at(4), 0b0001_0000);
        assert_eq!(mask_at(5), 0b0010_0000);
        assert_eq!(mask_at(6), 0b0100_0000);
        assert_eq!(mask_at(7), 0b1000_0000);

        assert_eq!(mask_upper(0), 0b1111_1110);
        assert_eq!(mask_upper(1), 0b1111_1100);
        assert_eq!(mask_upper(2), 0b1111_1000);
        assert_eq!(mask_upper(3), 0b1111_0000);
        assert_eq!(mask_upper(4), 0b1110_0000);
        assert_eq!(mask_upper(5), 0b1100_0000);
        assert_eq!(mask_upper(6), 0b1000_0000);
        assert_eq!(mask_upper(7), 0b0000_0000);

        assert_eq!(mask_upper_eq(0), 0b1111_1111);
        assert_eq!(mask_upper_eq(1), 0b1111_1110);
        assert_eq!(mask_upper_eq(2), 0b1111_1100);
        assert_eq!(mask_upper_eq(3), 0b1111_1000);
        assert_eq!(mask_upper_eq(4), 0b1111_0000);
        assert_eq!(mask_upper_eq(5), 0b1110_0000);
        assert_eq!(mask_upper_eq(6), 0b1100_0000);
        assert_eq!(mask_upper_eq(7), 0b1000_0000);

        assert_eq!(mask_lower(0), 0b0000_0000);
        assert_eq!(mask_lower(1), 0b0000_0001);
        assert_eq!(mask_lower(2), 0b0000_0011);
        assert_eq!(mask_lower(3), 0b0000_0111);
        assert_eq!(mask_lower(4), 0b0000_1111);
        assert_eq!(mask_lower(5), 0b0001_1111);
        assert_eq!(mask_lower(6), 0b0011_1111);
        assert_eq!(mask_lower(7), 0b0111_1111);

        assert_eq!(mask_lower_eq(0), 0b0000_0001);
        assert_eq!(mask_lower_eq(1), 0b0000_0011);
        assert_eq!(mask_lower_eq(2), 0b0000_0111);
        assert_eq!(mask_lower_eq(3), 0b0000_1111);
        assert_eq!(mask_lower_eq(4), 0b0001_1111);
        assert_eq!(mask_lower_eq(5), 0b0011_1111);
        assert_eq!(mask_lower_eq(6), 0b0111_1111);
        assert_eq!(mask_lower_eq(7), 0b1111_1111);

        assert_eq!(mask_range(0, 0), 0b0000_0000);
        assert_eq!(mask_range(1, 1), 0b0000_0000);
        assert_eq!(mask_range(7, 7), 0b0000_0000);
        assert_eq!(mask_range(1, 0), 0b0000_0001);
        assert_eq!(mask_range(2, 0), 0b0000_0011);
        assert_eq!(mask_range(7, 0), 0b0111_1111);
        assert_eq!(mask_range(5, 2), 0b0001_1100);
        assert_eq!(mask_range(7, 2), 0b0111_1100);
        assert_eq!(mask_range(8, 2), 0b1111_1100);
    }
}

pub mod clamp {
    //! Utilities for clamping numeric values to a defined range.

    /// Clamps the given value to the [0, 255] range.
    #[inline]
    pub fn clamp_u8(val: u16) -> u8 {
        if val & !0xff == 0 { val as u8 } else { 0xff }
    }

    /// Clamps the given value to the [-128, 127] range.
    #[inline]
    pub fn clamp_i8(val: i16) -> i8 {
        // Add 128 (0x80) to the given value, val, to make the i8 range of [-128,127] map to
        // [0,255]. Valid negative numbers are now positive so all bits above the 8th bit should be
        // 0. Check this by ANDing with 0xffffff00 (!0xff). If val wraps, the test is still valid as
        // it'll wrap around to the other numerical limit +/- 128, which is still well outside the
        // limits of an i8.
        if val.wrapping_add(0x80) & !0xff == 0 {
            val as i8
        }
        else {
            // The given value was determined to be outside the valid numerical range of i8.
            //
            // Shift right all the magnitude bits of val, leaving val to be either 0xff if val was
            // negative (sign bit was 1), or 0x00 if val was positive (sign bit was 0). Xor the
            // shift value with 0x7f (the positive limit) to obtain the appropriate numerical limit.
            //
            //  E.g., 0x7f ^ 0x00 = 0x7f (127)
            //  E.g., 0x7f ^ 0xff = 0x10 (-128)
            0x7f ^ val.wrapping_shr(15) as i8
        }
    }

    /// Clamps the given value to the [0, 65_535] range.
    #[inline]
    pub fn clamp_u16(val: u32) -> u16 {
        if val & !0xffff == 0 { val as u16 } else { 0xffff }
    }

    /// Clamps the given value to the [-32_767, 32_768] range.
    #[inline]
    pub fn clamp_i16(val: i32) -> i16 {
        if val.wrapping_add(0x8000) & !0xffff == 0 {
            val as i16
        }
        else {
            0x7fff ^ val.wrapping_shr(31) as i16
        }
    }

    /// Clamps the given value to the [0, 16_777_215] range.
    #[inline]
    pub fn clamp_u24(val: u32) -> u32 {
        if val & !0x00ff_ffff == 0 { val } else { 0x00ff_ffff }
    }

    /// Clamps the given value to the [-8_388_608, 8_388_607] range.
    #[inline]
    pub fn clamp_i24(val: i32) -> i32 {
        if val.wrapping_add(0x0080_0000) & !0x00ff_ffff == 0 {
            val
        }
        else {
            0x007f_ffff ^ val.wrapping_shr(31)
        }
    }

    /// Clamps the given value to the [0, 4_294_967_295] range.
    #[inline]
    pub fn clamp_u32(val: u64) -> u32 {
        if val & !0xffff_ffff == 0 { val as u32 } else { 0xffff_ffff }
    }

    /// Clamps the given value to the [-2_147_483_648, 2_147_483_647] range.
    #[inline]
    pub fn clamp_i32(val: i64) -> i32 {
        if val.wrapping_add(0x8000_0000) & !0xffff_ffff == 0 {
            val as i32
        }
        else {
            0x7fff_ffff ^ val.wrapping_shr(63) as i32
        }
    }

    /// Clamps the given value to the [-1.0, 1.0] range.
    #[inline]
    pub fn clamp_f32(val: f32) -> f32 {
        // This slightly inelegant code simply returns min(max(1.0, val), -1.0). In release mode on
        // platforms with SSE2 support, it will compile down to 4 SSE instructions with no branches,
        // thereby making it the most performant clamping implementation for floating-point samples.
        let mut clamped = val;
        clamped = if clamped > 1.0 { 1.0 } else { clamped };
        clamped = if clamped < -1.0 { -1.0 } else { clamped };
        clamped
    }

    /// Clamps the given value to the [-1.0, 1.0] range.
    #[inline]
    pub fn clamp_f64(val: f64) -> f64 {
        let mut clamped = val;
        clamped = if clamped > 1.0 { 1.0 } else { clamped };
        clamped = if clamped < -1.0 { -1.0 } else { clamped };
        clamped
    }

    #[cfg(test)]
    mod tests {
        use super::*;
        use std::{i8, i16, i32, i64, u8, u16, u32, u64};

        #[test]
        fn verify_clamp() {
            assert_eq!(clamp_u8(256u16), u8::MAX);
            assert_eq!(clamp_u8(u16::MAX), u8::MAX);

            assert_eq!(clamp_i8(128i16), i8::MAX);
            assert_eq!(clamp_i8(-129i16), i8::MIN);
            assert_eq!(clamp_i8(i16::MAX), i8::MAX);
            assert_eq!(clamp_i8(i16::MIN), i8::MIN);

            assert_eq!(clamp_u16(65536u32), u16::MAX);
            assert_eq!(clamp_u16(u32::MAX), u16::MAX);

            assert_eq!(clamp_i16(32_768i32), i16::MAX);
            assert_eq!(clamp_i16(-32_769i32), i16::MIN);
            assert_eq!(clamp_i16(i32::MAX), i16::MAX);
            assert_eq!(clamp_i16(i32::MIN), i16::MIN);

            assert_eq!(clamp_u32(4_294_967_296u64), u32::MAX);
            assert_eq!(clamp_u32(u64::MAX), u32::MAX);

            assert_eq!(clamp_i32(2_147_483_648i64), i32::MAX);
            assert_eq!(clamp_i32(-2_147_483_649i64), i32::MIN);
            assert_eq!(clamp_i32(i64::MAX), i32::MAX);
            assert_eq!(clamp_i32(i64::MIN), i32::MIN);

            assert_eq!(clamp_f32(1.1), 1.0);
            assert_eq!(clamp_f32(5.6), 1.0);
            assert_eq!(clamp_f32(0.5), 0.5);
            assert_eq!(clamp_f32(-1.1), -1.0);
            assert_eq!(clamp_f32(-5.6), -1.0);
            assert_eq!(clamp_f32(-0.5), -0.5);

            assert_eq!(clamp_f64(1.1), 1.0);
            assert_eq!(clamp_f64(5.6), 1.0);
            assert_eq!(clamp_f64(0.5), 0.5);
            assert_eq!(clamp_f64(-1.1), -1.0);
            assert_eq!(clamp_f64(-5.6), -1.0);
            assert_eq!(clamp_f64(-0.5), -0.5);
        }
    }
}

pub mod text {
    //! Utilities for decoding encoded characters from byte slices, and text handling.

    use char;

    /// Create an iterator over ASCII encoded characters in `buf`.
    ///
    /// Invalid characters are substituted with the Unicode `U+FFFD REPLACEMENT CHARACTER` (�).
    pub fn decode_ascii_lossy(buf: &[u8]) -> impl Iterator<Item = char> + '_ {
        buf.iter().map(|&c| if c.is_ascii() { char::from(c) } else { char::REPLACEMENT_CHARACTER })
    }

    /// Create an iterator over ISO/IEC 8859-1 encoded characters in `buf`.
    ///
    /// Invalid characters are substituted with the Unicode `U+FFFD REPLACEMENT CHARACTER` (�).
    ///
    /// Note: ISO/IEC 8859-1 does not encode any of the C0 or C1 control characters. This is in
    /// contrast to the IANA's similarly named ISO-8859-1 encoding which does. See
    /// [`decode_iso8859_1_lossy`].
    pub fn decode_isoiec8859_1_lossy(buf: &[u8]) -> impl Iterator<Item = char> + '_ {
        // ISO/IEC 8859-1 maps directly to the first two Unicode blocks (Basic Latin, & Latin-1
        // Supplement). However, unlike those Unicode code blocks (which form the ISO-8859-1
        // character set), the C0 and C1 control characters are not valid ISO/IEC 8859-1. Replace
        // these characters with the Unicode replacement character.
        buf.iter().map(|&c| {
            match c {
                // C0 and C1 control characters.
                0x00..=0x1f | 0x80..=0x9f => char::REPLACEMENT_CHARACTER,
                // All other non-control characters.
                _ => char::from(c),
            }
        })
    }

    /// Create an iterator over ISO-8859-1 (IANA defined character set) encoded characters in `buf`.
    ///
    /// Note: ISO-8859-1 as defined by the IANA, is a superset of the similarly named ISO/IEC 8859-1
    /// character set that encodes C0 and C1 control characters. See [`decode_isoiec8859_1_lossy`].
    pub fn decode_iso8859_1_lossy(buf: &[u8]) -> impl Iterator<Item = char> + '_ {
        buf.iter().copied().map(char::from)
    }

    /// Create an iterator over Windows-1252 encoded characters in `buf`.
    ///
    /// Invalid characters are substituted with the Unicode `U+FFFD REPLACEMENT CHARACTER` (�).
    ///
    /// Windows-1252 is a strict superset of ISO/IEC 8859-1. In addition to the characters defined
    /// in ISO/IEC 8859-1, it contains all C0 control characters from ISO-8859-1, but uses the space
    /// reserved for the C1 control characters for additional printable characters.
    ///
    /// Note: Some character codes residing in the C1 control character space are officially
    /// undefined in Windows-1252, but are mapped to C1 control characters when using Windows
    /// multi-byte conversion APIs. These character codes are considered invalid.
    pub fn decode_cp1252_lossy(buf: &[u8]) -> impl Iterator<Item = char> + '_ {
        // Mapping of C1 control codes to Unicode characters.
        const CP1252_C1: [char; 32] = [
            '\u{20ac}',                  // 0x80
            char::REPLACEMENT_CHARACTER, // 0x81
            '\u{201a}',                  // 0x82
            '\u{0192}',                  // 0x83
            '\u{201e}',                  // 0x84
            '\u{2026}',                  // 0x85
            '\u{2020}',                  // 0x86
            '\u{2021}',                  // 0x87
            '\u{02c6}',                  // 0x88
            '\u{2030}',                  // 0x89
            '\u{0160}',                  // 0x8a
            '\u{2039}',                  // 0x8b
            '\u{0152}',                  // 0x8c
            char::REPLACEMENT_CHARACTER, // 0x8d
            '\u{017d}',                  // 0x8e
            char::REPLACEMENT_CHARACTER, // 0x8f
            char::REPLACEMENT_CHARACTER, // 0x90
            '\u{2018}',                  // 0x91
            '\u{2019}',                  // 0x92
            '\u{201c}',                  // 0x93
            '\u{201d}',                  // 0x94
            '\u{2022}',                  // 0x95
            '\u{2013}',                  // 0x96
            '\u{2014}',                  // 0x97
            '\u{02dc}',                  // 0x98
            '\u{2122}',                  // 0x99
            '\u{0161}',                  // 0x9a
            '\u{203a}',                  // 0x9b
            '\u{0153}',                  // 0x9c
            char::REPLACEMENT_CHARACTER, // 0x9d
            '\u{017e}',                  // 0x9e
            '\u{0178}',                  // 0x9f
        ];

        buf.iter().map(|&c| match c {
            // The C1 control code range is mapped to other characters.
            0x80..=0x9f => CP1252_C1[usize::from(c - 0x80)],
            // All other characters map to identical Unicode blocks.
            _ => char::from(c),
        })
    }

    /// Create an iterator over UTF-16 big-endian (BE) encoded characters in `buf`.
    ///
    /// If a byte-order marker (BOM) is present, it will be respected. Otherwise, big-endian is
    /// assumed.
    ///
    /// Invalid characters are substituted with the Unicode `U+FFFD REPLACEMENT CHARACTER` (�).
    pub fn decode_utf16be_lossy(buf: &[u8]) -> impl Iterator<Item = char> + '_ {
        // Check byte-order-marker, then decode.
        match buf.first_chunk::<2>() {
            // Big-endian. Remove BOM.
            Some([0xfe, 0xff]) => decode_utf16_bytes(&buf[2..], u16::from_be_bytes),
            // Little-endian. Remove BOM.
            Some([0xff, 0xfe]) => decode_utf16_bytes(&buf[2..], u16::from_le_bytes),
            // No BOM, or buffer too short. Decode entire buffer as big-endian.
            _ => decode_utf16_bytes(buf, u16::from_be_bytes),
        }
    }

    /// Create an iterator over UTF-16 little-endian (LE) encoded characters in `buf`.
    ///
    /// If a byte-order marker (BOM) is present, it will be respected. Otherwise, little-endian is
    /// assumed.
    ///
    /// Invalid characters are substituted with the Unicode `U+FFFD REPLACEMENT CHARACTER` (�).
    pub fn decode_utf16le_lossy(buf: &[u8]) -> impl Iterator<Item = char> + '_ {
        // Check byte-order-marker, then decode.
        match buf.first_chunk::<2>() {
            // Big-endian. Remove BOM.
            Some([0xfe, 0xff]) => decode_utf16_bytes(&buf[2..], u16::from_be_bytes),
            // Little-endian. Remove BOM.
            Some([0xff, 0xfe]) => decode_utf16_bytes(&buf[2..], u16::from_le_bytes),
            // No BOM, or buffer too short. Decode entire buffer as little-endian.
            _ => decode_utf16_bytes(buf, u16::from_le_bytes),
        }
    }

    /// Utility function to create an iterator over UTF-16 encoded characters given a buffer and
    /// a conversion function from `[u8; 2]` to `u16`.
    fn decode_utf16_bytes(buf: &[u8], f: fn([u8; 2]) -> u16) -> impl Iterator<Item = char> + '_ {
        // TODO: Use `buf.array_chunks::<2>()` when stabilized.
        char::decode_utf16(buf.chunks_exact(2).map(move |bytes| f(bytes.try_into().unwrap())))
            .map(|r| r.unwrap_or(char::REPLACEMENT_CHARACTER))
    }

    pub mod filter {
        //! Character iterator predicates for categories of characters.

        /// Predicate for matching null-terminator characters.
        #[inline]
        pub fn null(c: &char) -> bool {
            *c == '\0'
        }

        /// Predicate for matching non-null terminator characters.
        #[inline]
        pub fn not_null(c: &char) -> bool {
            !null(c)
        }

        /// Predicate for matching C0 or C1 control characters.
        ///
        /// Returns `true` if the character is a C0 or C1 control character (Unicode CC category),
        /// or `false` otherwise.
        #[inline]
        pub fn control(c: &char) -> bool {
            matches!(c, '\0'..='\u{1f}' | '\u{7f}'..='\u{9f}')
        }

        /// Predicate for matching characters that are not C0 or C1 control characters.
        ///
        /// Returns `true` if the character is not a C0 or C1 control character (Unicode CC
        /// category), or `false` otherwise.
        #[inline]
        pub fn not_control(c: &char) -> bool {
            !control(c)
        }

        /// Predicate for matching C0 control characters.
        #[inline]
        pub fn c0_control(c: &char) -> bool {
            matches!(c, '\0'..='\u{1f}' | '\u{7f}')
        }

        /// Predicate for matching characters that are not C0 control characters.
        #[inline]
        pub fn not_c0_control(c: &char) -> bool {
            !c0_control(c)
        }

        /// Predicate for matching C1 control characters.
        #[inline]
        pub fn c1_control(c: &char) -> bool {
            matches!(c, '\u{80}'..='\u{9f}')
        }

        /// Predicate for matching characters that are not C1 control characters.
        #[inline]
        pub fn not_c1_control(c: &char) -> bool {
            !c1_control(c)
        }

        /// Predicate for matching ASCII text characters.
        ///
        /// ASCII text characters are all ASCII graphical characters and SPACE.
        #[inline]
        pub fn ascii_text(c: &char) -> bool {
            matches!(c, ' '..='~')
        }

        /// Predicate for matching non-text ASCII characters.
        ///
        /// ASCII text characters are all ASCII graphical characters and SPACE.
        #[inline]
        pub fn not_ascii_text(c: &char) -> bool {
            !ascii_text(c)
        }
    }

    #[cfg(test)]
    #[allow(clippy::byte_char_slices)]
    mod tests {
        #[test]
        fn verify_decode_utf16be() {
            use super::decode_utf16be_lossy;

            assert_eq!(decode_utf16be_lossy(&[]).collect::<String>(), String::new());
            assert_eq!(decode_utf16be_lossy(&[b' ']).collect::<String>(), String::new());
            assert_eq!(decode_utf16be_lossy(&[0x00, 0x20]).collect::<String>(), String::from(" "));

            // Little-endian forced by BOM.
            assert_eq!(
                decode_utf16be_lossy(&[0xff, 0xfe, 0x20, 0x00]).collect::<String>(),
                String::from(" ")
            );

            // BOM used in string.
            assert_eq!(
                decode_utf16be_lossy(&[0x00, 0x20, 0xff, 0xfe]).collect::<String>(),
                String::from(" \u{fffe}")
            );
            assert_eq!(
                decode_utf16be_lossy(&[0x00, 0x20, 0xfe, 0xff]).collect::<String>(),
                String::from(" \u{feff}")
            );
        }

        #[test]
        fn verify_decode_utf16le() {
            use super::decode_utf16le_lossy;

            assert_eq!(decode_utf16le_lossy(&[]).collect::<String>(), String::new());
            assert_eq!(decode_utf16le_lossy(&[b' ']).collect::<String>(), String::new());
            assert_eq!(decode_utf16le_lossy(&[0x20, 0x00]).collect::<String>(), String::from(" "));

            // Big-endian forced by BOM.
            assert_eq!(
                decode_utf16le_lossy(&[0xfe, 0xff, 0x00, 0x20]).collect::<String>(),
                String::from(" ")
            );

            // BOM used in string.
            assert_eq!(
                decode_utf16le_lossy(&[0x20, 0x00, 0xff, 0xfe]).collect::<String>(),
                String::from(" \u{feff}")
            );
            assert_eq!(
                decode_utf16le_lossy(&[0x20, 0x00, 0xfe, 0xff]).collect::<String>(),
                String::from(" \u{fffe}")
            );
        }
    }
}
