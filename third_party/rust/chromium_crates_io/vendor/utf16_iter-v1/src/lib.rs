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

#![no_std]
#![warn(missing_docs)]
#![forbid(unsafe_code)]

//! Provides iteration by `char` over `&[u16]` containing potentially-invalid
//! UTF-16 such that errors are replaced with the REPLACEMENT CHARACTER.
//!
//! The trait `Utf16CharsEx` provides the convenience method `chars()` on
//! `u16` slices themselves instead of having to use the more verbose
//! `Utf16Chars::new(slice)`.

mod basic;
mod handler;
mod indices;
mod macros;
mod report;

pub use crate::basic::Utf16CharIndices;
pub use crate::basic::Utf16Chars;
pub use crate::basic::Utf16CharsEx;
pub use crate::handler::Utf16CharsWithHandler;
pub use crate::handler::Utf16Handler;
pub use crate::indices::Utf16CharIndicesWithHandler;
pub use crate::report::ErrorReportingUtf16CharIndices;
pub use crate::report::ErrorReportingUtf16Chars;
pub use crate::report::Utf16CharsError;

#[cfg(test)]
mod tests {
    use crate::Utf16CharsEx;

    #[test]
    fn test_boundaries() {
        assert!([0xD7FFu16]
            .as_slice()
            .chars()
            .eq(core::iter::once('\u{D7FF}')));
        assert!([0xE000u16]
            .as_slice()
            .chars()
            .eq(core::iter::once('\u{E000}')));
        assert!([0xD800u16]
            .as_slice()
            .chars()
            .eq(core::iter::once('\u{FFFD}')));
        assert!([0xDFFFu16]
            .as_slice()
            .chars()
            .eq(core::iter::once('\u{FFFD}')));
    }

    #[test]
    fn test_unpaired() {
        assert!([0xD800u16, 0x0061u16]
            .as_slice()
            .chars()
            .eq([0xFFFDu16, 0x0061u16].as_slice().chars()));
        assert!([0xDFFFu16, 0x0061u16]
            .as_slice()
            .chars()
            .eq([0xFFFDu16, 0x0061u16].as_slice().chars()));
    }

    #[test]
    fn test_unpaired_rev() {
        assert!([0xD800u16, 0x0061u16]
            .as_slice()
            .chars()
            .rev()
            .eq([0xFFFDu16, 0x0061u16].as_slice().chars().rev()));
        assert!([0xDFFFu16, 0x0061u16]
            .as_slice()
            .chars()
            .rev()
            .eq([0xFFFDu16, 0x0061u16].as_slice().chars().rev()));
    }

    #[test]
    fn test_paired() {
        assert!([0xD83Eu16, 0xDD73u16]
            .as_slice()
            .chars()
            .eq(core::iter::once('🥳')));
    }

    #[test]
    fn test_paired_rev() {
        assert!([0xD83Eu16, 0xDD73u16]
            .as_slice()
            .chars()
            .rev()
            .eq(core::iter::once('🥳')));
    }

    #[test]
    fn test_as_slice() {
        let mut iter = [0x0061u16, 0x0062u16].as_slice().chars();
        let at_start = iter.as_slice();
        assert_eq!(iter.next(), Some('a'));
        let in_middle = iter.as_slice();
        assert_eq!(iter.next(), Some('b'));
        let at_end = iter.as_slice();
        assert_eq!(at_start.len(), 2);
        assert_eq!(in_middle.len(), 1);
        assert_eq!(at_end.len(), 0);
        assert_eq!(at_start[0], 0x0061u16);
        assert_eq!(at_start[1], 0x0062u16);
        assert_eq!(in_middle[0], 0x0062u16);
    }
}
