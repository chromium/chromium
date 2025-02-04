// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use core::ops::RangeInclusive;

    /// Result of a single iteration of [`CodePointRangeIterator`].
    /// Logically can be considered to be an `Option<RangeInclusive<DiplomatChar>>`,
    ///
    /// `start` and `end` represent an inclusive range of code points [start, end],
    /// and `done` will be true if the iterator has already finished. The last contentful
    /// iteration will NOT produce a range done=true, in other words `start` and `end` are useful
    /// values if and only if `done=false`.
    #[diplomat::out]
    pub struct CodePointRangeIteratorResult {
        pub start: DiplomatChar,
        pub end: DiplomatChar,
        pub done: bool,
    }

    /// An iterator over code point ranges, produced by `CodePointSetData` or
    /// one of the `CodePointMapData` types
    #[diplomat::opaque]
    pub struct CodePointRangeIterator<'a>(
        pub Box<dyn Iterator<Item = RangeInclusive<DiplomatChar>> + 'a>,
    );

    impl<'a> CodePointRangeIterator<'a> {
        /// Advance the iterator by one and return the next range.
        ///
        /// If the iterator is out of items, `done` will be true
        pub fn next(&mut self) -> CodePointRangeIteratorResult {
            self.0
                .next()
                .map(|r| CodePointRangeIteratorResult {
                    start: *r.start(),
                    end: *r.end(),
                    done: false,
                })
                .unwrap_or(CodePointRangeIteratorResult {
                    start: 0,
                    end: 0,
                    done: true,
                })
        }
    }
}
