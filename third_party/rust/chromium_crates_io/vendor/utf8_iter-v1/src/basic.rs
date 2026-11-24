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

use utf_types::utf8::Ascii;
use utf_types::utf8::FourByteSequence;
use utf_types::utf8::ThreeByteSequence;
use utf_types::utf8::TwoByteSequence;

use crate::Utf8Handler;

/// The basic `Output = char` case.
///
/// (Normally this is not particularly interesting outside the
/// crate, but exporting this is useful for testing.)
#[derive(Debug, Clone)]
pub struct DefaultHandler;

impl DefaultHandler {
    /// Constructs a new `DefaultHandler`
    pub const fn new() -> Self {
        Self {}
    }
}

impl Default for DefaultHandler {
    fn default() -> Self {
        Self::new()
    }
}

impl Utf8Handler for DefaultHandler {
    type Output = char;

    /// Map a single-byte UTF-8 sequence to `Output`.
    #[inline(always)]
    fn single_byte(&self, ascii: Ascii) -> Self::Output {
        ascii.to_char()
    }

    /// Map a two-byte UTF-8 sequence to `Output`.
    #[inline(always)]
    fn two_byte(&self, sequence: TwoByteSequence) -> Self::Output {
        sequence.to_char()
    }

    /// Map a three-byte UTF-8 sequence to `Output`.
    #[inline(always)]
    fn three_byte(&self, sequence: ThreeByteSequence) -> Self::Output {
        sequence.to_char()
    }

    /// Map a four-byte UTF-8 sequence to `Output`.
    #[inline(always)]
    fn four_byte(&self, sequence: FourByteSequence) -> Self::Output {
        sequence.to_char()
    }

    /// Map a single UTF-8 error to `Output`.
    #[inline(always)]
    fn error(&self) -> Self::Output {
        char::REPLACEMENT_CHARACTER
    }
}

crate::macros::named_iterators_from_no_argument_handler!(
    DefaultHandler,
    char,
    /// Iterator by `char` over `&[u8]` that contains
    /// potentially-invalid UTF-8. See the crate documentation.
    ,
    Utf8Chars,
    /// Iterator by `char` and their indices over `&[u8]` that contains
    /// potentially-invalid UTF-8. See the crate documentation.
    ,
    Utf8CharIndices,
);

/// Convenience trait that adds `chars()` and `char_indices()` methods
/// similar to the ones on string slices to byte slices.
pub trait Utf8CharsEx {
    /// Iterator over `char`s
    fn chars(&self) -> Utf8Chars<'_>;
    /// Iterator over `char`s and their indices
    fn char_indices(&self) -> Utf8CharIndices<'_>;
}

impl Utf8CharsEx for [u8] {
    /// Convenience method for creating an UTF-8 iterator
    /// for the slice.
    #[inline(always)]
    fn chars(&self) -> Utf8Chars<'_> {
        Utf8Chars::new(self)
    }
    /// Convenience method for creating a byte index and
    /// UTF-8 iterator for the slice.
    #[inline(always)]
    fn char_indices(&self) -> Utf8CharIndices<'_> {
        Utf8CharIndices::new(self)
    }
}

// No manually-written tests for forward-iteration, because the code passed multiple
// days of fuzzing comparing with known-good behavior.
