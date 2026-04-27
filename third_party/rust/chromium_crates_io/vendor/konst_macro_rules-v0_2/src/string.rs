use core::fmt;

// copied from core
// https://github.com/rust-lang/rust/blob/673d0db5e393e9c64897005b470bfeb6d5aec61b/library/core/src/str/validations.rs#L232
const UTF8_CHAR_WIDTH: &[u8; 256] = &[
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, // 0x1F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, // 0x3F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, // 0x5F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, // 0x7F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, // 0x9F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, // 0xBF
    0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, // 0xDF
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 0xEF
    4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xFF
];

///////////////////////////////////////////////////////////////////////////////

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Utf8Error {
    valid_up_to: usize,
}

impl Utf8Error {
    /// The index up to which a `&str` can be validly constructed from the input `&[u8]`.
    ///
    /// `&input[..error.valid_up_to()]` is valid utf8.
    pub const fn valid_up_to(&self) -> usize {
        self.valid_up_to
    }

    /// For erroring with an error message.
    pub const fn panic(&self) -> ! {
        let offset = self.valid_up_to();
        [/*Could not interpret bytes from offset as a str*/][offset]
    }
}

impl fmt::Display for Utf8Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "invalid utf-8 sequence starting from index {}",
            self.valid_up_to
        )
    }
}

///////////////////////////////////////////////////////////////////////////////

pub const fn check_utf8(mut bytes: &[u8]) -> Result<(), Utf8Error> {
    let in_len = bytes.len();

    macro_rules! try_nexts {
        ($rema:ident, [$second:ident $(,$nexts:ident)*], $extra_checks:expr ) => ({
            if let [$second, $($nexts,)* ref rem @ ..] = *$rema  {
                if $( is_continuation_byte($nexts) && )* $extra_checks {
                    bytes = rem;
                } else {
                    return Err(Utf8Error{valid_up_to: in_len - bytes.len()});
                }

            } else {
                return Err(Utf8Error{valid_up_to: in_len - bytes.len()});
            }
        })
    }

    while let [first, ref rema @ ..] = *bytes {
        let utf8len = UTF8_CHAR_WIDTH[first as usize];
        if bytes.len() < utf8len as usize {
            return Err(Utf8Error {
                valid_up_to: in_len - bytes.len(),
            });
        }

        match utf8len {
            1 => {
                bytes = rema;
                continue;
            }
            2 => try_nexts!(rema, [second], is_continuation_byte(second)),
            3 => try_nexts!(
                rema,
                [second, third],
                matches! {
                    (first, second),
                    (0xE0, 0xA0..=0xBF)
                    | (0xE1..=0xEC, 0x80..=0xBF)
                    | (0xED, 0x80..=0x9F)
                    | (0xEE..=0xEF, 0x80..=0xBF)
                }
            ),
            4 => try_nexts!(
                rema,
                [second, third, fourth],
                matches!(
                    (first, second),
                    (0xF0, 0x90..=0xBF) | (0xF1..=0xF3, 0x80..=0xBF) | (0xF4, 0x80..=0x8F)
                )
            ),
            _ => {
                return Err(Utf8Error {
                    valid_up_to: in_len - bytes.len(),
                })
            }
        }
    }
    Ok(())
}

const fn is_continuation_byte(b: u8) -> bool {
    (b & 0b11_000000) == 0b10_000000
}

#[cfg(not(feature = "rust_1_55"))]
#[macro_export]
macro_rules! from_utf8_macro {
    ($slice:expr) => {
        match $slice {
            x => unsafe {
                match $crate::string::check_utf8(x) {
                    $crate::__::Ok(()) => {
                        let ptr = x as *const [$crate::__::u8] as *const $crate::__::str;
                        unsafe { Ok($crate::utils::Dereference { ptr }.reff) }
                    }
                    $crate::__::Err(e) => $crate::__::Err(e),
                }
            },
        }
    };
}

#[cfg(feature = "rust_1_55")]
#[macro_export]
macro_rules! from_utf8_macro {
    ($slice:expr) => {
        $crate::string::from_utf8_fn($slice)
    };
}

#[cfg(feature = "rust_1_55")]
#[inline]
pub const fn from_utf8_fn(slice: &[u8]) -> Result<&str, Utf8Error> {
    match check_utf8(slice) {
        Ok(()) => unsafe { Ok(core::str::from_utf8_unchecked(slice)) },
        Err(e) => Err(e),
    }
}
