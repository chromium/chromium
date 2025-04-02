// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::provider::ffi::DataProvider;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::{errors::ffi::DataError, locale_core::ffi::Locale};

    #[diplomat::enum_convert(icu_segmenter::options::WordType, needs_wildcard)]
    #[diplomat::rust_link(icu::segmenter::options::WordType, Enum)]
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
        #[diplomat::rust_link(icu::segmenter::options::WordType::is_word_like, FnInEnum)]
        #[diplomat::attr(auto, getter)]
        pub fn is_word_like(self) -> bool {
            icu_segmenter::options::WordType::from(self).is_word_like()
        }
    }

    impl WordSegmenter {
        /// Construct an [`WordSegmenter`] with automatically selecting the best available LSTM
        /// or dictionary payload data, using compiled data. This does not assume any content locale.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::new_auto, FnInStruct)]
        #[diplomat::rust_link(icu::segmenter::options::WordBreakInvariantOptions, Struct, hidden)]
        #[diplomat::attr(auto, named_constructor = "auto")]
        #[cfg(feature = "compiled_data")]
        pub fn create_auto() -> Box<WordSegmenter> {
            Box::new(WordSegmenter(icu_segmenter::WordSegmenter::new_auto(
                Default::default(),
            )))
        }

        /// Construct an [`WordSegmenter`] with automatically selecting the best available LSTM
        /// or dictionary payload data, using compiled data.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_auto, FnInStruct)]
        #[diplomat::rust_link(icu::segmenter::options::WordBreakOptions, Struct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "auto_with_content_locale")]
        #[cfg(feature = "compiled_data")]
        pub fn create_auto_with_content_locale(
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::try_new_auto(locale.into())?,
            )))
        }

        /// Construct an [`WordSegmenter`] with automatically selecting the best available LSTM
        /// or dictionary payload data, using a particular data source.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_auto, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "auto_with_content_locale_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_auto_with_content_locale_and_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::try_new_auto_with_buffer_provider(
                    provider.get()?,
                    locale.into(),
                )?,
            )))
        }

        /// Construct an [`WordSegmenter`] with LSTM payload data for Burmese, Khmer, Lao, and
        /// Thai, using compiled data.  This does not assume any content locale.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::new_lstm, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "lstm")]
        #[cfg(feature = "compiled_data")]
        pub fn create_lstm() -> Box<WordSegmenter> {
            Box::new(WordSegmenter(icu_segmenter::WordSegmenter::new_lstm(
                Default::default(),
            )))
        }

        /// Construct an [`WordSegmenter`] with LSTM payload data for Burmese, Khmer, Lao, and
        /// Thai, using compiled data.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_lstm, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "lstm_with_content_locale")]
        #[cfg(feature = "compiled_data")]
        pub fn create_lstm_with_content_locale(
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::try_new_lstm(locale.into())?,
            )))
        }

        /// Construct an [`WordSegmenter`] with LSTM payload data for Burmese, Khmer, Lao, and
        /// Thai, using a particular data source.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_lstm, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "lstm_with_content_locale_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_lstm_with_content_locale_and_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::try_new_lstm_with_buffer_provider(
                    provider.get()?,
                    locale.into(),
                )?,
            )))
        }

        /// Construct an [`WordSegmenter`] with with dictionary payload data for Chinese, Japanese,
        /// Burmese, Khmer, Lao, and Thai, using compiled data.  This does not assume any content locale.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and dictionary for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::new_dictionary, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "dictionary")]
        #[cfg(feature = "compiled_data")]
        pub fn create_dictionary() -> Box<WordSegmenter> {
            Box::new(WordSegmenter(icu_segmenter::WordSegmenter::new_dictionary(
                Default::default(),
            )))
        }

        /// Construct an [`WordSegmenter`] with dictionary payload data for Chinese, Japanese,
        /// Burmese, Khmer, Lao, and Thai, using compiled data.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and dictionary for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_dictionary, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "dictionary_with_content_locale")]
        #[cfg(feature = "compiled_data")]
        pub fn create_dictionary_with_content_locale(
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::try_new_dictionary(locale.into())?,
            )))
        }

        /// Construct an [`WordSegmenter`] with dictionary payload data for Chinese, Japanese,
        /// Burmese, Khmer, Lao, and Thai, using a particular data source.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and dictionary for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_dictionary, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "dictionary_with_content_locale_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_dictionary_with_content_locale_and_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::try_new_dictionary_with_buffer_provider(
                    provider.get()?,
                    locale.into(),
                )?,
            )))
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

impl<'a> From<&'a crate::locale_core::ffi::Locale>
    for icu_segmenter::options::WordBreakOptions<'a>
{
    fn from(other: &'a crate::locale_core::ffi::Locale) -> Self {
        let mut options = icu_segmenter::options::WordBreakOptions::default();
        options.content_locale = Some(&other.0.id);
        options
    }
}
