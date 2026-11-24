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

//! Types for UTF-16 code unit sequences and their parts.

use core::fmt::Formatter;
use core::num::NonZeroU16;

/// A type for signaling UTF-16 errors.
#[derive(Debug, PartialEq, Clone, Copy, Hash, Eq)]
#[non_exhaustive]
pub struct Utf16CodeUnitError;

impl core::fmt::Display for Utf16CodeUnitError {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), core::fmt::Error> {
        write!(f, "code unit does not fit the requested position in UTF-16")
    }
}

impl core::error::Error for Utf16CodeUnitError {}

macro_rules! surrogate_impl {
    ($min:expr,
     $max:expr,
     $(#[$is_meta:meta])*,
     $(#[$unchecked_meta:meta])*,
     $(#[$checked_meta:meta])*,
    ) => {
        /// Minimum possible value.
        pub const MIN: Self = const {
            if $min == 0 {
                panic!();
            }
            Self {
                code_unit: if let Some(min) = NonZeroU16::new($min) {
                    min
                } else {
                    panic!();
                },
            }
        };

        /// Maximum possible value.
        pub const MAX: Self = const {
            Self {
                code_unit: if let Some(max) = NonZeroU16::new($max) {
                    max
                } else {
                    panic!();
                },
            }
        };

        $(#[$is_meta])*
        #[inline(always)]
        pub const fn is(code_unit: u16) -> bool {
            code_unit >= Self::MIN.to_u16() && code_unit <= Self::MAX.to_u16()
        }

        $(#[$unchecked_meta])*
        #[inline(always)]
        pub const unsafe fn new_unchecked(code_unit: u16) -> Self {
            debug_assert!(Self::is(code_unit));
            // INVARIANT UPHELD: The range is this function's safety invariant.
            // SAFETY: The range is enforced via `MIN` (and `MAX`), and `MIN` is checked
            // not to be zero in its `const` initialization.
            Self { code_unit: unsafe { NonZeroU16::new_unchecked(code_unit) } }
        }

        $(#[$checked_meta])*
        #[inline(always)]
        pub const fn try_new(code_unit: u16) -> Result<Self, Utf16CodeUnitError> {
            if Self::is(code_unit) {
                // SAFETY: We checked the safety invariant immediately above.
                Ok(unsafe { Self::new_unchecked(code_unit) })
            } else {
                Err(Utf16CodeUnitError)
            }
        }

        /// Convert to plain code unit
        #[inline(always)]
        pub const fn to_u16(self) -> u16 {
            self.code_unit.get()
        }
    };
}

macro_rules! to_surrogate {
    () => {
        /// Convert to `Surrogate`
        #[inline(always)]
        pub const fn to_surrogate(self) -> Surrogate {
            // INVARIANT UPHELD: `Self` has a stricter invariant than the target
            // type, so the conversion is safe.
            Surrogate {
                code_unit: self.code_unit,
            }
        }
    };
}

macro_rules! code_unit_impls {
    ($type:ident,
     $type_specific_ident:ident,
    ) => {
        impl TryFrom<u16> for $type {
            type Error = Utf16CodeUnitError;

            #[inline(always)]
            fn try_from(code_unit: u16) -> Result<Self, Utf16CodeUnitError> {
                Self::try_new(code_unit)
            }
        }

        impl From<$type> for u16 {
            #[inline(always)]
            fn from($type_specific_ident: $type) -> Self {
                $type_specific_ident.to_u16()
            }
        }
    };
}

/// For making sure that reviewing this file is sufficient for
/// reviewing the implementations of `Utf16CodeUnitSequence`.
trait Seal {}

/// Common output conversions for both UTF-16 code unit sequences.
#[allow(private_bounds)] // allow sealing
pub trait Utf16CodeUnitSequence: Seal + Copy + Clone + Into<char> + AsRef<[u16]> {}

macro_rules! sequence_impls {
    ($type:ident,
     $type_specific_ident:ident,
    ) => {
        impl Seal for $type {}

        impl Utf16CodeUnitSequence for $type {}

        impl From<$type> for char {
            #[inline(always)]
            fn from($type_specific_ident: $type) -> Self {
                $type_specific_ident.to_char()
            }
        }

        impl AsRef<[u16]> for $type {
            #[inline(always)]
            fn as_ref(&self) -> &[u16] {
                self.as_slice()
            }
        }
    };
}

/// A single-code-unit UTF-16 sequence representing a (non-surrogate)
/// character in the Basic Multilingual Plane.
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(transparent)]
pub struct Bmp {
    /// # Safety invariant
    ///
    /// Not a surrogate.
    code_unit: u16, // Would be nice to have a niche without hurting perf
}

impl Bmp {
    /// Whether `code_unit` is in range for this type (not a surrogate)
    ///
    /// # Safety-usable invariant
    ///
    /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
    /// is satisfied.
    #[inline(always)]
    pub const fn is(code_unit: u16) -> bool {
        !Surrogate::is(code_unit)
    }

    /// Unchecked constructor
    ///
    /// # Safety
    ///
    /// The caller must ensure that `code_unit` is _not_ in the range `0xD800..=0xDFFF`.
    ///
    /// # Panics
    ///
    /// When debug assertions are enabled, panics if the safety invariant
    /// is not upheld.
    #[inline(always)]
    pub const unsafe fn new_unchecked(code_unit: u16) -> Self {
        debug_assert!(Self::is(code_unit));
        // INVARIANT UPHELD: The range is this function's safety invariant.
        Self { code_unit }
    }

    /// Fallible constructor
    ///
    /// `Ok` if _not_ surrogate and `Err` otherwise.
    #[inline(always)]
    pub const fn try_new(code_unit: u16) -> Result<Self, Utf16CodeUnitError> {
        if Self::is(code_unit) {
            // SAFETY: We checked the safety invariant immediately above.
            Ok(unsafe { Self::new_unchecked(code_unit) })
        } else {
            Err(Utf16CodeUnitError)
        }
    }

    /// Convert to plain code unit
    #[inline(always)]
    pub const fn to_u16(self) -> u16 {
        self.code_unit
    }

    /// Convert to `char`
    #[inline(always)]
    pub const fn to_char(self) -> char {
        let scalar = self.code_unit as u32;
        debug_assert!(char::from_u32(scalar).is_some());
        // SAFETY: The invariant of `Bmp` guarantees that
        // we don't have a surrogate, so `scalar` is indeed
        // a Unicode scalar value.
        unsafe { char::from_u32_unchecked(scalar) }
    }

    /// View as `&[u16]`
    #[inline(always)]
    pub const fn as_slice(&self) -> &[u16] {
        core::slice::from_ref(&self.code_unit)
    }
}

code_unit_impls!(Bmp, bmp,);

sequence_impls!(Bmp, bmp,);

/// A single surrogate code unit, which can be a high
/// surrogate or a low surrogate
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(transparent)]
pub struct Surrogate {
    /// # Safety invariant
    ///
    /// A surrogate.
    code_unit: NonZeroU16,
}

impl Surrogate {
    surrogate_impl!(
        0xD800,
        0xDFFF,
        /// Whether `code_unit` is a surrogate.
        ///
        /// # Safety-usable invariant
        ///
        /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
        /// is satisfied.
        ,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// The caller must ensure that `code_unit` is in the range `0xD800..=0xDFFF`.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if surrogate and `Err` otherwise.
        ,
    );
}

code_unit_impls!(Surrogate, surrogate,);

/// A high surrogate
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(transparent)]
pub struct HighSurrogate {
    /// # Safety invariant
    ///
    /// A high surrogate.
    code_unit: NonZeroU16,
}

impl HighSurrogate {
    surrogate_impl!(
        0xD800,
        0xDBFF,
        /// Whether `code_unit` is a high surrogate.
        ///
        /// # Safety-usable invariant
        ///
        /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
        /// is satisfied.
        ,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// The caller must ensure that `code_unit` is in the range `0xD800..=0xDBFF`.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if high surrogate and `Err` otherwise.
        ,
    );

    to_surrogate!();

    /// Fallible constructor given a surrogate
    ///
    /// `Ok` if high surrogate and `Err` otherwise.
    #[inline(always)]
    pub const fn try_new_with_surrogate(surrogate: Surrogate) -> Result<Self, Utf16CodeUnitError> {
        let code_unit = surrogate.to_u16();
        if code_unit <= Self::MAX.to_u16() {
            // SAFETY: We checked the safety invariant immediately above (the other end
            // of the range comes from the field invariant of `Surrogate`).
            Ok(unsafe { Self::new_unchecked(code_unit) })
        } else {
            Err(Utf16CodeUnitError)
        }
    }
}

code_unit_impls!(HighSurrogate, high_surrogate,);

/// A low surrogate
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(transparent)]
pub struct LowSurrogate {
    /// # Safety invariant
    ///
    /// A low surrogate.
    code_unit: NonZeroU16,
}

impl LowSurrogate {
    surrogate_impl!(
        0xDC00,
        0xDFFF,
        /// Whether `code_unit` is a low surrogate.
        ///
        /// # Safety-usable invariant
        ///
        /// Iff this method returns `true`, the safety invariant of `new_unchecked()`
        /// is satisfied.
        ,
        /// Unchecked constructor
        ///
        /// # Safety
        ///
        /// The caller must ensure that `code_unit` is in the range `0xDC00..=0xDFFF`.
        ///
        /// # Panics
        ///
        /// When debug assertions are enabled, panics if the safety invariant
        /// is not upheld.
        ,
        /// Fallible constructor
        ///
        /// `Ok` if low surrogate and `Err` otherwise.
        ,
    );

    to_surrogate!();

    /// Fallible constructor given a surrogate
    ///
    /// `Ok` if low surrogate and `Err` otherwise.
    #[inline(always)]
    pub const fn try_new_with_surrogate(surrogate: Surrogate) -> Result<Self, Utf16CodeUnitError> {
        let code_unit = surrogate.to_u16();
        if code_unit >= Self::MIN.to_u16() {
            // SAFETY: We checked the safety invariant immediately above (the other end
            // of the range comes from the field invariant of `Surrogate`).
            Ok(unsafe { Self::new_unchecked(code_unit) })
        } else {
            Err(Utf16CodeUnitError)
        }
    }
}

code_unit_impls!(LowSurrogate, low_surrogate,);

/// A two-code-unit UTF-16 sequence
///
/// A high surrogate followed by a low surrogate representing
/// a Unicode scalar value outside the Basic Multilingual Plane.
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(C)]
pub struct SurrogatePair {
    high: HighSurrogate,
    low: LowSurrogate,
}

impl SurrogatePair {
    /// Constructor from a high surrogate and a low surrogate
    #[inline(always)]
    pub const fn new(high: HighSurrogate, low: LowSurrogate) -> Self {
        Self { high, low }
    }

    /// Convert to typed code unit tuple.
    #[inline(always)]
    pub const fn to_typed_code_units(self) -> (HighSurrogate, LowSurrogate) {
        (self.high, self.low)
    }

    /// Convert to `char`
    #[inline(always)]
    pub const fn to_char(self) -> char {
        let scalar = ((self.high.to_u16() as u32) << 10) + (self.low.to_u16() as u32)
            - (((0xD800u32 << 10) - 0x10000u32) + 0xDC00u32);
        debug_assert!(char::from_u32(scalar).is_some());
        // SAFETY: The above formulation results in a value in the `char` range given
        // the value range invariants of `HighSurrogate` and `LowSurrogate`.
        unsafe { char::from_u32_unchecked(scalar) }
    }

    /// View as `&[u16; 2]`
    ///
    /// # Safety-usable invariant
    ///
    /// The array contains a single well-formed surrogate pair.
    #[inline(always)]
    pub const fn as_code_units(&self) -> &[u16; 2] {
        if core::mem::size_of::<Self>() != core::mem::size_of::<[u16; 2]>() {
            panic!();
        }
        let ptr: *const Self = core::ptr::from_ref(self);
        // SAFETY: The `repr` of `Self` guarantees that it
        // is safe to view `Self` as `[u16; 2]`.
        unsafe { &*ptr.cast() }
    }

    /// View as `&[u16]`
    ///
    /// # Safety-usable invariant
    ///
    /// The slice contains a single well-formed surrogate pair.
    #[inline(always)]
    pub const fn as_slice(&self) -> &[u16] {
        self.as_code_units()
    }
}

sequence_impls!(SurrogatePair, surrogate_pair,);

// Separate type to keep the `try_new` constructors consistent in `Err()` containing
// an actual error type and to not characterize surrogates as errors.

/// Type for categorizing code units as either non-surrogates or surrogates.
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub enum BmpOrSurrogate {
    /// Non-surrogate code unit on the Basic Multilingual Plane
    Bmp(Bmp),
    /// Surrogate
    Surrogate(Surrogate),
}

impl BmpOrSurrogate {
    /// Constructor
    #[inline(always)]
    pub const fn new(code_unit: u16) -> Self {
        if Bmp::is(code_unit) {
            // SAFETY: We checked the invariant above.
            Self::Bmp(unsafe { Bmp::new_unchecked(code_unit) })
        } else {
            // SAFETY: We checked the invariant above.
            Self::Surrogate(unsafe { Surrogate::new_unchecked(code_unit) })
        }
    }
}

// TODO: Add 16-bit types that correspond to the value ranges of 1, 2, and 3-byte
// UTF-8 sequences.

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_code_unit_ranges() {
        for u in 0..=0xFFFFu16 {
            let surrogate = (0xD800..=0xDFFF).contains(&u);
            let high = (0xD800..=0xDBFF).contains(&u);
            let low = (0xDC00..=0xDFFF).contains(&u);

            assert_eq!(Bmp::is(u), !surrogate);
            assert_eq!(Surrogate::is(u), surrogate);
            assert_eq!(HighSurrogate::is(u), high);
            assert_eq!(LowSurrogate::is(u), low);

            assert_eq!(
                Bmp::try_new(u).map(Bmp::to_u16),
                Bmp::is(u).then_some(u).ok_or(Utf16CodeUnitError)
            );
            assert_eq!(
                Surrogate::try_new(u).map(Surrogate::to_u16),
                surrogate.then_some(u).ok_or(Utf16CodeUnitError)
            );
            assert_eq!(
                HighSurrogate::try_new(u).map(HighSurrogate::to_u16),
                high.then_some(u).ok_or(Utf16CodeUnitError)
            );
            assert_eq!(
                LowSurrogate::try_new(u).map(LowSurrogate::to_u16),
                low.then_some(u).ok_or(Utf16CodeUnitError)
            );

            match BmpOrSurrogate::new(u) {
                BmpOrSurrogate::Bmp(bmp) => {
                    assert!(!surrogate);
                    assert_eq!(bmp.to_u16(), u);
                    assert_eq!(Some(bmp.to_char()), char::from_u32(u as u32));
                    assert_eq!(bmp.as_slice(), &[u]);
                }
                BmpOrSurrogate::Surrogate(s) => {
                    assert!(surrogate);
                    assert_eq!(s.to_u16(), u);
                    assert_eq!(
                        HighSurrogate::try_new_with_surrogate(s).map(HighSurrogate::to_u16),
                        high.then_some(u).ok_or(Utf16CodeUnitError)
                    );
                    assert_eq!(
                        LowSurrogate::try_new_with_surrogate(s).map(LowSurrogate::to_u16),
                        low.then_some(u).ok_or(Utf16CodeUnitError)
                    );
                    if let Ok(h) = HighSurrogate::try_new(u) {
                        assert_eq!(h.to_surrogate(), s);
                    }
                    if let Ok(l) = LowSurrogate::try_new(u) {
                        assert_eq!(l.to_surrogate(), s);
                    }
                }
            }
        }
    }

    #[test]
    fn test_surrogate_pairs() {
        let mut previous: Option<(SurrogatePair, char)> = None;
        for high in 0xD800..=0xDBFFu16 {
            for low in 0xDC00..=0xDFFFu16 {
                let pair = SurrogatePair::new(
                    HighSurrogate::try_new(high).unwrap(),
                    LowSurrogate::try_new(low).unwrap(),
                );
                let expected = char::decode_utf16([high, low]).next().unwrap().unwrap();
                let c = pair.to_char();
                assert_eq!(c, expected);
                assert_eq!(pair.as_slice(), &[high, low]);
                let (h, l) = pair.to_typed_code_units();
                assert_eq!((h.to_u16(), l.to_u16()), (high, low));
                if let Some((previous_pair, previous_char)) = previous {
                    // `Ord` is consistent with scalar value order.
                    assert!(previous_pair < pair);
                    assert!(previous_char < c);
                }
                previous = Some((pair, c));
            }
        }
        assert_eq!(previous.unwrap().1, char::MAX);
    }

    #[test]
    fn test_niche() {
        // No niche for `Bmp` for now!
        assert_ne!(
            core::mem::size_of::<Option<Bmp>>(),
            core::mem::size_of::<Bmp>()
        );
        assert_eq!(
            core::mem::size_of::<Option<Surrogate>>(),
            core::mem::size_of::<Surrogate>()
        );
        assert_eq!(
            core::mem::size_of::<Option<HighSurrogate>>(),
            core::mem::size_of::<HighSurrogate>()
        );
        assert_eq!(
            core::mem::size_of::<Option<LowSurrogate>>(),
            core::mem::size_of::<LowSurrogate>()
        );
        assert_eq!(
            core::mem::size_of::<Option<SurrogatePair>>(),
            core::mem::size_of::<SurrogatePair>()
        );
    }
}
