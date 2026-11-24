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

use utf_types::utf16::Bmp;
use utf_types::utf16::SurrogatePair;

use crate::Utf16Handler;

/// The basic `Output = char` case.
#[derive(Debug, Clone)]
pub(crate) struct DefaultHandler;

impl DefaultHandler {
    pub(crate) fn new() -> Self {
        Self {}
    }
}

impl Utf16Handler for DefaultHandler {
    type Output = char;

    /// Map a single-code-unit UTF-16 sequence to `Output`.
    #[inline(always)]
    fn bmp(&self, bmp: Bmp) -> Self::Output {
        bmp.to_char()
    }

    /// Map a two-code-unit UTF-16 sequence to `Output`.
    #[inline(always)]
    fn surrogate_pair(&self, surrogate_pair: SurrogatePair) -> Self::Output {
        surrogate_pair.to_char()
    }
}

crate::macros::named_iterators_from_no_argument_handler!(
    DefaultHandler,
    char,
    /// Iterator by `char` over `&[u16]` that contains
    /// potentially-invalid UTF-16. See the crate documentation.
    ,
    Utf16Chars,
    /// Iterator by `char` and their indices over `&[u16]` that contains
    /// potentially-invalid UTF-16. See the crate documentation.
    ,
    Utf16CharIndices,
);

/// Convenience trait that adds `chars()` and `char_indices()` methods
/// similar to the ones on string slices to `u16` slices.
pub trait Utf16CharsEx {
    /// Iterator over `char`s
    fn chars(&self) -> Utf16Chars<'_>;
    /// Iterator over `char`s and their indices
    fn char_indices(&self) -> Utf16CharIndices<'_>;
}

impl Utf16CharsEx for [u16] {
    /// Convenience method for creating an UTF-16 iterator
    /// for the slice.
    #[inline(always)]
    fn chars(&self) -> Utf16Chars<'_> {
        Utf16Chars::new(self)
    }
    /// Convenience method for creating a code unit index and
    /// UTF-16 iterator for the slice.
    #[inline(always)]
    fn char_indices(&self) -> Utf16CharIndices<'_> {
        Utf16CharIndices::new(self)
    }
}
