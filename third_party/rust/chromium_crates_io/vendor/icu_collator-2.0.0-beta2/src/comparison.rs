// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Various collation-related algorithms and constants in this file are
// adapted from ICU4C and, therefore, are subject to the ICU license as
// described in LICENSE.

//! This module holds the `Collator` struct whose `compare_impl()` contains
//! the comparison of collation element sequences.

use crate::elements::{
    CollationElement, CollationElements, NonPrimary, JAMO_COUNT, NO_CE, NO_CE_PRIMARY,
    NO_CE_SECONDARY, NO_CE_TERTIARY, OPTIMIZED_DIACRITICS_MAX_COUNT, QUATERNARY_MASK,
};
use crate::options::CollatorOptionsBitField;
use crate::options::{
    AlternateHandling, CollatorOptions, MaxVariable, ResolvedCollatorOptions, Strength,
};
use crate::preferences::{CollationCaseFirst, CollationNumericOrdering, CollationType};
use crate::provider::CollationData;
use crate::provider::CollationDiacritics;
use crate::provider::CollationDiacriticsV1;
use crate::provider::CollationJamo;
use crate::provider::CollationJamoV1;
use crate::provider::CollationMetadataV1;
use crate::provider::CollationReordering;
use crate::provider::CollationReorderingV1;
use crate::provider::CollationRootV1;
use crate::provider::CollationSpecialPrimaries;
use crate::provider::CollationSpecialPrimariesV1;
use crate::provider::CollationTailoringV1;
use core::cmp::Ordering;
use core::convert::TryFrom;
use icu_normalizer::provider::DecompositionData;
use icu_normalizer::provider::DecompositionTables;
use icu_normalizer::provider::NormalizerNfdDataV1;
use icu_normalizer::provider::NormalizerNfdTablesV1;
use icu_normalizer::Decomposition;
use icu_provider::prelude::*;
use smallvec::SmallVec;
use utf16_iter::Utf16CharsEx;
use utf8_iter::Utf8CharsEx;
use zerovec::ule::AsULE;

const MERGE_SEPARATOR_PRIMARY: u32 = 0x02000000; // for U+FFFE

struct AnyQuaternaryAccumulator(u32);

impl AnyQuaternaryAccumulator {
    #[inline(always)]
    pub fn new() -> Self {
        AnyQuaternaryAccumulator(0)
    }
    #[inline(always)]
    pub fn accumulate(&mut self, non_primary: NonPrimary) {
        self.0 |= non_primary.bits()
    }
    #[inline(always)]
    pub fn has_quaternary(&self) -> bool {
        self.0 & u32::from(QUATERNARY_MASK) != 0
    }
}

/// Holder struct for payloads that are locale-dependent. (For code
/// reuse between owned and borrowed cases.)
#[derive(Debug)]
struct LocaleSpecificDataHolder {
    tailoring: Option<DataPayload<CollationTailoringV1>>,
    diacritics: DataPayload<CollationDiacriticsV1>,
    reordering: Option<DataPayload<CollationReorderingV1>>,
    merged_options: CollatorOptionsBitField,
    lithuanian_dot_above: bool,
}

icu_locale_core::preferences::define_preferences!(
    /// The preferences for collation.
    ///
    /// # Preferences
    ///
    /// Examples for using the different preferences below can be found in the [crate-level docs](crate).
    ///
    /// ## Case First
    ///
    /// See the [spec](https://www.unicode.org/reports/tr35/tr35-collation.html#Case_Parameters).
    /// This is the BCP47 key `kf`. Three possibilities: [`CollationCaseFirst::False`] (default,
    /// except for Danish and Maltese), [`CollationCaseFirst::Lower`], and [`CollationCaseFirst::Upper`]
    /// (default for Danish and Maltese).
    ///
    /// ## Numeric
    ///
    /// This is the BCP47 key `kn`. When set to [`CollationNumericOrdering::True`], any sequence of decimal
    /// digits (General_Category = Nd) is sorted at the primary level according to the
    /// numeric value. The default is [`CollationNumericOrdering::False`].
    [Copy]
    CollatorPreferences,
    {
        /// The collation type. This corresponds to the `-u-co` BCP-47 tag.
        collation_type: CollationType,
        /// Treatment of case. (Large and small kana differences are treated as case differences.)
        /// This corresponds to the `-u-kf` BCP-47 tag.
        case_first: CollationCaseFirst,
        /// When set to `True`, any sequence of decimal digits is sorted at a primary level according
        /// to the numeric value.
        /// This corresponds to the `-u-kn` BPC-47 tag.
        numeric_ordering: CollationNumericOrdering
    }
);

impl LocaleSpecificDataHolder {
    /// The constructor code reused between owned and borrowed cases.
    fn try_new_unstable_internal<D>(
        provider: &D,
        prefs: CollatorPreferences,
        options: CollatorOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<CollationTailoringV1>
            + DataProvider<CollationDiacriticsV1>
            + DataProvider<CollationMetadataV1>
            + DataProvider<CollationReorderingV1>
            + ?Sized,
    {
        let marker_attributes = prefs
            .collation_type
            .as_ref()
            // all collation types are valid marker attributes
            .map(|c| DataMarkerAttributes::from_str_or_panic(c.as_str()))
            .unwrap_or_default();

        let data_locale = CollationTailoringV1::make_locale(prefs.locale_preferences);
        let req = DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                marker_attributes,
                &data_locale,
            ),
            metadata: {
                let mut metadata = DataRequestMetadata::default();
                metadata.silent = true;
                metadata
            },
        };

        let fallback_req = DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                Default::default(),
                &data_locale,
            ),
            ..Default::default()
        };

        let metadata_payload: DataPayload<crate::provider::CollationMetadataV1> = provider
            .load(req)
            .or_else(|_| provider.load(fallback_req))?
            .payload;

        let metadata = metadata_payload.get();

        let tailoring: Option<DataPayload<crate::provider::CollationTailoringV1>> =
            if metadata.tailored() {
                Some(
                    provider
                        .load(req)
                        .or_else(|_| provider.load(fallback_req))?
                        .payload,
                )
            } else {
                None
            };

        let reordering: Option<DataPayload<crate::provider::CollationReorderingV1>> =
            if metadata.reordering() {
                Some(
                    provider
                        .load(req)
                        .or_else(|_| provider.load(fallback_req))?
                        .payload,
                )
            } else {
                None
            };

        if let Some(reordering) = &reordering {
            if reordering.get().reorder_table.len() != 256 {
                return Err(DataError::custom("invalid").with_marker(CollationReorderingV1::INFO));
            }
        }

        let tailored_diacritics = metadata.tailored_diacritics();
        let diacritics: DataPayload<CollationDiacriticsV1> = provider
            .load(if tailored_diacritics {
                req
            } else {
                Default::default()
            })?
            .payload;

        if tailored_diacritics {
            // In the tailored case we accept a shorter table in which case the tailoring is
            // responsible for supplying the missing values in the trie.
            // As of June 2022, none of the collations actually use a shortened table.
            // Vietnamese and Ewe load a full-length alternative table and the rest use
            // the default one.
            if diacritics.get().secondaries.len() > OPTIMIZED_DIACRITICS_MAX_COUNT {
                return Err(DataError::custom("invalid").with_marker(CollationDiacriticsV1::INFO));
            }
        } else if diacritics.get().secondaries.len() != OPTIMIZED_DIACRITICS_MAX_COUNT {
            return Err(DataError::custom("invalid").with_marker(CollationDiacriticsV1::INFO));
        }

        let mut altered_defaults = CollatorOptionsBitField::default();

        if metadata.alternate_shifted() {
            altered_defaults.set_alternate_handling(Some(AlternateHandling::Shifted));
        }
        if metadata.backward_second_level() {
            altered_defaults.set_backward_second_level(Some(true));
        }

        altered_defaults.set_case_first(Some(metadata.case_first()));
        altered_defaults.set_max_variable(Some(metadata.max_variable()));

        let mut merged_options = CollatorOptionsBitField::from(options);
        merged_options.set_case_first(prefs.case_first);
        merged_options.set_numeric_from_enum(prefs.numeric_ordering);
        merged_options.set_defaults(altered_defaults);

        Ok(LocaleSpecificDataHolder {
            tailoring,
            diacritics,
            merged_options,
            reordering,
            lithuanian_dot_above: metadata.lithuanian_dot_above(),
        })
    }
}

/// Compares strings according to culturally-relevant ordering.
#[derive(Debug)]
pub struct Collator {
    special_primaries: Option<DataPayload<CollationSpecialPrimariesV1>>,
    root: DataPayload<CollationRootV1>,
    tailoring: Option<DataPayload<CollationTailoringV1>>,
    jamo: DataPayload<CollationJamoV1>,
    diacritics: DataPayload<CollationDiacriticsV1>,
    options: CollatorOptionsBitField,
    reordering: Option<DataPayload<CollationReorderingV1>>,
    decompositions: DataPayload<NormalizerNfdDataV1>,
    tables: DataPayload<NormalizerNfdTablesV1>,
    lithuanian_dot_above: bool,
}

impl Collator {
    /// Constructs a borrowed version of this type for more efficient querying.
    pub fn as_borrowed(&self) -> CollatorBorrowed {
        CollatorBorrowed {
            special_primaries: self.special_primaries.as_ref().map(|s| s.get()),
            root: self.root.get(),
            tailoring: self.tailoring.as_ref().map(|s| s.get()),
            jamo: self.jamo.get(),
            diacritics: self.diacritics.get(),
            options: self.options,
            reordering: self.reordering.as_ref().map(|s| s.get()),
            decompositions: self.decompositions.get(),
            tables: self.tables.get(),
            lithuanian_dot_above: self.lithuanian_dot_above,
        }
    }

    /// Creates `CollatorBorrowed` for the given locale and options from compiled data.
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: CollatorPreferences,
        options: CollatorOptions,
    ) -> Result<CollatorBorrowed<'static>, DataError> {
        CollatorBorrowed::try_new(prefs, options)
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: CollatorPreferences, options: CollatorOptions) -> error: DataError,
        functions: [
            try_new: skip,
                        try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D>(
        provider: &D,
        prefs: CollatorPreferences,
        options: CollatorOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<CollationSpecialPrimariesV1>
            + DataProvider<CollationRootV1>
            + DataProvider<CollationTailoringV1>
            + DataProvider<CollationDiacriticsV1>
            + DataProvider<CollationJamoV1>
            + DataProvider<CollationMetadataV1>
            + DataProvider<CollationReorderingV1>
            + DataProvider<NormalizerNfdDataV1>
            + DataProvider<NormalizerNfdTablesV1>
            + ?Sized,
    {
        Self::try_new_unstable_internal(
            provider,
            provider.load(Default::default())?.payload,
            provider.load(Default::default())?.payload,
            provider.load(Default::default())?.payload,
            provider.load(Default::default())?.payload,
            || provider.load(Default::default()).map(|r| r.payload),
            prefs,
            options,
        )
    }

    #[allow(clippy::too_many_arguments)]
    fn try_new_unstable_internal<D>(
        provider: &D,
        root: DataPayload<CollationRootV1>,
        decompositions: DataPayload<NormalizerNfdDataV1>,
        tables: DataPayload<NormalizerNfdTablesV1>,
        jamo: DataPayload<CollationJamoV1>,
        special_primaries: impl FnOnce() -> Result<DataPayload<CollationSpecialPrimariesV1>, DataError>,
        prefs: CollatorPreferences,
        options: CollatorOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<CollationRootV1>
            + DataProvider<CollationTailoringV1>
            + DataProvider<CollationDiacriticsV1>
            + DataProvider<CollationMetadataV1>
            + DataProvider<CollationReorderingV1>
            + ?Sized,
    {
        let locale_dependent =
            LocaleSpecificDataHolder::try_new_unstable_internal(provider, prefs, options)?;

        // TODO: redesign Korean search collation handling
        if jamo.get().ce32s.len() != JAMO_COUNT {
            return Err(DataError::custom("invalid").with_marker(CollationJamoV1::INFO));
        }

        let special_primaries = if locale_dependent.merged_options.alternate_handling()
            == AlternateHandling::Shifted
            || locale_dependent.merged_options.numeric()
        {
            let special_primaries = special_primaries()?;
            // `variant_count` isn't stable yet:
            // https://github.com/rust-lang/rust/issues/73662
            if special_primaries.get().last_primaries.len() <= (MaxVariable::Currency as usize) {
                return Err(
                    DataError::custom("invalid").with_marker(CollationSpecialPrimariesV1::INFO)
                );
            }
            Some(special_primaries)
        } else {
            None
        };

        Ok(Collator {
            special_primaries,
            root,
            tailoring: locale_dependent.tailoring,
            jamo,
            diacritics: locale_dependent.diacritics,
            options: locale_dependent.merged_options,
            reordering: locale_dependent.reordering,
            decompositions,
            tables,
            lithuanian_dot_above: locale_dependent.lithuanian_dot_above,
        })
    }
}

/// Compares strings according to culturally-relevant ordering,
/// borrowed version.
#[derive(Debug)]
pub struct CollatorBorrowed<'a> {
    special_primaries: Option<&'a CollationSpecialPrimaries<'a>>,
    root: &'a CollationData<'a>,
    tailoring: Option<&'a CollationData<'a>>,
    jamo: &'a CollationJamo<'a>,
    diacritics: &'a CollationDiacritics<'a>,
    options: CollatorOptionsBitField,
    reordering: Option<&'a CollationReordering<'a>>,
    decompositions: &'a DecompositionData<'a>,
    tables: &'a DecompositionTables<'a>,
    lithuanian_dot_above: bool,
}

impl CollatorBorrowed<'static> {
    /// Creates a collator for the given locale and options from compiled data.
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: CollatorPreferences,
        options: CollatorOptions,
    ) -> Result<Self, DataError> {
        // These are assigned to locals in order to keep the code after these assignments
        // copypaste-compatible with `Collator::try_new_unstable_internal`.

        let provider = &crate::provider::Baked;
        let decompositions = icu_normalizer::provider::Baked::SINGLETON_NORMALIZER_NFD_DATA_V1;
        let tables = icu_normalizer::provider::Baked::SINGLETON_NORMALIZER_NFD_TABLES_V1;
        let root = crate::provider::Baked::SINGLETON_COLLATION_ROOT_V1;
        let jamo = crate::provider::Baked::SINGLETON_COLLATION_JAMO_V1;

        let locale_dependent =
            LocaleSpecificDataHolder::try_new_unstable_internal(provider, prefs, options)?;

        // TODO: redesign Korean search collation handling
        if jamo.ce32s.len() != JAMO_COUNT {
            return Err(DataError::custom("invalid").with_marker(CollationJamoV1::INFO));
        }

        let special_primaries = if locale_dependent.merged_options.alternate_handling()
            == AlternateHandling::Shifted
            || locale_dependent.merged_options.numeric()
        {
            let special_primaries =
                crate::provider::Baked::SINGLETON_COLLATION_SPECIAL_PRIMARIES_V1;
            // `variant_count` isn't stable yet:
            // https://github.com/rust-lang/rust/issues/73662
            if special_primaries.last_primaries.len() <= (MaxVariable::Currency as usize) {
                return Err(
                    DataError::custom("invalid").with_marker(CollationSpecialPrimariesV1::INFO)
                );
            }
            Some(special_primaries)
        } else {
            None
        };

        // Attribute belongs closer to `unwrap`, but
        // https://github.com/rust-lang/rust/issues/15701
        #[allow(clippy::unwrap_used)]
        Ok(CollatorBorrowed {
            special_primaries,
            root,
            // Unwrap is OK, because we know we have the baked provider.
            tailoring: locale_dependent.tailoring.map(|s| s.get_static().unwrap()),
            jamo,
            // Unwrap is OK, because we know we have the baked provider.
            diacritics: locale_dependent.diacritics.get_static().unwrap(),
            options: locale_dependent.merged_options,
            // Unwrap is OK, because we know we have the baked provider.
            reordering: locale_dependent.reordering.map(|s| s.get_static().unwrap()),
            decompositions,
            tables,
            lithuanian_dot_above: locale_dependent.lithuanian_dot_above,
        })
    }

    /// Cheaply converts a [`CollatorBorrowed<'static>`] into a [`Collator`].
    ///
    /// Note: Due to branching and indirection, using [`Collator`] might inhibit some
    /// compile-time optimizations that are possible with [`CollatorBorrowed`].
    pub const fn static_to_owned(self) -> Collator {
        Collator {
            special_primaries: if let Some(s) = self.special_primaries {
                // `map` not available in const context
                Some(DataPayload::from_static_ref(s))
            } else {
                None
            },
            root: DataPayload::from_static_ref(self.root),
            tailoring: if let Some(s) = self.tailoring {
                // `map` not available in const context
                Some(DataPayload::from_static_ref(s))
            } else {
                None
            },
            jamo: DataPayload::from_static_ref(self.jamo),
            diacritics: DataPayload::from_static_ref(self.diacritics),
            options: self.options,
            reordering: if let Some(s) = self.reordering {
                // `map` not available in const context
                Some(DataPayload::from_static_ref(s))
            } else {
                None
            },
            decompositions: DataPayload::from_static_ref(self.decompositions),
            tables: DataPayload::from_static_ref(self.tables),
            lithuanian_dot_above: self.lithuanian_dot_above,
        }
    }
}

impl CollatorBorrowed<'_> {
    /// The resolved options showing how the default options, the requested options,
    /// and the options from locale data were combined.
    pub fn resolved_options(&self) -> ResolvedCollatorOptions {
        self.options.into()
    }

    /// Compare potentially ill-formed UTF-16 slices. Unpaired surrogates
    /// are compared as if each one was a REPLACEMENT CHARACTER.
    pub fn compare_utf16(&self, left: &[u16], right: &[u16]) -> Ordering {
        // TODO(#2010): Identical prefix skipping not implemented.
        let ret = self.compare_impl(left.chars(), right.chars());
        if self.options.strength() == Strength::Identical && ret == Ordering::Equal {
            return Decomposition::new(left.chars(), self.decompositions, self.tables).cmp(
                Decomposition::new(right.chars(), self.decompositions, self.tables),
            );
        }
        ret
    }

    /// Compare guaranteed well-formed UTF-8 slices.
    pub fn compare(&self, left: &str, right: &str) -> Ordering {
        // TODO(#2010): Identical prefix skipping not implemented.
        let ret = self.compare_impl(left.chars(), right.chars());
        if self.options.strength() == Strength::Identical && ret == Ordering::Equal {
            return Decomposition::new(left.chars(), self.decompositions, self.tables).cmp(
                Decomposition::new(right.chars(), self.decompositions, self.tables),
            );
        }
        ret
    }

    /// Compare potentially well-formed UTF-8 slices. Ill-formed input is compared
    /// as if errors had been replaced with REPLACEMENT CHARACTERs according
    /// to the WHATWG Encoding Standard.
    pub fn compare_utf8(&self, left: &[u8], right: &[u8]) -> Ordering {
        // TODO(#2010): Identical prefix skipping not implemented.
        let ret = self.compare_impl(left.chars(), right.chars());
        if self.options.strength() == Strength::Identical && ret == Ordering::Equal {
            return Decomposition::new(left.chars(), self.decompositions, self.tables).cmp(
                Decomposition::new(right.chars(), self.decompositions, self.tables),
            );
        }
        ret
    }

    fn compare_impl<I: Iterator<Item = char>>(&self, left_chars: I, right_chars: I) -> Ordering {
        let tailoring: &CollationData = if let Some(tailoring) = &self.tailoring {
            tailoring
        } else {
            // If the root collation is valid for the locale,
            // use the root as the tailoring so that reads from the
            // tailoring always succeed.
            //
            // TODO(#2011): Do we instead want to have an untailored
            // copypaste of the iterator that omits the tailoring
            // branches for performance at the expense of code size
            // and having to maintain both a tailoring-capable and
            // a tailoring-incapable version of the iterator?
            // Or, in order not to flip the branch prediction around,
            // should we have a no-op tailoring that contains a
            // specially-crafted CodePointTrie that always returns
            // a FALLBACK_CE32 after a single branch?
            self.root
        };

        // Sadly, it looks like variable CEs and backward second level
        // require us to store the full 64-bit CEs instead of storing only
        // the NonPrimary part.
        //
        // TODO(#2008): Consider having two monomorphizations of this method:
        // one that can deal with variables shifted to quaternary and
        // backward second level and another that doesn't support that
        // and only stores `NonPrimary` in `left_ces` and `right_ces`
        // with double the number of stack allocated elements.

        // TODO(#2007): figure out a proper stack buffer length for these
        let mut left_ces: SmallVec<[CollationElement; 8]> = SmallVec::new();
        let mut right_ces: SmallVec<[CollationElement; 8]> = SmallVec::new();

        // The algorithm comes from CollationCompare::compareUpToQuaternary in ICU4C.

        let mut any_variable = false;
        // Attribute belongs closer to `unwrap`, but
        // https://github.com/rust-lang/rust/issues/15701
        #[allow(clippy::unwrap_used)]
        let variable_top = if self.options.alternate_handling() == AlternateHandling::NonIgnorable {
            0
        } else {
            // +1 so that we can use "<" and primary ignorables test out early.
            self.special_primaries
                .as_ref()
                // `unwrap()` is OK, because we've ensured in the constructor that value
                // is `Some` if we have alternate handling.
                .unwrap()
                .last_primary_for_group(self.options.max_variable())
                + 1
        };

        // Attribute belongs on inner expression, but
        // https://github.com/rust-lang/rust/issues/15701
        #[allow(clippy::unwrap_used)]
        let numeric_primary = if self.options.numeric() {
            Some(
                self.special_primaries
                    .as_ref()
                    // `unwrap` is OK, because we've ensured `Some` in the constructor
                    .unwrap()
                    .numeric_primary,
            )
        } else {
            None
        };

        // Attribute belongs on inner expression, but
        // https://github.com/rust-lang/rust/issues/15701
        #[allow(clippy::unwrap_used)]
        let mut left = CollationElements::new(
            left_chars,
            self.root,
            tailoring,
            <&[<u32 as AsULE>::ULE; JAMO_COUNT]>::try_from(self.jamo.ce32s.as_ule_slice()).unwrap(), // `unwrap` OK, because length already validated
            &self.diacritics.secondaries,
            self.decompositions,
            self.tables,
            numeric_primary,
            self.lithuanian_dot_above,
        );
        // Attribute belongs on inner expression, but
        // https://github.com/rust-lang/rust/issues/15701
        #[allow(clippy::unwrap_used)]
        let mut right = CollationElements::new(
            right_chars,
            self.root,
            tailoring,
            <&[<u32 as AsULE>::ULE; JAMO_COUNT]>::try_from(self.jamo.ce32s.as_ule_slice()).unwrap(), // `unwrap` OK, because length already validated
            &self.diacritics.secondaries,
            self.decompositions,
            self.tables,
            numeric_primary,
            self.lithuanian_dot_above,
        );
        loop {
            let mut left_primary;
            'left_primary_loop: loop {
                let ce = left.next();
                left_primary = ce.primary();
                // TODO(#2008): Consider compiling out the variable handling when we know we aren't
                // shifting variable CEs.
                if !(left_primary < variable_top && left_primary > MERGE_SEPARATOR_PRIMARY) {
                    left_ces.push(ce);
                } else {
                    // Variable CE, shift it to quaternary level.
                    // Ignore all following primary ignorables, and shift further variable CEs.
                    any_variable = true;
                    // Relative to ICU4C, the next line is hoisted out of the following loop
                    // in order to keep the variables called `ce` immutable to make it easier
                    // to reason about each assignment into `ce` resulting in exactly a single
                    // push into `left_ces`.
                    left_ces.push(ce.clone_with_non_primary_zeroed());
                    loop {
                        // This loop is simpler than in ICU4C; unlike in C++, we get to break by label.
                        let ce = left.next();
                        left_primary = ce.primary();
                        if left_primary != 0
                            && !(left_primary < variable_top
                                && left_primary > MERGE_SEPARATOR_PRIMARY)
                        {
                            // Neither a primary ignorable nor a variable CE.
                            left_ces.push(ce);
                            break 'left_primary_loop;
                        }
                        // If `left_primary == 0`, the following line ignores a primary-ignorable.
                        // Otherwise, it shifts a variable CE.
                        left_ces.push(ce.clone_with_non_primary_zeroed());
                    }
                }
                if left_primary != 0 {
                    break;
                }
            }
            let mut right_primary;
            'right_primary_loop: loop {
                let ce = right.next();
                right_primary = ce.primary();
                // TODO(#2008): Consider compiling out the variable handling when we know we aren't
                // shifting variable CEs.
                if !(right_primary < variable_top && right_primary > MERGE_SEPARATOR_PRIMARY) {
                    right_ces.push(ce);
                } else {
                    // Variable CE, shift it to quaternary level.
                    // Ignore all following primary ignorables, and shift further variable CEs.
                    any_variable = true;
                    // Relative to ICU4C, the next line is hoisted out of the following loop
                    // in order to keep the variables called `ce` immutable to make it easier
                    // to reason about each assignment into `ce` resulting in exactly a single
                    // push into `right_ces`.
                    right_ces.push(ce.clone_with_non_primary_zeroed());
                    loop {
                        // This loop is simpler than in ICU4C; unlike in C++, we get to break by label.
                        let ce = right.next();
                        right_primary = ce.primary();
                        if right_primary != 0
                            && !(right_primary < variable_top
                                && right_primary > MERGE_SEPARATOR_PRIMARY)
                        {
                            // Neither a primary ignorable nor a variable CE.
                            right_ces.push(ce);
                            break 'right_primary_loop;
                        }
                        // If `right_primary == 0`, the following line ignores a primary-ignorable.
                        // Otherwise, it shifts a variable CE.
                        right_ces.push(ce.clone_with_non_primary_zeroed());
                    }
                }
                if right_primary != 0 {
                    break;
                }
            }
            if left_primary != right_primary {
                if let Some(reordering) = &self.reordering {
                    left_primary = reordering.reorder(left_primary);
                    right_primary = reordering.reorder(right_primary);
                }
                if left_primary < right_primary {
                    return Ordering::Less;
                }
                return Ordering::Greater;
            }
            if left_primary == NO_CE_PRIMARY {
                break;
            }
        }

        // Sadly, we end up pushing the sentinel value, which means these
        // `SmallVec`s allocate more often than if we didn't actually
        // store the sentinel.
        debug_assert_eq!(left_ces.last(), Some(&NO_CE));
        debug_assert_eq!(right_ces.last(), Some(&NO_CE));

        // Note: `unwrap_or_default` in the iterations below should never
        // actually end up using the "_or_default" part, because the sentinel
        // is in the `SmallVec`s. These could be changed to `unwrap()` if we
        // preferred panic in case of a bug.
        // TODO(#2009): Should we save one slot by not putting the sentinel in
        // the `SmallVec`s? So far, the answer seems "no", as it would complicate
        // the primary comparison above.

        // Compare the buffered secondary & tertiary weights.
        // We might skip the secondary level but continue with the case level
        // which is turned on separately.
        if self.options.strength() >= Strength::Secondary {
            if !self.options.backward_second_level() {
                let mut left_iter = left_ces.iter();
                let mut right_iter = right_ces.iter();
                let mut left_secondary;
                let mut right_secondary;
                loop {
                    loop {
                        left_secondary = left_iter.next().unwrap_or_default().secondary();
                        if left_secondary != 0 {
                            break;
                        }
                    }
                    loop {
                        right_secondary = right_iter.next().unwrap_or_default().secondary();
                        if right_secondary != 0 {
                            break;
                        }
                    }
                    if left_secondary != right_secondary {
                        if left_secondary < right_secondary {
                            return Ordering::Less;
                        }
                        return Ordering::Greater;
                    }
                    if left_secondary == NO_CE_SECONDARY {
                        break;
                    }
                }
            } else {
                let mut left_remaining = &left_ces[..];
                let mut right_remaining = &right_ces[..];
                loop {
                    if left_remaining.is_empty() {
                        debug_assert!(right_remaining.is_empty());
                        break;
                    }
                    let (left_prefix, right_prefix) = {
                        let mut left_iter = left_remaining.iter();
                        loop {
                            let left_primary = left_iter.next().unwrap_or_default().primary();
                            if left_primary != 0 && left_primary <= MERGE_SEPARATOR_PRIMARY {
                                break;
                            }
                            debug_assert_ne!(left_primary, NO_CE_PRIMARY);
                        }
                        let left_new_remaining = left_iter.as_slice();
                        // Index in range by construction
                        #[allow(clippy::indexing_slicing)]
                        let left_prefix =
                            &left_remaining[..left_remaining.len() - 1 - left_new_remaining.len()];
                        left_remaining = left_new_remaining;

                        let mut right_iter = right_remaining.iter();
                        loop {
                            let right_primary = right_iter.next().unwrap_or_default().primary();
                            if right_primary != 0 && right_primary <= MERGE_SEPARATOR_PRIMARY {
                                break;
                            }
                            debug_assert_ne!(right_primary, NO_CE_PRIMARY);
                        }
                        let right_new_remaining = right_iter.as_slice();
                        // Index in range by construction
                        #[allow(clippy::indexing_slicing)]
                        let right_prefix = &right_remaining
                            [..right_remaining.len() - 1 - right_new_remaining.len()];
                        right_remaining = right_new_remaining;

                        (left_prefix, right_prefix)
                    };
                    let mut left_iter = left_prefix.iter();
                    let mut right_iter = right_prefix.iter();

                    let mut left_secondary;
                    let mut right_secondary;
                    loop {
                        loop {
                            left_secondary = left_iter.next_back().unwrap_or_default().secondary();
                            if left_secondary != 0 {
                                break;
                            }
                        }
                        loop {
                            right_secondary =
                                right_iter.next_back().unwrap_or_default().secondary();
                            if right_secondary != 0 {
                                break;
                            }
                        }
                        if left_secondary != right_secondary {
                            if left_secondary < right_secondary {
                                return Ordering::Less;
                            }
                            return Ordering::Greater;
                        }
                        if left_secondary == NO_CE_SECONDARY {
                            break;
                        }
                    }
                }
            }
        }

        if self.options.case_level() {
            if self.options.strength() == Strength::Primary {
                // Primary+caseLevel: Ignore case level weights of primary ignorables.
                // Otherwise we would get a-umlaut > a
                // which is not desirable for accent-insensitive sorting.
                // Check for (lower 32 bits) == 0 as well because variable CEs are stored
                // with only primary weights.
                let mut left_non_primary;
                let mut right_non_primary;
                let mut left_case;
                let mut right_case;
                let mut left_iter = left_ces.iter();
                let mut right_iter = right_ces.iter();
                loop {
                    loop {
                        let ce = left_iter.next().unwrap_or_default();
                        left_non_primary = ce.non_primary();
                        if !ce.either_half_zero() {
                            break;
                        }
                    }
                    left_case = left_non_primary.case();
                    loop {
                        let ce = right_iter.next().unwrap_or_default();
                        right_non_primary = ce.non_primary();
                        if !ce.either_half_zero() {
                            break;
                        }
                    }
                    right_case = right_non_primary.case();
                    // No need to handle NO_CE and MERGE_SEPARATOR specially:
                    // There is one case weight for each previous-level weight,
                    // so level length differences were handled there.
                    if left_case != right_case {
                        if !self.options.upper_first() {
                            if left_case < right_case {
                                return Ordering::Less;
                            }
                            return Ordering::Greater;
                        }
                        if left_case < right_case {
                            return Ordering::Greater;
                        }
                        return Ordering::Less;
                    }
                    if left_non_primary.secondary() == NO_CE_SECONDARY {
                        break;
                    }
                }
            } else {
                // Secondary+caseLevel: By analogy with the above,
                // ignore case level weights of secondary ignorables.
                //
                // Note: A tertiary CE has uppercase case bits (0.0.ut)
                // to keep tertiary+caseFirst well-formed.
                //
                // Tertiary+caseLevel: Also ignore case level weights of secondary ignorables.
                // Otherwise a tertiary CE's uppercase would be no greater than
                // a primary/secondary CE's uppercase.
                // (See UCA well-formedness condition 2.)
                // We could construct a special case weight higher than uppercase,
                // but it's simpler to always ignore case weights of secondary ignorables,
                // turning 0.0.ut into 0.0.0.t.
                // (See LDML Collation, Case Parameters.)
                let mut left_non_primary;
                let mut right_non_primary;
                let mut left_case;
                let mut right_case;
                let mut left_iter = left_ces.iter();
                let mut right_iter = right_ces.iter();
                loop {
                    loop {
                        left_non_primary = left_iter.next().unwrap_or_default().non_primary();
                        if left_non_primary.secondary() != 0 {
                            break;
                        }
                    }
                    left_case = left_non_primary.case();
                    loop {
                        right_non_primary = right_iter.next().unwrap_or_default().non_primary();
                        if right_non_primary.secondary() != 0 {
                            break;
                        }
                    }
                    right_case = right_non_primary.case();
                    // No need to handle NO_CE and MERGE_SEPARATOR specially:
                    // There is one case weight for each previous-level weight,
                    // so level length differences were handled there.
                    if left_case != right_case {
                        if !self.options.upper_first() {
                            if left_case < right_case {
                                return Ordering::Less;
                            }
                            return Ordering::Greater;
                        }
                        if left_case < right_case {
                            return Ordering::Greater;
                        }
                        return Ordering::Less;
                    }
                    if left_non_primary.secondary() == NO_CE_SECONDARY {
                        break;
                    }
                }
            }
        }

        if let Some(tertiary_mask) = self.options.tertiary_mask() {
            let mut any_quaternaries = AnyQuaternaryAccumulator::new();
            let mut left_iter = left_ces.iter();
            let mut right_iter = right_ces.iter();
            loop {
                let mut left_non_primary;
                let mut left_tertiary;
                loop {
                    left_non_primary = left_iter.next().unwrap_or_default().non_primary();
                    any_quaternaries.accumulate(left_non_primary);
                    debug_assert!(
                        left_non_primary.tertiary() != 0 || left_non_primary.case_quaternary() == 0
                    );
                    left_tertiary = left_non_primary.tertiary_case_quarternary(tertiary_mask);
                    if left_tertiary != 0 {
                        break;
                    }
                }

                let mut right_non_primary;
                let mut right_tertiary;
                loop {
                    right_non_primary = right_iter.next().unwrap_or_default().non_primary();
                    any_quaternaries.accumulate(right_non_primary);
                    debug_assert!(
                        right_non_primary.tertiary() != 0
                            || right_non_primary.case_quaternary() == 0
                    );
                    right_tertiary = right_non_primary.tertiary_case_quarternary(tertiary_mask);
                    if right_tertiary != 0 {
                        break;
                    }
                }

                if left_tertiary != right_tertiary {
                    if self.options.upper_first() {
                        // Pass through NO_CE and keep real tertiary weights larger than that.
                        // Do not change the artificial uppercase weight of a tertiary CE (0.0.ut),
                        // to keep tertiary CEs well-formed.
                        // Their case+tertiary weights must be greater than those of
                        // primary and secondary CEs.
                        // Magic numbers from ICU4C.
                        if left_tertiary > NO_CE_TERTIARY {
                            if left_non_primary.secondary() != 0 {
                                left_tertiary ^= 0xC000;
                            } else {
                                left_tertiary += 0x4000;
                            }
                        }
                        if right_tertiary > NO_CE_TERTIARY {
                            if right_non_primary.secondary() != 0 {
                                right_tertiary ^= 0xC000;
                            } else {
                                right_tertiary += 0x4000;
                            }
                        }
                    }
                    if left_tertiary < right_tertiary {
                        return Ordering::Less;
                    }
                    return Ordering::Greater;
                }

                if left_tertiary == NO_CE_TERTIARY {
                    break;
                }
            }
            if !any_variable && !any_quaternaries.has_quaternary() {
                return Ordering::Equal;
            }
        } else {
            return Ordering::Equal;
        }

        if self.options.strength() <= Strength::Tertiary {
            return Ordering::Equal;
        }

        let mut left_iter = left_ces.iter();
        let mut right_iter = right_ces.iter();
        loop {
            let mut left_quaternary;
            loop {
                let ce = left_iter.next().unwrap_or_default();
                if ce.tertiary_ignorable() {
                    left_quaternary = ce.primary();
                } else {
                    left_quaternary = ce.quaternary();
                }
                if left_quaternary != 0 {
                    break;
                }
            }
            let mut right_quaternary;
            loop {
                let ce = right_iter.next().unwrap_or_default();
                if ce.tertiary_ignorable() {
                    right_quaternary = ce.primary();
                } else {
                    right_quaternary = ce.quaternary();
                }
                if right_quaternary != 0 {
                    break;
                }
            }
            if left_quaternary != right_quaternary {
                if let Some(reordering) = &self.reordering {
                    left_quaternary = reordering.reorder(left_quaternary);
                    right_quaternary = reordering.reorder(right_quaternary);
                }
                if left_quaternary < right_quaternary {
                    return Ordering::Less;
                }
                return Ordering::Greater;
            }
            if left_quaternary == NO_CE_PRIMARY {
                break;
            }
        }

        Ordering::Equal
    }
}
