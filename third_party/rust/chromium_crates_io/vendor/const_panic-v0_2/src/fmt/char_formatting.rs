//! `char`-formatted related items

use crate::{
    fmt::{FmtArg, FmtKind},
    fmt_impls::basic_fmt_impls::primitive_static_panicfmt,
    panic_val::{PanicVal, PanicVariant},
    utils::{string_cap, PreFmtString, StartAndBytes},
};

#[cfg(all(test, not(miri)))]
mod tests;

impl PanicVal<'_> {
    /// Constructs a `PanicVal` from a `char`.
    pub const fn from_char(c: char, fmtarg: FmtArg) -> Self {
        let StartAndBytes { start, bytes } = match fmtarg.fmt_kind {
            FmtKind::Display => {
                let (arr, len) = char_to_utf8(c);
                crate::utils::tail_byte_array::<{ string_cap::PREFMT }>(len, &arr)
            }
            FmtKind::Debug => {
                let fmtchar = char_to_debug(c);
                crate::utils::tail_byte_array(fmtchar.len(), &fmtchar.encoded)
            }
        };
        // SAFETY:
        // char_to_utf8 is exhaustively tested in the tests module.
        // char_to_debug is exhaustively tested in the tests module.
        // tail_byte_array is also tested for smaller/equal/larger input arrays.
        let prefmt = unsafe { PreFmtString::new(start, bytes) };
        PanicVal {
            var: PanicVariant::PreFmt(prefmt),
        }
    }
}

primitive_static_panicfmt! {
    fn[](&self: char, fmtarg) {
        PanicVal::from_char(*self.0, fmtarg)
    }
}

/// Converts 0..=0xF to its ascii representation of '0'..='9' and 'A'..='F'
#[inline]
const fn hex_as_ascii(n: u8) -> u8 {
    if n < 10 {
        n + b'0'
    } else {
        n - 10 + b'A'
    }
}

#[cfg(test)]
pub(crate) const fn char_debug_len(c: char) -> usize {
    let inner = match c {
        '\t' | '\r' | '\n' | '\\' | '\'' => 2,
        '\x00'..='\x1F' => 4,
        _ => c.len_utf8(),
    };
    inner + 2
}

const fn char_to_utf8(char: char) -> ([u8; 4], usize) {
    let u32 = char as u32;
    match u32 {
        0..=127 => ([u32 as u8, 0, 0, 0], 1),
        0x80..=0x7FF => {
            let b0 = 0b1100_0000 | (u32 >> 6) as u8;
            let b1 = 0b1000_0000 | (u32 & 0b0011_1111) as u8;
            ([b0, b1, 0, 0], 2)
        }
        0x800..=0xFFFF => {
            let b0 = 0b1110_0000 | (u32 >> 12) as u8;
            let b1 = 0b1000_0000 | ((u32 >> 6) & 0b0011_1111) as u8;
            let b2 = 0b1000_0000 | (u32 & 0b0011_1111) as u8;
            ([b0, b1, b2, 0], 3)
        }
        0x10000..=u32::MAX => {
            let b0 = 0b1111_0000 | (u32 >> 18) as u8;
            let b1 = 0b1000_0000 | ((u32 >> 12) & 0b0011_1111) as u8;
            let b2 = 0b1000_0000 | ((u32 >> 6) & 0b0011_1111) as u8;
            let b3 = 0b1000_0000 | (u32 & 0b0011_1111) as u8;
            ([b0, b1, b2, b3], 4)
        }
    }
}

/// Display formats a `char`
pub const fn char_to_display(char: char) -> FmtChar {
    let ([b0, b1, b2, b3], len) = char_to_utf8(char);
    FmtChar {
        encoded: [b0, b1, b2, b3, 0, 0, 0, 0, 0, 0, 0, 0],
        len: len as u8,
    }
}

/// Debug formats a `char`
pub const fn char_to_debug(c: char) -> FmtChar {
    let ([b0, b1, b2, b3], len) = match c {
        '\t' => (*br#"\t  "#, 2),
        '\r' => (*br#"\r  "#, 2),
        '\n' => (*br#"\n  "#, 2),
        '\\' => (*br#"\\  "#, 2),
        '\'' => (*br#"\'  "#, 2),
        '\"' => (*br#""   "#, 1),
        '\x00'..='\x1F' => {
            let n = c as u8;
            (
                [b'\\', b'x', hex_as_ascii(n >> 4), hex_as_ascii(n & 0b1111)],
                4,
            )
        }
        _ => char_to_utf8(c),
    };

    let mut encoded = [b'\'', b0, b1, b2, b3, 0, 0, 0, 0, 0, 0, 0];
    encoded[len + 1] = b'\'';

    FmtChar {
        encoded,
        len: (len as u8) + 2,
    }
}

/// An byte slice with a display/debug formatted `char`.
///
/// To get the encoded character, you need to do
/// `&fmt_char.encoded()[..fmt_char.len()]`.
#[derive(Copy, Clone)]
pub struct FmtChar {
    encoded: [u8; 12],
    len: u8,
}

impl FmtChar {
    /// Array which contains the display/debug-formatted  `char`,
    /// and trailing `0` padding.
    pub const fn encoded(&self) -> &[u8; 12] {
        &self.encoded
    }

    /// The length of the subslice that contains the formatted character.
    pub const fn len(&self) -> usize {
        self.len as usize
    }
}
