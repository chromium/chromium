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

    #[diplomat::enum_convert(icu_segmenter::WordType, needs_wildcard)]
    #[diplomat::rust_link(icu::segmenter::WordType, Enum)]
    pub enum SegmenterWordType {
        None = 0,
        Number = 1,
        Letter = 2,
    }

    #[diplomat::opaque]
    /// An ICU4X word-break segmenter, capable of finding word breakpoints in strings.
    #[diplomat::rust_link(icu::segmenter::WordSegmenter, Struct)]
    #[diplomat::demo(custom_func = "../../npm/demo_gen_custom/WordSegmenter.mjs")]
    pub struct WordSegmenter(icu_segmenter::WordSegmenter);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::WordBreakIterator, Struct)]
    #[diplomat::rust_link(
        icu::segmenter::WordBreakIteratorPotentiallyIllFormedUtf8,
        Typedef,
        hidden
    )]
    #[diplomat::rust_link(icu::segmenter::WordBreakIteratorUtf8, Typedef, hidden)]
    pub struct WordBreakIteratorUtf8<'a>(
        icu_segmenter::WordBreakIteratorPotentiallyIllFormedUtf8<'a, 'a>,
    );

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::WordBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::WordBreakIteratorUtf16, Typedef, hidden)]
    pub struct WordBreakIteratorUtf16<'a>(icu_segmenter::WordBreakIteratorUtf16<'a, 'a>);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::WordBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::WordBreakIteratorLatin1, Typedef, hidden)]
    pub struct WordBreakIteratorLatin1<'a>(icu_segmenter::WordBreakIteratorLatin1<'a, 'a>);

    impl SegmenterWordType {
        #[diplomat::rust_link(icu::segmenter::WordType::is_word_like, FnInEnum)]
        #[diplomat::attr(auto, getter)]
        pub fn is_word_like(self) -> bool {
            icu_segmenter::WordType::from(self).is_word_like()
        }
    }

    impl WordSegmenter {
        /// Construct an [`WordSegmenter`] with automatically selecting the best available LSTM
        /// or dictionary payload data.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::new_auto, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "auto")]
        pub fn create_auto(provider: &DataProvider) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(call_constructor!(
                icu_segmenter::WordSegmenter::new_auto [r => Ok(r)],
                icu_segmenter::WordSegmenter::try_new_auto_with_any_provider,
                icu_segmenter::WordSegmenter::try_new_auto_with_buffer_provider,
                provider
            )?)))
        }

        /// Construct an [`WordSegmenter`] with automatically selecting the best available LSTM
        /// or dictionary payload data.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_auto_with_options, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "auto_with_content_locale")]
        pub fn create_auto_with_content_locale(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(call_constructor!(
                icu_segmenter::WordSegmenter::try_new_auto_with_options,
                icu_segmenter::WordSegmenter::try_new_auto_with_options_with_any_provider,
                icu_segmenter::WordSegmenter::try_new_auto_with_options_with_buffer_provider,
                provider,
                locale.into(),
            )?)))
        }

        /// Construct an [`WordSegmenter`] with LSTM payload data for Burmese, Khmer, Lao, and
        /// Thai.
        ///
        /// Warning: [`WordSegmenter`] created by this function doesn't handle Chinese or
        /// Japanese.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::new_lstm, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "lstm")]
        pub fn create_lstm(provider: &DataProvider) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(call_constructor!(
                icu_segmenter::WordSegmenter::new_lstm [r => Ok(r)],
                icu_segmenter::WordSegmenter::try_new_lstm_with_any_provider,
                icu_segmenter::WordSegmenter::try_new_lstm_with_buffer_provider,
                provider,
            )?)))
        }

        /// Construct an [`WordSegmenter`] with given a locale, and LSTM payload data for Burmese,
        /// Khmer, Lao, and Thai.
        ///
        /// Warning: [`WordSegmenter`] created by this function doesn't handle Chinese or
        /// Japanese.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_lstm_with_options, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "lstm_with_content_locale")]
        pub fn create_lstm_with_content_locale(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(call_constructor!(
                icu_segmenter::WordSegmenter::try_new_lstm_with_options,
                icu_segmenter::WordSegmenter::try_new_lstm_with_options_with_any_provider,
                icu_segmenter::WordSegmenter::try_new_lstm_with_options_with_buffer_provider,
                provider,
                locale.into(),
            )?)))
        }

        /// Construct an [`WordSegmenter`] with dictionary payload data for Chinese, Japanese,
        /// Burmese, Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::new_dictionary, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "dictionary")]
        pub fn create_dictionary(provider: &DataProvider) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(call_constructor!(
                icu_segmenter::WordSegmenter::new_dictionary [r => Ok(r)],
                icu_segmenter::WordSegmenter::try_new_dictionary_with_any_provider,
                icu_segmenter::WordSegmenter::try_new_dictionary_with_buffer_provider,
                provider,
            )?)))
        }

        /// Construct an [`WordSegmenter`] with given a locale, and dictionary payload data for Chinese,
        /// Japanese, Burmese, Khmer, Lao, and Thai.
        #[diplomat::rust_link(
            icu::segmenter::WordSegmenter::try_new_dictionary_with_options,
            FnInStruct
        )]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "dictionary_with_content_locale")]
        pub fn create_dictionary_with_content_locale(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(call_constructor!(
                icu_segmenter::WordSegmenter::try_new_dictionary_with_options,
                icu_segmenter::WordSegmenter::try_new_dictionary_with_options_with_any_provider,
                icu_segmenter::WordSegmenter::try_new_dictionary_with_options_with_buffer_provider,
                provider,
                locale.into(),
            )?)))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::segment_utf8, FnInStruct)]
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::segment_str, FnInStruct, hidden)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "segment")]
        pub fn segment_utf8<'a>(
            &'a self,
            input: &'a DiplomatStr,
        ) -> Box<WordBreakIteratorUtf8<'a>> {
            Box::new(WordBreakIteratorUtf8(self.0.segment_utf8(input)))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::segment_utf16, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), rename = "segment")]
        #[diplomat::attr(supports = utf8_strings, rename = "segment16")]
        pub fn segment_utf16<'a>(
            &'a self,
            input: &'a DiplomatStr16,
        ) -> Box<WordBreakIteratorUtf16<'a>> {
            Box::new(WordBreakIteratorUtf16(self.0.segment_utf16(input)))
        }

        /// Segments a Latin-1 string.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::segment_latin1, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        pub fn segment_latin1<'a>(&'a self, input: &'a [u8]) -> Box<WordBreakIteratorLatin1<'a>> {
            Box::new(WordBreakIteratorLatin1(self.0.segment_latin1(input)))
        }
    }

    impl<'a> WordBreakIteratorUtf8<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::WordBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }

        /// Return the status value of break boundary.
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::word_type, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn word_type(&self) -> SegmenterWordType {
            self.0.word_type().into()
        }

        /// Return true when break boundary is word-like such as letter/number/CJK
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::is_word_like, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn is_word_like(&self) -> bool {
            self.0.is_word_like()
        }
    }

    impl<'a> WordBreakIteratorUtf16<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::WordBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }

        /// Return the status value of break boundary.
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::word_type, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::WordBreakIterator::iter_with_word_type,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, getter)]
        pub fn word_type(&self) -> SegmenterWordType {
            self.0.word_type().into()
        }

        /// Return true when break boundary is word-like such as letter/number/CJK
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::is_word_like, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn is_word_like(&self) -> bool {
            self.0.is_word_like()
        }
    }

    impl<'a> WordBreakIteratorLatin1<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::WordBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }

        /// Return the status value of break boundary.
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::word_type, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn word_type(&self) -> SegmenterWordType {
            self.0.word_type().into()
        }

        /// Return true when break boundary is word-like such as letter/number/CJK
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::is_word_like, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn is_word_like(&self) -> bool {
            self.0.is_word_like()
        }
    }
}

impl<'a> From<&'a crate::locale_core::ffi::Locale> for icu_segmenter::WordBreakOptions<'a> {
    fn from(other: &'a crate::locale_core::ffi::Locale) -> Self {
        let mut options = icu_segmenter::WordBreakOptions::default();
        options.content_locale = Some(&other.0.id);
        options
    }
}
