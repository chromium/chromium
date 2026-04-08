// most of this copied from const_format, which is mine, but still..

#[doc(hidden)]
pub(crate) struct ForEscaping {
    pub(crate) is_escaped: u128,
    pub(crate) is_backslash_escaped: u128,
    pub(crate) escape_char: [u8; 16],
}

impl ForEscaping {
    /// Gets the backslash escape for a character that is kwown to be escaped with a backslash.
    #[inline(always)]
    pub(crate) const fn get_backslash_escape(b: u8) -> u8 {
        FOR_ESCAPING.escape_char[(b & 0b1111) as usize]
    }

    // how long this byte inside a utf8 string takes to represent in debug formatting.
    pub(crate) const fn byte_len(c: u8) -> usize {
        if c < 128 {
            let shifted = 1 << c;

            if (FOR_ESCAPING.is_escaped & shifted) != 0 {
                if (FOR_ESCAPING.is_backslash_escaped & shifted) != 0 {
                    2
                } else {
                    4
                }
            } else {
                1
            }
        } else {
            1
        }
    }

    pub(crate) const fn is_escaped(c: u8) -> bool {
        (c < 128) && ((FOR_ESCAPING.is_escaped & (1 << c)) != 0)
    }

    pub(crate) const fn is_backslash_escaped(c: u8) -> bool {
        (c < 128) && ((FOR_ESCAPING.is_backslash_escaped & (1 << c)) != 0)
    }
}

#[doc(hidden)]
/// Converts 0..=0xF to its ascii representation of '0'..='9' and 'A'..='F'
#[inline(always)]
pub(crate) const fn hex_as_ascii(n: u8) -> u8 {
    if n < 10 {
        n + b'0'
    } else {
        n - 10 + b'A'
    }
}

#[doc(hidden)]
pub(crate) const FOR_ESCAPING: &ForEscaping = {
    let mut is_backslash_escaped = 0;

    let escaped = [
        (b'\t', b't'),
        (b'\n', b'n'),
        (b'\r', b'r'),
        (b'\'', b'\''),
        (b'"', b'"'),
        (b'\\', b'\\'),
    ];

    // Using the fact that the characters above all have different bit patterns for
    // the lowest 4 bits.
    let mut escape_char = [0u8; 16];

    let escaped_len = escaped.len();
    let mut i = 0;
    while i < escaped_len {
        let (code, escape) = escaped[i];
        is_backslash_escaped |= 1 << code;

        let ei = (code & 0b1111) as usize;
        if escape_char[ei] != 0 {
            panic!("Oh no, some escaped character uses the same 4 lower bits as another")
        }
        escape_char[ei] = escape;
        i += 1;
    }

    // Setting all the control characters as being escaped.
    let is_escaped = is_backslash_escaped | 0xFFFF_FFFF;

    &ForEscaping {
        escape_char,
        is_backslash_escaped,
        is_escaped,
    }
};
