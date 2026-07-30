// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_segmenter::scaffold::{Latin1, PotentiallyIllFormedUtf8, Utf16};

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::{errors::ffi::DataError, provider::ffi::DataProvider};
    use diplomat_runtime::DiplomatOption;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use icu_segmenter::options::LineBreakOptions;

    #[diplomat::opaque]
    /// An ICU4X line-break segmenter, capable of finding breakpoints in strings.
    #[diplomat::rust_link(icu::segmenter::LineSegmenter, Struct)]
    #[diplomat::rust_link(icu::segmenter::LineSegmenterBorrowed, Struct, hidden)]
    pub struct LineSegmenter(icu_segmenter::LineSegmenter);

    #[diplomat::rust_link(icu::segmenter::options::LineBreakStrictness, Enum)]
    #[diplomat::enum_convert(icu_segmenter::options::LineBreakStrictness, needs_wildcard)]
    #[non_exhaustive]
    pub enum LineBreakStrictness {
        Loose,
        Normal,
        #[diplomat::attr(auto, default)]
        Strict,
        Anywhere,
    }

    #[diplomat::rust_link(icu::segmenter::options::LineBreakWordOption, Enum)]
    #[diplomat::enum_convert(icu_segmenter::options::LineBreakWordOption, needs_wildcard)]
    #[non_exhaustive]
    pub enum LineBreakWordOption {
        #[diplomat::attr(auto, default)]
        Normal,
        BreakAll,
        KeepAll,
    }

    #[diplomat::rust_link(icu::segmenter::options::LineBreakOptions, Struct)]
    #[diplomat::rust_link(icu::segmenter::options::LineBreakOptions::default, FnInStruct, hidden)]
    #[diplomat::attr(supports = non_exhaustive_structs, rename = "LineBreakOptions")]
    pub struct LineBreakOptionsV2 {
        pub strictness: DiplomatOption<LineBreakStrictness>,
        pub word_option: DiplomatOption<LineBreakWordOption>,
    }

    #[diplomat::opaque_mut]
    #[diplomat::rust_link(icu::segmenter::iterators::LineBreakIterator, Struct)]
    #[diplomat::attr(demo_gen, disable)] // iterator type
    pub struct LineBreakIteratorUtf8<'a>(
        icu_segmenter::iterators::LineBreakIterator<'a, 'a, PotentiallyIllFormedUtf8>,
    );

    #[diplomat::opaque_mut]
    #[diplomat::rust_link(icu::segmenter::iterators::LineBreakIterator, Struct)]
    #[diplomat::attr(demo_gen, disable)] // iterator type
    pub struct LineBreakIteratorUtf16<'a>(
        icu_segmenter::iterators::LineBreakIterator<'a, 'a, Utf16>,
    );

    #[diplomat::opaque_mut]
    #[diplomat::rust_link(icu::segmenter::iterators::LineBreakIterator, Struct)]
    #[diplomat::attr(demo_gen, disable)] // iterator type
    pub struct LineBreakIteratorLatin1<'a>(
        icu_segmenter::iterators::LineBreakIterator<'a, 'a, Latin1>,
    );

    impl LineSegmenter {
        /// Construct a [`LineSegmenter`] with default options (no locale-based tailoring) using compiled data. It automatically loads the best
        /// available payload data for Burmese, Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::LineSegmenter::new_auto, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "auto")]
        #[cfg(feature = "compiled_data")]
        pub fn create_auto() -> Box<LineSegmenter> {
            Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::new_auto(Default::default()).static_to_owned(),
            ))
        }

        /// Construct a [`LineSegmenter`] with default options (no locale-based tailoring) and LSTM payload data for
        /// Burmese, Khmer, Lao, and Thai, using compiled data.
        #[diplomat::rust_link(icu::segmenter::LineSegmenter::new_lstm, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "lstm")]
        #[cfg(feature = "compiled_data")]
        pub fn create_lstm() -> Box<LineSegmenter> {
            Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::new_lstm(Default::default()).static_to_owned(),
            ))
        }

        /// Construct a [`LineSegmenter`] with default options (no locale-based tailoring) and dictionary payload data for
        /// Burmese, Khmer, Lao, and Thai, using compiled data
        #[diplomat::rust_link(icu::segmenter::LineSegmenter::new_dictionary, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "dictionary")]
        #[cfg(feature = "compiled_data")]
        pub fn create_dictionary() -> Box<LineSegmenter> {
            Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::new_dictionary(Default::default()).static_to_owned(),
            ))
        }

        /// Construct a [`LineSegmenter`] with default options (no locale-based tailoring) and no support for scripts requiring complex context dependent line breaks
        /// (Burmese, Khmer, Lao, and Thai), using compiled data
        #[diplomat::rust_link(
            icu::segmenter::LineSegmenter::new_for_non_complex_scripts,
            FnInStruct
        )]
        #[diplomat::attr(auto, named_constructor = "for_non_complex_scripts")]
        #[cfg(feature = "compiled_data")]
        pub fn create_for_non_complex_scripts() -> Box<LineSegmenter> {
            Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::new_for_non_complex_scripts(Default::default())
                    .static_to_owned(),
            ))
        }

        /// Construct a [`LineSegmenter`] with custom options using compiled data. It automatically loads the best
        /// available payload data for Burmese, Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::LineSegmenter::new_auto, FnInStruct)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "auto_with_options")]
        #[diplomat::attr(all(supports = non_exhaustive_structs, supports = named_constructors), named_constructor = "auto_with_options")]
        #[diplomat::attr(all(not(supports = non_exhaustive_structs), supports = named_constructors), named_constructor = "auto_with_options_v2")]
        #[cfg(feature = "compiled_data")]
        pub fn create_auto_with_options_v2(
            content_locale: Option<&Locale>,
            options: LineBreakOptionsV2,
        ) -> Box<LineSegmenter> {
            let mut options: LineBreakOptions = options.into();
            options.content_locale = content_locale.map(|c| &c.0.id);
            Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::new_auto(options).static_to_owned(),
            ))
        }
        /// Construct a [`LineSegmenter`] with custom options. It automatically loads the best
        /// available payload data for Burmese, Khmer, Lao, and Thai, using a particular data source.
        #[diplomat::rust_link(icu::segmenter::LineSegmenter::new_auto, FnInStruct)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "auto_with_options_and_provider")]
        #[diplomat::attr(all(supports = non_exhaustive_structs, supports = fallible_constructors, supports = named_constructors), named_constructor = "auto_with_options_and_provider")]
        #[diplomat::attr(all(not(supports = non_exhaustive_structs), supports = fallible_constructors, supports = named_constructors), named_constructor = "auto_with_options_v2_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_auto_with_options_v2_and_provider(
            provider: &DataProvider,
            content_locale: Option<&Locale>,
            options: LineBreakOptionsV2,
        ) -> Result<Box<LineSegmenter>, DataError> {
            let mut options: LineBreakOptions = options.into();
            options.content_locale = content_locale.map(|c| &c.0.id);

            Ok(Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::try_new_auto_with_buffer_provider(
                    provider.get()?,
                    options,
                )?,
            )))
        }
        /// Construct a [`LineSegmenter`] with custom options and LSTM payload data for
        /// Burmese, Khmer, Lao, and Thai, using compiled data.
        #[diplomat::rust_link(icu::segmenter::LineSegmenter::new_lstm, FnInStruct)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "lstm_with_options")]
        #[diplomat::attr(all(supports = non_exhaustive_structs, supports = named_constructors), named_constructor = "lstm_with_options")]
        #[diplomat::attr(all(not(supports = non_exhaustive_structs), supports = named_constructors), named_constructor = "lstm_with_options_v2")]
        #[cfg(feature = "compiled_data")]
        pub fn create_lstm_with_options_v2(
            content_locale: Option<&Locale>,
            options: LineBreakOptionsV2,
        ) -> Box<LineSegmenter> {
            let mut options: LineBreakOptions = options.into();
            options.content_locale = content_locale.map(|c| &c.0.id);

            Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::new_lstm(options).static_to_owned(),
            ))
        }
        /// Construct a [`LineSegmenter`] with custom options and LSTM payload data for
        /// Burmese, Khmer, Lao, and Thai, using a particular data source.
        #[diplomat::rust_link(icu::segmenter::LineSegmenter::new_lstm, FnInStruct)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "lstm_with_options_and_provider")]
        #[diplomat::attr(all(supports = non_exhaustive_structs, supports = fallible_constructors, supports = named_constructors), named_constructor = "lstm_with_options_and_provider")]
        #[diplomat::attr(all(not(supports = non_exhaustive_structs), supports = fallible_constructors, supports = named_constructors), named_constructor = "lstm_with_options_v2_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_lstm_with_options_v2_and_provider(
            provider: &DataProvider,
            content_locale: Option<&Locale>,
            options: LineBreakOptionsV2,
        ) -> Result<Box<LineSegmenter>, DataError> {
            let mut options: LineBreakOptions = options.into();
            options.content_locale = content_locale.map(|c| &c.0.id);

            Ok(Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::try_new_lstm_with_buffer_provider(
                    provider.get()?,
                    options,
                )?,
            )))
        }
        /// Construct a [`LineSegmenter`] with custom options and dictionary payload data for
        /// Burmese, Khmer, Lao, and Thai, using compiled data.
        #[diplomat::rust_link(icu::segmenter::LineSegmenter::new_dictionary, FnInStruct)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "dictionary_with_options")]
        #[diplomat::attr(all(supports = non_exhaustive_structs, supports = named_constructors), named_constructor = "dictionary_with_options")]
        #[diplomat::attr(all(not(supports = non_exhaustive_structs), supports = named_constructors), named_constructor = "dictionary_with_options_v2")]
        #[cfg(feature = "compiled_data")]
        pub fn create_dictionary_with_options_v2(
            content_locale: Option<&Locale>,
            options: LineBreakOptionsV2,
        ) -> Box<LineSegmenter> {
            let mut options: LineBreakOptions = options.into();
            options.content_locale = content_locale.map(|c| &c.0.id);

            Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::new_dictionary(options).static_to_owned(),
            ))
        }
        /// Construct a [`LineSegmenter`] with custom options and dictionary payload data for
        /// Burmese, Khmer, Lao, and Thai, using a particular data source.
        #[diplomat::rust_link(icu::segmenter::LineSegmenter::new_dictionary, FnInStruct)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "dictionary_with_options_and_provider")]
        #[diplomat::attr(all(supports = non_exhaustive_structs, supports = fallible_constructors, supports = named_constructors), named_constructor = "dictionary_with_options_and_provider")]
        #[diplomat::attr(all(not(supports = non_exhaustive_structs), supports = fallible_constructors, supports = named_constructors), named_constructor = "dictionary_with_options_v2_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_dictionary_with_options_v2_and_provider(
            provider: &DataProvider,
            content_locale: Option<&Locale>,
            options: LineBreakOptionsV2,
        ) -> Result<Box<LineSegmenter>, DataError> {
            let mut options: LineBreakOptions = options.into();
            options.content_locale = content_locale.map(|c| &c.0.id);

            Ok(Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::try_new_dictionary_with_buffer_provider(
                    provider.get()?,
                    options,
                )?,
            )))
        }
        /// Construct a [`LineSegmenter`] with custom options and no support for scripts requiring complex context dependent line breaks
        /// (Burmese, Khmer, Lao, and Thai), using compiled data.
        #[diplomat::rust_link(
            icu::segmenter::LineSegmenter::new_for_non_complex_scripts,
            FnInStruct
        )]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "for_non_complex_scripts_with_options")]
        #[diplomat::attr(all(supports = non_exhaustive_structs, supports = named_constructors), named_constructor = "for_non_complex_scripts_with_options")]
        #[diplomat::attr(all(not(supports = non_exhaustive_structs), supports = named_constructors), named_constructor = "for_non_complex_scripts_with_options_v2")]
        #[cfg(feature = "compiled_data")]
        pub fn create_for_non_complex_scripts_with_options_v2(
            content_locale: Option<&Locale>,
            options: LineBreakOptionsV2,
        ) -> Box<LineSegmenter> {
            let mut options: LineBreakOptions = options.into();
            options.content_locale = content_locale.map(|c| &c.0.id);

            Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::new_for_non_complex_scripts(options)
                    .static_to_owned(),
            ))
        }
        /// Construct a [`LineSegmenter`] with custom options and no support for complex languages
        /// (Burmese, Khmer, Lao, and Thai), using a particular data source.
        #[diplomat::rust_link(
            icu::segmenter::LineSegmenter::new_for_non_complex_scripts,
            FnInStruct
        )]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "for_non_complex_scripts_with_options_and_provider")]
        #[diplomat::attr(all(supports = non_exhaustive_structs, supports = fallible_constructors, supports = named_constructors), named_constructor = "for_non_complex_scripts_with_options_and_provider")]
        #[diplomat::attr(all(not(supports = non_exhaustive_structs), supports = fallible_constructors, supports = named_constructors), named_constructor = "for_non_complex_scripts_with_options_v2_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_for_non_complex_scripts_with_options_v2_and_provider(
            provider: &DataProvider,
            content_locale: Option<&Locale>,
            options: LineBreakOptionsV2,
        ) -> Result<Box<LineSegmenter>, DataError> {
            let mut options: LineBreakOptions = options.into();
            options.content_locale = content_locale.map(|c| &c.0.id);

            Ok(Box::new(LineSegmenter(
                icu_segmenter::LineSegmenter::try_new_for_non_complex_scripts_with_buffer_provider(
                    provider.get()?,
                    options,
                )?,
            )))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::LineSegmenterBorrowed::segment_utf8, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::LineSegmenterBorrowed::segment_str,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "segment")]
        pub fn segment_utf8<'a>(
            &'a self,
            input: &'a DiplomatStr,
        ) -> Box<LineBreakIteratorUtf8<'a>> {
            Box::new(LineBreakIteratorUtf8(
                self.0.as_borrowed().segment_utf8(input),
            ))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::LineSegmenterBorrowed::segment_utf16, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), rename = "segment")]
        #[diplomat::attr(supports = utf8_strings, rename = "segment16")]
        pub fn segment_utf16<'a>(
            &'a self,
            input: &'a DiplomatStr16,
        ) -> Box<LineBreakIteratorUtf16<'a>> {
            Box::new(LineBreakIteratorUtf16(
                self.0.as_borrowed().segment_utf16(input),
            ))
        }

        /// Segments a Latin-1 string.
        #[diplomat::rust_link(icu::segmenter::LineSegmenterBorrowed::segment_latin1, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        pub fn segment_latin1<'a>(&'a self, input: &'a [u8]) -> Box<LineBreakIteratorLatin1<'a>> {
            Box::new(LineBreakIteratorLatin1(
                self.0.as_borrowed().segment_latin1(input),
            ))
        }
    }

    impl<'a> LineBreakIteratorUtf8<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::iterators::LineBreakIterator::next, FnInStruct)]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }
    }

    impl<'a> LineBreakIteratorUtf16<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::iterators::LineBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::iterators::LineBreakIterator::Item,
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

    impl<'a> LineBreakIteratorLatin1<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::iterators::LineBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::iterators::LineBreakIterator::Item,
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

impl From<ffi::LineBreakOptionsV2> for icu_segmenter::options::LineBreakOptions<'_> {
    fn from(other: ffi::LineBreakOptionsV2) -> Self {
        let mut options = icu_segmenter::options::LineBreakOptions::default();
        options.strictness = other.strictness.into_converted_option();
        options.word_option = other.word_option.into_converted_option();
        options
    }
}
