// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::errors::ffi::DataError;
    use crate::locale_core::ffi::Locale;
    use crate::provider::ffi::DataProvider;

    #[diplomat::opaque]
    /// An ICU4X sentence-break segmenter, capable of finding sentence breakpoints in strings.
    #[diplomat::rust_link(icu::segmenter::SentenceSegmenter, Struct)]
    pub struct SentenceSegmenter(icu_segmenter::SentenceSegmenter);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator, Struct)]
    #[diplomat::rust_link(
        icu::segmenter::SentenceBreakIteratorPotentiallyIllFormedUtf8,
        Typedef,
        hidden
    )]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIteratorUtf8, Typedef, hidden)]
    pub struct SentenceBreakIteratorUtf8<'a>(
        icu_segmenter::SentenceBreakIteratorPotentiallyIllFormedUtf8<'a, 'a>,
    );

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIteratorUtf16, Typedef, hidden)]
    pub struct SentenceBreakIteratorUtf16<'a>(icu_segmenter::SentenceBreakIteratorUtf16<'a, 'a>);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIteratorLatin1, Typedef, hidden)]
    pub struct SentenceBreakIteratorLatin1<'a>(icu_segmenter::SentenceBreakIteratorLatin1<'a, 'a>);

    impl SentenceSegmenter {
        /// Construct an [`SentenceSegmenter`].
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::new, FnInStruct, hidden)]
        pub fn create(provider: &DataProvider) -> Result<Box<SentenceSegmenter>, DataError> {
            Ok(Box::new(SentenceSegmenter(call_constructor!(
                icu_segmenter::SentenceSegmenter::new [r => Ok(r)],
                icu_segmenter::SentenceSegmenter::try_new_with_any_provider,
                icu_segmenter::SentenceSegmenter::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Construct an [`SentenceSegmenter`].
        #[diplomat::rust_link(
            icu::segmenter::SentenceSegmenter::try_new_with_options,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "with_content_locale")]
        pub fn create_with_content_locale(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<SentenceSegmenter>, DataError> {
            Ok(Box::new(SentenceSegmenter(call_constructor!(
                icu_segmenter::SentenceSegmenter::try_new_with_options,
                icu_segmenter::SentenceSegmenter::try_new_with_options_with_any_provider,
                icu_segmenter::SentenceSegmenter::try_new_with_options_with_buffer_provider,
                provider,
                locale.into(),
            )?)))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::segment_utf8, FnInStruct)]
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::segment_str, FnInStruct, hidden)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "segment")]
        pub fn segment_utf8<'a>(
            &'a self,
            input: &'a DiplomatStr,
        ) -> Box<SentenceBreakIteratorUtf8<'a>> {
            Box::new(SentenceBreakIteratorUtf8(self.0.segment_utf8(input)))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::segment_utf16, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), rename = "segment")]
        #[diplomat::attr(supports = utf8_strings, rename = "segment16")]
        pub fn segment_utf16<'a>(
            &'a self,
            input: &'a DiplomatStr16,
        ) -> Box<SentenceBreakIteratorUtf16<'a>> {
            Box::new(SentenceBreakIteratorUtf16(self.0.segment_utf16(input)))
        }

        /// Segments a Latin-1 string.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::segment_latin1, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        pub fn segment_latin1<'a>(
            &'a self,
            input: &'a [u8],
        ) -> Box<SentenceBreakIteratorLatin1<'a>> {
            Box::new(SentenceBreakIteratorLatin1(self.0.segment_latin1(input)))
        }
    }

    impl<'a> SentenceBreakIteratorUtf8<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::SentenceBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }
    }

    impl<'a> SentenceBreakIteratorUtf16<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::SentenceBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }
    }

    impl<'a> SentenceBreakIteratorLatin1<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::SentenceBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }
    }
}

impl<'a> From<&'a crate::locale_core::ffi::Locale> for icu_segmenter::SentenceBreakOptions<'a> {
    fn from(other: &'a crate::locale_core::ffi::Locale) -> Self {
        let mut options = icu_segmenter::SentenceBreakOptions::default();
        options.content_locale = Some(&other.0.id);
        options
    }
}
