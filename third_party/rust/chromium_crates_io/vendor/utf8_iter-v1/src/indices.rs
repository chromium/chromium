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

use crate::CharsWithHandler;
use crate::Utf8CharsWithHandler;
use crate::Utf8Handler;
use core::iter::FusedIterator;

/// An iterator over the [`char`]s  and their positions.
#[derive(Debug)]
#[must_use = "iterators are lazy and do nothing unless consumed"]
pub struct Utf8CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler,
{
    front_offset: usize,
    iter: Utf8CharsWithHandler<'a, H>,
}

impl<'a, H> Iterator for Utf8CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler,
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

impl<'a, H> DoubleEndedIterator for Utf8CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler,
{
    #[inline(always)]
    fn next_back(&mut self) -> Option<(usize, H::Output)> {
        self.iter.next_back().map(|ch| {
            let index = self.front_offset + self.as_slice().len();
            (index, ch)
        })
    }
}

impl<H> FusedIterator for Utf8CharIndicesWithHandler<'_, H> where H: Utf8Handler {}

impl<'a, H> Clone for Utf8CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler + Clone,
{
    fn clone(&self) -> Self {
        Self {
            front_offset: self.front_offset,
            iter: self.iter.clone(),
        }
    }
}

impl<'a, H> Utf8CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler,
{
    #[inline(always)]
    /// Creates the iterator from a byte slice and handler.
    pub fn new(bytes: &'a [u8], handler: H) -> Self {
        Utf8CharIndicesWithHandler::<'a> {
            front_offset: 0,
            iter: Utf8CharsWithHandler::new(bytes, handler),
        }
    }

    /// Views the underlying data as a subslice of the original data.
    ///
    /// This has the same lifetime as the original slice, and so the
    /// iterator can continue to be used while this exists.
    #[must_use]
    #[inline(always)]
    pub fn as_slice(&self) -> &'a [u8] {
        self.iter.as_slice()
    }

    /// Obtains a reference to the handler.
    #[inline(always)]
    pub fn handler(&self) -> &H {
        self.iter.handler()
    }

    /// Returns the byte position of the next character, or the length
    /// of the underlying string if there are no more characters.
    #[inline(always)]
    #[must_use]
    pub fn offset(&self) -> usize {
        self.front_offset
    }
}

// ---

/// An iterator over the [`char`]s  and their positions.
#[derive(Debug)]
#[must_use = "iterators are lazy and do nothing unless consumed"]
pub struct CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler,
{
    front_offset: usize,
    iter: CharsWithHandler<'a, H>,
}

impl<'a, H> Iterator for CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler,
{
    type Item = (usize, H::Output);

    #[inline(always)]
    fn next(&mut self) -> Option<(usize, H::Output)> {
        let pre_len = self.as_str().len();
        match self.iter.next() {
            None => None,
            Some(ch) => {
                let index = self.front_offset;
                let len = self.as_str().len();
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

impl<'a, H> DoubleEndedIterator for CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler,
{
    #[inline(always)]
    fn next_back(&mut self) -> Option<(usize, H::Output)> {
        self.iter.next_back().map(|ch| {
            let index = self.front_offset + self.as_str().len();
            (index, ch)
        })
    }
}

impl<H> FusedIterator for CharIndicesWithHandler<'_, H> where H: Utf8Handler {}

impl<'a, H> Clone for CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler + Clone,
{
    fn clone(&self) -> Self {
        Self {
            front_offset: self.front_offset,
            iter: self.iter.clone(),
        }
    }
}

impl<'a, H> CharIndicesWithHandler<'a, H>
where
    H: Utf8Handler,
{
    #[inline(always)]
    /// Creates the iterator from a `&str` and handler.
    pub fn new(s: &'a str, handler: H) -> Self {
        CharIndicesWithHandler::<'a> {
            front_offset: 0,
            iter: CharsWithHandler::new(s, handler),
        }
    }

    /// Views the underlying data as a subslice of the original data.
    ///
    /// This has the same lifetime as the original slice, and so the
    /// iterator can continue to be used while this exists.
    #[must_use]
    #[inline(always)]
    pub fn as_str(&self) -> &'a str {
        self.iter.as_str()
    }

    /// Obtains a reference to the handler.
    #[inline(always)]
    pub fn handler(&self) -> &H {
        self.iter.handler()
    }

    /// Returns the byte position of the next character, or the length
    /// of the underlying string if there are no more characters.
    #[inline(always)]
    #[must_use]
    pub fn offset(&self) -> usize {
        self.front_offset
    }
}
