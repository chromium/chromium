// Copyright Mozilla Foundation
//
// Licensed under the Apache License (Version 2.0), or the MIT license,
// (the "Licenses") at your option. You may not use this file except in
// compliance with one of the Licenses. You may obtain copies of the
// Licenses at:
//
//    https://www.apache.org/licenses/LICENSE-2.0
//    https://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Licenses is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the Licenses for the specific language governing permissions and
// limitations under the Licenses.

//! Types for UTF-8 code unit sequences and their parts.

/// A type for signaling UTF-8 errors.
#[derive(Debug, PartialEq, Clone, Copy, Hash, Eq)]
#[non_exhaustive]
pub struct Utf8ByteError;

impl core::fmt::Display for Utf8ByteError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> Result<(), core::fmt::Error> {
        write!(f, "byte does not fit the requested position in UTF-8")
    }
}

impl core::error::Error for Utf8ByteError {}

#[repr(align(64))] // Align to cache lines
struct Utf8Data {
    table: [u8; 384],
}

impl Utf8Data {
    /// The table lookup for backing prefix checks.
    ///
    /// # Safety-usable invariant
    ///
    /// The return value is 0 iff `first` and `second` form a prefix of a
    /// well-formed multi-byte UTF-8 sequence (including a complete two-byte
    /// sequence).
    /// The lowest two bits are always 0.
    #[inline(always)]
    const fn lookup(&self, first: u8, second: u8) -> u8 {
        self.table[second as usize] & self.table[first as usize + 0x80]
    }
}

/// The first 256 entries are for looking up `second`.
/// The rest are for looking up `first` on the assumption
/// that `first` is not ASCII.
///
/// Lanes 0 and 1 are left unused to allow the tag
/// bits of the third byte in a three-byte sequence
/// to be shifted there for a single-branch comparison.
static UTF8_DATA: Utf8Data = Utf8Data {
    table: const {
        let mut table = [0u8; 384];

        // Trails
        let mut i = 0u16;
        while i < 0x100 {
            let second = i as u8;
            // If a `second` is valid given a lane, we leave a lane as zero.
            // We put 1 on the lane if the `second` is invalid given a lane.
            let mut combined = 1 << 2; // invalid lead
            if second < 0x80 || second > 0xBF {
                combined |= 1 << 3; // normal trail
            }
            if second < 0xA0 || second > 0xBF {
                combined |= 1 << 4; // three-byte special lower bound
            }
            if second < 0x80 || second > 0x9F {
                combined |= 1 << 5; // three-byte special upper bound
            }
            if second < 0x90 || second > 0xBF {
                combined |= 1 << 6; // four-byte special lower bound
            }
            if second < 0x80 || second > 0x8F {
                combined |= 1 << 7; // four-byte special upper bound
            }
            table[second as usize] = combined;
            i += 1;
        }

        // Non-ASCII leads
        i = 0x80;
        while i < 0x100 {
            let first = i as u8;
            let lane = match first {
                0xC2..=0xDF | 0xE1..=0xEC | 0xEE..=0xEF | 0xF1..=0xF3 => {
                    1 << 3 // normal trail
                }
                0xE0 => {
                    1 << 4 // three-byte special lower bound
                }
                0xED => {
                    1 << 5 // three-byte special upper bound
                }
                0xF0 => {
                    1 << 6 // four-byte special lower bound
                }
                0xF4 => {
                    1 << 7 // four-byte special upper bound
                }
                _ => {
                    // invalid lead
                    1 << 2
                }
            };
            table[0x80 + first as usize] = lane;
            i += 1;
        }

        // We didn't explicitly generate table entries that are
        // looked up when `first` is ASCII. However, from the construction
        // of the entries for trails, the overlapping part ends up with
        // suitable values. Let's verify that.
        i = 0;
        while i < 0x80 {
            if table[0x80 + i as usize] & (1 << 2) == 0 {
                panic!();
            }
            i += 1;
        }

        table
    },
};

// `Ascii` and `NonAscii` benefit from `enum` representation, because it communicates
// integer range information e.g. when they are used for table lookup.
//
// It's unclear if the other types benefit from communicating range information
// to the optimizer, but they enable niches anyway.

macro_rules! byte_methods {
    ($type_specific_ident:ident,
     $(#[$unchecked_meta:meta])*,
     $(#[$checked_meta:meta])*,
    ) => {
        /// Whether `byte` is in range for this type.
        ///
        /// # Safety-usable invariant
        ///
        /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
        /// is satisfied.
        #[inline(always)]
        pub const fn is(byte: u8) -> bool {
            byte >= Self::MIN.to_u8() && byte <= Self::MAX.to_u8()
        }

        $(#[$unchecked_meta])*
        #[inline(always)]
        pub const unsafe fn new_unchecked($type_specific_ident: u8) -> Self {
            debug_assert!(Self::is($type_specific_ident));
            // SAFETY: Our `repr(u8)` matches argument `u8` size and the
            // value range is this function's safety invariant.
            unsafe { core::mem::transmute($type_specific_ident) }
        }

        $(#[$checked_meta])*
        #[inline(always)]
        pub const fn try_new(byte: u8) -> Result<Self, Utf8ByteError> {
            if Self::is(byte) {
                // SAFETY: We checked the safety invariant immediately above.
                Ok(unsafe { Self::new_unchecked(byte) })
            } else {
                Err(Utf8ByteError)
            }
        }

        /// Convert to plain byte
        #[inline(always)]
        pub const fn to_u8(self) -> u8 {
            self as u8
        }
    };
}

macro_rules! to_non_ascii {
    () => {
        /// Convert to `NonAscii`
        #[inline(always)]
        pub const fn to_non_ascii(self) -> NonAscii {
            match NonAscii::try_new(self.to_u8()) {
                Ok(non_ascii) => non_ascii,
                Err(_) => {
                    // `Self` has a stricter `enum` value space
                    // than the target type.
                    unreachable!();
                }
            }
        }
    };
}

macro_rules! to_lead_for_multi_byte {
    () => {
        to_non_ascii!();

        /// Convert to `LeadForMultiByte`
        #[inline(always)]
        pub const fn to_lead_for_multi_byte(self) -> LeadForMultiByte {
            match LeadForMultiByte::try_new(self.to_u8()) {
                Ok(lead) => lead,
                Err(_) => {
                    // `Self` has a stricter `enum` value space
                    // than the target type.
                    unreachable!();
                }
            }
        }
    };
}

macro_rules! byte_impls {
    ($type:ident,
     $type_specific_ident:ident,
    ) => {
        impl TryFrom<u8> for $type {
            type Error = Utf8ByteError;

            #[inline(always)]
            fn try_from(byte: u8) -> Result<Self, Utf8ByteError> {
                Self::try_new(byte)
            }
        }

        impl From<$type> for u8 {
            #[inline(always)]
            fn from($type_specific_ident: $type) -> Self {
                $type_specific_ident.to_u8()
            }
        }
    };
}

macro_rules! non_ascii_impls {
    ($type:ident,
     $type_specific_ident:ident,
    ) => {
        impl TryFrom<NonAscii> for $type {
            type Error = Utf8ByteError;

            #[inline(always)]
            fn try_from(non_ascii: NonAscii) -> Result<Self, Utf8ByteError> {
                Self::try_new(non_ascii.to_u8())
            }
        }

        impl From<$type> for NonAscii {
            #[inline(always)]
            fn from($type_specific_ident: $type) -> Self {
                $type_specific_ident.to_non_ascii()
            }
        }
    };
}

macro_rules! lead_impls {
    ($type:ident,
) => {
        non_ascii_impls!($type, lead,);

        impl TryFrom<LeadForMultiByte> for $type {
            type Error = Utf8ByteError;

            #[inline(always)]
            fn try_from(lead: LeadForMultiByte) -> Result<Self, Utf8ByteError> {
                Self::try_new(lead.to_u8())
            }
        }

        impl From<$type> for LeadForMultiByte {
            #[inline(always)]
            fn from(lead: $type) -> Self {
                lead.to_lead_for_multi_byte()
            }
        }
    };
}

macro_rules! sequence_methods {
    ($(#[$as_bytes_meta:meta])*,
     $(#[$as_str_meta:meta])*,
    ) => {
        $(#[$as_bytes_meta])*
        #[inline(always)]
        pub const fn as_bytes(&self) -> &[u8; Self::LEN] {
            if core::mem::size_of::<Self>() != Self::LEN {
                panic!();
            }
            let ptr: *const Self = core::ptr::from_ref(self);
            // SAFETY: The `repr` of `Self` guarantees that it
            // is safe to view `Self` as a sequence of `u8`, that
            // the `u8`s are in the right order, and that the
            // size of `Self` matches the length in the return type.
            unsafe {
                &*ptr.cast()
            }
        }

        $(#[$as_str_meta])*
        #[inline(always)]
        pub const fn as_str(&self) -> &str {
            // SAFETY: The safety-usable invariant of `as_bytes()` satisfies
            // the safety invariant of `from_utf8_unchecked()`.
            unsafe { core::str::from_utf8_unchecked(self.as_bytes()) }
        }
    };
}

/// For making sure that reviewing this file is sufficient for
/// reviewing the implementations of `Utf8ByteSequence`.
trait Seal {}

/// Marker trait for all four UTF-8 byte sequences
#[allow(private_bounds)] // allow sealing
pub trait Utf8ByteSequence: Seal + Copy + Clone + Into<char> + AsRef<str> {}

macro_rules! sequence_impls {
    ($type:ident,
     $type_specific_ident:ident,
    ) => {
        impl Seal for $type {}

        impl Utf8ByteSequence for $type {}

        impl From<$type> for char {
            #[inline(always)]
            fn from($type_specific_ident: $type) -> Self {
                $type_specific_ident.to_char()
            }
        }

        impl AsRef<str> for $type {
            #[inline(always)]
            fn as_ref(&self) -> &str {
                self.as_str()
            }
        }
    };
}

/// A byte in the ASCII range (single-byte UTF-8 sequence).
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(u8)]
pub enum Ascii {
    B00 = 0x00,
    B01 = 0x01,
    B02 = 0x02,
    B03 = 0x03,
    B04 = 0x04,
    B05 = 0x05,
    B06 = 0x06,
    B07 = 0x07,
    B08 = 0x08,
    B09 = 0x09,
    B0a = 0x0a,
    B0b = 0x0b,
    B0c = 0x0c,
    B0d = 0x0d,
    B0e = 0x0e,
    B0f = 0x0f,
    B10 = 0x10,
    B11 = 0x11,
    B12 = 0x12,
    B13 = 0x13,
    B14 = 0x14,
    B15 = 0x15,
    B16 = 0x16,
    B17 = 0x17,
    B18 = 0x18,
    B19 = 0x19,
    B1a = 0x1a,
    B1b = 0x1b,
    B1c = 0x1c,
    B1d = 0x1d,
    B1e = 0x1e,
    B1f = 0x1f,
    B20 = 0x20,
    B21 = 0x21,
    B22 = 0x22,
    B23 = 0x23,
    B24 = 0x24,
    B25 = 0x25,
    B26 = 0x26,
    B27 = 0x27,
    B28 = 0x28,
    B29 = 0x29,
    B2a = 0x2a,
    B2b = 0x2b,
    B2c = 0x2c,
    B2d = 0x2d,
    B2e = 0x2e,
    B2f = 0x2f,
    B30 = 0x30,
    B31 = 0x31,
    B32 = 0x32,
    B33 = 0x33,
    B34 = 0x34,
    B35 = 0x35,
    B36 = 0x36,
    B37 = 0x37,
    B38 = 0x38,
    B39 = 0x39,
    B3a = 0x3a,
    B3b = 0x3b,
    B3c = 0x3c,
    B3d = 0x3d,
    B3e = 0x3e,
    B3f = 0x3f,
    B40 = 0x40,
    B41 = 0x41,
    B42 = 0x42,
    B43 = 0x43,
    B44 = 0x44,
    B45 = 0x45,
    B46 = 0x46,
    B47 = 0x47,
    B48 = 0x48,
    B49 = 0x49,
    B4a = 0x4a,
    B4b = 0x4b,
    B4c = 0x4c,
    B4d = 0x4d,
    B4e = 0x4e,
    B4f = 0x4f,
    B50 = 0x50,
    B51 = 0x51,
    B52 = 0x52,
    B53 = 0x53,
    B54 = 0x54,
    B55 = 0x55,
    B56 = 0x56,
    B57 = 0x57,
    B58 = 0x58,
    B59 = 0x59,
    B5a = 0x5a,
    B5b = 0x5b,
    B5c = 0x5c,
    B5d = 0x5d,
    B5e = 0x5e,
    B5f = 0x5f,
    B60 = 0x60,
    B61 = 0x61,
    B62 = 0x62,
    B63 = 0x63,
    B64 = 0x64,
    B65 = 0x65,
    B66 = 0x66,
    B67 = 0x67,
    B68 = 0x68,
    B69 = 0x69,
    B6a = 0x6a,
    B6b = 0x6b,
    B6c = 0x6c,
    B6d = 0x6d,
    B6e = 0x6e,
    B6f = 0x6f,
    B70 = 0x70,
    B71 = 0x71,
    B72 = 0x72,
    B73 = 0x73,
    B74 = 0x74,
    B75 = 0x75,
    B76 = 0x76,
    B77 = 0x77,
    B78 = 0x78,
    B79 = 0x79,
    B7a = 0x7a,
    B7b = 0x7b,
    B7c = 0x7c,
    B7d = 0x7d,
    B7e = 0x7e,
    B7f = 0x7f,
}

impl Ascii {
    /// Minimum possible value.
    pub const MIN: Self = Self::B00;

    /// Maximum possible value.
    pub const MAX: Self = Self::B7f;

    /// The number of bytes in this kind of UTF-8 sequence.
    pub const LEN: usize = 1;

    byte_methods!(
        ascii,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// `ascii` must be < 0x80.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if ASCII and `Err` otherwise.
        ,
    );

    /// Convert to `char`
    #[inline(always)]
    pub const fn to_char(self) -> char {
        self as u8 as char
    }

    sequence_methods!(
        /// View as `&[u8; 1]`
        ///
        /// # Safety-usable invariant
        ///
        /// The slice contains a single ASCII byte.
        ,
        /// View as `&str`
        ///
        /// # Safety-usable invariant
        ///
        /// The string slice contains a single ASCII byte/character.
        ,
    );
}

byte_impls!(Ascii, ascii,);

sequence_impls!(Ascii, ascii,);

/// A byte in the non-ASCII range.
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(u8)]
pub enum NonAscii {
    B80 = 0x80,
    B81 = 0x81,
    B82 = 0x82,
    B83 = 0x83,
    B84 = 0x84,
    B85 = 0x85,
    B86 = 0x86,
    B87 = 0x87,
    B88 = 0x88,
    B89 = 0x89,
    B8a = 0x8a,
    B8b = 0x8b,
    B8c = 0x8c,
    B8d = 0x8d,
    B8e = 0x8e,
    B8f = 0x8f,
    B90 = 0x90,
    B91 = 0x91,
    B92 = 0x92,
    B93 = 0x93,
    B94 = 0x94,
    B95 = 0x95,
    B96 = 0x96,
    B97 = 0x97,
    B98 = 0x98,
    B99 = 0x99,
    B9a = 0x9a,
    B9b = 0x9b,
    B9c = 0x9c,
    B9d = 0x9d,
    B9e = 0x9e,
    B9f = 0x9f,
    Ba0 = 0xa0,
    Ba1 = 0xa1,
    Ba2 = 0xa2,
    Ba3 = 0xa3,
    Ba4 = 0xa4,
    Ba5 = 0xa5,
    Ba6 = 0xa6,
    Ba7 = 0xa7,
    Ba8 = 0xa8,
    Ba9 = 0xa9,
    Baa = 0xaa,
    Bab = 0xab,
    Bac = 0xac,
    Bad = 0xad,
    Bae = 0xae,
    Baf = 0xaf,
    Bb0 = 0xb0,
    Bb1 = 0xb1,
    Bb2 = 0xb2,
    Bb3 = 0xb3,
    Bb4 = 0xb4,
    Bb5 = 0xb5,
    Bb6 = 0xb6,
    Bb7 = 0xb7,
    Bb8 = 0xb8,
    Bb9 = 0xb9,
    Bba = 0xba,
    Bbb = 0xbb,
    Bbc = 0xbc,
    Bbd = 0xbd,
    Bbe = 0xbe,
    Bbf = 0xbf,
    Bc0 = 0xc0,
    Bc1 = 0xc1,
    Bc2 = 0xc2,
    Bc3 = 0xc3,
    Bc4 = 0xc4,
    Bc5 = 0xc5,
    Bc6 = 0xc6,
    Bc7 = 0xc7,
    Bc8 = 0xc8,
    Bc9 = 0xc9,
    Bca = 0xca,
    Bcb = 0xcb,
    Bcc = 0xcc,
    Bcd = 0xcd,
    Bce = 0xce,
    Bcf = 0xcf,
    Bd0 = 0xd0,
    Bd1 = 0xd1,
    Bd2 = 0xd2,
    Bd3 = 0xd3,
    Bd4 = 0xd4,
    Bd5 = 0xd5,
    Bd6 = 0xd6,
    Bd7 = 0xd7,
    Bd8 = 0xd8,
    Bd9 = 0xd9,
    Bda = 0xda,
    Bdb = 0xdb,
    Bdc = 0xdc,
    Bdd = 0xdd,
    Bde = 0xde,
    Bdf = 0xdf,
    Be0 = 0xe0,
    Be1 = 0xe1,
    Be2 = 0xe2,
    Be3 = 0xe3,
    Be4 = 0xe4,
    Be5 = 0xe5,
    Be6 = 0xe6,
    Be7 = 0xe7,
    Be8 = 0xe8,
    Be9 = 0xe9,
    Bea = 0xea,
    Beb = 0xeb,
    Bec = 0xec,
    Bed = 0xed,
    Bee = 0xee,
    Bef = 0xef,
    Bf0 = 0xf0,
    Bf1 = 0xf1,
    Bf2 = 0xf2,
    Bf3 = 0xf3,
    Bf4 = 0xf4,
    Bf5 = 0xf5,
    Bf6 = 0xf6,
    Bf7 = 0xf7,
    Bf8 = 0xf8,
    Bf9 = 0xf9,
    Bfa = 0xfa,
    Bfb = 0xfb,
    Bfc = 0xfc,
    Bfd = 0xfd,
    Bfe = 0xfe,
    Bff = 0xff,
}

impl NonAscii {
    /// Minimum possible value.
    pub const MIN: Self = Self::B80;

    /// Maximum possible value.
    pub const MAX: Self = Self::Bff;

    byte_methods!(
        non_ascii,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// `non_ascii` must be >= 0x80.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if non-ASCII and `Err` otherwise.
        ,
    );
}

byte_impls!(NonAscii, non_ascii,);

/// An unconstrained UTF-8 continuation byte (0x80 to 0xBF, inclusive)
///
/// Any continuation byte is valid as this type, but this type is not
/// sufficiently constrained to guarantee validity as the second byte
/// of a three-byte or four-byte sequence whose lead byte is 0xE0, 0xED,
/// 0xF0, or 0xF4.
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(u8)]
pub enum UnconstrainedContinuation {
    B80 = 0x80,
    B81 = 0x81,
    B82 = 0x82,
    B83 = 0x83,
    B84 = 0x84,
    B85 = 0x85,
    B86 = 0x86,
    B87 = 0x87,
    B88 = 0x88,
    B89 = 0x89,
    B8a = 0x8a,
    B8b = 0x8b,
    B8c = 0x8c,
    B8d = 0x8d,
    B8e = 0x8e,
    B8f = 0x8f,
    B90 = 0x90,
    B91 = 0x91,
    B92 = 0x92,
    B93 = 0x93,
    B94 = 0x94,
    B95 = 0x95,
    B96 = 0x96,
    B97 = 0x97,
    B98 = 0x98,
    B99 = 0x99,
    B9a = 0x9a,
    B9b = 0x9b,
    B9c = 0x9c,
    B9d = 0x9d,
    B9e = 0x9e,
    B9f = 0x9f,
    Ba0 = 0xa0,
    Ba1 = 0xa1,
    Ba2 = 0xa2,
    Ba3 = 0xa3,
    Ba4 = 0xa4,
    Ba5 = 0xa5,
    Ba6 = 0xa6,
    Ba7 = 0xa7,
    Ba8 = 0xa8,
    Ba9 = 0xa9,
    Baa = 0xaa,
    Bab = 0xab,
    Bac = 0xac,
    Bad = 0xad,
    Bae = 0xae,
    Baf = 0xaf,
    Bb0 = 0xb0,
    Bb1 = 0xb1,
    Bb2 = 0xb2,
    Bb3 = 0xb3,
    Bb4 = 0xb4,
    Bb5 = 0xb5,
    Bb6 = 0xb6,
    Bb7 = 0xb7,
    Bb8 = 0xb8,
    Bb9 = 0xb9,
    Bba = 0xba,
    Bbb = 0xbb,
    Bbc = 0xbc,
    Bbd = 0xbd,
    Bbe = 0xbe,
    Bbf = 0xbf,
}

impl UnconstrainedContinuation {
    /// Minimum possible value.
    pub const MIN: Self = Self::B80;

    /// Maximum possible value.
    pub const MAX: Self = Self::Bbf;

    byte_methods!(
        continuation,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// `continuation` must be in the range 0x80 to 0xBF, inclusive.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if in the range 0x80 to 0xBF, inclusive, and `Err` otherwise.
        ,
    );

    to_non_ascii!();

    /// Obtain the least-significant six bits (i.e. the non-tag bits).
    ///
    /// # Safety-usable invariant
    ///
    /// Only the least-significant six bits can have non-zero values.
    #[inline(always)]
    pub const fn low_six(self) -> u8 {
        self.to_u8() & 0b111_111
    }

    /// Obtain the least-significant six bits (i.e. the non-tag bits) as `u32`.
    ///
    /// # Safety-usable invariant
    ///
    /// Only the least-significant six bits can have non-zero values.
    #[inline(always)]
    pub const fn low_six_u32(self) -> u32 {
        self.low_six() as u32
    }
}

byte_impls!(UnconstrainedContinuation, continuation,);

non_ascii_impls!(UnconstrainedContinuation, continuation,);

/// A lead byte for a multibyte sequence (two-byte, three-byte, or
/// four-byte).
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(u8)]
pub enum LeadForMultiByte {
    Bc2 = 0xc2,
    Bc3 = 0xc3,
    Bc4 = 0xc4,
    Bc5 = 0xc5,
    Bc6 = 0xc6,
    Bc7 = 0xc7,
    Bc8 = 0xc8,
    Bc9 = 0xc9,
    Bca = 0xca,
    Bcb = 0xcb,
    Bcc = 0xcc,
    Bcd = 0xcd,
    Bce = 0xce,
    Bcf = 0xcf,
    Bd0 = 0xd0,
    Bd1 = 0xd1,
    Bd2 = 0xd2,
    Bd3 = 0xd3,
    Bd4 = 0xd4,
    Bd5 = 0xd5,
    Bd6 = 0xd6,
    Bd7 = 0xd7,
    Bd8 = 0xd8,
    Bd9 = 0xd9,
    Bda = 0xda,
    Bdb = 0xdb,
    Bdc = 0xdc,
    Bdd = 0xdd,
    Bde = 0xde,
    Bdf = 0xdf,
    Be0 = 0xe0,
    Be1 = 0xe1,
    Be2 = 0xe2,
    Be3 = 0xe3,
    Be4 = 0xe4,
    Be5 = 0xe5,
    Be6 = 0xe6,
    Be7 = 0xe7,
    Be8 = 0xe8,
    Be9 = 0xe9,
    Bea = 0xea,
    Beb = 0xeb,
    Bec = 0xec,
    Bed = 0xed,
    Bee = 0xee,
    Bef = 0xef,
    Bf0 = 0xf0,
    Bf1 = 0xf1,
    Bf2 = 0xf2,
    Bf3 = 0xf3,
    Bf4 = 0xf4,
}

impl LeadForMultiByte {
    /// Minimum possible value.
    pub const MIN: Self = Self::Bc2;

    /// Maximum possible value.
    pub const MAX: Self = Self::Bf4;

    byte_methods!(
        lead,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// `lead` must be in the range 0xC2 to 0xF4, inclusive.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if in the range 0xC2 to 0xF4, inclusive, and `Err` otherwise.
        ,
    );

    to_non_ascii!();
}

byte_impls!(LeadForMultiByte, lead,);

non_ascii_impls!(LeadForMultiByte, lead,);

/// A lead byte for a two-byte UTF-8 sequence (0xC2 to 0xDF, inclusive)
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(u8)]
pub enum LeadForTwoByte {
    Bc2 = 0xc2,
    Bc3 = 0xc3,
    Bc4 = 0xc4,
    Bc5 = 0xc5,
    Bc6 = 0xc6,
    Bc7 = 0xc7,
    Bc8 = 0xc8,
    Bc9 = 0xc9,
    Bca = 0xca,
    Bcb = 0xcb,
    Bcc = 0xcc,
    Bcd = 0xcd,
    Bce = 0xce,
    Bcf = 0xcf,
    Bd0 = 0xd0,
    Bd1 = 0xd1,
    Bd2 = 0xd2,
    Bd3 = 0xd3,
    Bd4 = 0xd4,
    Bd5 = 0xd5,
    Bd6 = 0xd6,
    Bd7 = 0xd7,
    Bd8 = 0xd8,
    Bd9 = 0xd9,
    Bda = 0xda,
    Bdb = 0xdb,
    Bdc = 0xdc,
    Bdd = 0xdd,
    Bde = 0xde,
    Bdf = 0xdf,
}

impl LeadForTwoByte {
    /// Minimum possible value.
    pub const MIN: Self = Self::Bc2;

    /// Maximum possible value.
    pub const MAX: Self = Self::Bdf;

    byte_methods!(
        lead,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// `lead` must be in the range 0xC2 to 0xDF, inclusive.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if in the range 0xC2 to 0xDF, inclusive, and `Err` otherwise.
        ,
    );

    to_lead_for_multi_byte!();

    /// Obtain the least-significant five bits (i.e. the non-tag bits).
    ///
    /// # Safety-usable invariant
    ///
    /// Only the least-significant five bits can have non-zero values.
    #[inline(always)]
    pub const fn low_five(self) -> u8 {
        self.to_u8() & 0b11_111
    }

    /// Obtain the least-significant five bits (i.e. the non-tag bits) as `u32`.
    ///
    /// # Safety-usable invariant
    ///
    /// Only the least-significant five bits can have non-zero values.
    #[inline(always)]
    pub const fn low_five_u32(self) -> u32 {
        self.low_five() as u32
    }
}

byte_impls!(LeadForTwoByte, lead,);

lead_impls!(LeadForTwoByte,);

/// A lead byte for a three-byte UTF-8 sequence (0xE0 to 0xEF, inclusive)
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(u8)]
pub enum LeadForThreeByte {
    Be0 = 0xe0,
    Be1 = 0xe1,
    Be2 = 0xe2,
    Be3 = 0xe3,
    Be4 = 0xe4,
    Be5 = 0xe5,
    Be6 = 0xe6,
    Be7 = 0xe7,
    Be8 = 0xe8,
    Be9 = 0xe9,
    Bea = 0xea,
    Beb = 0xeb,
    Bec = 0xec,
    Bed = 0xed,
    Bee = 0xee,
    Bef = 0xef,
}

impl LeadForThreeByte {
    /// Minimum possible value.
    pub const MIN: Self = Self::Be0;

    /// Maximum possible value.
    pub const MAX: Self = Self::Bef;

    byte_methods!(
        lead,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// `lead` must be in the range 0xE0 to 0xEF, inclusive.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if in the range 0xE0 to 0xEF, inclusive, and `Err` otherwise.
        ,
    );

    to_lead_for_multi_byte!();

    /// Obtain the least-significant four bits (i.e. the non-tag bits).
    ///
    /// # Safety-usable invariant
    ///
    /// Only the least-significant four bits can have non-zero values.
    #[inline(always)]
    pub const fn low_four(self) -> u8 {
        self.to_u8() & 0b1111
    }

    /// Obtain the least-significant four bits (i.e. the non-tag bits) as `u32`.
    ///
    /// # Safety-usable invariant
    ///
    /// Only the least-significant four bits can have non-zero values.
    #[inline(always)]
    pub const fn low_four_u32(self) -> u32 {
        self.low_four() as u32
    }
}

byte_impls!(LeadForThreeByte, lead,);

lead_impls!(LeadForThreeByte,);

/// A lead byte for a four-byte UTF-8 sequence (0xF0 to 0xF4, inclusive)
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(u8)]
pub enum LeadForFourByte {
    Bf0 = 0xf0,
    Bf1 = 0xf1,
    Bf2 = 0xf2,
    Bf3 = 0xf3,
    Bf4 = 0xf4,
}

impl LeadForFourByte {
    /// Minimum possible value.
    pub const MIN: Self = Self::Bf0;

    /// Maximum possible value.
    pub const MAX: Self = Self::Bf4;

    byte_methods!(
        lead,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// `lead` must be in the range 0xF0 to 0xF4, inclusive.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if in the range 0xF0 to 0xF4, inclusive, and `Err` otherwise.
        ,
    );

    to_lead_for_multi_byte!();

    /// Obtain the least-significant three bits (i.e. the non-tag bits).
    ///
    /// # Safety-usable invariant
    ///
    /// Only the least-significant three bits can have non-zero values.
    #[inline(always)]
    pub const fn low_three(self) -> u8 {
        self.to_u8() & 0b111
    }

    /// Obtain the least-significant three bits (i.e. the non-tag bits) as `u32`.
    ///
    /// # Safety-usable invariant
    ///
    /// Only the least-significant three bits can have non-zero values.
    #[inline(always)]
    pub const fn low_three_u32(self) -> u32 {
        self.low_three() as u32
    }
}

byte_impls!(LeadForFourByte, lead,);

lead_impls!(LeadForFourByte,);

/// Two-byte UTF-8 sequence
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(C)]
pub struct TwoByteSequence {
    first: LeadForTwoByte,
    second: UnconstrainedContinuation,
}

impl TwoByteSequence {
    /// The number of bytes in this kind of UTF-8 sequence.
    pub const LEN: usize = 2;

    /// Whether `prepared` forms a well-formed two-byte UTF-8 sequence
    ///
    /// # Safety-usable invariant
    ///
    /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
    /// is satisfied.
    #[inline(always)]
    pub const fn is_with_prepared(prepared: PreparedTwoBytes) -> bool {
        // The low two bits of `check` are always zero, so we can shift it right by two
        // without data loss.
        // For `first` to be a two-byte lead byte, its top 3 bits have to be 110.
        // For `second` to be a continuation, `check` has to be zero.
        // We can let the third bit from the top overlap: It has to be zero for
        // both checks, so when we OR the checks together, if both sides succeed,
        // and it can be 1 only if either side fails.
        ((prepared.first & 0b1110_0000) | (prepared.check >> 2)) == 0b1100_0000
    }

    /// Checks whether the bytes form a well-formed two-byte UTF-8 sequence.
    ///
    /// # Safety-usable invariant
    ///
    /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
    /// is satisfied.
    #[inline(always)]
    pub const fn is(first: u8, second: u8) -> bool {
        LeadForTwoByte::is(first) && UnconstrainedContinuation::is(second)
    }

    /// Typed constructor
    #[inline(always)]
    pub const fn new(first: LeadForTwoByte, second: UnconstrainedContinuation) -> Self {
        Self { first, second }
    }

    /// Unchecked constructor
    ///
    /// # Safety
    ///
    /// `first` and `second` must form a two-byte UTF-8 sequence.
    ///
    /// # Panics
    ///
    /// When debug assertions are enabled, panics if the safety invariant
    /// is not upheld.
    #[inline(always)]
    pub const unsafe fn new_unchecked(first: u8, second: u8) -> Self {
        debug_assert!(Self::is(first, second));
        // SAFETY: `first` and `second` are in the right ranges per the
        // safety invariant of this method.
        unsafe {
            Self {
                first: LeadForTwoByte::new_unchecked(first),
                second: UnconstrainedContinuation::new_unchecked(second),
            }
        }
    }

    /// Fallible constructor
    ///
    /// `Ok` if `first` and `second` form a two-byte UTF-8 sequence and `Err` otherwise.
    #[inline(always)]
    pub const fn try_new(first: u8, second: u8) -> Result<Self, Utf8ByteError> {
        if Self::is(first, second) {
            // SAFETY: We checked the safety invariant immediately above.
            Ok(unsafe { Self::new_unchecked(first, second) })
        } else {
            Err(Utf8ByteError)
        }
    }

    /// Fallible constructor with prepared bytes
    ///
    /// `Ok` if the prepared bytes form a two-byte UTF-8 sequence and `Err` otherwise.
    #[inline(always)]
    pub const fn try_new_with_prepared(prepared: PreparedTwoBytes) -> Result<Self, Utf8ByteError> {
        if Self::is_with_prepared(prepared) {
            // SAFETY: We checked the safety invariant immediately above.
            Ok(unsafe { Self::new_unchecked(prepared.first, prepared.second) })
        } else {
            Err(Utf8ByteError)
        }
    }

    /// Convert to typed byte tuple.
    #[inline(always)]
    pub const fn to_typed_bytes(self) -> (LeadForTwoByte, UnconstrainedContinuation) {
        (self.first, self.second)
    }

    /// Convert to `char`
    #[inline(always)]
    pub const fn to_char(self) -> char {
        let scalar = (self.first.low_five_u32() << 6) | self.second.low_six_u32();
        if let Some(c) = char::from_u32(scalar) {
            c
        } else {
            // The optimizer can see from the `enum` ranges that we
            // never reach this.
            unreachable!()
        }
    }

    sequence_methods!(
        /// View as `&[u8; 2]`
        ///
        /// # Safety-usable invariant
        ///
        /// The bytes form a single well-formed two-byte UTF-8 sequence.
        ,
        /// View as `&str`
        ///
        /// # Safety-usable invariant
        ///
        /// The string slice contains a single two-byte UTF-8 sequence.
        ,
    );
}

sequence_impls!(TwoByteSequence, two_byte,);

/// Three-byte UTF-8 sequence
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(C)]
pub struct ThreeByteSequence {
    first: LeadForThreeByte,
    /// # Safety invariant
    ///
    /// `second` must be in the correct range given the value of
    /// `first` so that `first`, `second`, and `third` form a
    /// well-formed UTF-8 sequence.
    second: UnconstrainedContinuation,
    third: UnconstrainedContinuation,
}

impl ThreeByteSequence {
    /// The number of bytes in this kind of UTF-8 sequence.
    pub const LEN: usize = 3;

    /// Whether `prepared` forms a well-formed three-byte UTF-8 sequence.
    ///
    /// # Safety-usable invariant
    ///
    /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
    /// is satisfied.
    #[inline(always)]
    pub const fn is_with_prepared(prepared: PreparedThreeBytes) -> bool {
        // The top four bits of `first` have to be 1110 for `first` to
        // be a valid three-byte lead byte.
        // `check` must be 0b10 for `second` and `third` to be
        // contextually valid continuations assuming that `first`
        // is a three-byte lead byte.
        // Shifting masked `first` by 4 instead of 8 results
        // in a number to compare with that fits in an immediate
        // on aarch64. This is one instruction shorter on aarch64
        // than shifting the other part left the way it's shifted
        // in the four-byte case.
        ((((prepared.first & 0b1111_0000) as u16) << 4) | (prepared.check as u16))
            == 0b1110_0000_0010
    }

    /// Whether the bytes form a well-formed three-byte UTF-8 sequence.
    ///
    /// # Safety-usable invariant
    ///
    /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
    /// is satisfied.
    #[inline(always)]
    pub const fn is(first: u8, second: u8, third: u8) -> bool {
        Self::is_with_prepared(PreparedThreeBytes::new(first, second, third))
    }

    /// Unchecked constructor
    ///
    /// # Safety
    ///
    /// `first`, `second`, and `third` must form a three-byte UTF-8 sequence.
    ///
    /// # Panics
    ///
    /// When debug assertions are enabled, panics if the safety invariant
    /// is not upheld.
    #[inline(always)]
    pub const unsafe fn new_unchecked(first: u8, second: u8, third: u8) -> Self {
        debug_assert!(Self::is(first, second, third));
        // SAFETY: `first`, `second`, and `third` have values in the correct ranges per
        // the safety invariant of this method.
        unsafe {
            Self {
                first: LeadForThreeByte::new_unchecked(first),
                // INVARIANT UPHELD: From the safety invariant of this method.
                second: UnconstrainedContinuation::new_unchecked(second),
                third: UnconstrainedContinuation::new_unchecked(third),
            }
        }
    }

    /// Fallible constructor
    ///
    /// `Ok` if `first`, `second`, and `third` form a three-byte UTF-8 sequence and `Err` otherwise.
    #[inline(always)]
    pub const fn try_new(first: u8, second: u8, third: u8) -> Result<Self, Utf8ByteError> {
        if Self::is(first, second, third) {
            // SAFETY: We checked the safety invariant immediately above.
            Ok(unsafe { Self::new_unchecked(first, second, third) })
        } else {
            Err(Utf8ByteError)
        }
    }

    /// Fallible constructor with prepared bytes
    ///
    /// `Ok` if the prepared bytes form a three-byte UTF-8 sequence and `Err` otherwise.
    #[inline(always)]
    pub const fn try_new_with_prepared(
        prepared: PreparedThreeBytes,
    ) -> Result<Self, Utf8ByteError> {
        if Self::is_with_prepared(prepared) {
            // SAFETY: We checked the safety invariant immediately above.
            Ok(unsafe { Self::new_unchecked(prepared.first, prepared.second, prepared.third) })
        } else {
            Err(Utf8ByteError)
        }
    }

    /// Convert to typed byte tuple.
    ///
    /// # Safety-usable invariant
    ///
    /// The tuple represents a well-formed three-byte UTF-8 sequence. In particular,
    /// the value of the second tuple item is within the permissible range given the
    /// value of the first tuple item.
    #[inline(always)]
    pub const fn to_typed_bytes(
        self,
    ) -> (
        LeadForThreeByte,
        UnconstrainedContinuation,
        UnconstrainedContinuation,
    ) {
        (self.first, self.second, self.third)
    }

    /// Convert to `char`
    #[inline(always)]
    pub const fn to_char(self) -> char {
        let scalar = (self.first.low_four_u32() << 12)
            | (self.second.low_six_u32() << 6)
            | self.third.low_six_u32();
        debug_assert!(char::from_u32(scalar).is_some());
        // SAFETY: We distribute the bits in a way that forms
        // a valid scalar on the assumption that the bytes represent
        // a well-formed three-byte UTF-8 byte sequence.
        unsafe { char::from_u32_unchecked(scalar) }
    }

    sequence_methods!(
        /// View as `&[u8; 3]`
        ///
        /// # Safety-usable invariant
        ///
        /// The bytes form a single well-formed three-byte UTF-8 sequence.
        ,
        /// View as `&str`
        ///
        /// # Safety-usable invariant
        ///
        /// The string slice contains a single three-byte UTF-8 sequence.
        ,
    );
}

sequence_impls!(ThreeByteSequence, three_byte,);

/// Four-byte UTF-8 sequence
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(C)]
pub struct FourByteSequence {
    first: LeadForFourByte,
    /// # Safety invariant
    ///
    /// `second` must be in the correct range given the value of
    /// `first` so that `first`, `second`, `third`, and `fourth`
    /// form a well-formed UTF-8 sequence.
    second: UnconstrainedContinuation,
    third: UnconstrainedContinuation,
    fourth: UnconstrainedContinuation,
}

impl FourByteSequence {
    /// The number of bytes in this kind of UTF-8 sequence.
    pub const LEN: usize = 4;

    /// Whether `prepared` and `fourth` form a well-formed four-byte UTF-8 sequence.
    ///
    /// # Safety-usable invariant
    ///
    /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
    /// is satisfied.
    #[inline(always)]
    pub const fn is_with_prepared(prepared: PreparedThreeBytes, fourth: u8) -> bool {
        // The top five bits of `first` have to be 11110 for `first` to
        // be a valid four-byte lead byte.
        // `check` must be 0b10 for `second` and `third` to be
        // contextually valid continuations assuming that `first`
        // is a four-byte lead byte.
        // The top two bits of `fourth` must be 10 for `fourth` to
        // be a valid continuation.
        // We shift `check` left, instead of the other way round to
        // make the comparison constant fit in an immediate on aarch64.
        // When the top two bits of `fourth` are shifted to be the low
        // two bits, they don't overlap with the top five bits from
        // `first`.
        (((prepared.check as u16) << 8) | (((prepared.first & 0b1111_1000) | (fourth >> 6)) as u16))
            == 0b10_1111_0010
    }

    /// Whether the bytes form a well-formed four-byte UTF-8 sequence.
    ///
    /// # Safety-usable invariant
    ///
    /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
    /// is satisfied.
    #[inline(always)]
    pub const fn is(first: u8, second: u8, third: u8, fourth: u8) -> bool {
        Self::is_with_prepared(PreparedThreeBytes::new(first, second, third), fourth)
    }

    /// Unchecked constructor
    ///
    /// # Safety
    ///
    /// `first`, `second`, `third`, and `fourth` must form a four-byte UTF-8 sequence.
    ///
    /// # Panics
    ///
    /// When debug assertions are enabled, panics if the safety invariant
    /// is not upheld.
    #[inline(always)]
    pub const unsafe fn new_unchecked(first: u8, second: u8, third: u8, fourth: u8) -> Self {
        debug_assert!(Self::is(first, second, third, fourth));
        // SAFETY: `first`, `second`, `third`, and `fourth` have values in the correct ranges per
        // the safety invariant of this method.
        unsafe {
            Self {
                first: LeadForFourByte::new_unchecked(first),
                // INVARIANT UPHELD: From the safety invariant of this method.
                second: UnconstrainedContinuation::new_unchecked(second),
                third: UnconstrainedContinuation::new_unchecked(third),
                fourth: UnconstrainedContinuation::new_unchecked(fourth),
            }
        }
    }

    /// Fallible constructor
    ///
    /// `Ok` if `first`, `second`, `third`, and `fourth` form a four-byte UTF-8 sequence and `Err` otherwise.
    #[inline(always)]
    pub const fn try_new(
        first: u8,
        second: u8,
        third: u8,
        fourth: u8,
    ) -> Result<Self, Utf8ByteError> {
        if Self::is(first, second, third, fourth) {
            // SAFETY: We checked the safety invariant immediately above.
            Ok(unsafe { Self::new_unchecked(first, second, third, fourth) })
        } else {
            Err(Utf8ByteError)
        }
    }

    /// Fallible constructor with prepared bytes
    ///
    /// `Ok` if the prepared bytes and `fourth` form a four-byte UTF-8 sequence and `Err` otherwise.
    #[inline(always)]
    pub const fn try_new_with_prepared(
        prepared: PreparedThreeBytes,
        fourth: u8,
    ) -> Result<Self, Utf8ByteError> {
        if Self::is_with_prepared(prepared, fourth) {
            // SAFETY: We checked the safety invariant immediately above.
            Ok(unsafe {
                Self::new_unchecked(prepared.first, prepared.second, prepared.third, fourth)
            })
        } else {
            Err(Utf8ByteError)
        }
    }

    /// Convert to typed byte tuple.
    ///
    /// # Safety-usable invariant
    ///
    /// The tuple represents a well-formed four-byte UTF-8 sequence. In particular,
    /// the value of the second tuple item is within the permissible range given the
    /// value of the first tuple item.
    #[inline(always)]
    pub const fn to_typed_bytes(
        self,
    ) -> (
        LeadForFourByte,
        UnconstrainedContinuation,
        UnconstrainedContinuation,
        UnconstrainedContinuation,
    ) {
        (self.first, self.second, self.third, self.fourth)
    }

    /// Convert to `char`
    #[inline(always)]
    pub const fn to_char(self) -> char {
        let scalar = (self.first.low_three_u32() << 18)
            | (self.second.low_six_u32() << 12)
            | (self.third.low_six_u32() << 6)
            | self.fourth.low_six_u32();
        debug_assert!(char::from_u32(scalar).is_some());
        // SAFETY: We distribute the bits in a way that forms
        // a valid scalar on the assumption that the bytes represent
        // a well-formed four-byte UTF-8 byte sequence.
        unsafe { char::from_u32_unchecked(scalar) }
    }

    sequence_methods!(
        /// View as `&[u8; 4]`
        ///
        /// # Safety-usable invariant
        ///
        /// The bytes form a single well-formed four-byte UTF-8 sequence.
        ,
        /// View as `&str`
        ///
        /// # Safety-usable invariant
        ///
        /// The string slice contains a single four-byte UTF-8 sequence.
        ,
    );
}

sequence_impls!(FourByteSequence, four_byte,);

/// Wrapper for the first two bytes and a precomputed value.
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub struct PreparedTwoBytes {
    first: u8,
    second: u8,
    /// # Safety invariant
    ///
    /// `check` is 0 iff `first` and `second` form a prefix
    /// of a well-formed multibyte UTF-8 sequence (including
    /// a complete two-byte sequence).
    /// The lowest 2 bits are always 0.
    check: u8,
}

impl PreparedTwoBytes {
    /// Constructor
    ///
    /// `first` and `second` represent the first two bytes of a
    /// potential UTF-8 sequence for efficient checking for whether
    /// they actually form such a prefix.
    #[inline(always)]
    pub const fn new(first: u8, second: u8) -> Self {
        Self {
            first,
            second,
            // INVARIANT UPHELD: The safety-usable invariant of
            // `lookup()` satisfies the invariant of this field.
            check: UTF8_DATA.lookup(first, second),
        }
    }

    /// `true` iff the two bytes form a prefix of a
    /// well-formed multi-byte UTF-8 sequence.
    ///
    /// # Safety-usable invariant
    ///
    /// Callers may depend on the above for safety.
    #[inline(always)]
    pub const fn sequence_prefix(self) -> bool {
        // Directly from the field invariant of `check`.
        // (The low two bits of `check` are zero in any case.
        // The high six bits are zero if this is a valid
        // prefix for any multibyte sequence.)
        self.check == 0
    }
}

/// Wrapper for the first three bytes and a precomputed value.
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub struct PreparedThreeBytes {
    first: u8,
    second: u8,
    third: u8,
    /// # Safety invariant
    ///
    /// If `first` is not a lead byte for a two-byte sequence,
    /// `check` is `0b10` iff `first`, `second`, and `third`
    /// form a well-formed three-byte UTF-8 sequence or a
    /// three-byte prefix of a four-byte UTF-8 sequence.
    /// (Establish that `first` is not a lead byte for a two-byte
    /// sequence when making use of `check`.)
    check: u8,
}

impl PreparedThreeBytes {
    /// Constructor
    ///
    /// `first`, `second`, and `third` represent the first three bytes of a
    /// potential UTF-8 sequence for efficient checking for whether
    /// they actually form a prefix of a well-formed UTF-8 sequence.
    #[inline(always)]
    pub const fn new(first: u8, second: u8, third: u8) -> Self {
        Self::new_with_prepared(PreparedTwoBytes::new(first, second), third)
    }

    /// Constructor with already-prepared first two bytes
    ///
    /// `prepared` and `third` represent the first three bytes of a
    /// potential UTF-8 sequence for efficient checking for whether
    /// they actually form a prefix of a well-formed UTF-8 sequence.
    #[inline(always)]
    pub const fn new_with_prepared(prepared: PreparedTwoBytes, third: u8) -> Self {
        Self {
            first: prepared.first,
            second: prepared.second,
            third,
            // INVARIANT UPHELD: The high two bits of `third` are 10 iff
            // it is a continuation byte. We put these in the low two bits
            // of `check`. We get the high six bits from `prepared.check`
            // whose field invariant guarantees that its low two bits are
            // always zero (and, therefore, don't conflict with our placement
            // of the new bits) and that the high six bits are zero iff
            // `first` and `second` are part of a well-formed prefix.
            check: prepared.check | (third >> 6),
        }
    }

    /// Return `true` iff these three bytes represent a three-byte
    /// prefix of a well-formed four-byte UTF-8 sequence.
    ///
    /// # Safety-usable invariant
    ///
    /// The caller may rely on the result for safety.
    #[inline(always)]
    pub const fn prefix_of_four_byte(self) -> bool {
        // The top five bits of `first` have to be 11110 for `first` to
        // be a valid four-byte lead byte.
        // From the field invariant, `check` must be 0b10 for `second`
        // and `third` to be contextually valid continuations assuming that
        // `first` is a four-byte lead byte.
        // We shift `check` left to use the structure of the full
        // four-byte check.
        (((self.check as u16) << 8) | ((self.first & 0b1111_1000) as u16)) == 0b10_1111_0000
    }
}

// Any copyright to the test code below this comment is dedicated to the
// Public Domain. http://creativecommons.org/publicdomain/zero/1.0/

#[cfg(test)]
mod tests {
    use super::*;

    /// `true` iff `bytes` is well-formed UTF-8 consisting of exactly one
    /// scalar value.
    fn single_scalar_reference(bytes: &[u8]) -> bool {
        core::str::from_utf8(bytes).is_ok_and(|s| s.chars().count() == 1)
    }

    fn two_byte_prefix_reference(first: u8, second: u8) -> bool {
        if !super::LeadForMultiByte::is(first) {
            return false;
        }
        let (lower_bound, upper_bound) = match first {
            0xE0 => (0xA0, 0xBF),
            0xED => (0x80, 0x9F),
            0xF0 => (0x90, 0xBF),
            0xF4 => (0x80, 0x8F),
            _ => (0x80, 0xBF),
        };
        lower_bound <= second && second <= upper_bound
    }

    fn prefix_of_four_byte_reference(first: u8, second: u8, third: u8) -> bool {
        if !super::LeadForFourByte::is(first) {
            return false;
        }
        if !super::UnconstrainedContinuation::is(third) {
            return false;
        }
        let (lower_bound, upper_bound) = match first {
            0xE0 => (0xA0, 0xBF),
            0xED => (0x80, 0x9F),
            0xF0 => (0x90, 0xBF),
            0xF4 => (0x80, 0x8F),
            _ => (0x80, 0xBF),
        };
        lower_bound <= second && second <= upper_bound
    }

    #[test]
    fn test_bytes() {
        for first in 0..=0xFF {
            let b = first;
            macro_rules! check_range {
                ($type:ident, $range:expr) => {
                    let in_range = $range.contains(&b);
                    assert_eq!($type::is(b), in_range);
                    assert_eq!(
                        $type::try_new(b).map($type::to_u8),
                        in_range.then_some(b).ok_or(Utf8ByteError)
                    );
                };
            }
            check_range!(Ascii, 0x00..=0x7F);
            check_range!(NonAscii, 0x80..=0xFF);
            check_range!(UnconstrainedContinuation, 0x80..=0xBF);
            check_range!(LeadForMultiByte, 0xC2..=0xF4);
            check_range!(LeadForTwoByte, 0xC2..=0xDF);
            check_range!(LeadForThreeByte, 0xE0..=0xEF);
            check_range!(LeadForFourByte, 0xF0..=0xF4);

            if let Ok(ascii) = Ascii::try_new(b) {
                assert_eq!(ascii.to_char() as u32, b as u32);
                assert_eq!(ascii.as_bytes(), &[b]);
                assert_eq!(ascii.as_str().as_bytes(), &[b]);
            }
            if let Ok(non_ascii) = NonAscii::try_new(b) {
                assert_eq!(
                    UnconstrainedContinuation::try_from(non_ascii).map(|c| c.to_non_ascii()),
                    UnconstrainedContinuation::try_new(b).map(|c| c.to_non_ascii())
                );
                assert_eq!(
                    LeadForMultiByte::try_from(non_ascii).map(|l| l.to_non_ascii()),
                    LeadForMultiByte::try_new(b).map(|l| l.to_non_ascii())
                );
            }
            if let Ok(continuation) = UnconstrainedContinuation::try_new(b) {
                assert_eq!(continuation.low_six(), b & 0x3F);
                assert_eq!(continuation.to_non_ascii().to_u8(), b);
            }
            if let Ok(lead) = LeadForMultiByte::try_new(b) {
                assert_eq!(
                    LeadForTwoByte::try_from(lead).map(|l| l.to_lead_for_multi_byte()),
                    LeadForTwoByte::try_new(b).map(|l| l.to_lead_for_multi_byte())
                );
                assert_eq!(
                    LeadForThreeByte::try_from(lead).map(|l| l.to_lead_for_multi_byte()),
                    LeadForThreeByte::try_new(b).map(|l| l.to_lead_for_multi_byte())
                );
                assert_eq!(
                    LeadForFourByte::try_from(lead).map(|l| l.to_lead_for_multi_byte()),
                    LeadForFourByte::try_new(b).map(|l| l.to_lead_for_multi_byte())
                );
            }
            if let Ok(lead) = LeadForTwoByte::try_new(b) {
                assert_eq!(lead.low_five(), b & 0x1F);
                assert_eq!(lead.to_non_ascii().to_u8(), b);
            }
            if let Ok(lead) = LeadForThreeByte::try_new(b) {
                assert_eq!(lead.low_four(), b & 0x0F);
                assert_eq!(lead.to_non_ascii().to_u8(), b);
            }
            if let Ok(lead) = LeadForFourByte::try_new(b) {
                assert_eq!(lead.low_three(), b & 0x07);
                assert_eq!(lead.to_non_ascii().to_u8(), b);
            }

            for second in 0..=0xFF {
                let prepared_two = PreparedTwoBytes::new(first, second);
                assert_eq!(
                    prepared_two.sequence_prefix(),
                    two_byte_prefix_reference(first, second)
                );
                let expected = single_scalar_reference(&[first, second]);
                assert_eq!(TwoByteSequence::is(first, second), expected);
                assert_eq!(TwoByteSequence::is_with_prepared(prepared_two), expected);

                for third in 0..=0xFF {
                    let prepared_three = PreparedThreeBytes::new(first, second, third);
                    assert_eq!(
                        prepared_three,
                        PreparedThreeBytes::new_with_prepared(prepared_two, third)
                    );
                    assert_eq!(
                        prepared_three.prefix_of_four_byte(),
                        prefix_of_four_byte_reference(first, second, third)
                    );

                    let expected = single_scalar_reference(&[first, second, third]);
                    assert_eq!(ThreeByteSequence::is(first, second, third), expected);
                    assert_eq!(
                        ThreeByteSequence::try_new_with_prepared(prepared_three).is_ok(),
                        expected
                    );

                    // Limited value space for fourth byte to limit the duration of the test.
                    for fourth in [
                        0x00, 0x3F, 0x40, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xFF,
                    ] {
                        let expected = single_scalar_reference(&[first, second, third, fourth]);
                        assert_eq!(FourByteSequence::is(first, second, third, fourth), expected);
                        assert_eq!(
                            FourByteSequence::try_new_with_prepared(prepared_three, fourth).is_ok(),
                            expected
                        );
                    }
                }
            }
        }
    }

    #[test]
    fn test_scalars() {
        let mut previous_two: Option<TwoByteSequence> = None;
        let mut previous_three: Option<ThreeByteSequence> = None;
        let mut previous_four: Option<FourByteSequence> = None;
        for c in '\u{0}'..=char::MAX {
            let mut buf = [0u8; 4];
            let s: &str = c.encode_utf8(&mut buf);
            let b = s.as_bytes();
            match *b {
                [first] => {
                    let ascii = Ascii::try_new(first).unwrap();
                    assert_eq!(ascii.to_char(), c);
                    assert_eq!(ascii.as_str(), s);
                    assert_eq!(ascii.as_bytes(), s.as_bytes());
                }
                [first, second] => {
                    let sequence = TwoByteSequence::try_new(first, second).unwrap();
                    assert_eq!(
                        TwoByteSequence::try_new_with_prepared(PreparedTwoBytes::new(
                            first, second
                        )),
                        Ok(sequence)
                    );
                    assert!(PreparedTwoBytes::new(first, second).sequence_prefix());
                    assert_eq!(sequence.to_char(), c);
                    assert_eq!(sequence.as_str(), s);
                    assert_eq!(sequence.as_bytes(), s.as_bytes());
                    let (lead, continuation) = sequence.to_typed_bytes();
                    assert_eq!([lead.to_u8(), continuation.to_u8()], [first, second]);
                    assert!(previous_two < Some(sequence));
                    previous_two = Some(sequence);
                }
                [first, second, third] => {
                    let sequence = ThreeByteSequence::try_new(first, second, third).unwrap();
                    assert_eq!(
                        ThreeByteSequence::try_new_with_prepared(PreparedThreeBytes::new(
                            first, second, third
                        )),
                        Ok(sequence)
                    );
                    assert_eq!(sequence.to_char(), c);
                    assert_eq!(sequence.as_str(), s);
                    assert_eq!(sequence.as_bytes(), s.as_bytes());
                    let (lead, c1, c2) = sequence.to_typed_bytes();
                    assert_eq!(
                        [lead.to_u8(), c1.to_u8(), c2.to_u8()],
                        [first, second, third]
                    );
                    assert!(previous_three < Some(sequence));
                    previous_three = Some(sequence);
                }
                [first, second, third, fourth] => {
                    let sequence = FourByteSequence::try_new(first, second, third, fourth).unwrap();
                    let prepared = PreparedThreeBytes::new(first, second, third);
                    assert!(prepared.prefix_of_four_byte());
                    assert_eq!(
                        FourByteSequence::try_new_with_prepared(prepared, fourth),
                        Ok(sequence)
                    );
                    assert_eq!(sequence.to_char(), c);
                    assert_eq!(sequence.as_str(), s);
                    assert_eq!(sequence.as_bytes(), s.as_bytes());
                    let (lead, c1, c2, c3) = sequence.to_typed_bytes();
                    assert_eq!(
                        [lead.to_u8(), c1.to_u8(), c2.to_u8(), c3.to_u8()],
                        [first, second, third, fourth]
                    );
                    assert!(previous_four < Some(sequence));
                    previous_four = Some(sequence);
                }
                _ => unreachable!(),
            }
        }
    }

    #[test]
    fn test_niche() {
        assert_eq!(
            core::mem::size_of::<Option<Ascii>>(),
            core::mem::size_of::<Ascii>()
        );
        assert_eq!(
            core::mem::size_of::<Option<NonAscii>>(),
            core::mem::size_of::<NonAscii>()
        );
        assert_eq!(
            core::mem::size_of::<Option<UnconstrainedContinuation>>(),
            core::mem::size_of::<UnconstrainedContinuation>()
        );
        assert_eq!(
            core::mem::size_of::<Option<LeadForMultiByte>>(),
            core::mem::size_of::<LeadForMultiByte>()
        );
        assert_eq!(
            core::mem::size_of::<Option<LeadForTwoByte>>(),
            core::mem::size_of::<LeadForTwoByte>()
        );
        assert_eq!(
            core::mem::size_of::<Option<LeadForThreeByte>>(),
            core::mem::size_of::<LeadForThreeByte>()
        );
        assert_eq!(
            core::mem::size_of::<Option<LeadForFourByte>>(),
            core::mem::size_of::<LeadForFourByte>()
        );
        assert_eq!(
            core::mem::size_of::<Option<TwoByteSequence>>(),
            core::mem::size_of::<TwoByteSequence>()
        );
        assert_eq!(
            core::mem::size_of::<Option<ThreeByteSequence>>(),
            core::mem::size_of::<ThreeByteSequence>()
        );
        assert_eq!(
            core::mem::size_of::<Option<FourByteSequence>>(),
            core::mem::size_of::<FourByteSequence>()
        );
    }
}
