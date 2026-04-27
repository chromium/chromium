//! Parsing using `const fn` methods.
//!
//! You can use the [`Parser`] type to parse from string slices, and byte slices,
//! more information in its documentation.
//!
//! If you're looking for functions to parse some type from an entire string
//! (instead of only part of it),
//! then you want to look in the module for that type, eg: [`primitive::parse_bool`].
//!
//! If you do want to parse a type fron only part of a string, then you can use
//! [`Parser`]'s `parser_*` methods, or the [`parse_with`] macro.
//!
//! [`Parser`]: ./struct.Parser.html
//! [`primitive::parse_bool`]: ../primitive/fn.parse_bool.html
//! [`parse_with`]: ../macro.parse_with.html
//!

mod get_parser;
mod non_parsing_methods;
mod parse_errors;
mod primitive_parsing;

/////////////////////////////////////////////////////////////////////////////////

pub use self::{
    get_parser::{ParserFor, StdParser},
    parse_errors::{ErrorKind, ParseDirection, ParseError, ParseValueResult, ParserResult},
};

/// For parsing and traversing over byte strings in const contexts.
///
/// If you're looking for functions to parse some type from an entire string
/// (instead of only part of it),
/// then you want to look in the module for that type, eg: [`primitive::parse_bool`].
///
/// [`primitive::parse_bool`]: ../primitive/fn.parse_bool.html
///
/// # Mutation
///
/// Because `konst` only requires Rust 1.46.0,
/// in order to mutate a parser you must reassign the parser returned by its methods.
/// <br>eg: `parser = parser.trim_start();`
///
/// To help make this more ergonomic for `Result`-returning methods, you can use these macros:
///
/// - [`try_rebind`]:
/// Like the `?` operator,
/// but also reassigns variables with the value in the `Ok` variant.
///
/// - [`rebind_if_ok`]:
/// Like an `if let Ok`,
/// but also reassigns variables with the value in the `Ok` variant.
///
/// - [`parse_any`]:
/// Parses any of the string literal patterns using a supported `Parser` method.
///
/// [`try_rebind`]: ../macro.try_rebind.html
/// [`rebind_if_ok`]: ../macro.rebind_if_ok.html
/// [`parse_any`]: ../macro.parse_any.html
///
/// # Examples
///
/// ### Parsing a variable-length array
///
/// Parses a variable-length array, requires the length to appear before the array.
///
/// This example requires the "parsing" feature (enabled by default)
/// because it uses the  [`parse_any`] macro.
///
#[cfg_attr(feature = "parsing", doc = "```rust")]
#[cfg_attr(not(feature = "parsing"), doc = "```ignore")]
/// use konst::{
///     parsing::{Parser, ParseValueResult},
///     for_range, parse_any, try_rebind, unwrap_ctx,
/// };
///
/// // We need to parse the length into a separate const to use it as the length of the array.
/// const LEN_AND_PARSER: (usize, Parser<'_>) = {
///     let input = "\
///         6;
///         up, 0, 90, down, left, right,
///     ";
///     
///     let parser = Parser::from_str(input);
///     let (len, parser) = unwrap_ctx!(parser.parse_usize());
///     (len, unwrap_ctx!(parser.strip_prefix_u8(b';')))
/// };
///
/// const LEN: usize = LEN_AND_PARSER.0;
///
/// const ANGLES: [Angle; LEN] = unwrap_ctx!(Angle::parse_array(LEN_AND_PARSER.1)).0;
///
/// fn main() {
///     assert_eq!(
///         ANGLES,
///         [Angle::UP, Angle::UP, Angle::RIGHT, Angle::DOWN, Angle::LEFT, Angle::RIGHT]
///     );
/// }
///
///
///
/// #[derive(Debug, PartialEq, Eq, Copy, Clone)]
/// struct Angle(u16);
///
/// impl Angle {
///     pub const UP: Self = Self(0);
///     pub const RIGHT: Self = Self(90);
///     pub const DOWN: Self = Self(180);
///     pub const LEFT: Self = Self(270);
///
///     pub const fn new(n: u64) -> Angle {
///         Angle((n % 360) as u16)
///     }
///
///     // This could take a `const LEN: usize` const parameter in Rust 1.51.0,
///     // so that the returned array can be any length.
///     const fn parse_array(mut parser: Parser<'_>) -> ParseValueResult<'_, [Angle; LEN]> {
///         let mut ret = [Angle::UP; LEN];
///         
///         for_range!{i in 0..LEN =>
///             try_rebind!{(ret[i], parser) = Angle::parse(parser.trim_start())}
///             
///             parser = parser.trim_start();
///             if !parser.is_empty() {
///                 try_rebind!{parser = parser.strip_prefix_u8(b',')}
///             }
///         }
///         Ok((ret, parser))
///     }
///
///     pub const fn parse(mut parser: Parser<'_>) -> ParseValueResult<'_, Angle> {
///         // Prefer using the `rebind_if_ok` macro if you don't `return` inside the `if let`,
///         // because the `parser` inside this `if let` is a different variable than outside.
///         if let Ok((angle, parser)) = parser.parse_u64() {
///             return Ok((Self::new(angle), parser))
///         }
///         
///         let angle = parse_any!{parser, strip_prefix;
///             "up" => Self::UP,
///             "right" => Self::RIGHT,
///             "down" => Self::DOWN,
///             "left" => Self::LEFT,
///             _ => return Err(parser.into_other_error())
///         };
///         Ok((angle, parser))
///     }
/// }
///
///
/// ```
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "parsing_no_proc")))]
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct Parser<'a> {
    parse_direction: ParseDirection,
    /// The offset of `bytes` in the string that this was created from.
    start_offset: u32,
    bytes: &'a [u8],
}

impl<'a> Parser<'a> {
    /// Gets the next unparsed byte.
    #[inline]
    pub const fn next_byte(mut self) -> ParseValueResult<'a, u8> {
        try_parsing! {self, FromStart, ret;
            if let [byte, rem @ ..] = self.bytes {
                self.bytes = rem;
                *byte
            } else {
                throw!(ErrorKind::SkipByte)
            }
        }
    }

    /// For skipping the first `bytes` bytes.
    ///
    /// # Performance
    ///
    /// If the "rust_1_64" feature is disabled,
    /// thich takes linear time to remove the leading elements,
    /// proportional to `bytes`.
    ///
    /// If the "rust_1_64" feature is enabled, it takes constant time to run.
    ///
    pub const fn skip(mut self, bytes: usize) -> Self {
        parsing! {self, FromStart;
            self.bytes = crate::slice::slice_from(self.bytes, bytes);
        }
    }

    /// Checks that the parsed bytes start with `matched`,
    /// returning the remainder of the bytes.
    ///
    /// For calling `strip_prefix` with multiple alternative `matched` string literals,
    /// you can use the [`parse_any`] macro,
    /// [example](../macro.parse_any.html#parsing-enum-example)
    ///
    /// # Examples
    ///
    /// ### Basic
    ///
    /// ```
    /// use konst::{Parser, rebind_if_ok};
    ///
    /// let mut parser = Parser::from_str("foo;bar;baz;");
    ///
    /// assert!(parser.strip_prefix("aaa").is_err());
    ///
    /// rebind_if_ok!{parser = parser.strip_prefix("foo;")}
    /// assert_eq!(parser.bytes(), "bar;baz;".as_bytes());
    ///
    /// rebind_if_ok!{parser = parser.strip_prefix("bar;")}
    /// assert_eq!(parser.bytes(), "baz;".as_bytes());
    ///
    /// rebind_if_ok!{parser = parser.strip_prefix("baz;")}
    /// assert_eq!(parser.bytes(), "".as_bytes());
    ///
    ///
    /// ```
    ///
    /// ### Use case
    ///
    /// ```rust
    /// use konst::{Parser, rebind_if_ok};
    ///
    /// #[derive(Debug, PartialEq)]
    /// struct Flags {
    ///     foo: bool,
    ///     bar: bool,
    /// }
    ///
    /// const fn parse_flags(mut parser: Parser<'_>) -> (Flags, Parser<'_>) {
    ///     let mut flags = Flags{foo: false, bar: false};
    ///     rebind_if_ok!{parser = parser.strip_prefix("foo;") =>
    ///         flags.foo = true;
    ///     }
    ///     rebind_if_ok!{parser = parser.strip_prefix("bar;") =>
    ///         flags.bar = true;
    ///     }
    ///     (flags, parser)
    /// }
    ///
    /// const VALUES: &[Flags] = &[
    ///     parse_flags(Parser::from_str("")).0,
    ///     parse_flags(Parser::from_str("foo;")).0,
    ///     parse_flags(Parser::from_str("bar;")).0,
    ///     parse_flags(Parser::from_str("foo;bar;")).0,
    /// ];
    ///
    /// assert_eq!(VALUES[0], Flags{foo: false, bar: false});
    /// assert_eq!(VALUES[1], Flags{foo: true, bar: false});
    /// assert_eq!(VALUES[2], Flags{foo: false, bar: true});
    /// assert_eq!(VALUES[3], Flags{foo: true, bar: true});
    ///
    /// ```
    #[inline]
    pub const fn strip_prefix(self, matched: &str) -> Result<Self, ParseError<'a>> {
        self.strip_prefix_b(matched.as_bytes())
    }

    /// Equivalent to [`strip_prefix`], but takes a byte slice.
    ///
    /// [`strip_prefix`]: #method.strip_prefix
    pub const fn strip_prefix_b(mut self, mut matched: &[u8]) -> Result<Self, ParseError<'a>> {
        try_parsing! {self, FromStart;
            impl_bytes_function!{
                strip_prefix;
                left = self.bytes;
                right = matched;
                on_error = throw!(ErrorKind::Strip),
            }
        }
    }

    /// Equivalent to [`strip_prefix`], but takes a single byte.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, rebind_if_ok};
    ///
    /// let mut parser = Parser::from_str("abcde");
    ///
    /// assert!(parser.strip_prefix_u8(1).is_err());
    ///
    /// rebind_if_ok!{parser = parser.strip_prefix_u8(b'a')}
    /// assert_eq!(parser.bytes(), "bcde".as_bytes());
    ///
    /// rebind_if_ok!{parser = parser.strip_prefix_u8(b'b')}
    /// assert_eq!(parser.bytes(), "cde".as_bytes());
    ///
    /// rebind_if_ok!{parser = parser.strip_prefix_u8(b'c')}
    /// assert_eq!(parser.bytes(), "de".as_bytes());
    ///
    /// ```
    ///
    /// [`strip_prefix`]: #method.strip_prefix
    pub const fn strip_prefix_u8(mut self, matched: u8) -> Result<Self, ParseError<'a>> {
        try_parsing! {self, FromStart;
            match self.bytes {
                [byte, rem @ ..] if *byte == matched => {
                    self.bytes = rem;
                }
                _ => throw!(ErrorKind::Strip),
            }
        }
    }

    /// Checks that the parsed bytes end with `matched`,
    /// returning the remainder of the bytes.
    ///
    /// For calling `strip_suffix` with multiple alternative `matched` string literals,
    /// you can use the [`parse_any`] macro.
    ///
    /// # Examples
    ///
    /// ### Basic
    ///
    /// ```
    /// use konst::{Parser, rebind_if_ok};
    ///
    /// let mut parser = Parser::from_str("foo;bar;baz;");
    ///
    /// assert!(parser.strip_suffix("aaa").is_err());
    ///
    /// rebind_if_ok!{parser = parser.strip_suffix("baz;")}
    /// assert_eq!(parser.bytes(), "foo;bar;".as_bytes());
    ///
    /// rebind_if_ok!{parser = parser.strip_suffix("bar;")}
    /// assert_eq!(parser.bytes(), "foo;".as_bytes());
    ///
    /// rebind_if_ok!{parser = parser.strip_suffix("foo;")}
    /// assert_eq!(parser.bytes(), "".as_bytes());
    ///
    /// ```
    ///
    #[inline]
    pub const fn strip_suffix(self, matched: &str) -> Result<Self, ParseError<'a>> {
        self.strip_suffix_b(matched.as_bytes())
    }

    /// Equivalent to [`strip_suffix`], but takes a byte slice.
    ///
    /// [`strip_suffix`]: #method.strip_suffix
    pub const fn strip_suffix_b(mut self, mut matched: &[u8]) -> Result<Self, ParseError<'a>> {
        try_parsing! {self, FromEnd;
            impl_bytes_function!{
                strip_suffix;
                left = self.bytes;
                right = matched;
                on_error = throw!(ErrorKind::Strip),
            }
        }
    }

    /// Equivalent to [`strip_suffix`], but takes a single byte.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, rebind_if_ok};
    ///
    /// let mut parser = Parser::from_str("edcba");
    ///
    /// assert!(parser.strip_suffix_u8(1).is_err());
    ///
    /// rebind_if_ok!{parser = parser.strip_suffix_u8(b'a')}
    /// assert_eq!(parser.bytes(), "edcb".as_bytes());
    ///
    /// rebind_if_ok!{parser = parser.strip_suffix_u8(b'b')}
    /// assert_eq!(parser.bytes(), "edc".as_bytes());
    ///
    /// rebind_if_ok!{parser = parser.strip_suffix_u8(b'c')}
    /// assert_eq!(parser.bytes(), "ed".as_bytes());
    ///
    /// ```
    ///
    /// [`strip_suffix`]: #method.strip_suffix
    pub const fn strip_suffix_u8(mut self, matched: u8) -> Result<Self, ParseError<'a>> {
        try_parsing! {self,  FromEnd;
            match self.bytes {
                [rem @ .., byte] if *byte == matched => {
                    self.bytes = rem;
                }
                _ => throw!(ErrorKind::Strip),
            }
        }
    }

    /////////////////////////////////////////
    //           *trim* methods            //
    /////////////////////////////////////////

    /// Removes whitespace from the start of the parsed bytes.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, unwrap_ctx};
    ///
    /// let mut parser = Parser::from_str("    foo\n\t bar");
    ///
    /// parser = parser.trim_start();
    /// assert_eq!(parser.bytes(), "foo\n\t bar".as_bytes());
    ///
    /// parser = unwrap_ctx!(parser.strip_prefix("foo")).trim_start();
    /// assert_eq!(parser.bytes(), "bar".as_bytes());
    ///
    /// ```
    pub const fn trim_start(mut self) -> Self {
        parsing! {self, FromStart;
            self.bytes = crate::slice::bytes_trim_start(self.bytes);
        }
    }

    /// Removes whitespace from the end of the parsed bytes.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, unwrap_ctx};
    ///
    /// let mut parser = Parser::from_str("foo,\n    bar,\n    ");
    ///
    /// parser = parser.trim_end();
    /// assert_eq!(parser.bytes(), "foo,\n    bar,".as_bytes());
    ///
    /// parser = unwrap_ctx!(parser.strip_suffix("bar,")).trim_end();
    /// assert_eq!(parser.bytes(), "foo,".as_bytes());
    ///
    /// ```
    pub const fn trim_end(mut self) -> Self {
        parsing! {self, FromEnd;
            self.bytes = crate::slice::bytes_trim_end(self.bytes);
        }
    }

    /// Repeatedly removes all instances of `needle` from the start of the parsed bytes.
    ///
    /// For trimming with multiple `needle`s, you can use the [`parse_any`] macro,
    /// [example](../macro.parse_any.html#trimming-example)
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::Parser;
    ///
    /// {
    ///     let mut parser = Parser::from_str("HelloHelloHello world!");
    ///     parser = parser.trim_start_matches("Hello");
    ///     assert_eq!(parser.bytes(), " world!".as_bytes());
    /// }
    /// {
    ///     let mut parser = Parser::from_str("        Hi!");
    ///     parser = parser.trim_start_matches("    ");
    ///     assert_eq!(parser.bytes(), "Hi!".as_bytes());
    /// }
    /// {
    ///     let mut parser = Parser::from_str("------Bye!");
    ///     parser = parser.trim_start_matches("----");
    ///     assert_eq!(parser.bytes(), "--Bye!".as_bytes());
    /// }
    ///
    /// ```
    ///
    pub const fn trim_start_matches(self, needle: &str) -> Self {
        self.trim_start_matches_b(needle.as_bytes())
    }

    /// Equivalent to [`trim_start_matches`], but takes a byte slice.
    ///
    /// [`trim_start_matches`]: #method.trim_start_matches
    pub const fn trim_start_matches_b(mut self, needle: &[u8]) -> Self {
        parsing! {self, FromStart;
            self.bytes = crate::slice::bytes_trim_start_matches(self.bytes, needle);
        }
    }

    /// Equivalent to [`trim_start_matches`], but takes a single byte.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::Parser;
    ///
    /// let mut parser = Parser::from_str("    ----world");
    ///
    /// parser = parser.trim_start_matches_u8(b' ');
    /// assert_eq!(parser.bytes(), "----world".as_bytes());
    ///
    /// parser = parser.trim_start_matches_u8(b'-');
    /// assert_eq!(parser.bytes(), "world".as_bytes());
    ///
    /// parser = parser.trim_start_matches_u8(b'-');
    /// assert_eq!(parser.bytes(), "world".as_bytes());
    ///
    /// ```
    ///
    /// [`trim_start_matches`]: #method.trim_start_matches
    pub const fn trim_start_matches_u8(mut self, needle: u8) -> Self {
        parsing! {self, FromStart;
            while let [b, rem @ ..] = self.bytes {
                if *b == needle {
                    self.bytes = rem;
                } else {
                    break;
                }
            }
        }
    }

    /// Repeatedly removes all instances of `needle` from the start of the parsed bytes.
    ///
    /// For trimming with multiple `needle`s, you can use the [`parse_any`] macro,
    /// [example](../macro.parse_any.html#trimming-example)
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::Parser;
    ///
    /// {
    ///     let mut parser = Parser::from_str("Hello world!world!world!");
    ///     parser = parser.trim_end_matches("world!");
    ///     assert_eq!(parser.bytes(), "Hello ".as_bytes());
    /// }
    /// {
    ///     let mut parser = Parser::from_str("Hi!        ");
    ///     parser = parser.trim_end_matches("    ");
    ///     assert_eq!(parser.bytes(), "Hi!".as_bytes());
    /// }
    /// {
    ///     let mut parser = Parser::from_str("Bye!------");
    ///     parser = parser.trim_end_matches("----");
    ///     assert_eq!(parser.bytes(), "Bye!--".as_bytes());
    /// }
    ///
    /// ```
    ///
    pub const fn trim_end_matches(self, needle: &str) -> Self {
        self.trim_end_matches_b(needle.as_bytes())
    }

    /// Equivalent to [`trim_end_matches`], but takes a byte slice.
    ///
    /// [`trim_end_matches`]: #method.trim_end_matches
    pub const fn trim_end_matches_b(mut self, needle: &[u8]) -> Self {
        parsing! {self, FromEnd;
            self.bytes = crate::slice::bytes_trim_end_matches(self.bytes, needle);
        }
    }

    /// Equivalent to [`trim_end_matches`], but takes a single byte.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::Parser;
    ///
    /// let mut parser = Parser::from_str("world----    ");
    ///
    /// parser = parser.trim_end_matches_u8(b' ');
    /// assert_eq!(parser.bytes(), "world----".as_bytes());
    ///
    /// parser = parser.trim_end_matches_u8(b'-');
    /// assert_eq!(parser.bytes(), "world".as_bytes());
    ///
    /// parser = parser.trim_end_matches_u8(b'-');
    /// assert_eq!(parser.bytes(), "world".as_bytes());
    ///
    /// ```
    ///
    /// [`trim_end_matches`]: #method.trim_end_matches
    pub const fn trim_end_matches_u8(mut self, needle: u8) -> Self {
        parsing! {self, FromEnd;
            while let [rem @ .., b] = self.bytes {
                if *b == needle {
                    self.bytes = rem;
                } else {
                    break;
                }
            }
        }
    }

    //////////////////////////////////////////////
    //           *find_skip* methods            //
    //////////////////////////////////////////////

    /// Skips the parser after the first instance of `needle`.
    ///
    /// For calling `find_skip` with multiple alternative `ǹeedle` string literals,
    /// you can use the [`parse_any`] macro,
    /// [example](../macro.parse_any.html#find-example)
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, unwrap_ctx};
    ///
    /// let mut parser = Parser::from_str("foo--bar,baz--qux");
    ///
    /// parser = unwrap_ctx!(parser.find_skip("--"));
    /// assert_eq!(parser.bytes(), "bar,baz--qux".as_bytes());
    ///
    /// parser = unwrap_ctx!(parser.find_skip("bar,"));
    /// assert_eq!(parser.bytes(), "baz--qux".as_bytes());
    ///
    /// parser = unwrap_ctx!(parser.find_skip("--"));
    /// assert_eq!(parser.bytes(), "qux".as_bytes());
    ///
    /// assert!(parser.find_skip("--").is_err());
    ///
    /// ```
    pub const fn find_skip(self, needle: &str) -> Result<Self, ParseError<'a>> {
        self.find_skip_b(needle.as_bytes())
    }

    /// Equivalent to [`find_skip`], but takes a byte slice.
    ///
    /// [`find_skip`]: #method.find_skip
    pub const fn find_skip_b(mut self, needle: &[u8]) -> Result<Self, ParseError<'a>> {
        try_parsing! {self, FromStart;
            self.bytes = match crate::slice::bytes_find_skip(self.bytes, needle) {
                Some(x) => x,
                None => throw!(ErrorKind::Find),
            };
        }
    }

    /// Equivalent to [`find_skip`], but takes a single byte.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, unwrap_ctx};
    ///
    /// let mut parser = Parser::from_str("foo-bar,baz");
    ///
    /// parser = unwrap_ctx!(parser.find_skip_u8(b'-'));
    /// assert_eq!(parser.bytes(), "bar,baz".as_bytes());
    ///
    /// parser = unwrap_ctx!(parser.find_skip_u8(b','));
    /// assert_eq!(parser.bytes(), "baz".as_bytes());
    ///
    /// ```
    ///
    /// [`find_skip`]: #method.find_skip
    pub const fn find_skip_u8(mut self, needle: u8) -> Result<Self, ParseError<'a>> {
        try_parsing! {self, FromStart;
            while let [byte, rem @ ..] = self.bytes {
                self.bytes = rem;

                if *byte == needle {
                    ret_!();
                }
            }
            throw!(ErrorKind::Find)
        }
    }

    /// Truncates the parsed bytes to before the last instance of `needle`.
    ///
    /// For calling `find_skip` with multiple alternative `ǹeedle` string literals,
    /// you can use the [`parse_any`] macro,
    /// [example](../macro.parse_any.html#find-example)
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, unwrap_ctx};
    ///
    /// let mut parser = Parser::from_str("foo--bar,baz--qux");
    ///
    /// parser = unwrap_ctx!(parser.rfind_skip("--"));
    /// assert_eq!(parser.bytes(), "foo--bar,baz".as_bytes());
    ///
    /// parser = unwrap_ctx!(parser.rfind_skip(",baz"));
    /// assert_eq!(parser.bytes(), "foo--bar".as_bytes());
    ///
    /// parser = unwrap_ctx!(parser.rfind_skip("--"));
    /// assert_eq!(parser.bytes(), "foo".as_bytes());
    ///
    /// assert!(parser.rfind_skip("--").is_err());
    ///
    /// ```
    pub const fn rfind_skip(self, needle: &str) -> Result<Self, ParseError<'a>> {
        self.rfind_skip_b(needle.as_bytes())
    }

    /// Equivalent to [`find_skip`], but takes a byte slice.
    ///
    /// [`find_skip`]: #method.find_skip
    pub const fn rfind_skip_b(mut self, needle: &[u8]) -> Result<Self, ParseError<'a>> {
        try_parsing! {self, FromEnd;
            self.bytes = match crate::slice::bytes_rfind_skip(self.bytes, needle) {
                Some(x) => x,
                None => throw!(ErrorKind::Find),
            };
        }
    }

    /// Equivalent to [`find_skip`], but takes a single byte.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, unwrap_ctx};
    ///
    /// let mut parser = Parser::from_str("foo,bar-baz");
    ///
    /// parser = unwrap_ctx!(parser.rfind_skip_u8(b'-'));
    /// assert_eq!(parser.bytes(), "foo,bar".as_bytes());
    ///
    /// parser = unwrap_ctx!(parser.rfind_skip_u8(b','));
    /// assert_eq!(parser.bytes(), "foo".as_bytes());
    ///
    /// ```
    ///
    /// [`find_skip`]: #method.find_skip
    pub const fn rfind_skip_u8(mut self, needle: u8) -> Result<Self, ParseError<'a>> {
        try_parsing! {self, FromEnd;
            while let [rem @ .., byte] = self.bytes {
                self.bytes = rem;

                if *byte == needle {
                    ret_!();
                }
            }
            throw!(ErrorKind::Find)
        }
    }
}
