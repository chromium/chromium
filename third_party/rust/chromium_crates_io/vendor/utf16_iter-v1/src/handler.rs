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

//! UTF-16 decoding with pluggable handler for the two code unit sequence
//! lengths and error.
//!
//! The handler implementation is expected not to mutate any state.
//! Its primary purpose is to allow read-only data structure lookups
//! to be fused into the decoder when the data structure layout benefits
//! from knowing BMP vs. non-BMP detail without having to re-discover it
//! from `char`.

use core::slice::Iter;
use utf_types::utf16::Bmp;
use utf_types::utf16::BmpOrSurrogate;
use utf_types::utf16::HighSurrogate;
use utf_types::utf16::LowSurrogate;
use utf_types::utf16::Surrogate;
use utf_types::utf16::SurrogatePair;

const REPLACEMENT_CHARACTER_AS_BMP: Bmp = const {
    if let Ok(bmp) = Bmp::try_new(char::REPLACEMENT_CHARACTER as u16) {
        bmp
    } else {
        panic!()
    }
};

/// Mapping from the two kinds of code unit sequences or error
/// to output.
pub trait Utf16Handler {
    /// The per-scalar-value output type. (In the common case,
    /// this is `char`.)
    type Output;

    /// Map a single-code-unit UTF-16 sequence to `Output`.
    ///
    /// When `Output` is `char`, `bmp.to_char()`
    /// is the appropriate implementation.
    ///
    /// The implementation of this method is expected to be
    /// declared `#[inline(always)]`.
    fn bmp(&self, bmp: Bmp) -> Self::Output;

    /// Map a two-code-unit UTF-16 sequence to `Output`.
    ///
    /// When `Output` is `char`, `surrogate_pair.to_char()`
    /// is the appropriate implementation.
    ///
    /// The implementation of this method is expected to be
    /// declared `#[inline(always)]`.
    fn surrogate_pair(&self, surrogate_pair: SurrogatePair) -> Self::Output;

    /// Map a single UTF-16 error (one unpaired surrogate) to `Output`.
    ///
    /// When `Output` is `char`,
    /// `char::REPLACEMENT_CHARACTER`
    /// is the appropriate implementation. The provided
    /// implementation delegates to `bmp` by passing
    /// the value corresponding to the REPLACEMENT CHARACTER.
    ///
    /// The implementation of this method is expected to
    /// be declared `#[inline(always)]`.
    #[inline(always)]
    fn error(&self) -> Self::Output {
        self.bmp(REPLACEMENT_CHARACTER_AS_BMP)
    }
}

/// Iterator by `char` over `&[u16]` that contains
/// potentially-invalid UTF-16. See the crate documentation.
#[derive(Debug, Clone)]
pub struct Utf16CharsWithHandler<'a, H>
where
    H: Utf16Handler,
{
    iter: Iter<'a, u16>,
    handler: H,
}

impl<'a, H> Utf16CharsWithHandler<'a, H>
where
    H: Utf16Handler,
{
    #[inline(always)]
    /// Creates the iterator from a `u16` slice.
    pub fn new(code_units: &'a [u16], handler: H) -> Self {
        Utf16CharsWithHandler::<'a, H> {
            iter: code_units.iter(),
            handler,
        }
    }

    /// Views the current remaining data in the iterator as a subslice
    /// of the original slice.
    #[inline(always)]
    pub fn as_slice(&self) -> &'a [u16] {
        self.iter.as_slice()
    }

    /// Obtains a reference to the handler.
    #[inline(always)]
    pub fn handler(&self) -> &H {
        &self.handler
    }

    #[cold]
    fn error(&self) -> H::Output {
        self.handler.error()
    }

    #[cold]
    #[inline(never)]
    fn surrogate_next(&mut self, surrogate: Surrogate) -> H::Output {
        if let Ok(high) = HighSurrogate::try_new_with_surrogate(surrogate) {
            let mut cloned_iter = self.iter.clone();
            if let Some(&u) = cloned_iter.next() {
                if let Ok(low) = LowSurrogate::try_new(u) {
                    self.iter = cloned_iter;
                    return self.handler.surrogate_pair(SurrogatePair::new(high, low));
                }
            }
        }
        self.error()
    }

    #[cold]
    #[inline(never)]
    fn surrogate_next_back(&mut self, surrogate: Surrogate) -> H::Output {
        if let Ok(low) = LowSurrogate::try_new_with_surrogate(surrogate) {
            let mut cloned_iter = self.iter.clone();
            if let Some(&u) = cloned_iter.next_back() {
                if let Ok(high) = HighSurrogate::try_new(u) {
                    self.iter = cloned_iter;
                    return self.handler.surrogate_pair(SurrogatePair::new(high, low));
                }
            }
        }
        self.error()
    }
}

impl<'a, H> Iterator for Utf16CharsWithHandler<'a, H>
where
    H: Utf16Handler,
{
    type Item = H::Output;

    #[inline(always)]
    fn next(&mut self) -> Option<H::Output> {
        match BmpOrSurrogate::new(*self.iter.next()?) {
            BmpOrSurrogate::Bmp(bmp) => Some(self.handler.bmp(bmp)),
            BmpOrSurrogate::Surrogate(surrogate) => Some(self.surrogate_next(surrogate)),
        }
    }

    // TODO: Optimize `count` without a circular dependency

    #[inline(always)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.as_slice().len();
        (len.div_ceil(2), Some(len))
    }

    #[inline(always)]
    fn last(mut self) -> Option<Self::Item> {
        // No need to go through the entire string.
        self.next_back()
    }
}

impl<'a, H> DoubleEndedIterator for Utf16CharsWithHandler<'a, H>
where
    H: Utf16Handler,
{
    #[inline(always)]
    fn next_back(&mut self) -> Option<H::Output> {
        match BmpOrSurrogate::new(*self.iter.next_back()?) {
            BmpOrSurrogate::Bmp(bmp) => Some(self.handler.bmp(bmp)),
            BmpOrSurrogate::Surrogate(surrogate) => Some(self.surrogate_next_back(surrogate)),
        }
    }
}

impl<H> core::iter::FusedIterator for Utf16CharsWithHandler<'_, H> where H: Utf16Handler {}
