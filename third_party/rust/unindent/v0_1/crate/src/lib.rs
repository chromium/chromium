//! [![github]](https://github.com/dtolnay/indoc)&ensp;[![crates-io]](https://crates.io/crates/unindent)&ensp;[![docs-rs]](https://docs.rs/unindent)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K
//!
//! <br>
//!
//! ## Unindent
//!
//! This crate provides [`indoc`]'s indentation logic for use with strings that
//! are not statically known at compile time. For unindenting string literals,
//! use `indoc` instead.
//!
//! [`indoc`]: https://github.com/dtolnay/indoc
//!
//! This crate exposes two unindent functions and an extension trait:
//!
//! - `fn unindent(&str) -> String`
//! - `fn unindent_bytes(&[u8]) -> Vec<u8>`
//! - `trait Unindent`
//!
//! ```
//! use unindent::unindent;
//!
//! fn main() {
//!     let indented = "
//!             line one
//!             line two";
//!     assert_eq!("line one\nline two", unindent(indented));
//! }
//! ```
//!
//! The `Unindent` extension trait expose the same functionality under an
//! extension method.
//!
//! ```
//! use unindent::Unindent;
//!
//! fn main() {
//!     let indented = format!("
//!             line {}
//!             line {}", "one", "two");
//!     assert_eq!("line one\nline two", indented.unindent());
//! }
//! ```

#![doc(html_root_url = "https://docs.rs/unindent/0.1.6")]
#![allow(clippy::type_complexity)]

use std::iter::Peekable;
use std::slice::Split;

pub fn unindent(s: &str) -> String {
    let bytes = s.as_bytes();
    let unindented = unindent_bytes(bytes);
    String::from_utf8(unindented).unwrap()
}

// Compute the maximal number of spaces that can be removed from every line, and
// remove them.
pub fn unindent_bytes(s: &[u8]) -> Vec<u8> {
    // Document may start either on the same line as opening quote or
    // on the next line
    let ignore_first_line = s.starts_with(b"\n") || s.starts_with(b"\r\n");

    // Largest number of spaces that can be removed from every
    // non-whitespace-only line after the first
    let spaces = s
        .lines()
        .skip(1)
        .filter_map(count_spaces)
        .min()
        .unwrap_or(0);

    let mut result = Vec::with_capacity(s.len());
    for (i, line) in s.lines().enumerate() {
        if i > 1 || (i == 1 && !ignore_first_line) {
            result.push(b'\n');
        }
        if i == 0 {
            // Do not un-indent anything on same line as opening quote
            result.extend_from_slice(line);
        } else if line.len() > spaces {
            // Whitespace-only lines may have fewer than the number of spaces
            // being removed
            result.extend_from_slice(&line[spaces..]);
        }
    }
    result
}

pub trait Unindent {
    type Output;

    fn unindent(&self) -> Self::Output;
}

impl Unindent for str {
    type Output = String;

    fn unindent(&self) -> Self::Output {
        unindent(self)
    }
}

impl Unindent for String {
    type Output = String;

    fn unindent(&self) -> Self::Output {
        unindent(self)
    }
}

impl Unindent for [u8] {
    type Output = Vec<u8>;

    fn unindent(&self) -> Self::Output {
        unindent_bytes(self)
    }
}

impl<'a, T: ?Sized + Unindent> Unindent for &'a T {
    type Output = T::Output;

    fn unindent(&self) -> Self::Output {
        (**self).unindent()
    }
}

// Number of leading spaces in the line, or None if the line is entirely spaces.
fn count_spaces(line: &[u8]) -> Option<usize> {
    for (i, ch) in line.iter().enumerate() {
        if *ch != b' ' && *ch != b'\t' {
            return Some(i);
        }
    }
    None
}

// Based on core::str::StrExt.
trait BytesExt {
    fn lines(&self) -> Lines;
}

impl BytesExt for [u8] {
    fn lines(&self) -> Lines {
        fn is_newline(b: &u8) -> bool {
            *b == b'\n'
        }
        let bytestring = if self.starts_with(b"\r\n") {
            &self[1..]
        } else {
            self
        };
        Lines {
            split: bytestring.split(is_newline as fn(&u8) -> bool).peekable(),
        }
    }
}

struct Lines<'a> {
    split: Peekable<Split<'a, u8, fn(&u8) -> bool>>,
}

impl<'a> Iterator for Lines<'a> {
    type Item = &'a [u8];

    fn next(&mut self) -> Option<Self::Item> {
        match self.split.next() {
            None => None,
            Some(fragment) => {
                if fragment.is_empty() && self.split.peek().is_none() {
                    None
                } else {
                    Some(fragment)
                }
            }
        }
    }
}
