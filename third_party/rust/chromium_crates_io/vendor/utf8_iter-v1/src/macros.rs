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
        #[derive(Debug)]
        pub struct $chars<'a> {
            inner: crate::Utf8CharsWithHandler<'a, $handler>,
        }

        impl<'a> $chars<'a> {
            #[inline(always)]
            /// Creates the iterator from a byte slice.
            pub fn new(bytes: &'a [u8]) -> Self {
                Self {
                    inner: crate::Utf8CharsWithHandler::new(bytes, $handler::new()),
                }
            }

            /// Views the current remaining data in the iterator as a subslice
            /// of the original slice.
            #[must_use]
            #[inline(always)]
            pub fn as_slice(&self) -> &'a [u8] {
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

        impl<'a> Clone for $chars<'a> {
            fn clone(&self) -> Self {
                Self {
                    inner: self.inner.clone(),
                }
            }
        }

        $(#[$chars_indices_meta])*
        #[derive(Debug)]
        pub struct $char_indices<'a> {
            inner: crate::Utf8CharIndicesWithHandler<'a, $handler>,
        }

        impl<'a> $char_indices<'a> {
            #[inline(always)]
            /// Creates the iterator from a byte slice.
            pub fn new(bytes: &'a [u8]) -> Self {
                Self {
                    inner: crate::Utf8CharIndicesWithHandler::new(bytes, $handler::new()),
                }
            }

            /// Views the current remaining data in the iterator as a subslice
            /// of the original slice.
            #[inline(always)]
            #[must_use]
            pub fn as_slice(&self) -> &'a [u8] {
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

        impl<'a> Clone for $char_indices<'a> {
            fn clone(&self) -> Self {
                Self {
                    inner: self.inner.clone(),
                }
            }
        }
    )
}

pub(crate) use named_iterators_from_no_argument_handler;
