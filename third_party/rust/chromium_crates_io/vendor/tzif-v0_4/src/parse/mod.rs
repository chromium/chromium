// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use combine::parser::combinator::Either;
use combine::parser::error::{unexpected_any, Unexpected};
use combine::parser::token::Value;
use combine::{value, Parser, Stream};

/// Parser definition for POSIX time-zone strings as specified by
/// <https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html>
pub mod posix;

/// Parser definition for `TZif` binary files as specified by
/// <https://datatracker.ietf.org/doc/html/rfc8536>
pub mod tzif;

/// Ensures that the predicate is [`true`], otherwise returns an error with the provided
/// messages though combine's error machinery.
fn ensure<Input: Stream, L>(
    output: L,
    predicate: impl FnOnce(&L) -> bool,
    message: &'static str,
) -> Either<Value<Input, L>, Unexpected<Input, L, &'static str>>
where
    L: Clone,
{
    if predicate(&output) {
        value(output).left()
    } else {
        Parser::right(unexpected_any(message))
    }
}

#[cfg(test)]
#[macro_export]
/// Helper macro to test that a parse should fail with Err().
macro_rules! assert_parse_err {
    ($parser:expr, bytes $src:expr $(,)?) => {
        assert!(
            $parser.parse($src).is_err(),
            "expected {:?}, parse {:?} as Err(), but got Ok() {:#?}",
            stringify!($parser),
            $src,
            $parser.easy_parse($src).unwrap().0,
        )
    };
    ($parser:expr, $src:expr $(,)?) => {
        assert!(
            $parser.parse($src.as_bytes()).is_err(),
            "expected {}, parse {} as Err(), but got Ok() {:#?}",
            stringify!($parser),
            $src,
            $parser.easy_parse($src.as_bytes()).unwrap().0,
        )
    };
}

#[cfg(test)]
#[macro_export]
/// Helper macro to test that a parse should succeed with Ok().
macro_rules! assert_parse_ok {
    ($parser:expr, bytes $src:expr $(,)?) => {
        assert!(
            $parser.parse($src).is_ok(),
            "expected {:?}, parse {:?} as Ok(), but got Err() {:#?}",
            stringify!($parser),
            $src,
            $parser.easy_parse($src),
        )
    };
    ($parser:expr, $src:expr $(,)?) => {
        assert!(
            $parser.parse(($src).as_bytes()).is_ok(),
            "expected {}, parse {} as Ok(), but got Err() {:#?}",
            stringify!($parser),
            $src,
            $parser.easy_parse($src.as_bytes()),
        )
    };
}

#[cfg(test)]
#[macro_export]
/// Helper macro to test the equality of the actual and expected parse.
macro_rules! assert_parse_eq {
    ($parser:expr, bytes $src:expr, $expected:expr $(,)?) => {
        $crate::assert_parse_ok!($parser, bytes $src);
        assert_eq!(
            $parser.parse($src).unwrap().0,
            $expected,
            "expected {:?}, parse as {:?} but got {:?}",
            $src,
            $expected,
            $parser.easy_parse($src).unwrap().0,
        )
    };
    ($parser:expr, $src:expr, $expected:expr $(,)?) => {
        $crate::assert_parse_ok!($parser, $src);
        assert_eq!(
            $parser.parse($src.as_bytes()).unwrap().0,
            $expected,
            "expected {:?}, parse as {:?} but got {:?}",
            $src,
            $expected,
            $parser.easy_parse($src.as_bytes()).unwrap().0,
        )
    };
}
