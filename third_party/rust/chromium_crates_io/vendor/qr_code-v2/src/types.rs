//! Type to support QR code encoding

use crate::cast::As;
use crate::structured::StructuredQrError;
use std::cmp::{Ordering, PartialOrd};
use std::default::Default;
use std::fmt::{Display, Error, Formatter};
use std::ops::Not;

/// `QrError` encodes the error encountered when generating a QR code.
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum QrError {
    /// The data is too long to encode into a QR code for the given version.
    DataTooLong,

    /// The provided version / error correction level combination is invalid.
    InvalidVersion,

    /// Some characters in the data cannot be supported by the provided QR code
    /// version.
    UnsupportedCharacterSet,

    /// The provided ECI designator is invalid. A valid designator should be
    /// between 0 and 999999.
    InvalidEciDesignator,

    /// A character not belonging to the character set is found.
    InvalidCharacter,

    ///
    Structured(StructuredQrError),
}

impl Display for QrError {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        match *self {
            QrError::DataTooLong => fmt.write_str("data too long"),
            QrError::InvalidVersion => fmt.write_str("invalid version"),
            QrError::UnsupportedCharacterSet => fmt.write_str("unsupported character set"),
            QrError::InvalidEciDesignator => fmt.write_str("invalid ECI designator"),
            QrError::InvalidCharacter => fmt.write_str("invalid character"),
            QrError::Structured(e) => write!(fmt, "{}", e),
        }
    }
}

impl ::std::error::Error for QrError {}

/// `QrResult` is a convenient alias for a QR code generation result.
pub type QrResult<T> = Result<T, QrError>;

//}}}
//------------------------------------------------------------------------------
//{{{ Color

/// The color of a module.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum Color {
    /// The module is light colored.
    Light,
    /// The module is dark colored.
    Dark,
}

impl Color {
    /// Selects a value according to color of the module. Equivalent to
    /// `if self != Color::Light { dark } else { light }`.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use qr_code::types::Color;
    /// assert_eq!(Color::Light.select(1, 0), 0);
    /// assert_eq!(Color::Dark.select("black", "white"), "black");
    /// ```
    pub fn select<T>(self, dark: T, light: T) -> T {
        match self {
            Color::Light => light,
            Color::Dark => dark,
        }
    }
}

impl Not for Color {
    type Output = Self;
    fn not(self) -> Self {
        match self {
            Color::Light => Color::Dark,
            Color::Dark => Color::Light,
        }
    }
}

//}}}
//------------------------------------------------------------------------------
//{{{ Error correction level

/// The error correction level. It allows the original information be recovered
/// even if parts of the code is damaged.
#[derive(Debug, PartialEq, Eq, Copy, Clone, PartialOrd, Ord)]
#[cfg_attr(feature = "fuzz", derive(arbitrary::Arbitrary))]
pub enum EcLevel {
    /// Low error correction. Allows up to 7% of wrong blocks.
    L = 0,

    /// Medium error correction (default). Allows up to 15% of wrong blocks.
    M = 1,

    /// "Quartile" error correction. Allows up to 25% of wrong blocks.
    Q = 2,

    /// High error correction. Allows up to 30% of wrong blocks.
    H = 3,
}

//}}}
//------------------------------------------------------------------------------
//{{{ Version

/// In QR code terminology, `Version` means the size of the generated image.
/// Larger version means the size of code is larger, and therefore can carry
/// more information.
///
/// The smallest version is `Version::Normal(1)` of size 21×21, and the largest
/// is `Version::Normal(40)` of size 177×177.
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum Version {
    /// A normal QR code version. The parameter should be between 1 and 40.
    Normal(i16),

    /// A Micro QR code version. The parameter should be between 1 and 4.
    Micro(i16),
}

impl Version {
    /// Get the number of "modules" on each size of the QR code, i.e. the width
    /// and height of the code.
    pub fn width(self) -> i16 {
        match self {
            Version::Normal(v) => v * 4 + 17,
            Version::Micro(v) => v * 2 + 9,
        }
    }

    /// Obtains an object from a hard-coded table.
    ///
    /// The table must be a 44×4 array. The outer array represents the content
    /// for each version. The first 40 entry corresponds to QR code versions 1
    /// to 40, and the last 4 corresponds to Micro QR code version 1 to 4. The
    /// inner array represents the content in each error correction level, in
    /// the order [L, M, Q, H].
    ///
    /// # Errors
    ///
    /// If the entry compares equal to the default value of `T`, this method
    /// returns `Err(QrError::InvalidVersion)`.
    pub fn fetch<T>(self, ec_level: EcLevel, table: &[[T; 4]]) -> QrResult<T>
    where
        T: PartialEq + Default + Copy,
    {
        match self {
            Version::Normal(v @ 1..=40) => {
                return Ok(table[(v - 1).as_usize()][ec_level as usize]);
            }
            Version::Micro(v @ 1..=4) => {
                let obj = table[(v + 39).as_usize()][ec_level as usize];
                if obj != T::default() {
                    return Ok(obj);
                }
            }
            _ => {}
        }
        Err(QrError::InvalidVersion)
    }

    /// The number of bits needed to encode the mode indicator.
    pub fn mode_bits_count(self) -> usize {
        match self {
            Version::Micro(a) => (a - 1).as_usize(),
            _ => 4,
        }
    }

    /// Checks whether is version refers to a Micro QR code.
    pub fn is_micro(self) -> bool {
        match self {
            Version::Normal(_) => false,
            Version::Micro(_) => true,
        }
    }
}

//}}}
//------------------------------------------------------------------------------
//{{{ Mode indicator

/// The mode indicator, which specifies the character set of the encoded data.
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum Mode {
    /// The data contains only characters 0 to 9.
    Numeric,

    /// The data contains only uppercase letters (A–Z), numbers (0–9) and a few
    /// punctuations marks (space, `$`, `%`, `*`, `+`, `-`, `.`, `/`, `:`).
    Alphanumeric,

    /// The data contains arbitrary binary data.
    Byte,

    /// The data contains Shift-JIS-encoded double-byte text.
    Kanji,
}

impl Mode {
    /// Computes the number of bits needed to encode the data length.
    ///
    ///     use qr_code::types::{Version, Mode};
    ///
    ///     assert_eq!(Mode::Numeric.length_bits_count(Version::Normal(1)), 10);
    ///
    /// This method will return `Err(QrError::UnsupportedCharacterSet)` if the
    /// mode is not supported in the given version.
    pub fn length_bits_count(self, version: Version) -> usize {
        match version {
            Version::Micro(a) => {
                let a = a.as_usize();
                match self {
                    Mode::Numeric => 2 + a,
                    Mode::Alphanumeric | Mode::Byte => 1 + a,
                    Mode::Kanji => a,
                }
            }
            Version::Normal(1..=9) => match self {
                Mode::Numeric => 10,
                Mode::Alphanumeric => 9,
                Mode::Byte | Mode::Kanji => 8,
            },
            Version::Normal(10..=26) => match self {
                Mode::Numeric => 12,
                Mode::Alphanumeric => 11,
                Mode::Byte => 16,
                Mode::Kanji => 10,
            },
            Version::Normal(_) => match self {
                Mode::Numeric => 14,
                Mode::Alphanumeric => 13,
                Mode::Byte => 16,
                Mode::Kanji => 12,
            },
        }
    }

    /// Computes the number of bits needed to some data of a given raw length.
    ///
    ///     use qr_code::types::Mode;
    ///
    ///     assert_eq!(Mode::Numeric.data_bits_count(7), 24);
    ///
    /// Note that in Kanji mode, the `raw_data_len` is the number of Kanjis,
    /// i.e. half the total size of bytes.
    pub fn data_bits_count(self, raw_data_len: usize) -> usize {
        match self {
            Mode::Numeric => (raw_data_len * 10 + 2) / 3,
            Mode::Alphanumeric => (raw_data_len * 11 + 1) / 2,
            Mode::Byte => raw_data_len * 8,
            Mode::Kanji => raw_data_len * 13,
        }
    }

    /// Find the lowest common mode which both modes are compatible with.
    ///
    ///     use qr_code::types::Mode;
    ///
    ///     let a = Mode::Numeric;
    ///     let b = Mode::Kanji;
    ///     let c = a.max(b);
    ///     assert!(a <= c);
    ///     assert!(b <= c);
    ///
    pub fn max(self, other: Self) -> Self {
        match self.partial_cmp(&other) {
            Some(Ordering::Less) | Some(Ordering::Equal) => other,
            Some(Ordering::Greater) => self,
            None => Mode::Byte,
        }
    }

    pub(crate) const ALL_MODES: &'static [Mode] =
        &[Mode::Numeric, Mode::Alphanumeric, Mode::Byte, Mode::Kanji];
}

impl PartialOrd for Mode {
    /// Defines a partial ordering between modes. If `a <= b`, then `b` contains
    /// a superset of all characters supported by `a`.
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        match (*self, *other) {
            (Mode::Numeric, Mode::Alphanumeric)
            | (Mode::Numeric, Mode::Byte)
            | (Mode::Alphanumeric, Mode::Byte)
            | (Mode::Kanji, Mode::Byte) => Some(Ordering::Less),
            (Mode::Alphanumeric, Mode::Numeric)
            | (Mode::Byte, Mode::Numeric)
            | (Mode::Byte, Mode::Alphanumeric)
            | (Mode::Byte, Mode::Kanji) => Some(Ordering::Greater),
            (a, b) if a == b => Some(Ordering::Equal),
            _ => None,
        }
    }
}

#[cfg(test)]
mod mode_tests {
    use crate::types::Mode::{Alphanumeric, Byte, Kanji, Numeric};

    #[test]
    fn test_mode_order() {
        assert!(Numeric < Alphanumeric);
        assert!(Byte > Kanji);
        assert!(!(Numeric < Kanji));
        assert!(!(Numeric >= Kanji));
    }

    #[test]
    fn test_max() {
        assert_eq!(Byte.max(Kanji), Byte);
        assert_eq!(Numeric.max(Alphanumeric), Alphanumeric);
        assert_eq!(Alphanumeric.max(Alphanumeric), Alphanumeric);
        assert_eq!(Numeric.max(Kanji), Byte);
        assert_eq!(Kanji.max(Numeric), Byte);
        assert_eq!(Alphanumeric.max(Numeric), Alphanumeric);
        assert_eq!(Kanji.max(Kanji), Kanji);
    }
}

//}}}
