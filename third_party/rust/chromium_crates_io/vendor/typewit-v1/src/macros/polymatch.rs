
/// Match which expands top-level `|` patterns to multiple match arms.
/// 
/// [**examples below**](#examples)
/// 
/// ### Clarification
/// 
/// "top-level `|` patterns" means that the `|` is not inside some other pattern.
/// <br>E.g.: the pattern in `Foo(x) | Bar(x) => ` is a top-level `|` pattern.
/// <br>E.g.: the pattern in `(Foo(x) | Bar(x)) => ` is not a top-level `|` pattern, 
/// because the `|` is inside parentheses.
/// 
/// # Syntax
/// 
/// This uses a `macro_rules!`-like syntax for the parameters of this macro
/// 
/// ```text
/// $matched_expression:expr;
///     $( $(|)? $($or_pattern:pat_param)|+ $(if $match_guard:expr)? => $arm_expr:expr ),*
///     $(,)? 
/// ```
/// 
/// [**example demonstrating all of this syntax**](#full-syntax)
/// 
/// # Examples
/// 
/// ### Basic
/// 
/// ```rust
/// assert_eq!(debugify(Ok(3)), "3");
/// assert_eq!(debugify(Err("hello")), r#""hello""#);
/// 
/// fn debugify(res: Result<u32, &'static str>) -> String {
///     typewit::polymatch! {res; 
///         Ok(x) | Err(x) => format!("{:?}", x)
///     }
/// }
/// ```
/// 
/// The above invocation of `polymatch` expands to:
/// 
/// ```rust
/// # assert_eq!(debugify(Ok(3)), "3");
/// # assert_eq!(debugify(Err("hello")), r#""hello""#);
/// # 
/// # fn debugify(res: Result<u32, &'static str>) -> String {
///     match res {
///         Ok(x) => format!("{:?}", x),
///         Err(x) => format!("{:?}", x),
///     }
/// # }
/// ```
/// 
/// ### Full syntax
/// 
/// Example that uses all of the syntax supported by this macro.
/// 
/// ```rust
/// assert_eq!(bar(Foo::Byte(3)), 6);
/// assert_eq!(bar(Foo::Byte(9)), 18);
/// assert_eq!(bar(Foo::Byte(10)), 10);
/// 
/// assert_eq!(bar(Foo::U16(3)), 6);
/// assert_eq!(bar(Foo::U16(9)), 18);
/// assert_eq!(bar(Foo::U16(10)), 10);
/// 
/// assert_eq!(bar(Foo::U32(3)), 0);
/// 
/// assert_eq!(bar(Foo::Long(3)), 0);
/// 
/// enum Foo {
///     Byte(u8),
///     U16(u16),
///     U32(u32),
///     Long(u64),
/// }
/// 
/// const fn bar(foo: Foo) -> u64 {
///     typewit::polymatch! {foo;
///         // top-level  `|` patterns generate a match arm for every alternate pattern
///         | Foo::Byte(x) 
///         | Foo::U16(x) 
///         if x < 10 
///         => (x as u64) * 2,
/// 
///         Foo::Byte(x) | Foo::U16(x) => { x as u64 }
/// 
///         // `|` inside patterns behaves like in regular `match` expressions
///         (Foo::U32(_) | Foo::Long(_)) => 0
///     }
/// }
/// ```
/// 
/// The above invocation of `polymatch` expands to:
/// ```rust
/// # assert_eq!(bar(Foo::Byte(3)), 6);
/// # assert_eq!(bar(Foo::Byte(9)), 18);
/// # assert_eq!(bar(Foo::Byte(10)), 10);
/// # 
/// # assert_eq!(bar(Foo::U16(3)), 6);
/// # assert_eq!(bar(Foo::U16(9)), 18);
/// # assert_eq!(bar(Foo::U16(10)), 10);
/// # 
/// # assert_eq!(bar(Foo::U32(3)), 0);
/// # 
/// # assert_eq!(bar(Foo::Long(3)), 0);
/// # enum Foo {
/// #     Byte(u8),
/// #     U16(u16),
/// #     U32(u32),
/// #     Long(u64),
/// # }
/// # 
/// # const fn bar(foo: Foo) -> u64 {
///     match foo {
///         Foo::Byte(x) if x < 10  => (x as u64) * 2,
///         Foo::U16(x) if x < 10  => (x as u64) * 2,
/// 
///         Foo::Byte(x) => { x as u64 }
///         Foo::U16(x) => { x as u64 }
/// 
///         (Foo::U32(_) | Foo::Long(_)) => 0
///     }
/// # }
/// ```
/// 
#[macro_export]
macro_rules! polymatch {
    ($matching:expr; $($match_arms:tt)*) => {
        $crate::__polymatch!{($matching) () $($match_arms)*}
    };
    ($($tt:tt)*) => {
        $crate::__::compile_error!{"expected arguments to be `matched expression; match arms`"}
    };
}



#[doc(hidden)]
#[macro_export]
macro_rules! __polymatch {
    // Parsing match like syntax
    (
        ($matching:expr)
        ( $( ($($pattern:tt)*) => ($expr:expr) )* )

        // Nothing left to parse
        $(,)?
    ) => {{
        #[allow(unused_parens)]
        match $matching {
            $($($pattern)* => $expr,)*
        }
    }};
    (
        $fixed:tt
        $prev_branch:tt

        $(|)? $($pattern:pat_param)|+ $( if $guard:expr )? => $expr:expr
        $(,$($rem:tt)*)?
    ) => {{
        $crate::__polymatch__handle_guard!{
            $fixed
            $prev_branch
            (($($pattern)+) => $expr)
            ($($guard)?)
            ($($($rem)*)?)
        }
    }};
    (
        $fixed:tt
        $prev_branch:tt

        $(|)? $($pattern:pat_param)|+ $( if $guard:expr )? => $expr:block
        $($rem:tt)*
    ) => {{
        $crate::__polymatch__handle_guard!{
            $fixed
            $prev_branch
            (($($pattern)+) => $expr)
            ($($guard)?)
            ($($rem)*)
        }
    }};
}

#[doc(hidden)]
#[macro_export]
macro_rules! __polymatch__handle_guard {
    (
        $fixed:tt
        ( $($prev_branch:tt)* )
        (($($pattern:tt)+) => $expr:tt)
        ()
        ($($rem:tt)*)
    ) => {
        $crate::__polymatch!{
            $fixed
            (
                $($prev_branch)*
                $(($pattern) => ($expr))*
            )
            $($rem)*
        }
    };
    (
        $fixed:tt
        ( $($prev_branch:tt)* )
        (($($pattern:tt)+) => $expr:tt)
        ($guard:expr)
        ($($rem:tt)*)
    ) => {
        $crate::__polymatch!{
            $fixed
            (
                $($prev_branch)*
                $(($pattern if $guard) => ($expr))*
            )
            $($rem)*
        }
    };
}

