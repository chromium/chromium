use super::{ErrorKind, ParseDirection, ParseValueResult, Parser};

impl<'a> Parser<'a> {
    /// Parses a `u128` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_u128`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `u128`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{
    ///     parsing::{Parser, ParseValueResult},
    ///     unwrap_ctx,
    ///     try_rebind,
    /// };
    ///
    /// {
    ///     let parser = Parser::from_str("12345");
    ///     let (num, parser) = unwrap_ctx!(parser.parse_u128());
    ///     assert_eq!(num, 12345);
    ///     assert!(parser.bytes().is_empty());
    /// }
    ///
    /// /// Parses a `[u128; 2]` from a parser starting with `"<number>;<number>", eg: `"100;400"`.
    /// const fn parse_pair(mut parser: Parser<'_>) -> ParseValueResult<'_, [u128; 2]> {
    ///     let mut ret = [0; 2];
    ///     
    ///     // `try_rebind` is like the `?` operator,
    ///     // and it assigns the value in the Ok variant into either a
    ///     // single pre-existing variable or multiple (if the Ok value is a tuple)
    ///     try_rebind!{(ret[0], parser) = parser.parse_u128()};
    ///     
    ///     // parsing the `;``between the integers.
    ///     //
    ///     // Note that because we don't use `.trim_start()` afterwards,
    ///     // this can't be followed by spaces.
    ///     try_rebind!{parser = parser.strip_prefix(";")};
    ///     
    ///     try_rebind!{(ret[1], parser) = parser.parse_u128()};
    ///     
    ///     Ok((ret, parser))
    /// }
    /// const PAIR: ([u128; 2], Parser<'_>) = {
    ///     let parser = Parser::from_str("1365;6789");
    ///     unwrap_ctx!(parse_pair(parser))
    /// };
    ///
    /// assert_eq!(PAIR.0[0], 1365);
    /// assert_eq!(PAIR.0[1], 6789);
    ///
    /// assert!(PAIR.1.is_empty());
    ///
    /// ```
    ///
    /// [`primitive::parse_u128`]: ../primitive/fn.parse_u128.html
    pub const fn parse_u128(mut self) -> ParseValueResult<'a, u128> {
        parse_integer! {unsigned, (u128, u128), self}
    }
    /// Parses a `i128` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_i128`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `i128`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, unwrap_ctx, rebind_if_ok};
    ///
    /// {
    ///     let parser = Parser::from_str("12345");
    ///     let (num, parser) = unwrap_ctx!(parser.parse_i128());
    ///     assert_eq!(num, 12345);
    ///     assert!(parser.bytes().is_empty());
    /// }
    /// {
    ///     let mut num = 0;
    ///     let mut parser = Parser::from_str("-54321;6789");
    ///     
    ///     // `rebind_if_ok` stores the return value of `.parse_i128()` in `num` and `parser`,
    ///     // if `.parse_i128()` returned an `Ok((u128, Parser))`.
    ///     rebind_if_ok!{(num, parser) = parser.parse_i128()}
    ///     assert_eq!(num, -54321);
    ///     assert_eq!(parser.bytes(), b";6789");
    ///
    ///     rebind_if_ok!{parser = parser.strip_prefix(";")}
    ///     assert_eq!(parser.bytes(), b"6789");
    ///
    ///     rebind_if_ok!{(num, parser) = parser.parse_i128()}
    ///     assert_eq!(num, 6789);
    ///     assert!(parser.is_empty());
    /// }
    ///
    /// ```
    ///
    /// [`primitive::parse_i128`]: ../primitive/fn.parse_i128.html
    pub const fn parse_i128(mut self) -> ParseValueResult<'a, i128> {
        parse_integer! {signed, (i128, u128), self}
    }
    /// Parses a `u64` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_u64`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `u64`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// For an example for how to use this method,
    /// you can look at the docs for the [`Parser::parse_u128`](#method.parse_u128) method.
    ///
    /// [`primitive::parse_u64`]: ../primitive/fn.parse_u64.html
    pub const fn parse_u64(mut self) -> ParseValueResult<'a, u64> {
        parse_integer! {unsigned, (u64, u64), self}
    }
    /// Parses a `i64` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_i64`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `i64`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// For an example for how to use this method,
    /// you can look at the docs for the [`Parser::parse_i128`](#method.parse_i128) method.
    ///
    /// [`primitive::parse_i64`]: ../primitive/fn.parse_i64.html
    pub const fn parse_i64(mut self) -> ParseValueResult<'a, i64> {
        parse_integer! {signed, (i64, u64), self}
    }
    /// Parses a `u32` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_u32`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `u32`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// For an example for how to use this method,
    /// you can look at the docs for the [`Parser::parse_u128`](#method.parse_u128) method.
    ///
    /// [`primitive::parse_u32`]: ../primitive/fn.parse_u32.html
    pub const fn parse_u32(mut self) -> ParseValueResult<'a, u32> {
        parse_integer! {unsigned, (u32, u32), self}
    }
    /// Parses a `i32` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_i32`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `i32`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// For an example for how to use this method,
    /// you can look at the docs for the [`Parser::parse_i128`](#method.parse_i128) method.
    ///
    /// [`primitive::parse_i32`]: ../primitive/fn.parse_i32.html
    pub const fn parse_i32(mut self) -> ParseValueResult<'a, i32> {
        parse_integer! {signed, (i32, u32), self}
    }
    /// Parses a `u16` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_u16`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `u16`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// For an example for how to use this method,
    /// you can look at the docs for the [`Parser::parse_u128`](#method.parse_u128) method.
    ///
    /// [`primitive::parse_u16`]: ../primitive/fn.parse_u16.html
    pub const fn parse_u16(mut self) -> ParseValueResult<'a, u16> {
        parse_integer! {unsigned, (u16, u16), self}
    }
    /// Parses a `i16` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_i16`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `i16`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// For an example for how to use this method,
    /// you can look at the docs for the [`Parser::parse_i128`](#method.parse_i128) method.
    ///
    /// [`primitive::parse_i16`]: ../primitive/fn.parse_i16.html
    pub const fn parse_i16(mut self) -> ParseValueResult<'a, i16> {
        parse_integer! {signed, (i16, u16), self}
    }
    /// Parses a `u8` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_u8`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `u8`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// For an example for how to use this method,
    /// you can look at the docs for the [`Parser::parse_u128`](#method.parse_u128) method.
    ///
    /// [`primitive::parse_u8`]: ../primitive/fn.parse_u8.html
    pub const fn parse_u8(mut self) -> ParseValueResult<'a, u8> {
        parse_integer! {unsigned, (u8, u8), self}
    }
    /// Parses a `i8` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_i8`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `i8`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// For an example for how to use this method,
    /// you can look at the docs for the [`Parser::parse_i128`](#method.parse_i128) method.
    ///
    /// [`primitive::parse_i8`]: ../primitive/fn.parse_i8.html
    pub const fn parse_i8(mut self) -> ParseValueResult<'a, i8> {
        parse_integer! {signed, (i8, u8), self}
    }
    /// Parses a `usize` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_usize`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `usize`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// [`primitive::parse_usize`]: ../primitive/fn.parse_usize.html
    pub const fn parse_usize(mut self) -> ParseValueResult<'a, usize> {
        parse_integer! {unsigned, (usize, usize), self}
    }
    /// Parses a `isize` until a non-digit is reached.
    ///
    /// To parse an integer from an entire string (erroring on non-digit bytes),
    /// you can use [`primitive::parse_isize`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `isize`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// For an example for how to use this method,
    /// you can look at the docs for the [`Parser::parse_i128`](#method.parse_i128) method.
    ///
    /// [`primitive::parse_isize`]: ../primitive/fn.parse_isize.html
    pub const fn parse_isize(mut self) -> ParseValueResult<'a, isize> {
        parse_integer! {signed, (isize, usize), self}
    }
}

macro_rules! parse_integer {
    ($signedness:ident, ($type:ty, $uns:ty), $parser:ident) => (try_parsing! {
        $parser, FromStart, ret;{
            let mut num: $uns;

            parse_integer! {@parse_signed $signedness, ($type, $uns), $parser, num, sign}

            while let [byte @ b'0'..=b'9', rem @ ..] = $parser.bytes {
                $parser.bytes = rem;

                let (next_mul, overflowed_mul) = num.overflowing_mul(10);
                let (next_add, overflowed_add) = next_mul.overflowing_add((*byte - b'0') as $uns);

                if overflowed_mul | overflowed_add {
                    throw!(ErrorKind::ParseInteger)
                }

                num = next_add;
            }

            parse_integer! {@apply_sign $signedness, ($type, $uns), num, sign}

            num
        }
    });
    (@parse_signed signed, ($type:ty, $uns:ty), $parser:ident, $num:ident, $isneg:ident) => {
        let $isneg = if let [b'-', rem @ ..] = $parser.bytes {
            $parser.bytes = rem;
            true
        } else {
            false
        };

        parse_integer!(@parse_signed unsigned, ($type, $uns), $parser, $num, $isneg)
    };
    (@parse_signed unsigned, ($type:ty, $uns:ty), $parser:ident, $num:ident, $isneg:ident) => {
        $num = if let [byte @ b'0'..=b'9', rem @ ..] = $parser.bytes {
            $parser.bytes = rem;
            (*byte - b'0') as $uns
        } else {
            throw!(ErrorKind::ParseInteger)
        };
    };
    (@apply_sign signed, ($type:ty, $uns:ty), $num:ident, $isneg:ident) => {
        const MAX_POS: $uns = <$type>::MAX as $uns;
        const MAX_NEG: $uns = <$type>::MIN as $uns;

        let $num = if $isneg {
            if $num <= MAX_NEG {
                ($num as $type).wrapping_neg()
            } else {
                throw!(ErrorKind::ParseInteger)
            }
        } else {
            if $num <= MAX_POS {
                $num as $type
            } else {
                throw!(ErrorKind::ParseInteger)
            }
        };
    };
    (@apply_sign unsigned, ($type:ty, $uns:ty), $num:ident, $isneg:ident) => {};
}
use parse_integer;

////////////////////////////////////////////////////////////////////////////////

impl<'a> Parser<'a> {
    /// Parses a `bool`.
    ///
    /// To parse a bool from an entire string
    /// (erroring if the string isn't exactly `"true"` or `"false"`),
    /// you can use [`primitive::parse_bool`]
    ///
    /// You also can use the [`parse_with`](../macro.parse_with.html)
    /// macro to parse a `bool`, and other [`ParserFor`](./trait.ParserFor.html) types.
    ///
    /// # Example
    ///
    /// ```rust
    /// use konst::{Parser, unwrap_ctx};
    ///
    /// {
    ///     let parser = Parser::from_str("falsemorestring");
    ///     let (boolean, parser) = unwrap_ctx!(parser.parse_bool());
    ///     assert_eq!(boolean, false);
    ///     assert_eq!(parser.bytes(), "morestring".as_bytes());
    /// }
    /// {
    ///     let parser = Parser::from_str("truefoo");
    ///     let (boolean, parser) = unwrap_ctx!(parser.parse_bool());
    ///     assert_eq!(boolean, true);
    ///     assert_eq!(parser.bytes(), "foo".as_bytes());
    /// }
    ///
    /// ```
    ///
    /// [`primitive::parse_bool`]: ../primitive/fn.parse_bool.html
    pub const fn parse_bool(mut self) -> ParseValueResult<'a, bool> {
        try_parsing! {self, FromStart, ret;
            match self.bytes {
                [b't', b'r', b'u', b'e', rem @ ..] => {
                    self.bytes = rem;
                    true
                }
                [b'f', b'a', b'l', b's', b'e', rem @ ..] => {
                    self.bytes = rem;
                    false
                }
                _ => throw!(ErrorKind::ParseBool),
            }
        }
    }
}
