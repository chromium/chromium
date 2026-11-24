// Copyright Mozilla Foundation
//
// Licensed under the Apache License (Version 2.0), or the MIT license,
// (the "Licenses") at your option. You may not use this file except in
// compliance with one of the Licenses. You may obtain copies of the
// Licenses at:
//
//    https://www.apache.org/licenses/LICENSE-2.0
//    https://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Licenses is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the Licenses for the specific language governing permissions and
// limitations under the Licenses.

macro_rules! named_iterators_from_no_argument_handler {
    ($handler:ident,
     $item:ty, // For not having to reference private `$handler` in public `Iterator::Type`
     $(#[$chars_meta:meta])*,
     $chars:ident,
     $(#[$chars_indices_meta:meta])*,
     $char_indices:ident,
    ) => (
        $(#[$chars_meta])*
        #[derive(Debug, Clone)]
        pub struct $chars<'a> {
            inner: crate::Utf16CharsWithHandler<'a, $handler>,
        }

        impl<'a> $chars<'a> {
            #[inline(always)]
            /// Creates the iterator from a slice of UTF-16 code units.
            pub fn new(code_units: &'a [u16]) -> Self {
                Self {
                    inner: crate::Utf16CharsWithHandler::new(code_units, $handler::new()),
                }
            }

            /// Views the current remaining data in the iterator as a subslice
            /// of the original slice.
            #[inline(always)]
            pub fn as_slice(&self) -> &'a [u16] {
                self.inner.as_slice()
            }
        }

        impl<'a> Iterator for $chars<'a> {
            type Item = $item;

            #[inline(always)]
            fn next(&mut self) -> Option<Self::Item> {
                self.inner.next()
            }

            #[inline(always)]
            fn count(self) -> usize {
                self.inner.count()
            }

            #[inline(always)]
            fn size_hint(&self) -> (usize, Option<usize>) {
                self.inner.size_hint()
            }

            #[inline(always)]
            fn last(mut self) -> Option<Self::Item> {
                // No need to go through the entire string.
                self.next_back()
            }
        }

        impl<'a> DoubleEndedIterator for $chars<'a> {
            #[inline(always)]
            fn next_back(&mut self) -> Option<Self::Item> {
                self.inner.next_back()
            }
        }

        impl core::iter::FusedIterator for $chars<'_> {}

        $(#[$chars_indices_meta])*
        #[derive(Debug, Clone)]
        pub struct $char_indices<'a> {
            inner: crate::Utf16CharIndicesWithHandler<'a, $handler>,
        }

        impl<'a> $char_indices<'a> {
            #[inline(always)]
            /// Creates the iterator with indices from a slice of UTF-16 code units.
            pub fn new(code_units: &'a [u16]) -> Self {
                Self {
                    inner: crate::Utf16CharIndicesWithHandler::new(code_units, $handler::new()),
                }
            }

            /// Views the current remaining data in the iterator as a subslice
            /// of the original slice.
            #[inline(always)]
            #[must_use]
            pub fn as_slice(&self) -> &'a [u16] {
                self.inner.as_slice()
            }

            /// The current index offset
            #[inline(always)]
            #[must_use]
            pub fn offset(&self) -> usize {
                self.inner.offset()
            }
        }

        impl<'a> Iterator for $char_indices<'a> {
            type Item = (usize, $item);

            #[inline(always)]
            fn next(&mut self) -> Option<Self::Item> {
                self.inner.next()
            }

            #[inline(always)]
            fn count(self) -> usize {
                self.inner.count()
            }

            #[inline(always)]
            fn size_hint(&self) -> (usize, Option<usize>) {
                self.inner.size_hint()
            }

            #[inline(always)]
            fn last(mut self) -> Option<Self::Item> {
                // No need to go through the entire string.
                self.next_back()
            }
        }

        impl<'a> DoubleEndedIterator for $char_indices<'a> {
            #[inline(always)]
            fn next_back(&mut self) -> Option<Self::Item> {
                self.inner.next_back()
            }
        }

        impl core::iter::FusedIterator for $char_indices<'_> {}

    )
}

pub(crate) use named_iterators_from_no_argument_handler;
