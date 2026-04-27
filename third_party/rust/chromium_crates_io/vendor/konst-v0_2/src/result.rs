//! `const` equivalents of `Result` methods.

/// For unwrapping `Result`s in const contexts with some error message.
///
/// The error type must have a method with this signature:
///
/// ```rust
/// # struct Foo;
/// # impl Foo {
/// pub const fn panic(&self) -> ! {
/// #   loop{}
/// # }
/// # }
/// ```
///
/// The Results returned by [`Parser`] methods can all be used with this macro.
///
/// # Example
///
/// ### Basic
///
#[cfg_attr(feature = "parsing_no_proc", doc = "```rust")]
#[cfg_attr(not(feature = "parsing_no_proc"), doc = "```ignore")]
/// use konst::{Parser, unwrap_ctx};
///
/// let mut parser = Parser::from_str("hello world");
///
/// parser = unwrap_ctx!(parser.strip_prefix("hello "));
///
/// assert_eq!(parser.bytes(), "world".as_bytes());
///
/// ```
///
/// ### Defining error type
///
/// ```rust
/// use konst::unwrap_ctx;
///
/// const UNWRAPPED: u32 = {
///     let res: Result<u32, FooError> = Ok(100);
///     unwrap_ctx!(res)
/// };
///
/// assert_eq!(UNWRAPPED, 100);
///
///
/// use std::fmt::{self, Display};
///
/// #[derive(Debug, Clone, PartialEq)]
/// pub struct FooError(usize);
///
/// impl FooError {
///     pub const fn panic(&self) -> ! {
///         let offset = self.0;
///         [/*Foo errored at offset*/][offset]
///         
///         // Alternatively, using the `const_panic` crate (requires Rust 1.57.0):
///         //
///         // const_panic::concat_panic!("Foo errored at offset: ", self.0)
///         
///         // Alternatively, using the panic macro (requires Rust 1.57.0).
///         // Unfortunately, formatted `panic!`s are not supported const contexts.
///         //
///         // panic!("Foo error")
///     }
/// }
///
/// impl Display for FooError {
///     fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
///         fmt::Debug::fmt(self, f)
///     }
/// }
///
/// impl std::error::Error for FooError {}
///
/// ```
///
/// If `res` was an error instead, this is the error message you would see:
///
/// ```text
/// error: any use of this value will cause an error
///   --> src/result.rs:55:9
///    |
/// 6  | / const UNWRAPPED: u32 = {
/// 7  | |     let res: Result<u32, FooError> = Err(FooError(100));
/// 8  | |     // let res: Result<u32, FooError> = Ok(100);
/// 9  | |     unwrap_ctx!(res)
/// 10 | | };
///    | |__-
/// ...
/// 23 |           [/*Foo errored at offset*/][offset]
///    |           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
///    |           |
///    |           index out of bounds: the length is 0 but the index is 100
///    |           inside `FooError::panic` at src/result.rs:23:9
///    |           inside `UNWRAPPED` at src/result_macros_.rs:6:35
///    |
///    = note: `#[deny(const_err)]` on by default
///
/// ```
///
/// [`Parser`]: ../parsing/struct.Parser.html
#[doc(inline)]
pub use konst_macro_rules::unwrap_ctx;

/// A const equivalent of `Result::unwrap_or`
///
/// # Example
///
/// ```rust
/// use konst::result;
///
/// // Necessary for type inference reasons.
/// type Res = Result<u32, u32>;
///
/// const ARR: &[u32] = &[
///     result::unwrap_or!(Res::Ok(3), 5),
///     result::unwrap_or!(Res::Err(8), 13),
/// ];
///
/// assert_eq!(ARR, &[3, 13]);
///
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::res_unwrap_or as unwrap_or;

/// A const equivalent of `Result::unwrap_or_else`
///
/// # Example
///
/// ```rust
/// use konst::result;
///
/// // Necessary for type inference reasons.
/// type Res = Result<u32, u32>;
///
/// const ARR: &[u32] = &[
///     // You can use a closure-like syntax to run code when the Result argument is Err.
///     // `return` inside the "closure" returns from the function where this macro is called.
///     result::unwrap_or_else!(Res::Ok(3), |_| loop{}),
///     result::unwrap_or_else!(Res::Err(8), |x| x + 5),
///
///     // You can also pass functions
///     result::unwrap_or_else!(Res::Ok(21), add_34),
///     result::unwrap_or_else!(Res::Err(55), add_34),
/// ];
///
/// assert_eq!(ARR, &[3, 13, 21, 89]);
///
/// const fn add_34(n: u32) -> u32 {
///     n + 34
/// }
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::res_unwrap_or_else as unwrap_or_else;

/// Returns the error in the `Err` variant,
/// otherwise runs a closure/function with the value in the `Ok` variant.
///
/// # Example
///
/// ```rust
/// use konst::result;
///
/// // Necessary for type inference reasons.
/// type Res = Result<u32, u32>;
///
/// const ARR: &[u32] = &[
///     // You can use a closure-like syntax to run code when the Result argument is Ok.
///     // `return` inside the "closure" returns from the function where this macro is called.
///     result::unwrap_err_or_else!(Res::Ok(3), |x| x + 2),
///     result::unwrap_err_or_else!(Res::Err(8), |_| loop{}),
///
///     // You can also pass functions
///     result::unwrap_err_or_else!(Res::Ok(16), add_34),
///     result::unwrap_err_or_else!(Res::Err(55), add_34),
/// ];
///
/// assert_eq!(ARR, &[5, 8, 50, 55]);
///
/// const fn add_34(n: u32) -> u32 {
///     n + 34
/// }
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::res_unwrap_err_or_else as unwrap_err_or_else;

/// A const equivalent of `Result::ok`
///
/// # Example
///
/// ```rust
/// use konst::result;
///
/// // Necessary for type inference reasons.
/// type Res = Result<u32, u32>;
///
/// const ARR: &[Option<u32>] = &[
///     result::ok!(Res::Ok(3)),
///     result::ok!(Res::Err(8)),
/// ];
///
/// assert_eq!(ARR, &[Some(3), None]);
///
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::res_ok as ok;

/// A const equivalent of `Result::err`
///
/// # Example
///
/// ```rust
/// use konst::result;
///
/// // Necessary for type inference reasons.
/// type Res = Result<u32, u32>;
///
/// const ARR: &[Option<u32>] = &[
///     result::err!(Res::Ok(3)),
///     result::err!(Res::Err(8)),
/// ];
///
/// assert_eq!(ARR, &[None, Some(8)]);
///
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::res_err as err;

/// A const equivalent of `Result::map`
///
/// # Example
///
/// ```rust
/// use konst::result;
///
/// // Necessary for type inference reasons.
/// type Res = Result<u32, u32>;
///
/// const ARR: &[Res] = &[
///     // You can use a closure-like syntax to run code when the Result argument is Ok.
///     // `return` inside the "closure" returns from the function where this macro is called.
///     result::map!(Res::Ok(3), |x| x + 2),
///     result::map!(Res::Err(8), |_| loop{}),
///
///     // You can also pass functions
///     result::map!(Res::Ok(16), add_34),
///     result::map!(Res::Err(55), add_34),
/// ];
///
/// assert_eq!(ARR, &[Ok(5), Err(8), Ok(50), Err(55)]);
///
/// const fn add_34(n: u32) -> u32 {
///     n + 34
/// }
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::res_map as map;

/// A const equivalent of `Result::map_err`
///
/// # Example
///
/// ```rust
/// use konst::result;
///
/// // Necessary for type inference reasons.
/// type Res = Result<u32, u32>;
///
/// const ARR: &[Res] = &[
///     // You can use a closure-like syntax to run code when the Result argument is Ok.
///     // `return` inside the "closure" returns from the function where this macro is called.
///     result::map_err!(Res::Ok(3), |_| loop{}),
///     result::map_err!(Res::Err(8), |x| x + 5),
///
///     // You can also pass functions
///     result::map_err!(Res::Ok(16), add_34),
///     result::map_err!(Res::Err(55), add_34),
/// ];
///
/// assert_eq!(ARR, &[Ok(3), Err(13), Ok(16), Err(89)]);
///
/// const fn add_34(n: u32) -> u32 {
///     n + 34
/// }
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::res_map_err as map_err;

/// A const equivalent of `Result::and_then`
///
/// # Example
///
/// ```rust
/// use konst::result;
///
/// // Necessary for type inference reasons.
/// type Res = Result<u32, u32>;
///
/// const ARR: &[Res] = &[
///     // You can use a closure-like syntax to run code when the Result argument is Ok.
///     // `return` inside the "closure" returns from the function where this macro is called.
///     result::and_then!(Res::Ok(1), |x| Ok(x + 2)),
///     result::and_then!(Res::Ok(10), |x| Err(x + 4)),
///     result::and_then!(Res::Err(20), |_| loop{}),
///
///     // You can also pass functions
///     result::and_then!(Res::Ok(40), add_2),
///     result::and_then!(Res::Ok(40), add_5),
///     result::and_then!(Res::Err(60), add_5),
/// ];
///
/// assert_eq!(ARR, &[Ok(3), Err(14), Err(20), Ok(42), Err(45), Err(60)]);
///
/// const fn add_2(n: u32) -> Res {
///     Ok(n + 2)
/// }
/// const fn add_5(n: u32) -> Res {
///     Err(n + 5)
/// }
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::res_and_then as and_then;

/// A const equivalent of `Result::or_else`
///
/// # Example
///
/// ```rust
/// use konst::result;
///
/// // Necessary for type inference reasons.
/// type Res = Result<u32, u32>;
///
/// const ARR: &[Res] = &[
///     // You can use a closure-like syntax to run code when the Result argument is Err.
///     // `return` inside the "closure" returns from the function where this macro is called.
///     result::or_else!(Res::Ok(1), |_| loop{}),
///     result::or_else!(Res::Err(20), |x| Ok(x + 5)),
///     result::or_else!(Res::Err(20), |x| Err(x + 7)),
///
///     // You can also pass functions
///     result::or_else!(Res::Ok(40), add_2),
///     result::or_else!(Res::Err(60), add_2),
///     result::or_else!(Res::Err(60), add_5),
/// ];
///
/// assert_eq!(ARR, &[Ok(1), Ok(25), Err(27), Ok(40), Ok(62), Err(65)]);
///
/// const fn add_2(n: u32) -> Res {
///     Ok(n + 2)
/// }
/// const fn add_5(n: u32) -> Res {
///     Err(n + 5)
/// }
/// ```
///
#[doc(inline)]
pub use konst_macro_rules::res_or_else as or_else;
