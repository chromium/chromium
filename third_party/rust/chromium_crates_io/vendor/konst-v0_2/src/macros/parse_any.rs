/// Calls a `Parser` method with many alternative string literals.
///
/// If any of the literals match, the parser is mutated accordingly,
/// otherwise it keeps the unparsed bytes to parse them again.
///
/// # Syntax
///
/// The general syntax for this macro is
/// `parse_any!{ <parser_expression> , <method_name> => <branches> }`
///
/// Where `<parser_expression>` is an expression (of [`Parser`] type) that can be assigned into.
///
/// Where `<method_name>` is the name of one of the [`Parser`] methods usable in this macro:
///
/// - [`find_skip`] and [`rfind_skip`]:
/// use the match-like syntax, running the branch of the first pattern that matches.
/// [example](#find-example)
///
/// - [`strip_prefix`] and [`strip_suffix`]:
/// use the match-like syntax, running the branch of the first pattern that matches.
/// [example](#parsing-enum-example)
///
/// - [`trim_start_matches`] and [`trim_end_matches`]:
/// use the pattern-only syntax, trimming the string while any pattern matches.
/// [example](#trimming-example)
///
/// Where `<branches>` can be either of these, depending on the `<method_name>`:
///
/// - A match-like syntax:
/// A comma separated sequence of `<patterns> => <expression>` branches,
/// the last branch must be `_ => <expression>`.
///
/// - Just the patterns: `<patterns>`.
///
/// Where `<patterns>` can be be any amount of `|`-separated string literals
/// (`const`ants don't work here), or `_`.
/// Note that passing a macro that expands to a literal doesn't work here,
/// `concat` and `stringify` are special cased to work.
///
/// [`find_skip`]: parsing/struct.Parser.html#method.find_skip
/// [`rfind_skip`]: parsing/struct.Parser.html#method.rfind_skip
/// [`strip_prefix`]: parsing/struct.Parser.html#method.strip_prefix
/// [`strip_suffix`]: parsing/struct.Parser.html#method.strip_suffix
/// [`trim_start_matches`]: parsing/struct.Parser.html#method.trim_start_matches
/// [`trim_end_matches`]: parsing/struct.Parser.html#method.trim_end_matches
///
///
/// # Example
///
/// <span id = "parsing-enum-example"></span>
/// ### Parsing enum
///
/// ```rust
/// use konst::{
///     parsing::{Parser, ParseValueResult},
///     parse_any, unwrap_ctx,
/// };
///
/// #[derive(Debug, PartialEq)]
/// enum Color {
///     Red,
///     Blue,
///     Green,
/// }
///
/// impl Color {
///     pub const fn try_parse(mut parser: Parser<'_>) -> ParseValueResult<'_, Color> {
///         parse_any!{parser, strip_prefix;
///             "Red"|"red" => Ok((Color::Red, parser)),
///             "Blue"|"blue" => Ok((Color::Blue, parser)),
///             "Green"|"green" => Ok((Color::Green, parser)),
///             _ => Err(parser.into_other_error()),
///         }
///     }
/// }
///
/// const COLORS: [Color; 4] = {
///     let parser = Parser::from_str("BlueRedGreenGreen");
///     let (c0, parser) = unwrap_ctx!(Color::try_parse(parser));
///     let (c1, parser) = unwrap_ctx!(Color::try_parse(parser));
///     let (c2, parser) = unwrap_ctx!(Color::try_parse(parser));
///     let (c3, _     ) = unwrap_ctx!(Color::try_parse(parser));
///     
///     [c0, c1, c2, c3]
/// };
///
/// assert_eq!(COLORS, [Color::Blue, Color::Red, Color::Green, Color::Green])
///
///
/// ```
///
/// <span id = "find-example"></span>
/// ### `find_skip`
///
/// ```rust
/// use konst::{
///     parsing::{Parser, ParseValueResult},
///     parse_any, unwrap_ctx,
/// };
///
/// {
///     let mut parser = Parser::from_str("baz_foo_bar_foo");
///
///     fn find(parser: &mut Parser<'_>) -> u32 {
///         let before = parser.bytes();
///         parse_any!{*parser, find_skip;
///             "foo" => 0,
///             "bar" => 1,
///             "baz" => 2,
///             _ => {
///                 assert_eq!(before, parser.bytes());
///                 3
///             }
///         }
///     }
///
///     assert_eq!(find(&mut parser), 2);
///     assert_eq!(parser.bytes(), "_foo_bar_foo".as_bytes());
///     
///     assert_eq!(find(&mut parser), 0);
///     assert_eq!(parser.bytes(), "_bar_foo".as_bytes());
///     
///     assert_eq!(find(&mut parser), 1);
///     assert_eq!(parser.bytes(), "_foo".as_bytes());
///     
///     assert_eq!(find(&mut parser), 0);
///     assert_eq!(parser.bytes(), "".as_bytes());
///     
///     assert_eq!(find(&mut parser), 3);
///     assert_eq!(parser.bytes(), "".as_bytes());
/// }
/// {
///     let mut parser = Parser::from_str("foo_bar_foo_baz");
///
///     fn rfind(parser: &mut Parser<'_>) -> u32 {
///         parse_any!{*parser, rfind_skip;
///             "foo" => 0,
///             "bar" => 1,
///             "baz" => 2,
///             _ => 3,
///         }
///     }
///
///     assert_eq!(rfind(&mut parser), 2);
///     assert_eq!(parser.bytes(), "foo_bar_foo_".as_bytes());
///     
///     assert_eq!(rfind(&mut parser), 0);
///     assert_eq!(parser.bytes(), "foo_bar_".as_bytes());
///     
///     assert_eq!(rfind(&mut parser), 1);
///     assert_eq!(parser.bytes(), "foo_".as_bytes());
///     
///     assert_eq!(rfind(&mut parser), 0);
///     assert_eq!(parser.bytes(), "".as_bytes());
///     
///     assert_eq!(rfind(&mut parser), 3);
///     assert_eq!(parser.bytes(), "".as_bytes());
/// }
///
///
///
/// ```
///
/// <span id = "trimming-example"></span>
/// ### Trimming
///
/// ```rust
/// use konst::{
///     parsing::{Parser, ParseValueResult},
///     parse_any, unwrap_ctx,
/// };
///
/// {
///     let mut parser = Parser::from_str("foobarhellofoobar");
///     parse_any!{parser, trim_start_matches; "foo" | "bar" }
///     assert_eq!(parser.bytes(), "hellofoobar".as_bytes());
/// }
/// {
///     let mut parser = Parser::from_str("foobarhellofoobar");
///     parse_any!{parser, trim_end_matches; "foo" | "bar" }
///     assert_eq!(parser.bytes(), "foobarhello".as_bytes());
/// }
/// {
///     let mut parser = Parser::from_str("foobar");
///     // Empty string literals make trimming finish when the previous patterns didn't match,
///     // so the "bar" pattern doesn't do anything here
///     parse_any!{parser, trim_start_matches; "foo" | "" | "bar" }
///     assert_eq!(parser.bytes(), "bar".as_bytes());
/// }
/// ```
///
/// [`Parser`]: parsing/struct.Parser.html
#[cfg(feature = "parsing")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "parsing")))]
#[macro_export]
macro_rules! parse_any {
    ($place:expr, find_skip; $($branches:tt)* ) => {
        $crate::__priv_pa_normalize_branches!{
            ($place, FromStart, __priv_pa_find_skip, outside_konst)
            ()
            $($branches)*
        }
    };
    ($place:expr, rfind_skip; $($branches:tt)* ) => {
        $crate::__priv_pa_normalize_branches!{
            ($place, FromEnd, __priv_pa_rfind_skip, outside_konst)
            ()
            $($branches)*
        }
    };
    ($place:expr, strip_prefix; $($branches:tt)* ) => {
        $crate::__priv_pa_normalize_branches!{
            ($place, FromStart, __priv_pa_strip_prefix, outside_konst)
            ()
            $($branches)*
        }
    };
    ($place:expr, strip_suffix; $($branches:tt)* ) => {
        $crate::__priv_pa_normalize_branches!{
            ($place, FromEnd, __priv_pa_strip_suffix, outside_konst)
            ()
            $($branches)*
        }
    };
    ($place:expr, trim_start_matches; $($branches:tt)* ) => {
        $crate::__priv_pa_normalize_branches!{
            ($place, FromStart, __priv_pa_trim_start_matches, outside_konst)
            ()
            $($branches)*
        }
    };
    ($place:expr, trim_end_matches; $($branches:tt)* ) => {
        $crate::__priv_pa_normalize_branches!{
            ($place, FromEnd, __priv_pa_trim_end_matches, outside_konst)
            ()
            $($branches)*
        }
    };
    ($place:expr, $unknown_method:ident; $($branches:tt)* ) => {
        $crate::__::compile_error!{"\
            Expected the second argument (the name of the Parser method) to be one of: \n\
                - find_skip \n\
                - rfind_skip \n\
                - strip_prefix \n\
                - strip_suffix \n\
                - trim_start_matches \n\
                - trim_end_matches \n\
        "}
    };
}

#[allow(unused_macros)]
macro_rules! parse_any_priv {
    ($place:expr, $parse_direction:ident,$method_macro:ident; $($branches:tt)* ) => {
        $crate::__priv_pa_normalize_branches!{
            ($place, $parse_direction, $method_macro, inside_konst)
            ()
            $($branches)*
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_pa_normalize_branches {
    // Parsing just pattens
    (
        ($place:expr, $parse_direction:ident, $method_macro:ident, $call_place:tt)
        ()

        $($pattern:pat)|*
    ) => {
        $crate::$method_macro!{
            ($place, $parse_direction, $call_place)

            $($pattern)|*
        }
    };

    // Parsing match like syntax
    (
        ($place:expr, $parse_direction:ident, $method_macro:ident, $call_place:tt)
        ( $($branches:tt)* )

        _ => $expr:expr
        // Nothing left to parse
        $(, $($rem:tt)*)?
    ) => {{
        $crate::__priv_no_tokens_after_last_branch!{$($($rem)*)?}

        $crate::$method_macro!{
            ($place, $parse_direction, $call_place)

            $($branches)*
            default => ($expr)
        }
    }};
    (
        $fixed_params:tt
        ( $($prev_branch:tt)* )

        $($pattern:pat)|* => $expr:expr,
        $($rem:tt)*
    ) => {{
        $crate::__priv_tokens_after_middle_branch!{$($rem)*}

        $crate::__priv_pa_normalize_branches!{
            $fixed_params
            (
                $($prev_branch)*

                ($($pattern)|*) => ($expr)
            )
            $($rem)*
        }
    }};
    (
        $fixed_params:tt
        ( $($prev_branch:tt)* )

        $($pattern:pat)|* => $expr:block
        $($rem:tt)*
    ) => {{
        $crate::__priv_tokens_after_middle_branch!{$($rem)*}

        $crate::__priv_pa_normalize_branches!{
            $fixed_params
            (
                $($prev_branch)*

                ($($pattern)|*) => ($expr)
            )
            $($rem)*
        }
    }};
}

//////////////////////////////////////////////////////////////////////////////////

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_pa_find_skip {
    ( $($args:tt)* ) => {{
        $crate::__priv_pa_find_skip_either!{
            brem, [_, brem @ ..], __priv_bstr_start,

            $($args)*
        }
    }};
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_pa_rfind_skip {
    ( $($args:tt)* ) => {{
        $crate::__priv_pa_find_skip_either!{
            brem, [brem @ .., _], __priv_bstr_end,

            $($args)*
        }
    }};
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_pa_find_skip_either {
    (
        $brem:ident,
        $split_first_pat:pat,
        $pat_proc_macro:ident,

        $accessor_args:tt

        $(
            ($($pattern:pat)|*)=>($e:expr)
        )*
        default => ($default:expr)
    ) => {{
        let mut bytes = $crate::__priv_pa_bytes_accessor!(get, $accessor_args);

        loop {
            match bytes {
                $(
                    $( $crate::$pat_proc_macro!(rem, $pattern))|* => {
                        $crate::__priv_pa_bytes_accessor!(set, $accessor_args, rem);
                        break $e
                    }
                )*
                _ => {
                    if let $split_first_pat = bytes {
                        bytes = $brem;
                    } else {
                        break $default;
                    }
                }
            }
        }
    }}
}

//////////////////////////////////////////////////////////////////////////////////

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_pa_strip_prefix {
    (
        $accessor_args:tt

        $(
            ($($pattern:pat)|*)=>($e:expr)
        )*
        default => $default:expr
    ) => {
        match $crate::__priv_pa_bytes_accessor!(get, $accessor_args) {
            $(
                $( $crate::__priv_bstr_start!(rem, $pattern))|* => {
                    $crate::__priv_pa_bytes_accessor!(set, $accessor_args, rem);
                    $e
                }
            )*
            _ => $default,
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_pa_strip_suffix {
    (
        $accessor_args:tt

        $(
            ($($pattern:pat)|*)=>($e:expr)
        )*
        default => ($default:expr)
    ) => {
        match $crate::__priv_pa_bytes_accessor!(get, $accessor_args) {
            $(
                $( $crate::__priv_bstr_end!(rem, $pattern))|* => {
                    $crate::__priv_pa_bytes_accessor!(set, $accessor_args, rem);
                    $e
                }
            )*
            _ => $default,
        }
    };
}

//////////////////////////////////////////////////////////////////////////////////

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_pa_trim_start_matches {
    ( $($args:tt)* ) => {
        $crate::__priv_pa_trim_matches_inner!{__priv_bstr_start $($args)*}
    }
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_pa_trim_end_matches {
    ( $($args:tt)* ) => {
        $crate::__priv_pa_trim_matches_inner!{__priv_bstr_end $($args)*}
    }
}

#[doc(hidden)]
#[macro_export]
macro_rules!  __priv_pa_trim_matches_inner{
    (
        $pat_proc_macro:ident

        $accessor_args:tt
        $($pattern:pat)|*
    ) => {{
        let mut bytes = $crate::__priv_pa_bytes_accessor!(get, $accessor_args);

        while let $( $crate::$pat_proc_macro!(rem, $pattern) )|* = bytes {
            if rem.len() == bytes.len() {
                break
            } else {
                bytes = rem;
            }
        }

        $crate::__priv_pa_bytes_accessor!(set, $accessor_args, bytes);
    }}
}

//////////////////////////////////////////////////////////////////////////////////

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_pa_bytes_accessor {
    (get, ($place:expr, $parse_direction:ident, inside_konst)) => {{
        $place.parse_direction = $crate::parsing::ParseDirection::$parse_direction;
        $place.bytes
    }};
    (get, ($place:expr, $parse_direction:ident, outside_konst)) => {
        $place.bytes()
    };
    (set, ($place:expr, FromStart, inside_konst), $rem:expr) => {
        #[allow(unused_assignments)]
        {
            let before = $place.bytes.len();
            $place.bytes = $rem;
            $place.start_offset += (before - $place.bytes.len()) as u32;
        }
    };
    (set, ($place:expr, FromEnd, inside_konst), $rem:expr) => {
        #[allow(unused_assignments)]
        {
            $place.bytes = $rem;
        }
    };

    (set, ($place:expr, FromStart, outside_konst), $rem:expr) => {
        #[allow(unused_assignments)]
        {
            $place = $place.advance_to_remainder_from_start($rem);
        }
    };
    (set, ($place:expr, FromEnd, outside_konst), $rem:expr) => {
        #[allow(unused_assignments)]
        {
            $place = $place.advance_to_remainder_from_end($rem);
        }
    };
}

//////////////////////////////////////////////////////////////////////////////////

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_no_tokens_after_last_branch {
    () => {};
    ($($tokens:tt)+) => {
        compile_error! {"expected no branches after the first `_ => <expression>` branch"}
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_tokens_after_middle_branch {
    () => {
        compile_error! {"expected more branches, ending with a `_ => <expression>` branch"}
    };
    ($($tokens:tt)+) => {};
}
