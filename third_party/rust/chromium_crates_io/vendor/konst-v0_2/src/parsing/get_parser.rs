use crate::parsing::{ParseValueResult, Parser};

use core::marker::PhantomData;

/// Gets a type that parses `Self` with a `parse_with` method.
///
/// Implementing this trait allows parsing a type with the [`parse_with`] macro.
///
/// # Implementing this trait
///
/// You can implement this trait like this:
/// ```rust
/// # struct SomeType;
/// # struct SomeParser;
/// # use konst::parsing::ParserFor;
/// impl ParserFor for SomeType {
///     // This is usually `Self` for user-defined types.
///     type Parser = SomeParser;
/// }
/// ```
/// Then `SomeParser` is expected to have a `parse_with` associated function with this signature:
/// ```rust
/// # /*
/// impl SomeParser {
///     const fn parse_with<'a>(
///         _: konst::Parser<'a>
///     ) -> Result<(This, konst::Parser<'a>), SomeErrorType>
/// }
/// # */
/// ```
///
/// # Example
///
/// ```rust
/// use konst::{parse_with, try_rebind, unwrap_ctx};
///
/// use konst::parsing::{ParserFor, Parser, ParseValueResult};
///
/// const PAIR: Pair = {
///     let parser = Parser::from_str("100,200");
///     unwrap_ctx!(parse_with!(parser, Pair)).0
/// };
///
/// assert_eq!(PAIR, Pair(100, 200));
///
///
/// #[derive(Debug, PartialEq)]
/// struct Pair(u32, u64);
///
/// impl ParserFor for Pair {
///     type Parser = Self;
/// }
///
/// impl Pair {
///     const fn parse_with(mut parser: Parser<'_>) -> ParseValueResult<'_, Self> {
///         try_rebind!{(let left, parser) = parse_with!(parser, u32)}
///         try_rebind!{parser = parser.strip_prefix_u8(b',')}
///         try_rebind!{(let right, parser) = parse_with!(parser, u64)}
///
///         Ok((Pair(left, right), parser))
///     }
/// }
/// ```
///
/// [`parse_with`]: ../macro.parse_with.html
/// [`ParserFor::Parser`]: #associatedtype.Parser
///
pub trait ParserFor: Sized {
    /// The type that parses `Self` with its `parse_with` associated function.
    ///
    /// This is usually `Self` for user-defined types.
    type Parser;
}

////////////////////////////////////////////////////////////////////////////////

/// Parses a standard library type, determined by the `StdType` type parameter.
///
///
///
pub struct StdParser<StdType>(PhantomData<StdType>);

macro_rules! impl_std_parser_one {
    ($method:ident, $type:ty, $parse_with_docs:expr) => {
        impl ParserFor for $type {
            type Parser = StdParser<$type>;
        }

        impl StdParser<$type> {
            #[doc = $parse_with_docs]
            pub const fn parse_with(parser: Parser<'_>) -> ParseValueResult<'_, $type> {
                parser.$method()
            }
        }
    };
}
macro_rules! impl_std_parser {
    ($($method:ident -> $type:ty;)*) => (
        $(
            impl_std_parser_one!{
                $method,
                $type,
                concat!("Atempts to parse `", stringify!($type), "`")
            }
        )*
    )
}

impl_std_parser! {
    parse_u128 -> u128;
    parse_i128 -> i128;
    parse_u64 -> u64;
    parse_i64 -> i64;
    parse_u32 -> u32;
    parse_i32 -> i32;
    parse_u16 -> u16;
    parse_i16 -> i16;
    parse_u8 -> u8;
    parse_i8 -> i8;
    parse_usize -> usize;
    parse_isize -> isize;
    parse_bool -> bool;
}
