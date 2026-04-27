use crate::Parser;

macro_rules! define_parse_methods {
    (
        $((
            $(#[$attr:meta])*
            fn $fn_name:ident,
            fn $fn_name_bytes:ident,
            $parsing:ty,
            $err:ident
            $(,)?
        ))*
    ) => (
        $(
            define_parse_methods_inner!{
                concat!(
                    "Parses a `", stringify!($parsing), "` from a `&str`.\n\n",
                    "This returns an `Err` if the string would not successfully `.parse()` into a `",
                    stringify!($parsing),
                    "`.\n\n",
                    "To parse a `",
                    stringify!($parsing),
                    "` from only part of a string, you can use [`Parser::parse_",
                    stringify!($parsing),
                    "`](../parsing/struct.Parser.html#method.parse_",
                    stringify!($parsing),
                    ")",
                    ".\n\n",
                ),
                concat!(
                    "Like [`", stringify!($fn_name), "`](./fn.", stringify!($fn_name),".html)",
                    "but takes a `&[u8]` argument."
                ),
                $(#[$attr])*,
                $fn_name,
                $fn_name_bytes,
                $parsing,
                $err,
            }
        )*
    );
}
macro_rules! define_parse_methods_inner{
    (
        $s_docs:expr,
        $b_docs:expr,
        $(#[$attr:meta])*,
        $fn_name:ident,
        $fn_name_bytes:ident,
        $parsing:ty,
        $err:ident,
    ) => {
        #[doc = $s_docs]
        $(#[$attr])*
        #[inline]
        #[cfg(feature = "parsing_no_proc")]
        #[cfg_attr(feature = "docsrs", doc(cfg(feature = "parsing_no_proc")))]
        pub const fn $fn_name(s: &str) -> Result<$parsing, $err> {
            $fn_name_bytes(s.as_bytes())
        }

        #[doc = $b_docs]
        #[cfg(feature = "parsing_no_proc")]
        #[cfg_attr(feature = "docsrs", doc(cfg(feature = "parsing_no_proc")))]
        pub const fn $fn_name_bytes(bytes: &[u8]) -> Result<$parsing, $err> {
            match Parser::from_bytes(bytes).$fn_name() {
                Ok((num, parser)) if parser.is_empty() => Ok(num),
                _ => Err($err {
                    _priv: (),
                }),
            }
        }
    }
}

define_parse_methods! {
    (
        /// # Example
        ///
        /// ```rust
        /// use konst::{
        ///     primitive::{ParseIntResult, parse_u128},
        ///     unwrap_ctx,
        /// };
        ///
        /// const I: ParseIntResult<u128> = parse_u128("1000");
        ///
        /// assert_eq!(I, Ok(1000));
        /// assert_eq!(parse_u128("123"), Ok(123));
        /// assert_eq!(parse_u128("0"), Ok(0));
        ///
        /// // This is how you can unwrap integers parsed from strings, at compile-time.
        /// const I2: u128 = unwrap_ctx!(parse_u128("1000"));
        /// assert_eq!(I2, 1000);
        ///
        /// assert!(parse_u128("-1").is_err());
        /// assert!(parse_u128("100A").is_err());
        /// assert!(parse_u128("-").is_err());
        ///
        /// ```
        ///
        fn parse_u128, fn parse_u128_b, u128, ParseIntError
    )
    (
        /// # Example
        ///
        /// ```rust
        /// use konst::{
        ///     primitive::{ParseIntResult, parse_i128},
        ///     unwrap_ctx,
        /// };
        ///
        /// const I: ParseIntResult<i128> = parse_i128("1234");
        ///
        /// assert_eq!(I, Ok(1234));
        /// assert_eq!(parse_i128("123"), Ok(123));
        /// assert_eq!(parse_i128("0"), Ok(0));
        /// assert_eq!(parse_i128("-1"), Ok(-1));
        ///
        /// // This is how you can unwrap integers parsed from strings, at compile-time.
        /// const I2: i128 = unwrap_ctx!(parse_i128("1234"));
        /// assert_eq!(I2, 1234);
        ///
        /// assert!(parse_i128("100A").is_err());
        /// assert!(parse_i128("-A").is_err());
        /// assert!(parse_i128("-").is_err());
        ///
        /// ```
        ///
        fn parse_i128, fn parse_i128_b, i128, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `u128`](./fn.parse_u128.html).
        fn parse_u64, fn parse_u64_b, u64, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `i128`](./fn.parse_i128.html).
        fn parse_i64, fn parse_i64_b, i64, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `u128`](./fn.parse_u128.html).
        fn parse_u32, fn parse_u32_b, u32, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `i128`](./fn.parse_i128.html).
        fn parse_i32, fn parse_i32_b, i32, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `u128`](./fn.parse_u128.html).
        fn parse_u16, fn parse_u16_b, u16, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `i128`](./fn.parse_i128.html).
        fn parse_i16, fn parse_i16_b, i16, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `u128`](./fn.parse_u128.html).
        fn parse_u8, fn parse_u8_b, u8, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `i128`](./fn.parse_i128.html).
        fn parse_i8, fn parse_i8_b, i8, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `u128`](./fn.parse_u128.html).
        fn parse_usize, fn parse_usize_b, usize, ParseIntError
    )
    (
        ///
        /// For an example of how to use this function, you can look at
        /// [the one for `i128`](./fn.parse_i128.html).
        fn parse_isize, fn parse_isize_b, isize, ParseIntError
    )
    (
        /// # Example
        ///
        /// ```rust
        /// use konst::{
        ///     primitive::{ParseBoolResult, parse_bool},
        ///     unwrap_ctx,
        /// };
        ///
        /// const T: ParseBoolResult = parse_bool("true");
        /// const F: ParseBoolResult = parse_bool("false");
        ///
        /// assert_eq!(T, Ok(true));
        /// assert_eq!(F, Ok(false));
        ///
        /// // This is how you can unwrap bools parsed from strings, at compile-time.
        /// const T2: bool = unwrap_ctx!(parse_bool("true"));
        /// const F2: bool = unwrap_ctx!(parse_bool("false"));
        ///
        /// assert_eq!(T2, true);
        /// assert_eq!(F2, false);
        ///
        /// assert!(parse_bool("0").is_err());
        /// assert!(parse_bool("FALSE").is_err());
        ///
        ///
        /// ```
        ///
        fn parse_bool, fn parse_bool_b, bool, ParseBoolError
    )
}

////////////////////////////////////////////////////////////////////////////////

/// An alias for `Result<T, konst::primitive::ParseIntError>`
#[cfg(feature = "parsing_no_proc")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "parsing_no_proc")))]
pub type ParseIntResult<T> = Result<T, ParseIntError>;

/// An alias for `Result<bool, konst::primitive::ParseBoolError>`
#[cfg(feature = "parsing_no_proc")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "parsing_no_proc")))]
pub type ParseBoolResult = Result<bool, ParseBoolError>;

////////////////////////////////////////////////////////////////////////////////

use core::fmt::{self, Display};

/// The error returned by integer-parsing methods.
#[cfg(feature = "parsing_no_proc")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "parsing_no_proc")))]
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct ParseIntError {
    _priv: (),
}

impl Display for ParseIntError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("could not parse an integer")
    }
}

impl ParseIntError {
    /// Panics with this error as the message
    pub const fn panic(&self) -> ! {
        let x = self.something();
        [/*could not parse an integer*/][x]
    }

    const fn something(&self) -> usize {
        0
    }
}

////////////////////////////////////////////////////////////////////////////////

/// The error returned by bool-parsing methods.
#[cfg(feature = "parsing_no_proc")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "parsing_no_proc")))]
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct ParseBoolError {
    _priv: (),
}

impl Display for ParseBoolError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("could not parse a bool")
    }
}

impl ParseBoolError {
    /// Panics with this error as the message
    pub const fn panic(&self) -> ! {
        let x = self.something();
        [/*could not parse a bool*/][x]
    }

    const fn something(&self) -> usize {
        0
    }
}
