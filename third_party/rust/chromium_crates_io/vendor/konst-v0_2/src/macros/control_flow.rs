/// For loop over a range
///
/// # Example
///
/// ```
/// use konst::for_range;    
///     
/// const LEN: usize = 10;
/// const ARR: [u32; LEN] = {
///     let mut ret = [1; LEN];
///     for_range!{i in 2..LEN =>
///         ret[i] = ret[i - 1] + ret[i - 2];
///     }
///     ret
/// };
///
/// assert_eq!(ARR, [1, 1, 2, 3, 5, 8, 13, 21, 34, 55]);
///
/// ```
#[macro_export]
macro_rules! for_range {
    ($pat:pat in $range:expr => $($code:tt)*) => {
        let $crate::__::Range{mut start, end} = $range;

        while start < end {
            let $pat = start;

            $($code)*

            start+=1;
        }
    };
}

/// Emulates the [inline const feature], eg: `const{ foo() }`,
///
/// As opposed to inline const, you must pass the type that the expression evaluates to.
///
/// # Limitations
///
/// This can't be used with expressions that reference generic parameters.
///
/// # Example
///
/// ```rust
/// use konst::{konst, eq_str};
///
/// const FOO: &str = "hello";
///
/// # const _: bool = konst!{bool, eq_str(FOO, "hi")};
/// #
/// // By using `konst` here, the function is unconditionally evaluated at compile-time.
/// if konst!{bool, eq_str(FOO, "hi")} {
///     panic!("The constants are equal, this wasn't supposed to happen!!");
/// }
///
/// ```
///
///
/// [inline const feature]:
/// https://doc.rust-lang.org/1.50.0/unstable-book/language-features/inline-const.html
#[macro_export]
macro_rules! konst {
    ($type:ty, $expr:expr $(,)*) => {{
        const __KONST__: $type = $expr;
        __KONST__
    }};
}
