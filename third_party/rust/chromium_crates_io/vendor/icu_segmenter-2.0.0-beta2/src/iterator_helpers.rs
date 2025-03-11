// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Macros and utilities to help implement the various iterator types.

macro_rules! derive_usize_iterator_with_type {
    ($ty:tt) => {
        impl<'l, 's, Y: RuleBreakType<'l, 's> + ?Sized> Iterator for $ty<'l, 's, Y> {
            type Item = usize;
            #[inline]
            fn next(&mut self) -> Option<Self::Item> {
                self.0.next()
            }
        }
    };
}

pub(crate) use derive_usize_iterator_with_type;
