// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! The primary per-codepoint casefolding data

#[cfg(feature = "datagen")]
use super::exceptions::Exception;
#[cfg(feature = "datagen")]
use alloc::collections::BTreeSet;
#[cfg(feature = "datagen")]
use alloc::vec::Vec;
use core::num::TryFromIntError;
use icu_collections::codepointtrie::TrieValue;
use zerovec::ule::{AsULE, RawBytesULE, ULE, UleError};

/// The case of a Unicode character
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
pub enum CaseType {
    /// Lowercase letter
    Lower = 1,
    /// Uppercase letter
    Upper = 2,
    /// Titlecase letter
    Title = 3,
}

impl CaseType {
    pub(crate) const CASE_MASK: u16 = 0x3;

    // The casetype is stored in the codepoint trie as two bits.
    // After masking them to get a value between 0 and 3, this
    // function converts to `CaseType`.
    //
    // Returns `None` for uncased
    #[inline]
    pub(crate) fn from_masked_bits(b: u16) -> Option<Self> {
        debug_assert!(b & Self::CASE_MASK == b);
        match b {
            0 => None,
            1 => Some(CaseType::Lower),
            2 => Some(CaseType::Upper),
            _ => Some(CaseType::Title),
        }
    }
}

/// The dot type of a Unicode character. This indicates how dotted
/// letters (like `i` and `j`) combine with accents placed above the
/// letter.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
#[derive(Default)]
pub enum DotType {
    /// Normal characters with combining class 0
    #[default]
    NoDot = 0,
    /// Soft-dotted characters with combining class 0
    SoftDotted = 1,
    /// "Above" accents with combining class 230
    Above = 2,
    /// Other accent characters
    OtherAccent = 3,
}

impl DotType {
    pub(crate) const DOT_MASK: u16 = 0x3;

    // The dot type is stored in either the codepoint trie or the
    // exception table as two bits.  After shifting and masking them
    // to get a value between 0 and 3, this function converts to
    // DotType.
    #[inline]
    pub(crate) fn from_masked_bits(b: u16) -> Self {
        debug_assert!(b & Self::DOT_MASK == b);
        match b {
            0 => DotType::NoDot,
            1 => DotType::SoftDotted,
            2 => DotType::Above,
            _ => DotType::OtherAccent,
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub(crate) enum MappingKind {
    Lower = 0,
    Fold = 1,
    Upper = 2,
    Title = 3,
}

/// Case mapping data associated with a single code point
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
pub struct CaseMapData {
    /// Whether this is default-ignoreable
    #[cfg_attr(feature = "serde", serde(rename = "ignoreable"))]
    pub is_ignoreable: bool,
    /// The rest of the case mapping data
    pub kind: CaseMapDataKind,
}

impl CaseMapData {
    /// The [`CaseMapData`] for a code point that is uncased, insensitive, and has no dot type.
    pub const UNCASED_INSENSITIVE_NO_DOT: Self = Self {
        is_ignoreable: false,
        kind: CaseMapDataKind::Uncased(NonExceptionData {
            is_sensitive: false,
            dot_type: DotType::NoDot,
        }),
    };

    /// Creates a new [`CaseMapData`] for a code point, given the relevant data.
    #[cfg(feature = "datagen")]
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        exceptions: &mut Vec<Exception<'_>>,
        c: char,
        is_ignoreable: bool,
        is_sensitive: bool,
        dot_type: DotType,
        case_type: Option<CaseType>,
        simple_upper: Option<char>,
        simple_lower: Option<char>,
        simple_title: Option<char>,
        full_lower: Option<&str>,
        full_upper: Option<&str>,
        full_title: Option<&str>,
        has_conditional_map: bool,
        simple_fold: Option<char>,
        full_fold: Option<&str>,
        has_conditional_fold: bool,
        mut full_closure: BTreeSet<char>,
    ) -> Self {
        // Don't store full mappings that are the same as the simple mapping, unless there is a conditional case mapping.
        let full_lower = full_lower.filter(|&s| {
            !has_conditional_map && s != simple_lower.unwrap_or(c).encode_utf8(&mut [0; 4])
        });
        let full_upper = full_upper.filter(|&s| {
            !has_conditional_map && s != simple_upper.unwrap_or(c).encode_utf8(&mut [0; 4])
        });
        let full_title = full_title.filter(|&s| {
            !has_conditional_map
                && s != simple_title
                    .or(simple_upper)
                    .unwrap_or(c)
                    .encode_utf8(&mut [0; 4])
        });

        // Don't store full case folding that is the same as the simple case folding, unless there is a conditional case folding.
        let full_fold = full_fold.filter(|f| {
            !has_conditional_fold
                && (f.chars().nth(1).is_some()
                    || f.chars()
                        .next()
                        .is_some_and(|c| c != simple_fold.unwrap_or(c)))
        });

        // We can use delta encoding if uppercase and titlecase match, and there is no special simple case folding.
        let delta = if simple_upper == simple_title
            && simple_fold.is_none_or(|s| s == simple_lower.unwrap_or(c))
        {
            if let Some(u) = simple_upper
                && Some(CaseType::Lower) == case_type
            {
                Some((u as i32 - c as i32, u))
            } else if let Some(l) = simple_lower
                && let Some(CaseType::Upper | CaseType::Title) = case_type
            {
                Some((l as i32 - c as i32, l))
            } else {
                Some((0, c))
            }
        } else {
            None
        };

        // Remove characters from the closure that are already covered by the simple case mappings.
        if let Some(l) = simple_lower {
            full_closure.remove(&l);
        }
        if let Some(u) = simple_upper {
            full_closure.remove(&u);
        }
        if let Some(t) = simple_title {
            full_closure.remove(&t);
        }
        if let Some(s) = simple_fold {
            full_closure.remove(&s);
        }

        let no_simple_case_folding = simple_fold.is_none() && simple_lower.is_some();

        let needs_exception_non_closure = delta
            .and_then(|(d, _)| u8::try_from(d.abs()).ok())
            .is_none()
            || no_simple_case_folding
            || has_conditional_fold
            || has_conditional_map
            || full_lower.is_some()
            || full_fold.is_some()
            || full_upper.is_some()
            || full_title.is_some();

        let kind = if needs_exception_non_closure || !full_closure.is_empty() {
            // Don't use the delta if we're only here because of a non-trivial closure.
            let delta = delta.filter(|_| needs_exception_non_closure);

            // TODO: it's not clear to me why we need this
            let delta = delta.filter(|&(d, _)| d != 0);

            let exception = super::exceptions::DecodedException {
                bits: super::exception_helpers::ExceptionBits {
                    no_simple_case_folding,
                    is_sensitive,
                    dot_type,
                    has_conditional_map,
                    has_conditional_fold,
                    negative_delta: delta.is_some_and(|(d, _)| d < 0),
                },
                simple_case_delta: delta.map(|(d, _)| d.unsigned_abs()),
                lowercase: simple_lower.filter(|&l| delta.is_none_or(|(_, d)| d != l)),
                uppercase: simple_upper.filter(|&u| delta.is_none_or(|(_, d)| d != u)),
                titlecase: simple_title.filter(|&t| Some(t) != simple_upper),
                full_lowercase: full_lower.map(Into::into),
                full_uppercase: full_upper.map(Into::into),
                full_titlecase: full_title.map(Into::into),
                casefold: simple_fold.filter(|&s| delta.is_none() && Some(s) != simple_lower),
                full_casefold: full_fold.map(Into::into),
                closure: (!full_closure.is_empty()).then(|| full_closure.into_iter().collect()),
            }
            .encode();

            CaseMapDataKind::Exception(
                case_type,
                exceptions
                    .iter()
                    .position(|x| x == &exception)
                    .unwrap_or_else(|| {
                        exceptions.push(exception);
                        exceptions.len() - 1
                    }) as u16,
            )
        } else if let Some((delta, _)) = delta
            && let Some(case_type) = case_type
        {
            CaseMapDataKind::Delta(
                NonExceptionData {
                    is_sensitive,
                    dot_type,
                },
                case_type,
                delta as i16,
            )
        } else {
            CaseMapDataKind::Uncased(NonExceptionData {
                is_sensitive,
                dot_type,
            })
        };

        Self {
            is_ignoreable,
            kind,
        }
    }
}

/// A subset of case mapping data associated with a single code point
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum CaseMapDataKind {
    /// This code point is an exception. Provides the case type of its own case
    /// and the exception index stored in [`CaseMapExceptions`]
    ///
    /// [`CaseMapExceptions`]: crate::provider::exceptions::CaseMapExceptions
    Exception(Option<CaseType>, u16),
    /// This code point is uncased, and has the following extra data
    Uncased(NonExceptionData),
    /// This code point is cased. We store the extra data, its case type, and a *delta*
    /// that can be used to get its casemapped codepoint.
    Delta(NonExceptionData, CaseType, i16),
}

/// Data that is stored in [`CaseMapData`] when it is *not* an exception
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider::data))]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct NonExceptionData {
    /// Whether or not the type is case-sensitive
    #[cfg_attr(feature = "serde", serde(rename = "sensitive"))]
    pub is_sensitive: bool,
    /// The "dot type"
    pub dot_type: DotType,
}

impl CaseMapData {
    #[inline]
    pub(crate) fn case_type(self) -> Option<CaseType> {
        match self.kind {
            CaseMapDataKind::Exception(case_type, ..) => case_type,
            CaseMapDataKind::Delta(_, case_type, _) => Some(case_type),
            CaseMapDataKind::Uncased(..) => None,
        }
    }

    #[inline]
    pub(crate) fn is_upper_or_title(self) -> bool {
        match self.case_type() {
            None | Some(CaseType::Lower) => false,
            Some(CaseType::Upper) | Some(CaseType::Title) => true,
        }
    }

    #[inline]
    pub(crate) fn is_relevant_to(self, kind: MappingKind) -> bool {
        match kind {
            MappingKind::Lower | MappingKind::Fold => self.is_upper_or_title(),
            MappingKind::Upper | MappingKind::Title => self.case_type() == Some(CaseType::Lower),
        }
    }

    #[inline]
    pub(crate) fn is_ignorable(self) -> bool {
        self.is_ignoreable
    }

    #[inline]
    pub(crate) fn has_exception(self) -> bool {
        matches!(self.kind, CaseMapDataKind::Exception(..))
    }

    // Returns true if this code point is case-sensitive.
    // only in the non-exception case
    // This is not currently exposed.
    #[inline]
    pub(crate) fn is_sensitive(self) -> bool {
        match self.kind {
            CaseMapDataKind::Exception(..) => false,
            CaseMapDataKind::Delta(ned, ..) => ned.is_sensitive,
            CaseMapDataKind::Uncased(ned) => ned.is_sensitive,
        }
    }

    #[inline]
    pub(crate) fn dot_type(self) -> DotType {
        match self.kind {
            CaseMapDataKind::Exception(..) => DotType::NoDot,
            CaseMapDataKind::Delta(ned, ..) => ned.dot_type,
            CaseMapDataKind::Uncased(ned) => ned.dot_type,
        }
    }

    // The delta between this code point and its upper/lowercase equivalent.
    // This should only be called for codepoints without exception data.
    //
    // Returns 0 for uncased types
    #[inline]
    pub(crate) fn delta(self) -> i16 {
        debug_assert!(!self.has_exception());
        match self.kind {
            CaseMapDataKind::Exception(..) => 0,
            CaseMapDataKind::Delta(.., delta) => delta,
            CaseMapDataKind::Uncased(..) => 0,
        }
    }

    // The index of the exception data for this codepoint in the exception
    // table. This should only be called for codepoints with exception data.
    #[inline]
    pub(crate) fn exception_index(self) -> u16 {
        debug_assert!(self.has_exception());
        if let CaseMapDataKind::Exception(_, i) = self.kind {
            i
        } else {
            0
        }
    }
}

impl TrieValue for CaseMapData {
    type TryFromU32Error = TryFromIntError;

    fn try_from_u32(i: u32) -> Result<Self, Self::TryFromU32Error> {
        u16::try_from(i).map(|u| AsULE::from_unaligned(CaseMapDataULE(u.to_unaligned())))
    }

    fn to_u32(self) -> u32 {
        u32::from(self.to_unaligned().0.as_unsigned_int())
    }
}

/// Packed casemappingdata type
///
/// Data format, copied from ICU4C casepropsbuilder.cpp:
///
/// ```text
/// Trie data word:
/// Bits
/// if(exception) {
///     15..4   unsigned exception index
/// } else {
///     if(not uncased) {
///         15..7   signed delta to simple case mapping code point
///                 (add delta to input code point)
///     } else {
///         15..7   reserved, 0
///     }
///      6..5   0 normal character with cc=0
///             1 soft-dotted character
///             2 cc=230
///             3 other cc
///             The runtime code relies on these two bits to be adjacent with this encoding.
/// }
///     4   case-sensitive
///     3   exception
///     2   case-ignorable
///  1..0   0 uncased
///         1 lowercase
///         2 uppercase
///         3 titlecase
///         The runtime code relies on the case-ignorable and case type bits 2..0
///         to be the lowest bits with this encoding.
/// ```
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[repr(transparent)]
pub struct CaseMapDataULE(RawBytesULE<2>);

impl CaseMapDataULE {
    // 1..0 case type
    const CASE_TYPE_BITS: u16 = 0x3;
    // 2 case-ignorable
    const CASE_IGNOREABLE_BIT: u16 = 0x4;
    // 3 exception
    const EXCEPTION_BIT: u16 = 0x8;
    // 4 case-sensitive
    const CASE_SENSITIVE_BIT: u16 = 0x10;
    // 15..4 unsigned exception index
    const EXCEPTION_SHIFT: u16 = 4;
    // 15..7 signed-delta to simple case mapping code point (or reserved)
    const DELTA_SHIFT: u16 = 7;
    // 6..5 dot type
    const DOT_TYPE_BITS: u16 = 0x60;
    const DOT_SHIFT: u16 = 5;
}

/// # Safety
///
/// Safety checklist for `ULE`:
///
/// 1. The type *must not* include any uninitialized or padding bytes: repr(transparent)
///    wrapper around ULE type
/// 2. The type must have an alignment of 1 byte: repr(transparent) wrapper around ULE type
/// 3. The impl of [`ULE::validate_bytes()`] *must* return an error if the given byte slice
///    would not represent a valid slice of this type: It does
/// 4. The impl of [`ULE::validate_bytes()`] *must* return an error if the given byte slice
///    cannot be used in its entirety (if its length is not a multiple of `size_of::<Self>()`):
///    it does, due to the [`RawBytesULE`] parse call
/// 5. All other methods *must* be left with their default impl, or else implemented according to
///    their respective safety guidelines: They have been
/// 6. The equality invariant is satisfied
unsafe impl ULE for CaseMapDataULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        let sixteens = RawBytesULE::<2>::parse_bytes_to_slice(bytes)?;

        for sixteen in sixteens {
            let sixteen = sixteen.as_unsigned_int();
            // The type has reserved bits in the
            // uncased + not exception case
            if sixteen & Self::EXCEPTION_BIT == 0 {
                // not an exception
                if sixteen & Self::CASE_TYPE_BITS == 0 {
                    // uncased
                    if sixteen >> Self::DELTA_SHIFT != 0 {
                        // We have some used bits in the reserved zone!
                        return Err(UleError::parse::<Self>());
                    }
                }
            }
        }
        Ok(())
    }
}

impl AsULE for CaseMapData {
    type ULE = CaseMapDataULE;

    fn from_unaligned(ule: Self::ULE) -> Self {
        let sixteen = ule.0.as_unsigned_int();

        let is_ignoreable = (sixteen & CaseMapDataULE::CASE_IGNOREABLE_BIT) != 0;
        let exception = (sixteen & CaseMapDataULE::EXCEPTION_BIT) != 0;

        let case_type = sixteen & CaseMapDataULE::CASE_TYPE_BITS;
        let case_type = CaseType::from_masked_bits(case_type);
        let kind = if exception {
            // No need to mask first since the exception bits start at 15
            let exception = sixteen >> CaseMapDataULE::EXCEPTION_SHIFT;
            CaseMapDataKind::Exception(case_type, exception)
        } else {
            let dot_type = (sixteen & CaseMapDataULE::DOT_TYPE_BITS) >> CaseMapDataULE::DOT_SHIFT;
            let dot_type = DotType::from_masked_bits(dot_type);
            let is_sensitive = (sixteen & CaseMapDataULE::CASE_SENSITIVE_BIT) != 0;
            let ned = NonExceptionData {
                dot_type,
                is_sensitive,
            };
            if let Some(case_type) = case_type {
                // no need to mask first since the delta bits start at 15
                // We can also cast as i16 first so we do not have to
                // sign-extend later
                let delta = (sixteen as i16) >> CaseMapDataULE::DELTA_SHIFT;
                CaseMapDataKind::Delta(ned, case_type, delta)
            } else {
                CaseMapDataKind::Uncased(ned)
            }
        };
        CaseMapData {
            is_ignoreable,
            kind,
        }
    }

    fn to_unaligned(self) -> Self::ULE {
        let mut sixteen = 0;
        if self.is_ignoreable {
            sixteen |= CaseMapDataULE::CASE_IGNOREABLE_BIT;
        }
        match self.kind {
            CaseMapDataKind::Exception(case_type, e) => {
                sixteen |= CaseMapDataULE::EXCEPTION_BIT;
                sixteen |= e << CaseMapDataULE::EXCEPTION_SHIFT;
                sixteen |= case_type.map(|c| c as u16).unwrap_or(0);
            }
            CaseMapDataKind::Uncased(ned) => {
                sixteen |= (ned.dot_type as u16) << CaseMapDataULE::DOT_SHIFT;
                if ned.is_sensitive {
                    sixteen |= CaseMapDataULE::CASE_SENSITIVE_BIT;
                }
                // Remaining bytes are left at zero
                // case_type is Uncased (0)
            }
            CaseMapDataKind::Delta(ned, case_type, delta) => {
                // First shift (which keeps the signedness), then cast to the
                // right type
                sixteen |= (delta << CaseMapDataULE::DELTA_SHIFT) as u16;
                sixteen |= (ned.dot_type as u16) << CaseMapDataULE::DOT_SHIFT;
                if ned.is_sensitive {
                    sixteen |= CaseMapDataULE::CASE_SENSITIVE_BIT;
                }
                sixteen |= case_type as u16;
            }
        }
        CaseMapDataULE(sixteen.to_unaligned())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_roundtrip() {
        const TESTCASES: &[CaseMapData] = &[
            CaseMapData {
                is_ignoreable: true,
                kind: CaseMapDataKind::Exception(Some(CaseType::Title), 923),
            },
            CaseMapData {
                is_ignoreable: false,
                kind: CaseMapDataKind::Exception(None, 923),
            },
            CaseMapData {
                is_ignoreable: true,
                kind: CaseMapDataKind::Delta(
                    NonExceptionData {
                        is_sensitive: true,
                        dot_type: DotType::SoftDotted,
                    },
                    CaseType::Upper,
                    50,
                ),
            },
            CaseMapData {
                is_ignoreable: false,
                kind: CaseMapDataKind::Delta(
                    NonExceptionData {
                        is_sensitive: true,
                        dot_type: DotType::SoftDotted,
                    },
                    CaseType::Upper,
                    -50,
                ),
            },
            CaseMapData {
                is_ignoreable: false,
                kind: CaseMapDataKind::Uncased(NonExceptionData {
                    is_sensitive: false,
                    dot_type: DotType::SoftDotted,
                }),
            },
        ];

        for case in TESTCASES {
            let ule = case.to_unaligned();
            let roundtrip = CaseMapData::from_unaligned(ule);
            assert_eq!(*case, roundtrip);
        }
    }
}
