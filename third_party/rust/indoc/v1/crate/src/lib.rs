//! [![github]](https://github.com/dtolnay/indoc)&ensp;[![crates-io]](https://crates.io/crates/indoc)&ensp;[![docs-rs]](https://docs.rs/indoc)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K
//!
//! <br>
//!
//! This crate provides a procedural macro for indented string literals. The
//! `indoc!()` macro takes a multiline string literal and un-indents it at
//! compile time so the leftmost non-space character is in the first column.
//!
//! ```toml
//! [dependencies]
//! indoc = "1.0"
//! ```
//!
//! <br>
//!
//! # Using indoc
//!
//! ```
//! use indoc::indoc;
//!
//! fn main() {
//!     let testing = indoc! {"
//!         def hello():
//!             print('Hello, world!')
//!
//!         hello()
//!     "};
//!     let expected = "def hello():\n    print('Hello, world!')\n\nhello()\n";
//!     assert_eq!(testing, expected);
//! }
//! ```
//!
//! Indoc also works with raw string literals:
//!
//! ```
//! use indoc::indoc;
//!
//! fn main() {
//!     let testing = indoc! {r#"
//!         def hello():
//!             print("Hello, world!")
//!
//!         hello()
//!     "#};
//!     let expected = "def hello():\n    print(\"Hello, world!\")\n\nhello()\n";
//!     assert_eq!(testing, expected);
//! }
//! ```
//!
//! And byte string literals:
//!
//! ```
//! use indoc::indoc;
//!
//! fn main() {
//!     let testing = indoc! {b"
//!         def hello():
//!             print('Hello, world!')
//!
//!         hello()
//!     "};
//!     let expected = b"def hello():\n    print('Hello, world!')\n\nhello()\n";
//!     assert_eq!(testing[..], expected[..]);
//! }
//! ```
//!
//! <br><br>
//!
//! # Formatting macros
//!
//! The indoc crate exports four additional macros to substitute conveniently
//! for the standard library's formatting macros:
//!
//! - `formatdoc!($fmt, ...)`&ensp;&mdash;&ensp;equivalent to `format!(indoc!($fmt), ...)`
//! - `printdoc!($fmt, ...)`&ensp;&mdash;&ensp;equivalent to `print!(indoc!($fmt), ...)`
//! - `eprintdoc!($fmt, ...)`&ensp;&mdash;&ensp;equivalent to `eprint!(indoc!($fmt), ...)`
//! - `writedoc!($dest, $fmt, ...)`&ensp;&mdash;&ensp;equivalent to `write!($dest, indoc!($fmt), ...)`
//!
//! ```
//! use indoc::printdoc;
//!
//! fn main() {
//!     printdoc! {"
//!         GET {url}
//!         Accept: {mime}
//!         ",
//!         url = "http://localhost:8080",
//!         mime = "application/json",
//!     }
//! }
//! ```
//!
//! <br><br>
//!
//! # Explanation
//!
//! The following rules characterize the behavior of the `indoc!()` macro:
//!
//! 1. Count the leading spaces of each line, ignoring the first line and any
//!    lines that are empty or contain spaces only.
//! 2. Take the minimum.
//! 3. If the first line is empty i.e. the string begins with a newline, remove
//!    the first line.
//! 4. Remove the computed number of spaces from the beginning of each line.

#![allow(clippy::needless_doctest_main)]

mod error;
mod expr;

use crate::error::{Error, Result};
use crate::expr::Expr;
use proc_macro::token_stream::IntoIter as TokenIter;
use proc_macro::{Delimiter, Group, Ident, Literal, Punct, Spacing, Span, TokenStream, TokenTree};
use std::iter::{self, FromIterator};
use std::str::FromStr;
use unindent::unindent;

#[derive(Copy, Clone, PartialEq)]
enum Macro {
    Indoc,
    Format,
    Print,
    Eprint,
    Write,
}

/// Unindent and produce `&'static str`.
///
/// # Example
///
/// ```
/// # use indoc::indoc;
/// #
/// // The type of `program` is &'static str
/// let program = indoc! {"
///     def hello():
///         print('Hello, world!')
///
///     hello()
/// "};
/// print!("{}", program);
/// ```
///
/// ```text
/// def hello():
///     print('Hello, world!')
///
/// hello()
/// ```
#[proc_macro]
pub fn indoc(input: TokenStream) -> TokenStream {
    expand(input, Macro::Indoc)
}

/// Unindent and call `format!`.
///
/// Argument syntax is the same as for [`std::format!`].
///
/// # Example
///
/// ```
/// # use indoc::formatdoc;
/// #
/// let request = formatdoc! {"
///     GET {url}
///     Accept: {mime}
///     ",
///     url = "http://localhost:8080",
///     mime = "application/json",
/// };
/// println!("{}", request);
/// ```
///
/// ```text
/// GET http://localhost:8080
/// Accept: application/json
/// ```
#[proc_macro]
pub fn formatdoc(input: TokenStream) -> TokenStream {
    expand(input, Macro::Format)
}

/// Unindent and call `print!`.
///
/// Argument syntax is the same as for [`std::print!`].
///
/// # Example
///
/// ```
/// # use indoc::printdoc;
/// #
/// printdoc! {"
///     GET {url}
///     Accept: {mime}
///     ",
///     url = "http://localhost:8080",
///     mime = "application/json",
/// }
/// ```
///
/// ```text
/// GET http://localhost:8080
/// Accept: application/json
/// ```
#[proc_macro]
pub fn printdoc(input: TokenStream) -> TokenStream {
    expand(input, Macro::Print)
}

/// Unindent and call `eprint!`.
///
/// Argument syntax is the same as for [`std::eprint!`].
///
/// # Example
///
/// ```
/// # use indoc::eprintdoc;
/// #
/// eprintdoc! {"
///     GET {url}
///     Accept: {mime}
///     ",
///     url = "http://localhost:8080",
///     mime = "application/json",
/// }
/// ```
///
/// ```text
/// GET http://localhost:8080
/// Accept: application/json
/// ```
#[proc_macro]
pub fn eprintdoc(input: TokenStream) -> TokenStream {
    expand(input, Macro::Eprint)
}

/// Unindent and call `write!`.
///
/// Argument syntax is the same as for [`std::write!`].
///
/// # Example
///
/// ```
/// # use indoc::writedoc;
/// # use std::io::Write;
/// #
/// let _ = writedoc!(
///     std::io::stdout(),
///     "
///         GET {url}
///         Accept: {mime}
///     ",
///     url = "http://localhost:8080",
///     mime = "application/json",
/// );
/// ```
///
/// ```text
/// GET http://localhost:8080
/// Accept: application/json
/// ```
#[proc_macro]
pub fn writedoc(input: TokenStream) -> TokenStream {
    expand(input, Macro::Write)
}

fn expand(input: TokenStream, mode: Macro) -> TokenStream {
    match try_expand(input, mode) {
        Ok(tokens) => tokens,
        Err(err) => err.to_compile_error(),
    }
}

fn try_expand(input: TokenStream, mode: Macro) -> Result<TokenStream> {
    let mut input = input.into_iter();

    let prefix = if mode == Macro::Write {
        Some(expr::parse(&mut input)?)
    } else {
        None
    };

    let first = input.next().ok_or_else(|| {
        Error::new(
            Span::call_site(),
            "unexpected end of macro invocation, expected format string",
        )
    })?;

    let unindented_lit = lit_indoc(first, mode)?;

    let macro_name = match mode {
        Macro::Indoc => {
            require_empty_or_trailing_comma(&mut input)?;
            return Ok(TokenStream::from(TokenTree::Literal(unindented_lit)));
        }
        Macro::Format => "format",
        Macro::Print => "print",
        Macro::Eprint => "eprint",
        Macro::Write => "write",
    };

    // #macro_name! { #unindented_lit #args }
    Ok(TokenStream::from_iter(vec![
        TokenTree::Ident(Ident::new(macro_name, Span::call_site())),
        TokenTree::Punct(Punct::new('!', Spacing::Alone)),
        TokenTree::Group(Group::new(
            Delimiter::Brace,
            prefix
                .map_or_else(TokenStream::new, Expr::into_tokens)
                .into_iter()
                .chain(iter::once(TokenTree::Literal(unindented_lit)))
                .chain(input)
                .collect(),
        )),
    ]))
}

fn lit_indoc(token: TokenTree, mode: Macro) -> Result<Literal> {
    let repr = token.to_string();
    let repr = repr.trim();
    let is_string = repr.starts_with('"') || repr.starts_with('r');
    let is_byte_string = repr.starts_with("b\"") || repr.starts_with("br");

    if !is_string && !is_byte_string {
        return Err(Error::new(
            token.span(),
            "argument must be a single string literal",
        ));
    }

    if is_byte_string && mode != Macro::Indoc {
        return Err(Error::new(
            token.span(),
            "byte strings are not supported in formatting macros",
        ));
    }

    let begin = repr.find('"').unwrap() + 1;
    let end = repr.rfind('"').unwrap();
    let repr = format!(
        "{open}{content}{close}",
        open = &repr[..begin],
        content = unindent(&repr[begin..end]),
        close = &repr[end..],
    );

    match TokenStream::from_str(&repr)
        .unwrap()
        .into_iter()
        .next()
        .unwrap()
    {
        TokenTree::Literal(mut lit) => {
            lit.set_span(token.span());
            Ok(lit)
        }
        _ => unreachable!(),
    }
}

fn require_empty_or_trailing_comma(input: &mut TokenIter) -> Result<()> {
    let first = match input.next() {
        Some(TokenTree::Punct(punct)) if punct.as_char() == ',' => match input.next() {
            Some(second) => second,
            None => return Ok(()),
        },
        Some(first) => first,
        None => return Ok(()),
    };
    let last = input.last();

    let begin_span = first.span();
    let end_span = last.as_ref().map_or(begin_span, TokenTree::span);
    let msg = format!(
        "unexpected {token} in macro invocation; indoc argument must be a single string literal",
        token = if last.is_some() { "tokens" } else { "token" }
    );
    Err(Error::new2(begin_span, end_span, &msg))
}
