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

    #[diplomat::opaque]
    /// An ICU4X sentence-break segmenter, capable of finding sentence breakpoints in strings.
    #[diplomat::rust_link(icu::segmenter::SentenceSegmenter, Struct)]
    #[diplomat::rust_link(icu::segmenter::SentenceSegmenterBorrowed, Struct, hidden)]
    pub struct SentenceSegmenter(icu_segmenter::SentenceSegmenter);

    #[diplomat::opaque_mut]
    #[diplomat::rust_link(icu::segmenter::iterators::SentenceBreakIterator, Struct)]
    #[diplomat::attr(demo_gen, disable)] // iterator type
    pub struct SentenceBreakIteratorUtf8<'a>(
        icu_segmenter::iterators::SentenceBreakIterator<'a, 'a, PotentiallyIllFormedUtf8>,
    );

    #[diplomat::opaque_mut]
    #[diplomat::rust_link(icu::segmenter::iterators::SentenceBreakIterator, Struct)]
    #[diplomat::attr(demo_gen, disable)] // iterator type
    pub struct SentenceBreakIteratorUtf16<'a>(
        icu_segmenter::iterators::SentenceBreakIterator<'a, 'a, Utf16>,
    );

    #[diplomat::opaque_mut]
    #[diplomat::rust_link(icu::segmenter::iterators::SentenceBreakIterator, Struct)]
    #[diplomat::attr(demo_gen, disable)] // iterator type
    pub struct SentenceBreakIteratorLatin1<'a>(
        icu_segmenter::iterators::SentenceBreakIterator<'a, 'a, Latin1>,
    );

    impl SentenceSegmenter {
        /// Construct a [`SentenceSegmenter`] using compiled data. This does not assume any content locale.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::options::SentenceBreakInvariantOptions,
            Struct,
            hidden
        )]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<SentenceSegmenter> {
            Box::new(SentenceSegmenter(
                icu_segmenter::SentenceSegmenter::new(Default::default()).static_to_owned(),
            ))
        }
        /// Construct a [`SentenceSegmenter`] for content known to be of a given locale, using compiled data.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::try_new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::segmenter::options::SentenceBreakOptions, Struct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_content_locale")]
        #[cfg(feature = "compiled_data")]
        pub fn create_with_content_locale(
            locale: &Locale,
        ) -> Result<Box<SentenceSegmenter>, DataError> {
            Ok(Box::new(SentenceSegmenter(
                icu_segmenter::SentenceSegmenter::try_new(locale.into())?,
            )))
        }

        /// Construct a [`SentenceSegmenter`]  for content known to be of a given locale, using a particular data source.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::try_new, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_content_locale_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_content_locale_and_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<SentenceSegmenter>, DataError> {
            Ok(Box::new(SentenceSegmenter(
                icu_segmenter::SentenceSegmenter::try_new_with_buffer_provider(
                    provider.get()?,
                    locale.into(),
                )?,
            )))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenterBorrowed::segment_utf8, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::SentenceSegmenterBorrowed::segment_str,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "segment")]
        pub fn segment_utf8<'a>(
            &'a self,
            input: &'a DiplomatStr,
        ) -> Box<SentenceBreakIteratorUtf8<'a>> {
            Box::new(SentenceBreakIteratorUtf8(
                self.0.as_borrowed().segment_utf8(input),
            ))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenterBorrowed::segment_utf16, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), rename = "segment")]
        #[diplomat::attr(supports = utf8_strings, rename = "segment16")]
        pub fn segment_utf16<'a>(
            &'a self,
            input: &'a DiplomatStr16,
        ) -> Box<SentenceBreakIteratorUtf16<'a>> {
            Box::new(SentenceBreakIteratorUtf16(
                self.0.as_borrowed().segment_utf16(input),
            ))
        }

        /// Segments a Latin-1 string.
        #[diplomat::rust_link(
            icu::segmenter::SentenceSegmenterBorrowed::segment_latin1,
            FnInStruct
        )]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        pub fn segment_latin1<'a>(
            &'a self,
            input: &'a [u8],
        ) -> Box<SentenceBreakIteratorLatin1<'a>> {
            Box::new(SentenceBreakIteratorLatin1(
                self.0.as_borrowed().segment_latin1(input),
            ))
        }
    }

    impl<'a> SentenceBreakIteratorUtf8<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::iterators::SentenceBreakIterator::next, FnInStruct)]
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
        #[diplomat::rust_link(icu::segmenter::iterators::SentenceBreakIterator::next, FnInStruct)]
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
        #[diplomat::rust_link(icu::segmenter::iterators::SentenceBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::iterators::SentenceBreakIterator::Item,
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

impl<'a> From<&'a crate::unstable::locale_core::ffi::Locale>
    for icu_segmenter::options::SentenceBreakOptions<'a>
{
    fn from(other: &'a crate::unstable::locale_core::ffi::Locale) -> Self {
        let mut options = icu_segmenter::options::SentenceBreakOptions::default();
        options.content_locale = Some(&other.0.id);
        options
    }
}
