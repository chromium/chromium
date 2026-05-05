// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Base64 decoding.

// Depending on which features are enabled, some parsers may be unused. Disable this lint as it
// would be too difficult to individually waive the lint.
#![allow(dead_code)]

/// Decode a RFC4648 Base64 encoded string.
pub fn decode(encoded: &str) -> Option<Box<[u8]>> {
    // A sentinel value indicating that an invalid symbol was encountered.
    const BAD_SYM: u8 = 0xff;

    /// Generates a lookup table mapping RFC4648 base64 symbols to their 6-bit decoded values at
    /// compile time.
    const fn rfc4648_base64_symbols() -> [u8; 256] {
        const SYMBOLS: &[u8; 64] =
            b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        let mut table = [BAD_SYM; 256];
        let mut i = 0;

        while i < SYMBOLS.len() {
            table[SYMBOLS[i] as usize] = i as u8;
            i += 1
        }

        table
    }

    const SYM_VALUE: [u8; 256] = rfc4648_base64_symbols();

    // Trim padding, since it's not required for decoding.
    let encoded = encoded.trim_end_matches('=');

    // Each valid base64 symbol decodes to 6 bits. Therefore, the decoded byte length is 3 / 4 the
    // number of symbols in the base64 encoded string.
    let mut decoded = Vec::with_capacity((encoded.len() * 3) / 4);

    // Decode in chunks of 4 symbols, yielding 3 bytes per chunk. Since base64 symbols are ASCII
    // characters (1 byte per character), iterate over the bytes of the base64 string instead of
    // chars (4 bytes per character). This allows the use of a lookup table to determine the symbol
    // value.
    let mut iter = encoded.as_bytes().chunks_exact(4);

    for enc in &mut iter {
        let v0 = SYM_VALUE[usize::from(enc[0])];
        let v1 = SYM_VALUE[usize::from(enc[1])];
        let v2 = SYM_VALUE[usize::from(enc[2])];
        let v3 = SYM_VALUE[usize::from(enc[3])];

        // Check for invalid symbols.
        if v0 == BAD_SYM || v1 == BAD_SYM || v2 == BAD_SYM || v3 == BAD_SYM {
            return None;
        }

        // 6 bits from v0, 2 bits from v1 (4 remaining).
        decoded.push(((v0 & 0x3f) << 2) | (v1 >> 4));
        // 4 bits from v1, 4 bits from v2 (2 remaining).
        decoded.push(((v1 & 0x0f) << 4) | (v2 >> 2));
        // 2 bits from v2, 6 bits from v3 (0 remaining).
        decoded.push(((v2 & 0x03) << 6) | (v3 >> 0));
    }

    // Decode the remaining 2 to 3 symbols.
    let rem = iter.remainder();

    // If there are atleast 2 symbols remaining, then a minimum of one extra byte may be decoded.
    if rem.len() >= 2 {
        let v0 = SYM_VALUE[usize::from(rem[0])];
        let v1 = SYM_VALUE[usize::from(rem[1])];

        if v0 == BAD_SYM || v1 == BAD_SYM {
            return None;
        }

        decoded.push(((v0 & 0x3f) << 2) | (v1 >> 4));

        // If there were 3 symbols remaining, then one additional byte may be decoded.
        if rem.len() >= 3 {
            let v2 = SYM_VALUE[usize::from(rem[2])];

            if v2 == BAD_SYM {
                return None;
            }

            decoded.push(((v1 & 0x0f) << 4) | (v2 >> 2));
        }
    }
    else if rem.len() == 1 {
        // Atleast 2 symbols are required to decode a single byte. Therefore, this is an error.
        return None;
    }

    Some(decoded.into_boxed_slice())
}

#[cfg(test)]
mod tests {
    use super::decode;

    #[test]
    fn verify_base64_decode() {
        // Valid, with padding.
        assert_eq!(Some(b"".as_slice()), decode("").as_deref());
        assert_eq!(Some(b"f".as_slice()), decode("Zg==").as_deref());
        assert_eq!(Some(b"fo".as_slice()), decode("Zm8=").as_deref());
        assert_eq!(Some(b"foo".as_slice()), decode("Zm9v").as_deref());
        assert_eq!(Some(b"foob".as_slice()), decode("Zm9vYg==").as_deref());
        assert_eq!(Some(b"fooba".as_slice()), decode("Zm9vYmE=").as_deref());
        assert_eq!(Some(b"foobar".as_slice()), decode("Zm9vYmFy").as_deref());
        // Valid, without padding.
        assert_eq!(Some(b"".as_slice()), decode("").as_deref());
        assert_eq!(Some(b"f".as_slice()), decode("Zg").as_deref());
        assert_eq!(Some(b"fo".as_slice()), decode("Zm8").as_deref());
        assert_eq!(Some(b"foo".as_slice()), decode("Zm9v").as_deref());
        assert_eq!(Some(b"foob".as_slice()), decode("Zm9vYg").as_deref());
        assert_eq!(Some(b"fooba".as_slice()), decode("Zm9vYmE").as_deref());
        assert_eq!(Some(b"foobar".as_slice()), decode("Zm9vYmFy").as_deref());
        // Invalid.
        assert_eq!(None, decode("a").as_deref());
        assert_eq!(None, decode("ab!c").as_deref());
        assert_eq!(None, decode("ab=c").as_deref());
    }
}
