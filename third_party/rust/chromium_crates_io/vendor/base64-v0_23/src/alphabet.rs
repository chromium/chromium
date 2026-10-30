//! Provides [Alphabet] and constants for alphabets commonly used in the wild.

use core::{array, convert, fmt};
#[cfg(any(feature = "std", test))]
use std::error;

/// Unsurprisingly, there are 64 symbols in a Base64 alphabet.
const ALPHABET_LEN: usize = 64;

/// Pad symbol for non-weird alphabets.
pub(crate) const PADDING_SYMBOL: Symbol = Symbol(b'=');

/// An alphabet defines the 64 ASCII characters (symbols) used for base64.
///
/// Common alphabets are provided as constants, and custom alphabets
/// can be made via `from_str` or the `TryFrom<str>` implementation.
///
/// # Examples
///
/// Building and using a custom Alphabet:
///
/// ```
/// let custom = base64::alphabet::Alphabet::new("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/").unwrap();
///
/// let engine = base64::engine::GeneralPurpose::new(
///     &custom,
///     base64::engine::general_purpose::PAD);
/// ```
///
/// Building a const:
///
/// ```
/// use base64::alphabet::Alphabet;
///
/// static CUSTOM: Alphabet = {
///     // Result::unwrap() isn't const yet, but panic!() is OK
///     match Alphabet::new("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/") {
///         Ok(x) => x,
///         Err(_) => panic!("creation of alphabet failed"),
///     }
/// };
/// ```
///
/// Building lazily:
///
/// ```
/// use base64::alphabet::Alphabet;
/// use std::sync::LazyLock;
///
/// static CUSTOM: LazyLock<Alphabet> = LazyLock::new(||
///     Alphabet::new("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/").unwrap()
/// );
/// ```
#[derive(Clone, Eq, PartialEq)]
pub struct Alphabet {
    /// All bytes are valid symbols, but left as u8 to allow `.as_str()` to work.
    pub(crate) symbols: [u8; ALPHABET_LEN],
    pub(crate) padding: Symbol,
}

impl Alphabet {
    /// Performs no checks so that it can be const.
    /// Used only for known-valid strings.
    const fn from_str_unchecked(alphabet: &str, padding: Symbol) -> Self {
        let mut symbols = [0_u8; ALPHABET_LEN];
        let source_bytes = alphabet.as_bytes();

        // a way to copy that's allowed in const fn
        let mut index = 0;
        while index < ALPHABET_LEN {
            symbols[index] = source_bytes[index];
            index += 1;
        }

        Self { symbols, padding }
    }

    /// Create an `Alphabet` from a string of 64 unique printable ASCII bytes with `=` as the
    /// padding symbol.
    ///
    /// The padding symbol `=` is not allowed in the alphabet.
    ///
    /// See [`Self::new_with_padding`] if a non-default padding symbol is needed.
    pub const fn new(alphabet: &str) -> Result<Self, ParseAlphabetError> {
        Self::new_with_padding(alphabet, PADDING_SYMBOL)
    }

    /// Create an `Alphabet` from a string of 64 unique printable ASCII bytes, with a custom
    /// padding symbol.
    ///
    /// The padding symbol must not appear in the alphabet.
    ///
    /// This is meant for strange alphabets that don't use `=` as the padding symbol.
    pub const fn new_with_padding(
        alphabet: &str,
        padding: Symbol,
    ) -> Result<Self, ParseAlphabetError> {
        let bytes = alphabet.as_bytes();
        if bytes.len() != ALPHABET_LEN {
            return Err(ParseAlphabetError::InvalidLength);
        }

        {
            let mut index = 0;
            while index < ALPHABET_LEN {
                let byte = bytes[index];

                if !is_valid_b64_symbol(byte) {
                    return Err(ParseAlphabetError::UnprintableByte(byte));
                }
                if byte == padding.as_u8() {
                    return Err(ParseAlphabetError::ReservedByte(byte));
                }

                // Check for duplicates while staying within what const allows.
                // It's n^2, but only over 64 hot bytes, and only once, so it's likely in the single digit
                // microsecond range.

                let mut probe_index = 0;
                while probe_index < ALPHABET_LEN {
                    if probe_index != index && byte == bytes[probe_index] {
                        return Err(ParseAlphabetError::DuplicatedByte(byte));
                    }

                    probe_index += 1;
                }

                index += 1;
            }
        }

        Ok(Self::from_str_unchecked(alphabet, padding))
    }

    /// A `&str` containing the symbols in the `Alphabet` (excluding padding)
    #[must_use]
    pub fn as_str(&self) -> &str {
        core::str::from_utf8(&self.symbols).unwrap()
    }

    /// The 64 symbols of the alphabet (excluding padding).
    pub fn symbols(&self) -> [Symbol; ALPHABET_LEN] {
        array::from_fn(|i| {
            // safe to construct Symbol since all symbol bytes have already been checked
            Symbol(self.symbols[i])
        })
    }

    /// The symbol used for padding.
    pub fn padding(&self) -> Symbol {
        self.padding
    }
}

impl fmt::Debug for Alphabet {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Alphabet {{ symbols: {:?}, padding: '{:?}' }}",
            self.as_str(),
            self.padding
        )
    }
}

/// An ASCII printable byte suitable for use as a base64 symbol in an alphabet or as custom padding.
///
/// This doesn't mean that a particular symbol is used in any particular alphabet, just that it
/// could be used in one.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Symbol(u8);

impl Symbol {
    /// Returns `Some` if `symbol` is a valid printable ASCII symbol, otherwise `None`.
    pub const fn new(symbol: u8) -> Option<Self> {
        if is_valid_b64_symbol(symbol) {
            Some(Self(symbol))
        } else {
            None
        }
    }

    /// Returns the symbol as an ASCII byte.
    pub const fn as_u8(&self) -> u8 {
        self.0
    }

    /// Returns the symbol as a char.
    pub fn as_char(&self) -> char {
        // ascii u8 is the same as the code point, conveniently
        char::from(self.0)
    }
}

impl From<Symbol> for u8 {
    fn from(value: Symbol) -> Self {
        value.0
    }
}

impl fmt::Debug for Symbol {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_char())
    }
}

/// Must be ascii printable. 127 (DEL) is commonly considered printable
/// for some reason but clearly unsuitable for base64.
pub(crate) const fn is_valid_b64_symbol(byte: u8) -> bool {
    byte >= 32_u8 && byte <= 126_u8
}

impl convert::TryFrom<&str> for Alphabet {
    type Error = ParseAlphabetError;

    fn try_from(value: &str) -> Result<Self, Self::Error> {
        Self::new(value)
    }
}

/// Possible errors when constructing an [Alphabet] from a `str`.
#[derive(Debug, Eq, PartialEq)]
pub enum ParseAlphabetError {
    /// Alphabets must be 64 ASCII bytes
    InvalidLength,
    /// All bytes must be unique
    DuplicatedByte(u8),
    /// All bytes must be printable (in the range `[32, 126]`).
    UnprintableByte(u8),
    /// Alphabet cannot contain the pad symbol (`=` by default)
    ReservedByte(u8),
}

impl fmt::Display for ParseAlphabetError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidLength => write!(f, "Invalid length - must be 64 bytes"),
            Self::DuplicatedByte(b) => write!(f, "Duplicated byte: {:#04x}", b),
            Self::UnprintableByte(b) => write!(f, "Unprintable byte: {:#04x}", b),
            Self::ReservedByte(b) => write!(f, "Reserved byte: {:#04x}", b),
        }
    }
}

#[cfg(any(feature = "std", test))]
impl error::Error for ParseAlphabetError {}

/// The standard alphabet (with `+` and `/`) specified in [RFC 4648][].
///
/// [RFC 4648]: https://datatracker.ietf.org/doc/html/rfc4648#section-4
pub const STANDARD: Alphabet = Alphabet::from_str_unchecked(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
    PADDING_SYMBOL,
);

/// The URL-safe alphabet (with `-` and `_`) specified in [RFC 4648][].
///
/// [RFC 4648]: https://datatracker.ietf.org/doc/html/rfc4648#section-5
pub const URL_SAFE: Alphabet = Alphabet::from_str_unchecked(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_",
    PADDING_SYMBOL,
);

/// The `crypt(3)` alphabet (with `.` and `/` as the _first_ two characters).
///
/// Not standardized, but folk wisdom on the net asserts that this alphabet is what crypt uses.
pub const CRYPT: Alphabet = Alphabet::from_str_unchecked(
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
    PADDING_SYMBOL,
);

/// The bcrypt alphabet.
pub const BCRYPT: Alphabet = Alphabet::from_str_unchecked(
    "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    PADDING_SYMBOL,
);

/// The alphabet used in IMAP-modified UTF-7 (with `+` and `,`).
///
/// See [RFC 3501](https://tools.ietf.org/html/rfc3501#section-5.1.3)
pub const IMAP_MUTF7: Alphabet = Alphabet::from_str_unchecked(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,",
    PADDING_SYMBOL,
);

/// The alphabet used in `BinHex` 4.0 files.
///
/// See [BinHex 4.0 Definition](http://files.stairways.com/other/binhex-40-specs-info.txt)
pub const BIN_HEX: Alphabet = Alphabet::from_str_unchecked(
    "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr",
    PADDING_SYMBOL,
);

#[cfg(test)]
mod tests {
    use crate::alphabet::*;
    use core::convert::TryFrom as _;

    #[test]
    fn detects_duplicate_start() {
        assert_eq!(
            ParseAlphabetError::DuplicatedByte(b'A'),
            Alphabet::new("AACDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
                .unwrap_err()
        );
    }

    #[test]
    fn detects_duplicate_end() {
        assert_eq!(
            ParseAlphabetError::DuplicatedByte(b'/'),
            Alphabet::new("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789//")
                .unwrap_err()
        );
    }

    #[test]
    fn detects_duplicate_middle() {
        assert_eq!(
            ParseAlphabetError::DuplicatedByte(b'Z'),
            Alphabet::new("ABCDEFGHIJKLMNOPQRSTUVWXYZZbcdefghijklmnopqrstuvwxyz0123456789+/")
                .unwrap_err()
        );
    }

    #[test]
    fn detects_length() {
        assert_eq!(
            ParseAlphabetError::InvalidLength,
            Alphabet::new(
                "xxxxxxxxxABCDEFGHIJKLMNOPQRSTUVWXYZZbcdefghijklmnopqrstuvwxyz0123456789+/",
            )
            .unwrap_err()
        );
    }

    #[test]
    fn detects_padding() {
        assert_eq!(
            ParseAlphabetError::ReservedByte(b'='),
            Alphabet::new("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+=")
                .unwrap_err()
        );
    }

    #[test]
    fn detects_unprintable() {
        // form feed
        assert_eq!(
            ParseAlphabetError::UnprintableByte(0xc),
            Alphabet::new("\x0cBCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
                .unwrap_err()
        );
    }

    #[test]
    fn same_as_unchecked() {
        assert_eq!(
            STANDARD,
            Alphabet::try_from("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
                .unwrap()
        );
    }

    #[test]
    fn str_same_as_input() {
        let alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        let a = Alphabet::try_from(alphabet).unwrap();
        assert_eq!(alphabet, a.as_str())
    }

    #[test]
    fn symbol_matches_char_for_all_valid_symbols() {
        for symbol in (0..=u8::MAX).filter_map(Symbol::new) {
            // treat the byte as UTF-8
            let bytes = &[symbol.as_u8()];
            let s = std::str::from_utf8(bytes).unwrap();
            assert_eq!(1, s.chars().count());

            let char = s.chars().next().unwrap();
            assert_eq!(char, symbol.as_char());
        }
    }

    #[test]
    fn alphabet_debug() {
        assert_eq!(
            r##"Alphabet { symbols: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", padding: '=' }"##,
            format!("{STANDARD:?}")
        );
    }

    #[test]
    fn alphabet_symbols() {
        assert_eq!(
            STANDARD.as_str(),
            STANDARD
                .symbols()
                .iter()
                .map(|s| s.as_char())
                .collect::<String>()
        );
    }
}
