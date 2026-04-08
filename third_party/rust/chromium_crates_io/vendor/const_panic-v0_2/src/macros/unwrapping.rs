/// Gets the value in the `Some` variant.
///
/// # Panics
///
/// Panics if `$opt` is a None.
///
/// # Example
///
/// ```rust
/// use const_panic::unwrap_some;
///
/// const SUM: u8 = unwrap_some!(add_up(&[3, 5, 8, 13]));
///
/// assert_eq!(SUM, 29);
///
///
/// const fn add_up(mut slice: &[u8]) -> Option<u8> {
///     let mut sum = 0u8;
///     
///     while let [x, ref rem @ ..] = *slice {
///         match sum.checked_add(x) {
///             Some(x) => sum = x,
///             None => return None,
///         }
///         slice = rem;
///     }
///     
///     Some(sum)
/// }
///
/// ```
///
///
/// ### Error
///
/// This is what the compile-time error looks like when attempting to unwrap a `None`:
///
/// ```text
/// error[E0080]: evaluation of constant value failed
///  --> src/macros/unwrapping.rs:10:17
///   |
/// 6 | const SUM: u8 = unwrap_some!(add_up(&[3, 5, 8, 13, 250]));
///   |                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ the evaluated program panicked at '
/// invoked `unwrap_some` macro with a `None` value', src/macros/unwrapping.rs:6:17
///   |
///   = note: this error originates in the macro `unwrap_some` (in Nightly builds, run with -Z macro-backtrace for more info)
///
/// ```
///
#[macro_export]
macro_rules! unwrap_some {
    ($opt:expr) => {
        match $opt {
            $crate::__::Some(x) => x,
            $crate::__::None => $crate::concat_panic(&[&[$crate::PanicVal::write_str(
                "\ninvoked `unwrap_some` macro with a `None` value",
            )]]),
        }
    };
}

/// Gets the value in the `Ok` variant.
///
/// # Panics
///
/// This panics if `$res` is an `Err`, including the debug-formatted error in the panic message.
///
/// # Example
///
/// The struct formatting below requires the `"non_basic"` feature (enabled by default)
///
#[cfg_attr(feature = "non_basic", doc = "```rust")]
#[cfg_attr(not(feature = "non_basic"), doc = "```ignore")]
/// use const_panic::unwrap_ok;
///
/// const SUM: u64 = unwrap_ok!(add_up_evens(&[2, 4, 8, 16]));
///
/// assert_eq!(SUM, 30);
///
/// const fn add_up_evens(slice: &[u8]) -> Result<u64, OddError> {
///     let mut sum = 0u64;
///     let mut i = 0;
///
///     while i < slice.len() {
///         let x = slice[i];
///
///         if x % 2 == 1 {
///             return Err(OddError{at: i, number: x});
///         }
///
///         sum += x as u64;
///         i += 1;
///     }
///     
///     Ok(sum)
/// }
///
///
/// struct OddError {
///     at: usize,
///     number: u8,
/// }
///
/// // You can also use `#[derive(PanicFmt))]` with the "derive" feature
/// const_panic::impl_panicfmt!{
///     struct OddError {
///         at: usize,
///         number: u8,
///     }
/// }
///
/// ```
///
/// ### Error
///
/// This is what the compile-time error looks like when attempting to unwrap an `Err`:
///
/// ```text
/// error[E0080]: evaluation of constant value failed
///  --> src/macros/unwrapping.rs:51:18
///   |
/// 6 | const SUM: u64 = unwrap_ok!(add_up_evens(&[3, 5, 8, 13]));
///   |                  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ the evaluated program panicked at '
/// invoked `unwrap_ok` macro with an `Err` value: OddError { at: 0, number: 3 }', src/macros/unwrapping.rs:6:18
///   |
/// ```
#[macro_export]
macro_rules! unwrap_ok {
    ($res:expr) => {
        match $res {
            $crate::__::Ok(x) => x,
            $crate::__::Err(e) => $crate::concat_panic(&[
                &[$crate::PanicVal::write_str(
                    "\ninvoked `unwrap_ok` macro with an `Err` value: ",
                )],
                &$crate::coerce_fmt!(e).to_panicvals($crate::FmtArg::DEBUG),
            ]),
        }
    };
}

/// Gets the value in the `Err` variant.
///
/// # Panics
///
/// This panics if `$res` is an `Ok`, including the debug-formatted value in the panic message.
///
/// # Example
///
/// ```rust
/// use const_panic::unwrap_err;
///
/// type Res = Result<u32, &'static str>;
///
/// const ERR: &str = unwrap_err!(Res::Err("this is an error"));
///
/// assert_eq!(ERR, "this is an error");
///
/// ```
///
/// ### Error
///
/// This is what the compile-time error looks like when attempting to unwrap an `Ok`:
///
/// ```text
/// error[E0080]: evaluation of constant value failed
///  --> src/macros/unwrapping.rs:174:19
///   |
/// 8 | const ERR: &str = unwrap_err!(Res::Ok(1234));
///   |                   ^^^^^^^^^^^^^^^^^^^^^^^^^^ the evaluated program panicked at '
/// invoked `unwrap_err` macro with an `Ok` value: 1234', src/macros/unwrapping.rs:8:19
///   |
/// ```
#[macro_export]
macro_rules! unwrap_err {
    ($res:expr) => {
        match $res {
            $crate::__::Ok(x) => $crate::concat_panic(&[
                &[$crate::PanicVal::write_str(
                    "\ninvoked `unwrap_err` macro with an `Ok` value: ",
                )],
                &$crate::coerce_fmt!(x).to_panicvals($crate::FmtArg::DEBUG),
            ]),
            $crate::__::Err(e) => e,
        }
    };
}
