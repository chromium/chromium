/// Parses a type that impls [`ParserFor`] with the passed in [`Parser`].
///
/// # Example
///
/// This example demonstrates how you can use this macro to parse both
/// standard library and user-defined types.
///
/// ```rust
/// use konst::{parse_with, try_rebind, unwrap_ctx};
///
/// use konst::parsing::{ParserFor, Parser, ParseValueResult};
///
/// const PAIR: (u32, Foo) = unwrap_ctx!(parse_pair(Parser::from_str("100,Baz"))).0;
///
/// assert_eq!(PAIR.0, 100);
/// assert_eq!(PAIR.1, Foo::Baz);
///
/// const fn parse_pair(mut parser: Parser<'_>) -> ParseValueResult<'_, (u32, Foo)> {
///     try_rebind!{(let left, parser) = parse_with!(parser, u32)}
///     try_rebind!{parser = parser.strip_prefix_u8(b',')}
///     try_rebind!{(let right, parser) = parse_with!(parser, Foo)}
///     
///     Ok(((left, right), parser))
/// }
///
///
/// #[derive(Debug, PartialEq)]
/// enum Foo {
///     Bar,
///     Baz,
///     Qux,
/// }
///
/// impl ParserFor for Foo {
///     type Parser = Self;
/// }
///
/// impl Foo {
///     const fn parse_with(parser: Parser<'_>) -> ParseValueResult<'_, Self> {
///         // You can use the `parse_any` macro instead of this chain of if elses
///         if let Ok(parser) = parser.strip_prefix("Bar") {
///             Ok((Foo::Bar, parser))
///         } else if let Ok(parser) = parser.strip_prefix("Baz") {
///             Ok((Foo::Baz, parser))
///         } else if let Ok(parser) = parser.strip_prefix("Qux") {
///             Ok((Foo::Qux, parser))
///         } else {
///             Err(parser.into_other_error())
///         }
///     }
/// }
/// ```
///
/// [`Parser`]: ./parsing/struct.Parser.html
/// [`ParserFor`]: ./parsing/trait.ParserFor.html
#[macro_export]
macro_rules! parse_with {
    ($parser:expr, $type:ty $(,)*) => {
        match $parser {
            parser @ $crate::Parser { .. } => {
                let res: $crate::__::Result<_, _> =
                    <<$type as $crate::parsing::ParserFor>::Parser>::parse_with(parser);
                res
            }
        }
    };
}
