/// Asserts that `$condition` is true.
///
/// When only the `$condition` argument is passed,
/// this delegates to the [`core::assert`] macro.
///
/// When two or more arguments are passed,
/// this panics with formatting by delegating the second and remaining arguments
/// to the [`concat_panic`](macro@crate::concat_panic) macro.
///
/// ### Examples
///
/// ### Formatted assertion
///
/// ```compile_fail
/// use const_panic::concat_assert;
///
/// const ONE: Even = Even::new(1);
///
/// struct Even(u32);
///
/// impl Even {
///     #[track_caller]
///     const fn new(n: u32) -> Self {
///         concat_assert!(n % 2 == 0, "\nexpected the argument to be even, found: ", n);
///         
///         Even(n)
///     }
/// }
/// ```
/// the above code errors with this message:
/// ```text
/// error[E0080]: evaluation of constant value failed
///  --> src/macros/concat_assert.rs:16:19
///   |
/// 4 | const ONE: Even = Even::new(1);
///   |                   ^^^^^^^^^^^^ the evaluated program panicked at '
/// expected the argument to be even, found: 1', src/macros/concat_assert.rs:4:19
///
/// ```
///
/// ### More formatting
///
/// This example demonstrates what error non-`#[track_caller]` functions produce,
/// and uses the `"non_basic"` feature(enabled by default).
///
/// ```compile_fail
/// use const_panic::concat_assert;
///
/// const SUM: u64 = sum(&[3, 5, 8], 1..40);
///
/// const fn sum(mut slice: &[u32], range: std::ops::Range<usize>) -> u64 {
///     concat_assert!(
///         range.start <= range.end && range.end <= slice.len(),
///         "\ncannot index slice of length `", slice.len(),
///         "` with `", range, "` range"
///     );
///     
///     let mut sum = 0u64;
///     
///     while let [curr, ref rem @ ..] = *slice {
///         sum += curr as u64;
///         
///         slice = rem;
///     }
///     
///     sum
/// }
/// ```
/// the above code errors with this message:
/// ```text
/// error[E0080]: evaluation of constant value failed
///   --> src/macros/concat_assert.rs:52:5
///    |
/// 6  |   const SUM: u64 = sum(&[3, 5, 8], 1..40);
///    |                    ---------------------- inside `SUM` at src/macros/concat_assert.rs:6:18
/// ...
/// 9  | /     concat_assert!(
/// 10 | |         range.start <= range.end && range.end <= slice.len(),
/// 11 | |         "\ncannot index slice of length `", slice.len(),
/// 12 | |         "` with `", range, "` range"
/// 13 | |     );
///    | |     ^
///    | |     |
///    | |_____the evaluated program panicked at '
/// cannot index slice of length `3` with `1..40` range', src/macros/concat_assert.rs:9:5
///    |       inside `_doctest_main_src_macros_concat_assert_rs_46_0::sum` at /home/matias/Documents/proyectos programacion/const_panic/src/macros.rs:240:21
///    |
///    = note: this error originates in the macro `$crate::concat_panic` (in Nightly builds, run with -Z macro-backtrace for more info)
/// ```
///
/// ### Unformatted assertion
///
/// When only the `$condition` argument is passed,
/// this delegates to the [`core::assert`] macro.
///
/// ```compile_fail
/// use const_panic::concat_assert;
///
/// const _: () = concat_assert!(cfg!(any(feature = "foo", feature = "bar")) );
/// ```
/// the above code errors with this message:
/// ```text
/// error[E0080]: evaluation of constant value failed
///  --> src/macros/concat_assert.rs:48:15
///   |
/// 6 | const _: () = concat_assert!(cfg!(any(feature = "foo", feature = "bar")) );
///   |               ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ the evaluated program panicked at 'assertion failed: cfg!(any(feature = \"foo\", feature = \"bar\"))', src/macros/concat_assert.rs:6:15
///   |
///   = note: this error originates in the macro `assert` (in Nightly builds, run with -Z macro-backtrace for more info)
/// ```
///
///
#[macro_export]
macro_rules! concat_assert {
    ($condition:expr $(,)?) => {
        $crate::__::assert!($condition);
    };
    ($condition:expr, $($fmt:tt)*) => {{
        #[allow(clippy::equatable_if_let)]
        if let false = $condition {
            $crate::concat_panic!{$($fmt)*}
        }
    }};
}
