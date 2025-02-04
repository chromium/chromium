// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

pub mod errors;
mod langid;
mod locale;

pub use errors::ParseError;
pub use langid::{
    parse_language_identifier, parse_language_identifier_from_iter,
    parse_language_identifier_with_single_variant,
    parse_locale_with_single_variant_single_keyword_unicode_extension_from_iter, ParserMode,
};

pub use locale::{
    parse_locale, parse_locale_with_single_variant_single_keyword_unicode_keyword_extension,
};

const fn skip_before_separator(slice: &[u8]) -> &[u8] {
    let mut end = 0;

    #[allow(clippy::indexing_slicing)] // very protected, should optimize out
    while end < slice.len() && !matches!(slice[end], b'-' | b'_') {
        // Advance until we reach end of slice or a separator.
        end += 1;
    }

    // Notice: this slice may be empty for cases like `"en-"` or `"en--US"`
    // MSRV 1.71/1.79: Use split_at/split_at_unchecked
    // SAFETY: end < slice.len() by while loop
    unsafe { core::slice::from_raw_parts(slice.as_ptr(), end) }
}

// `SubtagIterator` is a helper iterator for [`LanguageIdentifier`] and [`Locale`] parsing.
//
// It is quite extraordinary due to focus on performance and Rust limitations for `const`
// functions.
//
// The iterator is eager and fallible allowing it to reject invalid slices such as `"-"`, `"-en"`,
// `"en-"` etc.
//
// The iterator provides methods available for static users - `next_manual` and `peek_manual`,
// as well as typical `Peekable` iterator APIs - `next` and `peek`.
//
// All methods return an `Option` of a `Result`.
#[derive(Copy, Clone, Debug)]
pub struct SubtagIterator<'a> {
    remaining: &'a [u8],
    // current is a prefix of remaining
    current: Option<&'a [u8]>,
}

impl<'a> SubtagIterator<'a> {
    pub const fn new(rest: &'a [u8]) -> Self {
        Self {
            remaining: rest,
            current: Some(skip_before_separator(rest)),
        }
    }

    pub const fn next_const(mut self) -> (Self, Option<&'a [u8]>) {
        let Some(result) = self.current else {
            return (self, None);
        };

        self.current = if result.len() < self.remaining.len() {
            // If there is more after `result`, by construction `current` starts with a separator
            // MSRV 1.79: Use split_at_unchecked
            // SAFETY: `self.remaining` is strictly longer than `result`
            self.remaining = unsafe {
                core::slice::from_raw_parts(
                    self.remaining.as_ptr().add(result.len() + 1),
                    self.remaining.len() - (result.len() + 1),
                )
            };
            Some(skip_before_separator(self.remaining))
        } else {
            None
        };
        (self, Some(result))
    }

    pub const fn peek(&self) -> Option<&'a [u8]> {
        self.current
    }
}

impl<'a> Iterator for SubtagIterator<'a> {
    type Item = &'a [u8];

    fn next(&mut self) -> Option<Self::Item> {
        let (s, res) = self.next_const();
        *self = s;
        res
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn slice_to_str(input: &[u8]) -> &str {
        std::str::from_utf8(input).unwrap()
    }

    #[test]
    fn subtag_iterator_peek_test() {
        let slice = "de_at-u-ca-foobar";
        let mut si = SubtagIterator::new(slice.as_bytes());

        assert_eq!(si.peek().map(slice_to_str), Some("de"));
        assert_eq!(si.peek().map(slice_to_str), Some("de"));
        assert_eq!(si.next().map(slice_to_str), Some("de"));

        assert_eq!(si.peek().map(slice_to_str), Some("at"));
        assert_eq!(si.peek().map(slice_to_str), Some("at"));
        assert_eq!(si.next().map(slice_to_str), Some("at"));
    }

    #[test]
    fn subtag_iterator_test() {
        let slice = "";
        let mut si = SubtagIterator::new(slice.as_bytes());
        assert_eq!(si.next().map(slice_to_str), Some(""));

        let slice = "-";
        let mut si = SubtagIterator::new(slice.as_bytes());
        assert_eq!(si.next().map(slice_to_str), Some(""));

        let slice = "-en";
        let mut si = SubtagIterator::new(slice.as_bytes());
        assert_eq!(si.next().map(slice_to_str), Some(""));
        assert_eq!(si.next().map(slice_to_str), Some("en"));
        assert_eq!(si.next(), None);

        let slice = "en";
        let si = SubtagIterator::new(slice.as_bytes());
        assert_eq!(si.map(slice_to_str).collect::<Vec<_>>(), vec!["en",]);

        let slice = "en-";
        let si = SubtagIterator::new(slice.as_bytes());
        assert_eq!(si.map(slice_to_str).collect::<Vec<_>>(), vec!["en", "",]);

        let slice = "--";
        let mut si = SubtagIterator::new(slice.as_bytes());
        assert_eq!(si.next().map(slice_to_str), Some(""));
        assert_eq!(si.next().map(slice_to_str), Some(""));
        assert_eq!(si.next().map(slice_to_str), Some(""));
        assert_eq!(si.next(), None);

        let slice = "-en-";
        let mut si = SubtagIterator::new(slice.as_bytes());
        assert_eq!(si.next().map(slice_to_str), Some(""));
        assert_eq!(si.next().map(slice_to_str), Some("en"));
        assert_eq!(si.next().map(slice_to_str), Some(""));
        assert_eq!(si.next(), None);

        let slice = "de_at-u-ca-foobar";
        let si = SubtagIterator::new(slice.as_bytes());
        assert_eq!(
            si.map(slice_to_str).collect::<Vec<_>>(),
            vec!["de", "at", "u", "ca", "foobar",]
        );
    }

    #[test]
    fn skip_before_separator_test() {
        let current = skip_before_separator(b"");
        assert_eq!(current, b"");

        let current = skip_before_separator(b"en");
        assert_eq!(current, b"en");

        let current = skip_before_separator(b"en-");
        assert_eq!(current, b"en");

        let current = skip_before_separator(b"en--US");
        assert_eq!(current, b"en");

        let current = skip_before_separator(b"-US");
        assert_eq!(current, b"");

        let current = skip_before_separator(b"US");
        assert_eq!(current, b"US");

        let current = skip_before_separator(b"-");
        assert_eq!(current, b"");
    }
}
