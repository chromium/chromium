// The code in this file was adapted from the CharIndices implementation of
// the Rust standard library at revision ab32548539ec38a939c1b58599249f3b54130026
// (https://github.com/rust-lang/rust/blob/ab32548539ec38a939c1b58599249f3b54130026/library/core/src/str/iter.rs).
//
// Excerpt from https://github.com/rust-lang/rust/blob/ab32548539ec38a939c1b58599249f3b54130026/COPYRIGHT ,
// which refers to
// https://github.com/rust-lang/rust/blob/ab32548539ec38a939c1b58599249f3b54130026/LICENSE-APACHE
// and
// https://github.com/rust-lang/rust/blob/ab32548539ec38a939c1b58599249f3b54130026/LICENSE-MIT
// :
//
// For full authorship information, see the version control history or
// https://thanks.rust-lang.org
//
// Except as otherwise noted (below and/or in individual files), Rust is
// licensed under the Apache License, Version 2.0 <LICENSE-APACHE> or
// <http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT> or <http://opensource.org/licenses/MIT>, at your option.

use crate::Utf16CharsWithHandler;
use crate::Utf16Handler;
use core::iter::FusedIterator;

/// An iterator over the [`char`]s  and their positions.
#[derive(Clone, Debug)]
#[must_use = "iterators are lazy and do nothing unless consumed"]
pub struct Utf16CharIndicesWithHandler<'a, H>
where
    H: Utf16Handler,
{
    front_offset: usize,
    iter: Utf16CharsWithHandler<'a, H>,
}

impl<'a, H> Iterator for Utf16CharIndicesWithHandler<'a, H>
where
    H: Utf16Handler,
{
    type Item = (usize, H::Output);

    #[inline(always)]
    fn next(&mut self) -> Option<(usize, H::Output)> {
        let pre_len = self.as_slice().len();
        match self.iter.next() {
            None => None,
            Some(ch) => {
                let index = self.front_offset;
                let len = self.as_slice().len();
                self.front_offset += pre_len - len;
                Some((index, ch))
            }
        }
    }

    #[inline(always)]
    fn count(self) -> usize {
        self.iter.count()
    }

    #[inline(always)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }

    #[inline(always)]
    fn last(mut self) -> Option<(usize, H::Output)> {
        // No need to go through the entire string.
        self.next_back()
    }
}

impl<'a, H> DoubleEndedIterator for Utf16CharIndicesWithHandler<'a, H>
where
    H: Utf16Handler,
{
    #[inline(always)]
    fn next_back(&mut self) -> Option<(usize, H::Output)> {
        self.iter.next_back().map(|ch| {
            let index = self.front_offset + self.as_slice().len();
            (index, ch)
        })
    }
}

impl<H> FusedIterator for Utf16CharIndicesWithHandler<'_, H> where H: Utf16Handler {}

impl<'a, H> Utf16CharIndicesWithHandler<'a, H>
where
    H: Utf16Handler,
{
    #[inline(always)]
    /// Creates the iterator from a `u16` slice.
    pub fn new(code_units: &'a [u16], handler: H) -> Self {
        Utf16CharIndicesWithHandler::<'a, H> {
            front_offset: 0,
            iter: Utf16CharsWithHandler::new(code_units, handler),
        }
    }

    /// Obtains a reference to the handler.
    #[inline(always)]
    pub fn handler(&self) -> &H {
        self.iter.handler()
    }

    /// Views the underlying data as a subslice of the original data.
    ///
    /// This has the same lifetime as the original slice, and so the
    /// iterator can continue to be used while this exists.
    #[must_use]
    #[inline(always)]
    pub fn as_slice(&self) -> &'a [u16] {
        self.iter.as_slice()
    }

    /// Returns the code unit position of the next character, or the length
    /// of the underlying string if there are no more characters.
    ///
    /// # Examples
    ///
    /// ```
    /// use utf16_iter::Utf16CharsEx;
    /// let mut chars = [0xD83Eu16, 0xDD73u16, 0x697Du16].char_indices();
    ///
    /// assert_eq!(chars.offset(), 0);
    /// assert_eq!(chars.next(), Some((0, '🥳')));
    ///
    /// assert_eq!(chars.offset(), 2);
    /// assert_eq!(chars.next(), Some((2, '楽')));
    ///
    /// assert_eq!(chars.offset(), 3);
    /// assert_eq!(chars.next(), None);
    /// ```
    #[inline(always)]
    #[must_use]
    pub fn offset(&self) -> usize {
        self.front_offset
    }
}
