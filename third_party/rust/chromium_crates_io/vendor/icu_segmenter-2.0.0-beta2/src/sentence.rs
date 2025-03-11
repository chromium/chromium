// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::vec::Vec;
use icu_locale_core::LanguageIdentifier;
use icu_provider::prelude::*;

use crate::indices::{Latin1Indices, Utf16Indices};
use crate::iterator_helpers::derive_usize_iterator_with_type;
use crate::provider::*;
use crate::rule_segmenter::*;
use utf8_iter::Utf8CharIndices;

/// Options to tailor sentence breaking behavior.
#[non_exhaustive]
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub struct SentenceBreakOptions<'a> {
    /// Content locale for sentence segmenter.
    pub content_locale: Option<&'a LanguageIdentifier>,
    /// Options independent of the locale
    pub invariant_options: SentenceBreakInvariantOptions,
}

/// Locale-independent options to tailor sentence breaking behavior
///
/// Currently empty but may grow in the future
#[non_exhaustive]
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub struct SentenceBreakInvariantOptions {}

/// Implements the [`Iterator`] trait over the sentence boundaries of the given string.
///
/// Lifetimes:
///
/// - `'l` = lifetime of the segmenter object from which this iterator was created
/// - `'s` = lifetime of the string being segmented
///
/// The [`Iterator::Item`] is an [`usize`] representing index of a code unit
/// _after_ the boundary (for a boundary at the end of text, this index is the length
/// of the [`str`] or array of code units).
///
/// For examples of use, see [`SentenceSegmenter`].
#[derive(Debug)]
pub struct SentenceBreakIterator<'l, 's, Y: RuleBreakType<'l, 's> + ?Sized>(
    RuleBreakIterator<'l, 's, Y>,
);

derive_usize_iterator_with_type!(SentenceBreakIterator);

/// Sentence break iterator for an `str` (a UTF-8 string).
///
/// For examples of use, see [`SentenceSegmenter`].
pub type SentenceBreakIteratorUtf8<'l, 's> = SentenceBreakIterator<'l, 's, RuleBreakTypeUtf8>;

/// Sentence break iterator for a potentially invalid UTF-8 string.
///
/// For examples of use, see [`SentenceSegmenter`].
pub type SentenceBreakIteratorPotentiallyIllFormedUtf8<'l, 's> =
    SentenceBreakIterator<'l, 's, RuleBreakTypePotentiallyIllFormedUtf8>;

/// Sentence break iterator for a Latin-1 (8-bit) string.
///
/// For examples of use, see [`SentenceSegmenter`].
pub type SentenceBreakIteratorLatin1<'l, 's> = SentenceBreakIterator<'l, 's, RuleBreakTypeLatin1>;

/// Sentence break iterator for a UTF-16 string.
///
/// For examples of use, see [`SentenceSegmenter`].
pub type SentenceBreakIteratorUtf16<'l, 's> = SentenceBreakIterator<'l, 's, RuleBreakTypeUtf16>;

/// Supports loading sentence break data, and creating sentence break iterators for different string
/// encodings.
///
/// # Examples
///
/// Segment a string:
///
/// ```rust
/// use icu::segmenter::{
///     options::SentenceBreakInvariantOptions, SentenceSegmenter,
/// };
/// let segmenter =
///     SentenceSegmenter::new(SentenceBreakInvariantOptions::default());
///
/// let breakpoints: Vec<usize> =
///     segmenter.segment_str("Hello World").collect();
/// assert_eq!(&breakpoints, &[0, 11]);
/// ```
///
/// Segment a Latin1 byte string:
///
/// ```rust
/// use icu::segmenter::{
///     options::SentenceBreakInvariantOptions, SentenceSegmenter,
/// };
/// let segmenter =
///     SentenceSegmenter::new(SentenceBreakInvariantOptions::default());
///
/// let breakpoints: Vec<usize> =
///     segmenter.segment_latin1(b"Hello World").collect();
/// assert_eq!(&breakpoints, &[0, 11]);
/// ```
///
/// Successive boundaries can be used to retrieve the sentences.
/// In particular, the first boundary is always 0, and the last one is the
/// length of the segmented text in code units.
///
/// ```rust
/// # use icu::segmenter::{SentenceSegmenter, options::SentenceBreakInvariantOptions};
/// # let segmenter = SentenceSegmenter::new(SentenceBreakInvariantOptions::default());
/// use itertools::Itertools;
/// let text = "Ceci tuera cela. Le livre tuera l’édifice.";
/// let sentences: Vec<&str> = segmenter
///     .segment_str(text)
///     .tuple_windows()
///     .map(|(i, j)| &text[i..j])
///     .collect();
/// assert_eq!(
///     &sentences,
///     &["Ceci tuera cela. ", "Le livre tuera l’édifice."]
/// );
/// ```
#[derive(Debug)]
pub struct SentenceSegmenter {
    payload: DataPayload<SentenceBreakDataV2>,
    payload_locale_override: Option<DataPayload<SentenceBreakDataOverrideV1>>,
}

impl SentenceSegmenter {
    /// Constructs a [`SentenceSegmenter`] with an invariant locale and compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn new(_options: SentenceBreakInvariantOptions) -> Self {
        Self {
            payload: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_SENTENCE_BREAK_DATA_V2,
            ),
            payload_locale_override: None,
        }
    }

    icu_provider::gen_buffer_data_constructors!(
        (options: SentenceBreakOptions) -> error: DataError,
        /// Constructs a [`SentenceSegmenter`] for a given options and using compiled data.
        functions: [
            try_new,
                        try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D>(
        provider: &D,
        options: SentenceBreakOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<SentenceBreakDataV2> + DataProvider<SentenceBreakDataOverrideV1> + ?Sized,
    {
        let payload = provider.load(Default::default())?.payload;
        let payload_locale_override = if let Some(locale) = options.content_locale {
            let locale = DataLocale::from(locale);
            let req = DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                metadata: {
                    let mut metadata = DataRequestMetadata::default();
                    metadata.silent = true;
                    metadata
                },
            };
            provider
                .load(req)
                .allow_identifier_not_found()?
                .map(|r| r.payload)
        } else {
            None
        };

        Ok(Self {
            payload,
            payload_locale_override,
        })
    }

    /// Creates a sentence break iterator for an `str` (a UTF-8 string).
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_str<'l, 's>(&'l self, input: &'s str) -> SentenceBreakIteratorUtf8<'l, 's> {
        let locale_override = self
            .payload_locale_override
            .as_ref()
            .map(|payload| payload.get());
        SentenceBreakIterator(RuleBreakIterator {
            iter: input.char_indices(),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.payload.get(),
            complex: None,
            boundary_property: 0,
            locale_override,
        })
    }
    /// Creates a sentence break iterator for a potentially ill-formed UTF8 string
    ///
    /// Invalid characters are treated as REPLACEMENT CHARACTER
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_utf8<'l, 's>(
        &'l self,
        input: &'s [u8],
    ) -> SentenceBreakIteratorPotentiallyIllFormedUtf8<'l, 's> {
        let locale_override = self
            .payload_locale_override
            .as_ref()
            .map(|payload| payload.get());
        SentenceBreakIterator(RuleBreakIterator {
            iter: Utf8CharIndices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.payload.get(),
            complex: None,
            boundary_property: 0,
            locale_override,
        })
    }
    /// Creates a sentence break iterator for a Latin-1 (8-bit) string.
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_latin1<'l, 's>(
        &'l self,
        input: &'s [u8],
    ) -> SentenceBreakIteratorLatin1<'l, 's> {
        let locale_override = self
            .payload_locale_override
            .as_ref()
            .map(|payload| payload.get());
        SentenceBreakIterator(RuleBreakIterator {
            iter: Latin1Indices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.payload.get(),
            complex: None,
            boundary_property: 0,
            locale_override,
        })
    }

    /// Creates a sentence break iterator for a UTF-16 string.
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_utf16<'l, 's>(&'l self, input: &'s [u16]) -> SentenceBreakIteratorUtf16<'l, 's> {
        let locale_override = self
            .payload_locale_override
            .as_ref()
            .map(|payload| payload.get());
        SentenceBreakIterator(RuleBreakIterator {
            iter: Utf16Indices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.payload.get(),
            complex: None,
            boundary_property: 0,
            locale_override,
        })
    }
}

#[cfg(all(test, feature = "serde"))]
#[test]
fn empty_string() {
    let segmenter = SentenceSegmenter::new(Default::default());
    let breaks: Vec<usize> = segmenter.segment_str("").collect();
    assert_eq!(breaks, [0]);
}
