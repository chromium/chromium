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

#![allow(unsafe_code)] // Needed to make efficient use of the invariant of `str`

use crate::Utf8Handler;

use utf_types::utf8::Ascii;
use utf_types::utf8::FourByteSequence;
use utf_types::utf8::LeadForFourByte;
use utf_types::utf8::LeadForMultiByte;
use utf_types::utf8::LeadForThreeByte;
use utf_types::utf8::ThreeByteSequence;
use utf_types::utf8::TwoByteSequence;
use utf_types::utf8::UnconstrainedContinuation;

/// Iterator over guaranteed-well-formed UTF-8 with `Utf8Handler`.
/// The `Utf8Handler::error()` is, of course, never called.
#[derive(Debug)]
pub struct CharsWithHandler<'a, H>
where
    H: Utf8Handler,
{
    /// # Safety invariant
    ///
    /// `self.inner.as_slice()` is well-formed UTF-8, except when a method that
    /// advances the `CharsWithHandler` as an iterator ()`next`, `next_back`,
    /// `next_with_minimum_lead`) is in the process of reading a UTF-8 sequence
    /// from `self.inner`.
    ///
    /// Note: For panic-safety, methods that change the state of `self.inner`
    /// must first consume a full UTF-8 sequence (such that the above-stated
    /// invariant holds again) before calling into `self.handler`, so that
    /// the invariant of `self.inner` is upheld even if `self.handler`
    /// panics.
    inner: core::slice::Iter<'a, u8>,
    handler: H,
}

impl<'a, H> CharsWithHandler<'a, H>
where
    H: Utf8Handler,
{
    #[inline(always)]
    /// Creates the iterator from a `&str` and handler.
    pub fn new(s: &'a str, handler: H) -> Self {
        CharsWithHandler::<'a, H> {
            inner: s.as_bytes().iter(),
            handler,
        }
    }

    /// Views the current remaining data in the iterator as a subslice
    /// of the original slice.
    #[inline(always)]
    pub fn as_str(&self) -> &'a str {
        // SAFETY: OK per the safety invariant of `self.inner`.
        unsafe { core::str::from_utf8_unchecked(self.inner.as_slice()) }
    }

    /// Obtains a reference to the handler.
    #[inline(always)]
    pub fn handler(&self) -> &H {
        &self.handler
    }

    /// Obtains the output for the next UTF-8 sequence whose lead byte
    /// is greater than or equal to `multi_byte_lead` ignoring UTF-8
    /// sequences before such a UTF-8 sequence. (Or `None` if the iterator
    /// is consumed without finding such a UTF-8 sequence.)
    #[inline(always)]
    pub fn next_with_minimum_lead(
        &mut self,
        multi_byte_lead: LeadForMultiByte,
    ) -> Option<H::Output> {
        let bound = multi_byte_lead.to_u8();
        loop {
            let byte = *self.inner.next()?;
            if byte < bound {
                continue;
            }
            // SAFETY: From the safety invariant of `self.inner`
            // we know that upon entry into this method `self.inner`
            // represented well-formed UTF-8.
            //
            // We know that well-formed UTF-8 consists of three kinds
            // of bytes:
            // 1. ASCII bytes
            // 2. Continuation bytes
            // 3. Lead bytes for multi-byte sequences.
            //
            // We know that ASCII bytes and continuation bytes are
            // below lead bytes for multi-byte sequences in unsigned
            // integer order.
            //
            // Since `bound` is a lead byte for a multi-byte sequence,
            // we know that ASCII bytes and continuation bytes take
            // the `continue` path above.
            //
            // Therefore, we know that `byte` must now be a lead byte
            // for a multibyte sequence. Since we know that `self.inner`
            // represented well-formed UTF-8 upon entry into this method,
            // we know that `self.inner` will yield the continuation bytes
            // that are appropriate given `byte` as the lead byte for
            // a multibyte sequence.
            //
            // INVARIANT UPHELD for `self.inner`:
            // `consume_continuations` consumes the rest of the
            // multi-byte UTF-8 sequence causing a complete sequence
            // to be consumed.
            return Some(unsafe { self.consume_continuations(byte) });
        }
    }

    /// Consumes continuations given multi-byte lead byte `first`.
    ///
    /// # Safety
    ///
    /// `first` must be a lead byte for a multi-byte UTF-8 sequence and
    /// the concatenation of `first` and `self.inner` must be well-formed
    /// UTF-8.
    ///
    /// # Safety-usable invariant
    ///
    /// This method consumes from `self.inner` the continuation bytes that
    /// belong to the multi-byte sequence started by `first`.
    #[inline(always)]
    unsafe fn consume_continuations(&mut self, first: u8) -> H::Output {
        debug_assert!(LeadForMultiByte::is(first));
        // SAFETY: Since we don't have a single-byte sequence (per the
        // safety invariant of this method) and we know the concatenation
        // of `first` and `self.inner` is well-formed UTF-8, a second
        // byte must exist.
        let second = *unsafe { self.inner.next().unwrap_unchecked() };
        if first < LeadForThreeByte::MIN as u8 {
            // INVARIANT UPHELD for `self.inner`:
            // We consumed a complete UTF-8 sequence; see "SAFETY"
            // below.
            return self.handler.two_byte(
                // SAFETY: From the invariant of this method we know that
                // `first` is a multibyte lead and `second` must be a
                // continuation since the concatenation of `first` and
                // `self.inner` was well-formed UTF-8 upon entry into
                // the method. Since `first` is also less than the
                // smallest lead for a three-byte sequence, it must be
                // a lead for a two-byte sequence. Therefore, we have
                // a two-byte UTF-8 sequence, which satisfies the invariant
                // of `TwoByteSequence::new_unchecked()`.
                unsafe { TwoByteSequence::new_unchecked(first, second) },
            );
        }
        // SAFETY: Per the above two "SAFETY" comments, we don't have
        // a single-byte or a two-byte sequence if we get this far, so
        // a third byte must exist given that the concanation of `first`
        // and `self.inner` was well-formed UTF-8 at the start of this
        // method.
        let third = *unsafe { self.inner.next().unwrap_unchecked() };
        if first < LeadForFourByte::MIN as u8 {
            // INVARIANT UPHELD for `self.inner`:
            // We consumed a complete UTF-8 sequence; see "SAFETY"
            // below.
            return self.handler.three_byte(unsafe {
                // SAFETY: From the safety invariant of this method
                // we know that the concatenation of `first` and
                // `self.inner` was well-formed UTF-8 at the start of
                // this method. The safety invariant of this method
                // rules out a single-byte sequence and we have already
                // checked for and ruled out a two-byte sequence. Most
                // recently, we ruled out a four-byte sequence. Therefore,
                // we must have a three-byte sequence, which satisfies
                // the safety invariant of `ThreeByteSequence::new_unchecked()`.
                ThreeByteSequence::new_unchecked(first, second, third)
            });
        }
        // SAFETY: Since we don't have a single-byte sequence, a two-byte
        // sequence, or a three-byte sequence (per above) and we know
        // from the safety invariant of this method that the concatenation
        // of `first` and `self.inner` was well-formed UTF-8 upon entry
        // into this method. Therefore, a fourth byte must exist.
        let fourth = *unsafe { self.inner.next().unwrap_unchecked() };
        // INVARIANT UPHELD for `self.inner`:
        // We consumed a complete UTF-8 sequence; see "SAFETY"
        // below.
        self.handler.four_byte(
            // SAFETY: From the safety invariant of this method
            // we know that the concatenation of `first` and
            // `self.inner` was well-formed UTF-8 at the start of
            // this method. The safety invariant of this method
            // rules out a single-byte sequence and we have already
            // checked for and ruled out a two-byte and three-byte
            // sequences. Therefore, we must have a four-byte
            // sequence, which satisfies the safety invariant of
            // `FourByteSequence::new_unchecked()`.
            unsafe { FourByteSequence::new_unchecked(first, second, third, fourth) },
        )
    }
}

impl<'a, H> Clone for CharsWithHandler<'a, H>
where
    H: Utf8Handler + Clone,
{
    fn clone(&self) -> Self {
        Self {
            inner: self.inner.clone(),
            handler: self.handler.clone(),
        }
    }
}

impl<'a, H> Iterator for CharsWithHandler<'a, H>
where
    H: Utf8Handler,
{
    type Item = H::Output;

    #[inline(always)]
    fn next(&mut self) -> Option<H::Output> {
        let first = *self.inner.next()?;
        if let Ok(ascii) = Ascii::try_new(first) {
            return Some(self.handler.single_byte(ascii));
        }
        // SAFETY: Since `self.inner` represented well-formed UTF-8
        // upon entry into this method and `first` is not ASCII,
        // first must be a valid lead byte for a multi-byte UTF-8
        // sequence.
        //
        // INVARIANT UPHELD for `self.inner`:
        // `consume_continuations` consumes the rest of the
        // multi-byte UTF-8 sequence causing a complete sequence
        // to be consumed.
        Some(unsafe { self.consume_continuations(first) })
    }

    #[inline(always)]
    fn count(self) -> usize {
        self.as_str().chars().count()
    }

    #[inline(always)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.as_str().len();
        (len.div_ceil(4), Some(len))
    }

    #[inline(always)]
    fn last(mut self) -> Option<Self::Item> {
        // No need to go through the entire string.
        self.next_back()
    }
}

impl<'a, H> DoubleEndedIterator for CharsWithHandler<'a, H>
where
    H: Utf8Handler,
{
    #[inline(always)]
    fn next_back(&mut self) -> Option<H::Output> {
        let last = *self.inner.next_back()?;
        if let Ok(ascii) = Ascii::try_new(last) {
            return Some(self.handler.single_byte(ascii));
        }
        debug_assert!(UnconstrainedContinuation::is(last));
        // SAFETY: Since the last byte was not a single-byte sequence
        // given that `self.inner` represented well-formed UTF-8 upon
        // entry into this method, an earlier byte must exist.
        let second_last = *unsafe { self.inner.next_back().unwrap_unchecked() };
        if !UnconstrainedContinuation::is(second_last) {
            // INVARIANT UPHELD for `self.inner`:
            // We consumed a complete UTF-8 sequence; see "SAFETY" below.
            return Some(self.handler.two_byte(
                // SAFETY: Given that `self.inner` represented well-formed
                // UTF-8 upon entry into this method and that `last` is a continuation
                // but `second_last` is not, they must form a two-byte UTF-8
                // sequence, which satisfies the safety invariant of
                // `TwoByteSequence::new_unchecked()`.
                unsafe { TwoByteSequence::new_unchecked(second_last, last) },
            ));
        }
        // SAFETY: Given that `self.inner` represented well-formed
        // UTF-8 upon entry into this method and we've ruled out a
        // single-byte sequence and a two-byte sequence, an earlier
        // byte must exist.
        let third_last = *unsafe { self.inner.next_back().unwrap_unchecked() };
        if !UnconstrainedContinuation::is(third_last) {
            // INVARIANT UPHELD for `self.inner`:
            // We consumed a complete UTF-8 sequence; see "SAFETY" below.
            return Some(self.handler.three_byte(
                // SAFETY: Given that `self.inner` represented well-formed
                // UTF-8 upon entry into this method and that `last` and `second_last`
                // are continuations but `third_last` is not, they must form a three-byte UTF-8
                // sequence, which satisfies the safety invariant of `ThreeByteSequence::new_unchecked()`.
                unsafe { ThreeByteSequence::new_unchecked(third_last, second_last, last) },
            ));
        }
        // SAFETY: Given that `self.inner` represented well-formed
        // UTF-8 upon entry into this method and we've ruled out a
        // single-byte sequence, a two-byte sequence, and a three-byte
        // sequences, an earlier byte must exist.
        let fourth_last = *unsafe { self.inner.next_back().unwrap_unchecked() };
        // INVARIANT UPHELD for `self.inner`:
        // We consumed a complete UTF-8 sequence; see "SAFETY" below.
        Some(self.handler.four_byte(
            // SAFETY: Given that `self.inner` represented well-formed
            // UTF-8 upon entry into this method and we've ruled out a
            // single-byte sequence, a two-byte sequence, and a three-byte
            // sequences, this must be a four-byte sequence, which satisfies
            // the safety invariant of `FourByteSequence::new_unchecked()`.
            unsafe { FourByteSequence::new_unchecked(fourth_last, third_last, second_last, last) },
        ))
    }
}

impl<H> core::iter::FusedIterator for CharsWithHandler<'_, H> where H: Utf8Handler {}
