// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data structures for packing of datetime patterns.

use super::pattern::{
    runtime::{Pattern, PatternBorrowed, PatternMetadata},
    PatternItem,
};
use crate::{size_test_macro::size_test, Length};
use alloc::vec::Vec;
use icu_plurals::{
    provider::{FourBitMetadata, PluralElementsPackedULE},
    PluralElements,
};
use icu_provider::prelude::*;
use zerovec::{VarZeroVec, ZeroSlice};

/// A field of [`PackedPatternsBuilder`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LengthPluralElements<T> {
    /// The "long" length pattern plural elements.
    pub long: PluralElements<T>,
    /// The "medium" length pattern plural elements.
    pub medium: PluralElements<T>,
    /// The "short" length pattern plural elements.
    pub short: PluralElements<T>,
}

/// A builder for a [`PackedPatterns`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PackedPatternsBuilder<'a> {
    /// Patterns always available.
    pub standard: LengthPluralElements<Pattern<'a>>,
    /// Patterns for variant 0. If `None`, falls back to standard.
    pub variant0: Option<LengthPluralElements<Pattern<'a>>>,
    /// Patterns for variant 1. If `None`, falls back to standard.
    pub variant1: Option<LengthPluralElements<Pattern<'a>>>,
}

size_test!(PackedPatterns, packed_skeleton_data_size, 32);

icu_provider::data_marker!(
    /// `BuddhistDateNeoSkeletonPatternsV1`
    BuddhistDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `ChineseDateNeoSkeletonPatternsV1`
    ChineseDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `CopticDateNeoSkeletonPatternsV1`
    CopticDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `DangiDateNeoSkeletonPatternsV1`
    DangiDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `EthiopianDateNeoSkeletonPatternsV1`
    EthiopianDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `GregorianDateNeoSkeletonPatternsV1`
    GregorianDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `HebrewDateNeoSkeletonPatternsV1`
    HebrewDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `IndianDateNeoSkeletonPatternsV1`
    IndianDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `IslamicDateNeoSkeletonPatternsV1`
    IslamicDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `JapaneseDateNeoSkeletonPatternsV1`
    JapaneseDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `JapaneseExtendedDateNeoSkeletonPatternsV1`
    JapaneseExtendedDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `PersianDateNeoSkeletonPatternsV1`
    PersianDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `RocDateNeoSkeletonPatternsV1`
    RocDateNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);
icu_provider::data_marker!(
    /// `TimeNeoSkeletonPatternsV1`
    TimeNeoSkeletonPatternsV1,
    PackedPatterns<'static>
);

// Main data struct for packed datetime patterns.
#[doc = packed_skeleton_data_size!()]
///
/// ## Variants
///
/// This supports a set of "standard" patterns plus up to two "variants".
/// The variants are currently used by year formatting:
///
/// - Standard: Year, which could be partial precision (2-digit Gregorain)
/// - Variant 0: Full Year, which is always full precision
/// - Variant 1: Year With Era
///
/// Variants should be used when the pattern could depend on the value being
/// formatted. For example, with [`YearStyle::Auto`], any of these three
/// patterns could be selected based on the year value.
///
/// ## Representation
///
/// Currently, there are at most 9 patterns that need to be stored together,
/// named according to this table:
///
/// |        | Standard | Variant 0 | Variant 1 |
/// |--------|----------|-----------|-----------|
/// | Long   | La       | Lb        | Lc        |
/// | Medium | Ma       | Mb        | Mc        |
/// | Short  | Sa       | Sb        | Sc        |
///
/// The header byte encodes which pattern in the patterns array corresponds to
/// a particular cell in the table. It contains the following information:
///
/// - Bits 0-1: "LMS" value of the standard column
/// - Bit 2: "Q" value: 1 for directly-indexed variants; 0 for per-cell offsets
/// - Bits 3-20: Packed offset into patterns table for each variant cell
/// - Bits 21-31: unused/reserved
///
/// The LMS value determines which pattern index is used for the first column:
///
/// | LMS Value   | Long Index | Medium Index | Short Index |
/// |-------------|------------|--------------|-------------|
/// | 0 (L=M=S)   | 0          | 0            | 0           |
/// | 1 (L, M=S)  | 0          | 1            | 1           |
/// | 2 (L=M, S)  | 0          | 0            | 1           |
/// | 3 (L, M, S) | 0          | 1            | 2           |
///
/// If bit 2 is 1 (Q=1), it means there is one pattern per table cell,
/// with the index offset by the short index `S` from the table above.
/// However, this requires storing multiple, possibly duplicate, patterns in
/// the packed structure. The more common case is Q=0 and then to store
/// per-cell offsets in chunks of 3 bits per cell:
///
/// - Chunk = 0: Inherit according to the table below
/// - Chunk = 1-7: Use pattern index Chunk - 1
///
/// This is summarized below:
///
/// | Cell in Table | Q=1 Pattern Index | Q=0 Header Bits | Inheritance |
/// |---------------|-------------------|-----------------|-------------|
/// | Lb            | S + 1             | 3-5             | La          |
/// | Mb            | S + 2             | 6-8             | Ma          |
/// | Sb            | S + 3             | 9-11            | Sa          |
/// | Lc            | S + 4             | 12-14           | La          |
/// | Mc            | S + 5             | 15-17           | Ma          |
/// | Sc            | S + 6             | 18-20           | Sa          |
///
/// As a result, if there are no variants, bits 2 and higher will be all zero,
/// making the header int suitable for varint packing, such as that used by
/// postcard and other size-optimized serialization formats.
///
/// [`YearStyle::Auto`]: crate::options::YearStyle::Auto
#[derive(Debug, PartialEq, Eq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider))]
pub struct PackedPatterns<'data> {
    /// An encoding of which standard/variant cell corresponds to which entry
    /// in the patterns table. See class docs.
    pub header: u32,
    /// The list of patterns. Length should be between 1 and 9,
    /// depending on the header.
    pub elements: VarZeroVec<'data, PluralElementsPackedULE<ZeroSlice<PatternItem>>>,
}

icu_provider::data_struct!(
    PackedPatterns<'_>,
    #[cfg(feature = "datagen")]
);

mod constants {
    /// Value when standard long, medium, and short are all the same
    pub(super) const LMS: u32 = 0;
    /// Value when standard medium is the same as short but not long
    pub(super) const L_MS: u32 = 1;
    /// Value when standard medium is the same as long but not short
    pub(super) const LM_S: u32 = 2;
    /// Bit that indicates that standard medium differs from standard long
    pub(super) const M_DIFFERS: u32 = 0x1;
    /// Bit that indicates that standard short differs from standard medium
    pub(super) const S_DIFFERS: u32 = 0x2;
    /// Bitmask over all LMS values
    pub(super) const LMS_MASK: u32 = 0x3;
    /// Bit that indicates whether there are per-cell chunks
    pub(super) const Q_BIT: u32 = 0x4;
    /// A mask applied to individual chunks (the largest possible chunk)
    pub(super) const CHUNK_MASK: u32 = 0x7;
}

struct UnpackedPatterns<'a> {
    pub(super) has_explicit_medium: bool,
    pub(super) has_explicit_short: bool,
    pub(super) variant_indices: VariantIndices,
    pub(super) elements: Vec<PluralElements<Pattern<'a>>>,
}

#[repr(u8)]
#[derive(Copy, Clone)]
enum VariantPatternIndex {
    Inherit = 0,
    I0 = 1,
    I1 = 2,
    I2 = 3,
    I3 = 4,
    I4 = 5,
    I5 = 6,
    I6 = 7,
}

impl VariantPatternIndex {
    #[cfg(feature = "datagen")]
    pub(super) fn from_header_with_shift(header: u32, shift: u32) -> Self {
        match Self::try_from_u32((header >> shift) & constants::CHUNK_MASK) {
            Some(x) => x,
            None => {
                debug_assert!(false, "unreachable");
                Self::Inherit
            }
        }
    }

    fn try_from_u32(u: u32) -> Option<Self> {
        match u {
            0 => Some(Self::Inherit),
            1 => Some(Self::I0),
            2 => Some(Self::I1),
            3 => Some(Self::I2),
            4 => Some(Self::I3),
            5 => Some(Self::I4),
            6 => Some(Self::I5),
            7 => Some(Self::I6),
            _ => None,
        }
    }

    pub(super) fn try_from_chunks_u32(chunks: [u32; 6]) -> Option<[Self; 6]> {
        let [c0, c1, c2, c3, c4, c5] = chunks;
        Some([
            Self::try_from_u32(c0)?,
            Self::try_from_u32(c1)?,
            Self::try_from_u32(c2)?,
            Self::try_from_u32(c3)?,
            Self::try_from_u32(c4)?,
            Self::try_from_u32(c5)?,
        ])
    }

    pub(super) fn to_chunks_u32(chunks: [Self; 6]) -> [u32; 6] {
        let [c0, c1, c2, c3, c4, c5] = chunks;
        [
            c0 as u32, c1 as u32, c2 as u32, c3 as u32, c4 as u32, c5 as u32,
        ]
    }
}

enum VariantIndices {
    OnePatternPerVariant,
    IndicesPerVariant([VariantPatternIndex; 6]),
}

impl<'a> UnpackedPatterns<'a> {
    pub(super) fn build(&self) -> PackedPatterns<'static> {
        let mut header = 0u32;
        if self.has_explicit_medium {
            header |= constants::M_DIFFERS;
        }
        if self.has_explicit_short {
            header |= constants::S_DIFFERS;
        }
        match self.variant_indices {
            VariantIndices::OnePatternPerVariant => {
                header |= constants::Q_BIT;
            }
            VariantIndices::IndicesPerVariant(chunks) => {
                let mut shift = 3;
                for chunk_u32 in VariantPatternIndex::to_chunks_u32(chunks).iter() {
                    debug_assert!(*chunk_u32 <= constants::CHUNK_MASK);
                    header |= *chunk_u32 << shift;
                    shift += 3;
                }
            }
        }
        let elements: Vec<PluralElements<(FourBitMetadata, &ZeroSlice<PatternItem>)>> = self
            .elements
            .iter()
            .map(|plural_elements| {
                plural_elements.as_ref().map(|pattern| {
                    (
                        pattern.metadata.to_four_bit_metadata(),
                        pattern.items.as_slice(),
                    )
                })
            })
            .collect();
        PackedPatterns {
            header,
            elements: elements.as_slice().into(),
        }
    }

    #[cfg(feature = "datagen")]
    pub(super) fn from_packed(packed: &'a PackedPatterns<'_>) -> Self {
        let variant_indices = if (packed.header & constants::Q_BIT) != 0 {
            VariantIndices::OnePatternPerVariant
        } else {
            VariantIndices::IndicesPerVariant([
                VariantPatternIndex::from_header_with_shift(packed.header, 3),
                VariantPatternIndex::from_header_with_shift(packed.header, 6),
                VariantPatternIndex::from_header_with_shift(packed.header, 9),
                VariantPatternIndex::from_header_with_shift(packed.header, 12),
                VariantPatternIndex::from_header_with_shift(packed.header, 15),
                VariantPatternIndex::from_header_with_shift(packed.header, 18),
            ])
        };
        let elements = packed
            .elements
            .iter()
            .map(|plural_elements| {
                plural_elements.decode().map(|(metadata, items)| {
                    PatternBorrowed {
                        metadata: PatternMetadata::from_u8(metadata.get()),
                        items,
                    }
                    .as_pattern()
                })
            })
            .collect();
        Self {
            has_explicit_medium: (packed.header & constants::M_DIFFERS) != 0,
            has_explicit_short: (packed.header & constants::S_DIFFERS) != 0,
            variant_indices,
            elements,
        }
    }
}

impl PackedPatternsBuilder<'_> {
    /// Builds a packed pattern representation from the builder.
    pub fn build(mut self) -> PackedPatterns<'static> {
        self.simplify();

        // Initialize the elements vector with the standard patterns.
        let mut elements = Vec::new();
        let mut has_explicit_medium = false;
        let mut has_explicit_short = false;
        elements.push(self.standard.long.as_ref().map(Pattern::as_ref));
        let mut s_offset = 0;
        if self.standard.medium != self.standard.long {
            elements.push(self.standard.medium.as_ref().map(Pattern::as_ref));
            has_explicit_medium = true;
            s_offset += 1;
        }
        if self.standard.short != self.standard.medium {
            elements.push(self.standard.short.as_ref().map(Pattern::as_ref));
            has_explicit_short = true;
            s_offset += 1;
        }

        // Fill in the variant patterns
        let variant_patterns = [
            self.variant0.as_ref().map(|v| &v.long),
            self.variant0.as_ref().map(|v| &v.medium),
            self.variant0.as_ref().map(|v| &v.short),
            self.variant1.as_ref().map(|v| &v.long),
            self.variant1.as_ref().map(|v| &v.medium),
            self.variant1.as_ref().map(|v| &v.short),
        ];
        let fallbacks = [
            &self.standard.long,
            &self.standard.medium,
            &self.standard.short,
            &self.standard.long,
            &self.standard.medium,
            &self.standard.short,
        ];
        let mut chunks = [0u32; 6]; // per-cell chunk values
        for ((pattern, fallback), chunk) in variant_patterns
            .iter()
            .zip(fallbacks.iter())
            .zip(chunks.iter_mut())
        {
            if let Some(pattern) = pattern {
                if pattern != fallback {
                    *chunk = match elements.iter().position(|p| p == *pattern) {
                        Some(i) => i as u32 + 1,
                        None => {
                            elements.push(pattern.as_ref().map(Pattern::as_ref));
                            elements.len() as u32
                        }
                    }
                }
            }
        }

        // Check to see if we need to switch to Q=1 mode. We need to do this
        // if any of the calculated chunk values is too big (larger than 7).
        let variant_indices = if let Some(chunks) = VariantPatternIndex::try_from_chunks_u32(chunks)
        {
            // per-cell offsets
            VariantIndices::IndicesPerVariant(chunks)
        } else {
            // one pattern per table cell
            elements.truncate(s_offset + 1);
            elements.extend(variant_patterns.into_iter().zip(fallbacks.iter()).map(
                |(pattern, fallback)| pattern.unwrap_or(fallback).as_ref().map(Pattern::as_ref),
            ));
            VariantIndices::OnePatternPerVariant
        };

        // Now we can build the data representation
        let unpacked = UnpackedPatterns {
            has_explicit_medium,
            has_explicit_short,
            variant_indices,
            elements,
        };
        unpacked.build()
    }

    fn simplify(&mut self) {
        if self.variant0.as_ref() == Some(&self.standard) {
            self.variant0 = None;
        }
        if self.variant1.as_ref() == Some(&self.standard) {
            self.variant1 = None;
        }
    }
}

pub(crate) enum PackedSkeletonVariant {
    Standard,
    Variant0,
    Variant1,
}

impl PackedPatterns<'_> {
    pub(crate) fn get(&self, length: Length, variant: PackedSkeletonVariant) -> PatternBorrowed {
        use Length::*;
        use PackedSkeletonVariant::*;
        let lms = self.header & constants::LMS_MASK;
        let pattern_index = if matches!(variant, Standard) {
            // Standard pattern (first column)
            match (length, lms) {
                (Long, _) => 0,
                (Medium, constants::LMS | constants::LM_S) => 0,
                (Medium, _) => 1,
                (Short, constants::LMS) => 0,
                (Short, constants::L_MS | constants::LM_S) => 1,
                (Short, _) => 2,
            }
        } else {
            let s_offset = match lms {
                constants::LMS => 0,
                constants::L_MS | constants::LM_S => 1,
                _ => 2,
            };
            let q = self.header & constants::Q_BIT;
            if q == 0 {
                // per-cell offsets
                let chunk_in_low_bits = match (length, variant) {
                    (Long, Variant0) => self.header >> 3,
                    (Medium, Variant0) => self.header >> 6,
                    (Short, Variant0) => self.header >> 9,
                    (Long, Variant1) => self.header >> 12,
                    (Medium, Variant1) => self.header >> 15,
                    (Short, Variant1) => self.header >> 18,
                    (_, Standard) => {
                        debug_assert!(false, "unreachable");
                        return PatternBorrowed::DEFAULT;
                    }
                };
                let chunk = chunk_in_low_bits & constants::CHUNK_MASK;
                if chunk == 0 {
                    // Fall back to standard with the same length
                    return self.get(length, Standard);
                }
                chunk - 1
            } else {
                // one pattern per table cell
                let additional_offset = match (length, variant) {
                    (Long, Variant0) => 1,
                    (Medium, Variant0) => 2,
                    (Short, Variant0) => 3,
                    (Long, Variant1) => 4,
                    (Medium, Variant1) => 5,
                    (Short, Variant1) => 6,
                    (_, Standard) => {
                        debug_assert!(false, "unreachable");
                        return PatternBorrowed::DEFAULT;
                    }
                };
                s_offset + additional_offset
            }
        };
        let Some(plural_elements) = self.elements.get(pattern_index as usize) else {
            debug_assert!(false, "unreachable");
            return PatternBorrowed::DEFAULT;
        };
        let (metadata, items) = plural_elements.get_default();
        PatternBorrowed {
            metadata: PatternMetadata::from_u8(metadata.get()),
            items,
        }
    }

    fn get_as_plural_elements(
        &self,
        length: Length,
        variant: PackedSkeletonVariant,
    ) -> PluralElements<Pattern> {
        PluralElements::new(self.get(length, variant).as_pattern())
    }

    /// Converts this packed data to a builder that can be mutated.
    pub fn to_builder(&self) -> PackedPatternsBuilder {
        use Length::*;
        use PackedSkeletonVariant::*;
        let mut builder = PackedPatternsBuilder {
            standard: LengthPluralElements {
                long: self.get_as_plural_elements(Long, Standard),
                medium: self.get_as_plural_elements(Medium, Standard),
                short: self.get_as_plural_elements(Short, Standard),
            },
            variant0: Some(LengthPluralElements {
                long: self.get_as_plural_elements(Long, Variant0),
                medium: self.get_as_plural_elements(Medium, Variant0),
                short: self.get_as_plural_elements(Short, Variant0),
            }),
            variant1: Some(LengthPluralElements {
                long: self.get_as_plural_elements(Long, Variant1),
                medium: self.get_as_plural_elements(Medium, Variant1),
                short: self.get_as_plural_elements(Short, Variant1),
            }),
        };
        builder.simplify();
        builder
    }
}

#[cfg(feature = "serde")]
mod _serde {
    use super::*;
    use crate::provider::pattern::reference;
    use zerovec::VarZeroSlice;

    #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
    #[cfg_attr(feature = "datagen", derive(serde::Serialize))]
    struct PackedPatternsMachine<'data> {
        pub header: u32,
        #[serde(borrow)]
        pub elements: &'data VarZeroSlice<PluralElementsPackedULE<ZeroSlice<PatternItem>>>,
    }

    #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
    #[cfg_attr(feature = "datagen", derive(serde::Serialize))]
    #[derive(Default)]
    struct PackedPatternsHuman {
        #[cfg_attr(
            feature = "serde",
            serde(default, skip_serializing_if = "core::ops::Not::not")
        )]
        pub(super) has_explicit_medium: bool,
        #[cfg_attr(
            feature = "serde",
            serde(default, skip_serializing_if = "core::ops::Not::not")
        )]
        pub(super) has_explicit_short: bool,
        #[cfg_attr(
            feature = "serde",
            serde(default, skip_serializing_if = "core::ops::Not::not")
        )]
        pub(super) has_one_pattern_per_variant: bool,
        #[cfg_attr(
            feature = "serde",
            serde(default, skip_serializing_if = "Option::is_none")
        )]
        pub(super) variant_pattern_indices: Option<[u32; 6]>,
        pub(super) elements: Vec<reference::Pattern>,
    }

    impl<'de, 'data> serde::Deserialize<'de> for PackedPatterns<'data>
    where
        'de: 'data,
    {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: serde::Deserializer<'de>,
        {
            use serde::de::Error as _;
            if deserializer.is_human_readable() {
                let human = <PackedPatternsHuman>::deserialize(deserializer)?;
                let variant_indices = match (
                    human.has_one_pattern_per_variant,
                    human.variant_pattern_indices,
                ) {
                    (true, None) => VariantIndices::OnePatternPerVariant,
                    (false, Some(chunks)) => VariantIndices::IndicesPerVariant(
                        VariantPatternIndex::try_from_chunks_u32(chunks).ok_or_else(|| {
                            D::Error::custom("variant pattern index out of range")
                        })?,
                    ),
                    _ => {
                        return Err(D::Error::custom(
                            "must have either one pattern per variant or indices",
                        ))
                    }
                };
                let elements = human
                    .elements
                    .iter()
                    .map(|pattern| PluralElements::new(pattern.to_runtime_pattern()))
                    .collect();
                let unpacked = UnpackedPatterns {
                    has_explicit_medium: human.has_explicit_medium,
                    has_explicit_short: human.has_explicit_short,
                    variant_indices,
                    elements,
                };
                Ok(unpacked.build())
            } else {
                let machine = <PackedPatternsMachine>::deserialize(deserializer)?;
                Ok(Self {
                    header: machine.header,
                    elements: machine.elements.as_varzerovec(),
                })
            }
        }
    }

    #[cfg(feature = "datagen")]
    impl serde::Serialize for PackedPatterns<'_> {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: serde::Serializer,
        {
            use serde::ser::Error as _;
            if serializer.is_human_readable() {
                let unpacked = UnpackedPatterns::from_packed(self);
                let mut human = PackedPatternsHuman {
                    has_explicit_medium: unpacked.has_explicit_medium,
                    has_explicit_short: unpacked.has_explicit_short,
                    ..Default::default()
                };
                match unpacked.variant_indices {
                    VariantIndices::OnePatternPerVariant => {
                        human.has_one_pattern_per_variant = true;
                    }
                    VariantIndices::IndicesPerVariant(chunks) => {
                        let chunks = VariantPatternIndex::to_chunks_u32(chunks);
                        human.variant_pattern_indices = Some(chunks);
                    }
                }
                human.elements = Vec::with_capacity(unpacked.elements.len());
                for pattern_elements in unpacked.elements.into_iter() {
                    let pattern = pattern_elements
                        .try_into_other()
                        .ok_or_else(|| S::Error::custom("cannot yet serialize plural patterns"))?;
                    human.elements.push(reference::Pattern::from(&pattern));
                }
                human.serialize(serializer)
            } else {
                let machine = PackedPatternsMachine {
                    header: self.header,
                    elements: &self.elements,
                };
                machine.serialize(serializer)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::provider::pattern::reference;

    const PATTERN_STRS: &[&str] = &[
        "M/d/y",
        "HH:mm",
        "MMM d y G",
        "E",
        "E MMM d",
        "dd.MM.yy",
        "h a",
        "hh:mm:ss B",
        "y MMMM",
    ];

    fn get_patterns() -> Vec<Pattern<'static>> {
        PATTERN_STRS
            .iter()
            .map(|s| {
                s.parse::<reference::Pattern>()
                    .unwrap()
                    .to_runtime_pattern()
            })
            .collect::<Vec<_>>()
    }

    #[test]
    fn test_basic() {
        let patterns = get_patterns();
        let mut it = patterns.iter().cloned();
        let lms0 = LengthPluralElements {
            long: PluralElements::new(it.next().unwrap()),
            medium: PluralElements::new(it.next().unwrap()),
            short: PluralElements::new(it.next().unwrap()),
        };
        let lms1 = LengthPluralElements {
            long: PluralElements::new(it.next().unwrap()),
            medium: PluralElements::new(it.next().unwrap()),
            short: PluralElements::new(it.next().unwrap()),
        };
        let lms2 = LengthPluralElements {
            long: PluralElements::new(it.next().unwrap()),
            medium: PluralElements::new(it.next().unwrap()),
            short: PluralElements::new(it.next().unwrap()),
        };
        let lms0a = LengthPluralElements {
            long: PluralElements::new(patterns[0].clone()),
            medium: PluralElements::new(patterns[0].clone()),
            short: PluralElements::new(patterns[1].clone()),
        };
        let lms1a = LengthPluralElements {
            long: PluralElements::new(patterns[3].clone()),
            medium: PluralElements::new(patterns[4].clone()),
            short: PluralElements::new(patterns[4].clone()),
        };

        {
            // Q = 1
            let builder = PackedPatternsBuilder {
                standard: lms0.clone(),
                variant0: Some(lms1.clone()),
                variant1: Some(lms2.clone()),
            };
            let packed = builder.clone().build();
            assert_eq!(packed.header, 7);
            assert_eq!(packed.elements.len(), 9);
            for (pattern_elements, expected) in packed.elements.iter().zip(patterns.iter()) {
                assert_eq!(pattern_elements.get_default().1, &expected.items);
            }
            let recovered_builder = packed.to_builder();
            assert_eq!(builder, recovered_builder);
        }
        {
            // Q = 0
            let builder = PackedPatternsBuilder {
                standard: lms0.clone(),
                variant0: Some(lms0.clone()),
                variant1: Some(lms2.clone()),
            };
            let packed = builder.clone().build();
            assert_eq!(packed.header, 0x1AC003);
            assert_eq!(packed.elements.len(), 6);
            let recovered_builder = packed.to_builder();
            assert_ne!(builder, recovered_builder);
            let mut builder = builder;
            builder.simplify();
            assert_eq!(builder, recovered_builder);
        }
        {
            // No variants
            let builder = PackedPatternsBuilder {
                standard: lms0.clone(),
                variant0: None,
                variant1: None,
            };
            let packed = builder.clone().build();
            assert_eq!(packed.header, 3);
            assert_eq!(packed.elements.len(), 3);
            let recovered_builder = packed.to_builder();
            assert_eq!(builder, recovered_builder);
        }
        {
            // Some duplicate patterns and inheritance
            let builder = PackedPatternsBuilder {
                standard: lms0a.clone(),
                variant0: Some(lms0.clone()),
                variant1: Some(lms1.clone()),
            };
            let packed = builder.clone().build();
            assert_eq!(packed.header, 0x1AC682);
            assert_eq!(packed.elements.len(), 6);
            let recovered_builder = packed.to_builder();
            assert_eq!(builder, recovered_builder);
        }
        {
            // Q = 1 with 8 patterns (min for Q = 1)
            let builder = PackedPatternsBuilder {
                standard: lms0a.clone(),
                variant0: Some(lms1.clone()),
                variant1: Some(lms2.clone()),
            };
            let packed = builder.clone().build();
            assert_eq!(packed.header, 6);
            assert_eq!(packed.elements.len(), 8);
            let recovered_builder = packed.to_builder();
            assert_eq!(builder, recovered_builder);
        }
        {
            // Q = 0 with 7 patterns (max for Q = 0)
            let builder = PackedPatternsBuilder {
                standard: lms1a.clone(),
                variant0: Some(lms0a.clone()),
                variant1: Some(lms2.clone()),
            };
            let packed = builder.clone().build();
            assert_eq!(packed.header, 0x1F58D9);
            assert_eq!(packed.elements.len(), 7);
            let recovered_builder = packed.to_builder();
            assert_eq!(builder, recovered_builder);
        }
    }

    #[cfg(feature = "datagen")]
    #[test]
    fn test_serde() {
        let patterns = get_patterns();
        let lms0a = LengthPluralElements {
            long: PluralElements::new(patterns[0].clone()),
            medium: PluralElements::new(patterns[0].clone()),
            short: PluralElements::new(patterns[1].clone()),
        };
        let lms1 = LengthPluralElements {
            long: PluralElements::new(patterns[3].clone()),
            medium: PluralElements::new(patterns[4].clone()),
            short: PluralElements::new(patterns[5].clone()),
        };

        let builder = PackedPatternsBuilder {
            standard: lms0a,
            variant0: Some(lms1),
            variant1: None,
        };
        let packed = builder.clone().build();

        let bincode_bytes = bincode::serialize(&packed).unwrap();
        assert_eq!(
            bincode_bytes.as_slice(),
            &[
                26, 11, 0, 0, 72, 0, 0, 0, 0, 0, 0, 0, 5, 0, 16, 0, 26, 0, 30, 0, 46, 0, 0, 128,
                32, 1, 0, 0, 47, 128, 64, 1, 0, 0, 47, 128, 16, 1, 2, 128, 114, 2, 0, 0, 58, 128,
                128, 2, 0, 128, 80, 1, 0, 128, 80, 1, 0, 0, 32, 128, 32, 3, 0, 0, 32, 128, 64, 1,
                0, 128, 64, 2, 0, 0, 46, 128, 32, 2, 0, 0, 46, 128, 16, 2
            ][..]
        );
        let bincode_recovered = bincode::deserialize::<PackedPatterns>(&bincode_bytes).unwrap();
        assert_eq!(builder, bincode_recovered.to_builder());

        let json_str = serde_json::to_string(&packed).unwrap();
        assert_eq!(json_str, "{\"has_explicit_short\":true,\"variant_pattern_indices\":[3,4,5,0,0,0],\"elements\":[\"M/d/y\",\"HH:mm\",\"E\",\"E MMM d\",\"dd.MM.yy\"]}");
        let json_recovered = serde_json::from_str::<PackedPatterns>(&json_str).unwrap();
        assert_eq!(builder, json_recovered.to_builder());
    }
}
