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
use core::fmt::Formatter;

/// A type for signaling UTF-8 errors.
#[derive(Debug, PartialEq, Clone, Copy, Eq, Hash)]
#[non_exhaustive]
pub struct Utf8CharsError;

impl core::fmt::Display for Utf8CharsError {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), core::fmt::Error> {
        write!(f, "byte sequence not well-formed UTF-8")
    }
}

impl core::error::Error for Utf8CharsError {}

/// The `Output = Result<char, Utf8CharsError>` case.
#[derive(Debug, Clone)]
pub(crate) struct ErrorReportingHandler;

impl ErrorReportingHandler {
    pub(crate) fn new() -> Self {
        Self {}
    }
}

impl Utf8Handler for ErrorReportingHandler {
    type Output = Result<char, Utf8CharsError>;

    /// Map a single-byte UTF-8 sequence to `Output`.
    ///
    /// When `Output` is `char`, `ascii.to_char()`
    /// is the appropriate implementation.
    ///
    /// The implementation is expected to be marked
    /// `#[inline(always)]`.
    #[inline(always)]
    fn single_byte(&self, ascii: Ascii) -> Self::Output {
        Ok(ascii.to_char())
    }

    /// Map a two-byte UTF-8 sequence to `Output`.
    ///
    /// When `Output` is `char`, `sequence.to_char()`
    /// is the appropriate implementation.
    ///
    /// The implementation is expected to be marked
    /// `#[inline(always)]`.
    #[inline(always)]
    fn two_byte(&self, sequence: TwoByteSequence) -> Self::Output {
        Ok(sequence.to_char())
    }

    /// Map a three-byte UTF-8 sequence to `Output`.
    ///
    /// When `Output` is `char`, `sequence.to_char()`
    /// is the appropriate implementation.
    ///
    /// The implementation is expected to be marked
    /// `#[inline(always)]`.
    #[inline(always)]
    fn three_byte(&self, sequence: ThreeByteSequence) -> Self::Output {
        Ok(sequence.to_char())
    }

    /// Map a four-byte UTF-8 sequence to `Output`.
    ///
    /// When `Output` is `char`, `sequence.to_char()`
    /// is the appropriate implementation.
    ///
    /// The implementation is expected to be marked
    /// `#[inline(always)]`.
    #[inline(always)]
    fn four_byte(&self, sequence: FourByteSequence) -> Self::Output {
        Ok(sequence.to_char())
    }

    /// Map a single UTF-8 error to `Output`.
    ///
    /// What constitutes a single error is defined by the
    /// WHATWG Encoding Standard.
    ///
    /// When `Output` is `char`,
    /// `char::REPLACEMENT_CHARACTER`
    /// is the appropriate implementation.
    ///
    /// The implementation of this method is expected to
    /// be declared `#[inline(always)]`.
    #[inline(always)]
    fn error(&self) -> Self::Output {
        Err(Utf8CharsError)
    }
}

crate::macros::named_iterators_from_no_argument_handler!(
    ErrorReportingHandler,
    Result<char, Utf8CharsError>,
    /// Iterator by `Result<char,Utf8CharsError>` over `&[u8]` that contains
    /// potentially-invalid UTF-8. There is exactly one `Utf8CharsError` per
    /// each error as defined by the WHATWG Encoding Standard.
    ///
    /// ```
    /// let s = b"a\xFFb\xFF\x80c\xF0\x9F\xA4\xA6\xF0\x9F\xA4\xF0\x9F\xF0d";
    /// let plain = utf8_iter::Utf8Chars::new(s);
    /// let reporting = utf8_iter::ErrorReportingUtf8Chars::new(s);
    /// assert!(plain.eq(reporting.map(|r| r.unwrap_or('\u{FFFD}'))));
    /// ```
    ,
    ErrorReportingUtf8Chars,
    /// Iterator by `Result<char,Utf8CharsError>` and their indices over
    /// `&[u8]` that contains potentially-invalid UTF-8. See the crate
    /// documentation.
    ,
    ErrorReportingUtf8CharIndices,
);

#[cfg(test)]
mod tests {
    use crate::ErrorReportingUtf8Chars;

    // Should be a static assert, but not taking a dependency for this.
    #[test]
    fn test_size() {
        assert_eq!(
            core::mem::size_of::<Option<<ErrorReportingUtf8Chars<'_> as Iterator>::Item>>(),
            core::mem::size_of::<Option<char>>()
        );
    }

    #[test]
    fn test_eq() {
        let a: <ErrorReportingUtf8Chars<'_> as Iterator>::Item = Ok('a');
        let a_again: <ErrorReportingUtf8Chars<'_> as Iterator>::Item = Ok('a');
        assert_eq!(a, a_again);
    }
}
