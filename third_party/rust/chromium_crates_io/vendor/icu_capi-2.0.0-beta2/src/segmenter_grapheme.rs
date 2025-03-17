// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::{errors::ffi::DataError, provider::ffi::DataProvider};

    #[diplomat::opaque]
    /// An ICU4X grapheme-cluster-break segmenter, capable of finding grapheme cluster breakpoints
    /// in strings.
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter, Struct)]
    pub struct GraphemeClusterSegmenter(icu_segmenter::GraphemeClusterSegmenter);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator, Struct)]
    #[diplomat::rust_link(
        icu::segmenter::GraphemeClusterBreakIteratorPotentiallyIllFormedUtf8,
        Typedef,
        hidden
    )]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIteratorUtf8, Typedef, hidden)]
    pub struct GraphemeClusterBreakIteratorUtf8<'a>(
        icu_segmenter::GraphemeClusterBreakIteratorPotentiallyIllFormedUtf8<'a, 'a>,
    );

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIteratorUtf16, Typedef, hidden)]
    pub struct GraphemeClusterBreakIteratorUtf16<'a>(
        icu_segmenter::GraphemeClusterBreakIteratorUtf16<'a, 'a>,
    );

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIteratorLatin1, Typedef, hidden)]
    pub struct GraphemeClusterBreakIteratorLatin1<'a>(
        icu_segmenter::GraphemeClusterBreakIteratorLatin1<'a, 'a>,
    );

    impl GraphemeClusterSegmenter {
        /// Construct an [`GraphemeClusterSegmenter`] using compiled data.
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter::new, FnInStruct)]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<GraphemeClusterSegmenter> {
            Box::new(GraphemeClusterSegmenter(
                icu_segmenter::GraphemeClusterSegmenter::new(),
            ))
        }
        /// Construct an [`GraphemeClusterSegmenter`].
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter::new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<GraphemeClusterSegmenter>, DataError> {
            Ok(Box::new(GraphemeClusterSegmenter(
                icu_segmenter::GraphemeClusterSegmenter::try_new_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }
        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(
            icu::segmenter::GraphemeClusterSegmenter::segment_str,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter::segment_utf8, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "segment")]
        pub fn segment_utf8<'a>(
            &'a self,
            input: &'a DiplomatStr,
        ) -> Box<GraphemeClusterBreakIteratorUtf8<'a>> {
            Box::new(GraphemeClusterBreakIteratorUtf8(self.0.segment_utf8(input)))
        }

        /// Segments a string.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter::segment_utf16, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), rename = "segment")]
        #[diplomat::attr(supports = utf8_strings, rename = "segment16")]
        pub fn segment_utf16<'a>(
            &'a self,
            input: &'a DiplomatStr16,
        ) -> Box<GraphemeClusterBreakIteratorUtf16<'a>> {
            Box::new(GraphemeClusterBreakIteratorUtf16(
                self.0.segment_utf16(input),
            ))
        }

        /// Segments a Latin-1 string.
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter::segment_latin1, FnInStruct)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        pub fn segment_latin1<'a>(
            &'a self,
            input: &'a [u8],
        ) -> Box<GraphemeClusterBreakIteratorLatin1<'a>> {
            Box::new(GraphemeClusterBreakIteratorLatin1(
                self.0.segment_latin1(input),
            ))
        }
    }

    impl<'a> GraphemeClusterBreakIteratorUtf8<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::GraphemeClusterBreakIterator::Item,
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

    impl<'a> GraphemeClusterBreakIteratorUtf16<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::GraphemeClusterBreakIterator::Item,
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

    impl<'a> GraphemeClusterBreakIteratorLatin1<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::GraphemeClusterBreakIterator::Item,
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
