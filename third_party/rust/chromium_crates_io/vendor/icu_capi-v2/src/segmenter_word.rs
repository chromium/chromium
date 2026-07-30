// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_segmenter::scaffold::{Latin1, PotentiallyIllFormedUtf8, Utf16};

    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::{errors::ffi::DataError, locale_core::ffi::Locale};

    #[diplomat::enum_convert(icu_segmenter::options::WordType, needs_wildcard)]
    #[diplomat::rust_link(icu::segmenter::options::WordType, Enum)]
    #[non_exhaustive]
    pub enum SegmenterWordType {
        // This is an output type, so the default mostly impacts deferred initialization.
        #[diplomat::attr(auto, default)]
        None = 0,
        Number = 1,
        Letter = 2,
    }

    #[diplomat::opaque]
    /// An ICU4X word-break segmenter, capable of finding word breakpoints in strings.
    #[diplomat::rust_link(icu::segmenter::WordSegmenter, Struct)]
    #[diplomat::rust_link(icu::segmenter::WordSegmenterBorrowed, Struct, hidden)]
    #[diplomat::demo(custom_func = "../../../tools/web-demo/custom/WordSegmenter.mjs")]
    pub struct WordSegmenter(icu_segmenter::WordSegmenter);

    #[diplomat::opaque_mut]
    #[diplomat::rust_link(icu::segmenter::iterators::WordBreakIterator, Struct)]
    #[diplomat::attr(demo_gen, disable)] // iterator type
    pub struct WordBreakIteratorUtf8<'a>(
        icu_segmenter::iterators::WordBreakIterator<'a, 'a, PotentiallyIllFormedUtf8>,
    );

    #[diplomat::opaque_mut]
    #[diplomat::rust_link(icu::segmenter::iterators::WordBreakIterator, Struct)]
    #[diplomat::attr(demo_gen, disable)] // iterator type
    pub struct WordBreakIteratorUtf16<'a>(
        icu_segmenter::iterators::WordBreakIterator<'a, 'a, Utf16>,
    );
    #[diplomat::opaque_mut]
    #[diplomat::rust_link(icu::segmenter::iterators::WordBreakIterator, Struct)]
    #[diplomat::attr(demo_gen, disable)] // iterator type
    pub struct WordBreakIteratorLatin1<'a>(
        icu_segmenter::iterators::WordBreakIterator<'a, 'a, Latin1>,
    );

    impl SegmenterWordType {
        #[diplomat::rust_link(icu::segmenter::options::WordType::is_word_like, FnInEnum)]
        #[diplomat::attr(auto, getter)]
        pub fn is_word_like(self) -> bool {
            icu_segmenter::options::WordType::from(self).is_word_like()
        }
    }

    impl WordSegmenter {
        /// Construct a [`WordSegmenter`] with automatically selecting the best available LSTM
        /// or dictionary payload data, using compiled data. This does not assume any content locale.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::new_auto, FnInStruct)]
        #[diplomat::rust_link(icu::segmenter::options::WordBreakInvariantOptions, Struct, hidden)]
        #[diplomat::rust_link(
            icu::segmenter::options::WordBreakInvariantOptions::default,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, named_constructor = "auto")]
        #[cfg(feature = "compiled_data")]
        pub fn create_auto() -> Box<WordSegmenter> {
            Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::new_auto(Default::default()).static_to_owned(),
            ))
        }

        /// Construct a [`WordSegmenter`] with automatically selecting the best available LSTM
        /// or dictionary payload data, using compiled data.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_auto, FnInStruct)]
        #[diplomat::rust_link(icu::segmenter::options::WordBreakOptions, Struct, hidden)]
        #[diplomat::rust_link(
            icu::segmenter::options::WordBreakOptions::default,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "auto_with_content_locale")]
        #[cfg(feature = "compiled_data")]
        pub fn create_auto_with_content_locale(
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::try_new_auto(locale.into())?,
            )))
        }

        /// Construct a [`WordSegmenter`] with automatically selecting the best available LSTM
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

        /// Construct a [`WordSegmenter`] with LSTM payload data for Burmese, Khmer, Lao, and
        /// Thai, using compiled data. This does not assume any content locale.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::new_lstm, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "lstm")]
        #[cfg(feature = "compiled_data")]
        pub fn create_lstm() -> Box<WordSegmenter> {
            Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::new_lstm(Default::default()).static_to_owned(),
            ))
        }

        /// Construct a [`WordSegmenter`] with LSTM payload data for Burmese, Khmer, Lao, and
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

        /// Construct a [`WordSegmenter`] with LSTM payload data for Burmese, Khmer, Lao, and
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

        /// Construct a [`WordSegmenter`] with dictionary payload data for Chinese, Japanese,
        /// Burmese, Khmer, Lao, and Thai, using compiled data. This does not assume any content locale.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and dictionary for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::new_dictionary, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "dictionary")]
        #[cfg(feature = "compiled_data")]
        pub fn create_dictionary() -> Box<WordSegmenter> {
            Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::new_dictionary(Default::default()).static_to_owned(),
            ))
        }

        /// Construct a [`WordSegmenter`] with dictionary payload data for Chinese, Japanese,
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

        /// Construct a [`WordSegmenter`] with dictionary payload data for Chinese, Japanese,
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

        /// Construct a [`WordSegmenter`] with no support for scripts requiring complex context dependent word breaks (Chinese, Japanese,
        /// Burmese, Khmer, Lao, and Thai), using compiled data. This does not assume any content locale.
        #[diplomat::rust_link(
            icu::segmenter::WordSegmenter::new_for_non_complex_scripts,
            FnInStruct
        )]
        #[diplomat::attr(auto, named_constructor = "for_non_complex_scripts")]
        #[cfg(feature = "compiled_data")]
        pub fn create_for_non_complex_scripts() -> Box<WordSegmenter> {
            Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::new_for_non_complex_scripts(Default::default())
                    .static_to_owned(),
            ))
        }

        /// Construct a [`WordSegmenter`] with no support for scripts requiring complex context dependent word breaks (Chinese, Japanese,
        /// Burmese, Khmer, Lao, and Thai), using compiled data.
        #[diplomat::rust_link(
            icu::segmenter::WordSegmenter::try_new_for_non_complex_scripts,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "for_non_complex_scripts_with_content_locale")]
        #[cfg(feature = "compiled_data")]
        pub fn create_for_non_complex_scripts_with_content_locale(
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::try_new_for_non_complex_scripts(locale.into())?,
            )))
        }

        /// Construct a [`WordSegmenter`] with no support for scripts requiring complex context dependent word breaks (Chinese, Japanese,
        /// Burmese, Khmer, Lao, and Thai), using a particular data source.
        #[diplomat::rust_link(
            icu::segmenter::WordSegmenter::try_new_for_non_complex_scripts,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "for_non_complex_scripts_with_content_locale_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_for_non_complex_scripts_with_content_locale_and_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<WordSegmenter>, DataError> {
            Ok(Box::new(WordSegmenter(
                icu_segmenter::WordSegmenter::try_new_for_non_complex_scripts_with_buffer_provider(
                    provider.get()?,
                    locale.into(),
                )?,
            )))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::WordSegmenterBorrowed::segment_utf8, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::WordSegmenterBorrowed::segment_str,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "segment")]
        pub fn segment_utf8<'a>(
            &'a self,
            input: &'a DiplomatStr,
        ) -> Box<WordBreakIteratorUtf8<'a>> {
            Box::new(WordBreakIteratorUtf8(
                self.0.as_borrowed().segment_utf8(input),
            ))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::WordSegmenterBorrowed::segment_utf16, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), rename = "segment")]
        #[diplomat::attr(supports = utf8_strings, rename = "segment16")]
        pub fn segment_utf16<'a>(
            &'a self,
            input: &'a DiplomatStr16,
        ) -> Box<WordBreakIteratorUtf16<'a>> {
            Box::new(WordBreakIteratorUtf16(
                self.0.as_borrowed().segment_utf16(input),
            ))
        }

        /// Segments a Latin-1 string.
        #[diplomat::rust_link(icu::segmenter::WordSegmenterBorrowed::segment_latin1, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        pub fn segment_latin1<'a>(&'a self, input: &'a [u8]) -> Box<WordBreakIteratorLatin1<'a>> {
            Box::new(WordBreakIteratorLatin1(
                self.0.as_borrowed().segment_latin1(input),
            ))
        }
    }

    impl<'a> WordBreakIteratorUtf8<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::iterators::WordBreakIterator::next, FnInStruct)]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }

        /// Return the status value of break boundary.
        #[diplomat::rust_link(icu::segmenter::iterators::WordBreakIterator::word_type, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::iterators::WordBreakIteratorWithWordType,
            Struct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::segmenter::iterators::WordBreakIteratorWithWordType::next,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, getter)]
        pub fn word_type(&self) -> SegmenterWordType {
            self.0.word_type().into()
        }

        /// Return true when break boundary is word-like such as letter/number/CJK
        #[diplomat::rust_link(
            icu::segmenter::iterators::WordBreakIterator::is_word_like,
            FnInStruct
        )]
        #[diplomat::attr(auto, getter)]
        pub fn is_word_like(&self) -> bool {
            self.0.is_word_like()
        }
    }

    impl<'a> WordBreakIteratorUtf16<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::iterators::WordBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::iterators::WordBreakIterator::Item,
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
        #[diplomat::rust_link(icu::segmenter::iterators::WordBreakIterator::word_type, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::iterators::WordBreakIterator::iter_with_word_type,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, getter)]
        pub fn word_type(&self) -> SegmenterWordType {
            self.0.word_type().into()
        }

        /// Return true when break boundary is word-like such as letter/number/CJK
        #[diplomat::rust_link(
            icu::segmenter::iterators::WordBreakIterator::is_word_like,
            FnInStruct
        )]
        #[diplomat::attr(auto, getter)]
        pub fn is_word_like(&self) -> bool {
            self.0.is_word_like()
        }
    }

    impl<'a> WordBreakIteratorLatin1<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::iterators::WordBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::iterators::WordBreakIterator::Item,
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
        #[diplomat::rust_link(icu::segmenter::iterators::WordBreakIterator::word_type, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn word_type(&self) -> SegmenterWordType {
            self.0.word_type().into()
        }

        /// Return true when break boundary is word-like such as letter/number/CJK
        #[diplomat::rust_link(
            icu::segmenter::iterators::WordBreakIterator::is_word_like,
            FnInStruct
        )]
        #[diplomat::attr(auto, getter)]
        pub fn is_word_like(&self) -> bool {
            self.0.is_word_like()
        }
    }
}

impl<'a> From<&'a crate::unstable::locale_core::ffi::Locale>
    for icu_segmenter::options::WordBreakOptions<'a>
{
    fn from(other: &'a crate::unstable::locale_core::ffi::Locale) -> Self {
        let mut options = icu_segmenter::options::WordBreakOptions::default();
        options.content_locale = Some(&other.0.id);
        options
    }
}
