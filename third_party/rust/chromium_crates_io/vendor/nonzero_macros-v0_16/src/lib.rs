#![no_std]
#![cfg_attr(feature = "document-features", doc = document_features::document_features!())]
#![doc = include_str!("../README.md")]
#![deny(rustdoc::broken_intra_doc_links)]
#![deny(missing_docs)]
#![deny(unused_must_use)]

//! # `nonzero_macros`
//!
//! A collection of lightweight, `no_std`, dependency-free macros for `LibAFL` and general Rust development.
//!
//! ## Macros
//!
//! - [`nonzero!`](crate::nonzero): A compile-time checked way to create `NonZero*` types. Panics if the value is zero.
//! - [`try_nonzero!`](crate::try_nonzero): A compile-time checked way to create `Option<NonZero*>` types. Returns `None` if the value is zero.
//! - [`nonnull_raw_mut!`](crate::nonnull_raw_mut): A safe wrapper around `&raw mut` that returns a `NonNull` pointer.

/// Zero-cost way to construct `NonZero*` types from [`core::num`] at compile-time.
///
/// This macro ensures that the value is non-zero at compile-time and returns the corresponding `NonZero` type.
/// If the value is zero, it will cause a compile-time panic.
///
/// # Examples
///
/// ```
/// use core::num::NonZeroUsize;
///
/// use nonzero_macros::nonzero;
///
/// const VAL: NonZeroUsize = nonzero!(10);
/// assert_eq!(VAL.get(), 10);
/// ```
///
/// ```compile_fail
/// use nonzero_macros::nonzero;
/// let _ = nonzero!(0); // Panics at compile-time
/// ```
#[macro_export]
macro_rules! nonzero {
    ($val:expr) => {
        const { core::num::NonZero::new($val).unwrap() }
    };
}

/// Construct `Option<NonZero*>` types from [`core::num`].
///
/// This macro creates an `Option<NonZero*>` from a value.
/// It works both at compile-time (in const contexts) and at runtime.
///
/// # Examples
///
/// ```
/// use core::num::NonZeroU8;
///
/// use nonzero_macros::try_nonzero;
///
/// // Compile-time
/// const VAL: Option<NonZeroU8> = try_nonzero!(5);
/// assert!(VAL.is_some());
///
/// const ZERO: Option<NonZeroU8> = try_nonzero!(0);
/// assert!(ZERO.is_none());
///
/// // Runtime
/// let x = 5;
/// let val = try_nonzero!(x);
/// assert!(val.is_some());
/// ```
#[macro_export]
macro_rules! try_nonzero {
    ($val:expr) => {
        core::num::NonZero::new($val)
    };
}

/// Get a [`core::ptr::NonNull`] to a global static mut (or similar).
///
/// The same as [`core::ptr::addr_of_mut`] or `&raw mut`, but wrapped in said [`NonNull`](core::ptr::NonNull).
///
/// # Safety
///
/// The expression must be valid for `&raw mut`. The resulting pointer is guaranteed to be non-null
/// (unless the address of the variable is actually null, which shouldn't happen in safe Rust contexts for statics).
///
/// # Examples
///
/// ```
/// use nonzero_macros::nonnull_raw_mut;
///
/// static mut MY_STATIC: u32 = 0;
///
/// unsafe {
///     let ptr = nonnull_raw_mut!(MY_STATIC);
///     *ptr.as_ptr() = 42;
///     assert_eq!(*ptr.as_ptr(), 42);
/// }
/// ```
#[macro_export]
macro_rules! nonnull_raw_mut {
    ($val:expr) => {{
        let ptr = &raw mut $val;
        assert!(
            !ptr.is_null(),
            "Pointer to value was null in `nonnull_raw_mut!`"
        );
        // # Safety
        // The pointer is checked to be non-null by the assertion above.
        unsafe { core::ptr::NonNull::new_unchecked(ptr) }
    }};
}

#[cfg(test)]
mod tests {
    use core::num::{NonZeroU8, NonZeroUsize};

    #[test]
    fn test_nonzero() {
        const VAL: NonZeroUsize = nonzero!(10);
        assert_eq!(VAL.get(), 10);
    }

    #[test]
    fn test_try_nonzero() {
        // Const context
        const VAL: Option<NonZeroU8> = try_nonzero!(5);
        const ZERO: Option<NonZeroU8> = try_nonzero!(0);

        assert!(VAL.is_some());
        assert_eq!(VAL.unwrap().get(), 5);
        assert!(ZERO.is_none());

        // Runtime context
        let x = 5;
        let val: Option<NonZeroU8> = try_nonzero!(x);
        assert!(val.is_some());
        assert_eq!(val.unwrap().get(), 5);
    }

    #[test]
    fn test_nonnull_raw_mut() {
        static mut VAL: usize = 0;
        let ptr = nonnull_raw_mut!(VAL);
        unsafe {
            *ptr.as_ptr() = 123;
            assert_eq!(*ptr.as_ptr(), 123);
        }
    }
}
