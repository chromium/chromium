//! Convert a string in IBM codepage 437 to UTF-8

/// Trait to convert IBM codepage 437 to the target type
pub trait FromCp437 {
    /// Target type
    type Target;

    /// Function that does the conversion from cp437.
    /// Generally allocations will be avoided if all data falls into the ASCII range.
    #[allow(clippy::wrong_self_convention)]
    fn from_cp437(self) -> Self::Target;
}

impl<'a> FromCp437 for &'a [u8] {
    type Target = Result<std::borrow::Cow<'a, str>, std::io::Error>;

    fn from_cp437(self) -> Self::Target {
        let target = if self.iter().any(|c| *c >= 0x80) {
            let s = self.iter().copied().map(to_char).collect::<String>();
            std::borrow::Cow::Owned(s)
        } else {
            let s = std::str::from_utf8(self).map_err(|e| {
                std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    format!("Cannot translate path from cp437: {e}"),
                )
            })?;
            std::borrow::Cow::Borrowed(s)
        };
        Ok(target)
    }
}
fn to_char(input: u8) -> char {
    match input {
        0x00..=0x7f => input as char,
        0x80 => '\u{00c7}',
        0x81 => '\u{00fc}',
        0x82 => '\u{00e9}',
        0x83 => '\u{00e2}',
        0x84 => '\u{00e4}',
        0x85 => '\u{00e0}',
        0x86 => '\u{00e5}',
        0x87 => '\u{00e7}',
        0x88 => '\u{00ea}',
        0x89 => '\u{00eb}',
        0x8a => '\u{00e8}',
        0x8b => '\u{00ef}',
        0x8c => '\u{00ee}',
        0x8d => '\u{00ec}',
        0x8e => '\u{00c4}',
        0x8f => '\u{00c5}',
        0x90 => '\u{00c9}',
        0x91 => '\u{00e6}',
        0x92 => '\u{00c6}',
        0x93 => '\u{00f4}',
        0x94 => '\u{00f6}',
        0x95 => '\u{00f2}',
        0x96 => '\u{00fb}',
        0x97 => '\u{00f9}',
        0x98 => '\u{00ff}',
        0x99 => '\u{00d6}',
        0x9a => '\u{00dc}',
        0x9b => '\u{00a2}',
        0x9c => '\u{00a3}',
        0x9d => '\u{00a5}',
        0x9e => '\u{20a7}',
        0x9f => '\u{0192}',
        0xa0 => '\u{00e1}',
        0xa1 => '\u{00ed}',
        0xa2 => '\u{00f3}',
        0xa3 => '\u{00fa}',
        0xa4 => '\u{00f1}',
        0xa5 => '\u{00d1}',
        0xa6 => '\u{00aa}',
        0xa7 => '\u{00ba}',
        0xa8 => '\u{00bf}',
        0xa9 => '\u{2310}',
        0xaa => '\u{00ac}',
        0xab => '\u{00bd}',
        0xac => '\u{00bc}',
        0xad => '\u{00a1}',
        0xae => '\u{00ab}',
        0xaf => '\u{00bb}',
        0xb0 => '\u{2591}',
        0xb1 => '\u{2592}',
        0xb2 => '\u{2593}',
        0xb3 => '\u{2502}',
        0xb4 => '\u{2524}',
        0xb5 => '\u{2561}',
        0xb6 => '\u{2562}',
        0xb7 => '\u{2556}',
        0xb8 => '\u{2555}',
        0xb9 => '\u{2563}',
        0xba => '\u{2551}',
        0xbb => '\u{2557}',
        0xbc => '\u{255d}',
        0xbd => '\u{255c}',
        0xbe => '\u{255b}',
        0xbf => '\u{2510}',
        0xc0 => '\u{2514}',
        0xc1 => '\u{2534}',
        0xc2 => '\u{252c}',
        0xc3 => '\u{251c}',
        0xc4 => '\u{2500}',
        0xc5 => '\u{253c}',
        0xc6 => '\u{255e}',
        0xc7 => '\u{255f}',
        0xc8 => '\u{255a}',
        0xc9 => '\u{2554}',
        0xca => '\u{2569}',
        0xcb => '\u{2566}',
        0xcc => '\u{2560}',
        0xcd => '\u{2550}',
        0xce => '\u{256c}',
        0xcf => '\u{2567}',
        0xd0 => '\u{2568}',
        0xd1 => '\u{2564}',
        0xd2 => '\u{2565}',
        0xd3 => '\u{2559}',
        0xd4 => '\u{2558}',
        0xd5 => '\u{2552}',
        0xd6 => '\u{2553}',
        0xd7 => '\u{256b}',
        0xd8 => '\u{256a}',
        0xd9 => '\u{2518}',
        0xda => '\u{250c}',
        0xdb => '\u{2588}',
        0xdc => '\u{2584}',
        0xdd => '\u{258c}',
        0xde => '\u{2590}',
        0xdf => '\u{2580}',
        0xe0 => '\u{03b1}',
        0xe1 => '\u{00df}',
        0xe2 => '\u{0393}',
        0xe3 => '\u{03c0}',
        0xe4 => '\u{03a3}',
        0xe5 => '\u{03c3}',
        0xe6 => '\u{00b5}',
        0xe7 => '\u{03c4}',
        0xe8 => '\u{03a6}',
        0xe9 => '\u{0398}',
        0xea => '\u{03a9}',
        0xeb => '\u{03b4}',
        0xec => '\u{221e}',
        0xed => '\u{03c6}',
        0xee => '\u{03b5}',
        0xef => '\u{2229}',
        0xf0 => '\u{2261}',
        0xf1 => '\u{00b1}',
        0xf2 => '\u{2265}',
        0xf3 => '\u{2264}',
        0xf4 => '\u{2320}',
        0xf5 => '\u{2321}',
        0xf6 => '\u{00f7}',
        0xf7 => '\u{2248}',
        0xf8 => '\u{00b0}',
        0xf9 => '\u{2219}',
        0xfa => '\u{00b7}',
        0xfb => '\u{221a}',
        0xfc => '\u{207f}',
        0xfd => '\u{00b2}',
        0xfe => '\u{25a0}',
        0xff => '\u{00a0}',
    }
}

#[cfg(test)]
mod test {
    #[test]
    fn to_char_valid() {
        for i in u8::MIN..=u8::MAX {
            super::to_char(i);
        }
    }

    #[test]
    fn ascii() {
        for i in 0x00..0x80 {
            assert_eq!(super::to_char(i), i as char);
        }
    }

    #[test]
    #[allow(unknown_lints)] // invalid_from_utf8 was added in rust 1.72
    #[allow(invalid_from_utf8)]
    fn example_slice() {
        use super::FromCp437;
        let data = b"Cura\x87ao";
        assert!(::std::str::from_utf8(data).is_err());
        let converted = &(*data.from_cp437().unwrap());
        assert_eq!(converted, "Curaçao");
    }

    #[test]
    fn example_vec() {
        use super::FromCp437;
        let data = vec![0xCC, 0xCD, 0xCD, 0xB9];
        assert!(String::from_utf8(data.clone()).is_err());
        let converted = &(*data.from_cp437().unwrap());
        assert_eq!(converted, "╠══╣");
    }
}
