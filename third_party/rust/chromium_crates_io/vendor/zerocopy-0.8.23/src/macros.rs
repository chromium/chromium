// Copyright 2024 The Fuchsia Authors
//
// Licensed under the 2-Clause BSD License <LICENSE-BSD or
// https://opensource.org/license/bsd-2-clause>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

/// Safely transmutes a value of one type to a value of another type of the same
/// size.
///
/// This macro behaves like an invocation of this function:
///
/// ```ignore
/// const fn transmute<Src, Dst>(src: Src) -> Dst
/// where
///     Src: IntoBytes,
///     Dst: FromBytes,
///     size_of::<Src>() == size_of::<Dst>(),
/// {
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// However, unlike a function, this macro can only be invoked when the types of
/// `Src` and `Dst` are completely concrete. The types `Src` and `Dst` are
/// inferred from the calling context; they cannot be explicitly specified in
/// the macro invocation.
///
/// Note that the `Src` produced by the expression `$e` will *not* be dropped.
/// Semantically, its bits will be copied into a new value of type `Dst`, the
/// original `Src` will be forgotten, and the value of type `Dst` will be
/// returned.
///
/// # Examples
///
/// ```
/// # use zerocopy::transmute;
/// let one_dimensional: [u8; 8] = [0, 1, 2, 3, 4, 5, 6, 7];
///
/// let two_dimensional: [[u8; 4]; 2] = transmute!(one_dimensional);
///
/// assert_eq!(two_dimensional, [[0, 1, 2, 3], [4, 5, 6, 7]]);
/// ```
///
/// # Use in `const` contexts
///
/// This macro can be invoked in `const` contexts.
#[macro_export]
macro_rules! transmute {
    ($e:expr) => {{
        // NOTE: This must be a macro (rather than a function with trait bounds)
        // because there's no way, in a generic context, to enforce that two
        // types have the same size. `core::mem::transmute` uses compiler magic
        // to enforce this so long as the types are concrete.

        let e = $e;
        if false {
            // This branch, though never taken, ensures that the type of `e` is
            // `IntoBytes` and that the type of this macro invocation expression
            // is `FromBytes`.

            struct AssertIsIntoBytes<T: $crate::IntoBytes>(T);
            let _ = AssertIsIntoBytes(e);

            struct AssertIsFromBytes<U: $crate::FromBytes>(U);
            #[allow(unused, unreachable_code)]
            let u = AssertIsFromBytes(loop {});
            u.0
        } else {
            // SAFETY: `core::mem::transmute` ensures that the type of `e` and
            // the type of this macro invocation expression have the same size.
            // We know this transmute is safe thanks to the `IntoBytes` and
            // `FromBytes` bounds enforced by the `false` branch.
            //
            // We use this reexport of `core::mem::transmute` because we know it
            // will always be available for crates which are using the 2015
            // edition of Rust. By contrast, if we were to use
            // `std::mem::transmute`, this macro would not work for such crates
            // in `no_std` contexts, and if we were to use
            // `core::mem::transmute`, this macro would not work in `std`
            // contexts in which `core` was not manually imported. This is not a
            // problem for 2018 edition crates.
            let u = unsafe {
                // Clippy: We can't annotate the types; this macro is designed
                // to infer the types from the calling context.
                #[allow(clippy::missing_transmute_annotations)]
                $crate::util::macro_util::core_reexport::mem::transmute(e)
            };
            $crate::util::macro_util::must_use(u)
        }
    }}
}

/// Safely transmutes a mutable or immutable reference of one type to an
/// immutable reference of another type of the same size and compatible
/// alignment.
///
/// This macro behaves like an invocation of this function:
///
/// ```ignore
/// const fn transmute_ref<'src, 'dst, Src, Dst>(src: &'src Src) -> &'dst Dst
/// where
///     'src: 'dst,
///     Src: IntoBytes + Immutable,
///     Dst: FromBytes + Immutable,
///     size_of::<Src>() == size_of::<Dst>(),
///     align_of::<Src>() >= align_of::<Dst>(),
/// {
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// However, unlike a function, this macro can only be invoked when the types of
/// `Src` and `Dst` are completely concrete. The types `Src` and `Dst` are
/// inferred from the calling context; they cannot be explicitly specified in
/// the macro invocation.
///
/// # Examples
///
/// ```
/// # use zerocopy::transmute_ref;
/// let one_dimensional: [u8; 8] = [0, 1, 2, 3, 4, 5, 6, 7];
///
/// let two_dimensional: &[[u8; 4]; 2] = transmute_ref!(&one_dimensional);
///
/// assert_eq!(two_dimensional, &[[0, 1, 2, 3], [4, 5, 6, 7]]);
/// ```
///
/// # Use in `const` contexts
///
/// This macro can be invoked in `const` contexts.
///
/// # Alignment increase error message
///
/// Because of limitations on macros, the error message generated when
/// `transmute_ref!` is used to transmute from a type of lower alignment to a
/// type of higher alignment is somewhat confusing. For example, the following
/// code:
///
/// ```compile_fail
/// const INCREASE_ALIGNMENT: &u16 = zerocopy::transmute_ref!(&[0u8; 2]);
/// ```
///
/// ...generates the following error:
///
/// ```text
/// error[E0512]: cannot transmute between types of different sizes, or dependently-sized types
///  --> src/lib.rs:1524:34
///   |
/// 5 | const INCREASE_ALIGNMENT: &u16 = zerocopy::transmute_ref!(&[0u8; 2]);
///   |                                  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
///   |
///   = note: source type: `AlignOf<[u8; 2]>` (8 bits)
///   = note: target type: `MaxAlignsOf<[u8; 2], u16>` (16 bits)
///   = note: this error originates in the macro `$crate::assert_align_gt_eq` which comes from the expansion of the macro `transmute_ref` (in Nightly builds, run with -Z macro-backtrace for more info)
/// ```
///
/// This is saying that `max(align_of::<T>(), align_of::<U>()) !=
/// align_of::<T>()`, which is equivalent to `align_of::<T>() <
/// align_of::<U>()`.
#[macro_export]
macro_rules! transmute_ref {
    ($e:expr) => {{
        // NOTE: This must be a macro (rather than a function with trait bounds)
        // because there's no way, in a generic context, to enforce that two
        // types have the same size or alignment.

        // Ensure that the source type is a reference or a mutable reference
        // (note that mutable references are implicitly reborrowed here).
        let e: &_ = $e;

        #[allow(unused, clippy::diverging_sub_expression)]
        if false {
            // This branch, though never taken, ensures that the type of `e` is
            // `&T` where `T: 't + Sized + IntoBytes + Immutable`, that the type of
            // this macro expression is `&U` where `U: 'u + Sized + FromBytes +
            // Immutable`, and that `'t` outlives `'u`.

            struct AssertSrcIsSized<'a, T: ::core::marker::Sized>(&'a T);
            struct AssertSrcIsIntoBytes<'a, T: ?::core::marker::Sized + $crate::IntoBytes>(&'a T);
            struct AssertSrcIsImmutable<'a, T: ?::core::marker::Sized + $crate::Immutable>(&'a T);
            struct AssertDstIsSized<'a, T: ::core::marker::Sized>(&'a T);
            struct AssertDstIsFromBytes<'a, U: ?::core::marker::Sized + $crate::FromBytes>(&'a U);
            struct AssertDstIsImmutable<'a, T: ?::core::marker::Sized + $crate::Immutable>(&'a T);

            let _ = AssertSrcIsSized(e);
            let _ = AssertSrcIsIntoBytes(e);
            let _ = AssertSrcIsImmutable(e);

            if true {
                #[allow(unused, unreachable_code)]
                let u = AssertDstIsSized(loop {});
                u.0
            } else if true {
                #[allow(unused, unreachable_code)]
                let u = AssertDstIsFromBytes(loop {});
                u.0
            } else {
                #[allow(unused, unreachable_code)]
                let u = AssertDstIsImmutable(loop {});
                u.0
            }
        } else if false {
            // This branch, though never taken, ensures that `size_of::<T>() ==
            // size_of::<U>()` and that that `align_of::<T>() >=
            // align_of::<U>()`.

            // `t` is inferred to have type `T` because it's assigned to `e` (of
            // type `&T`) as `&t`.
            let mut t = loop {};
            e = &t;

            // `u` is inferred to have type `U` because it's used as `&u` as the
            // value returned from this branch.
            let u;

            $crate::assert_size_eq!(t, u);
            $crate::assert_align_gt_eq!(t, u);

            &u
        } else {
            // SAFETY: For source type `Src` and destination type `Dst`:
            // - We know that `Src: IntoBytes + Immutable` and `Dst: FromBytes +
            //   Immutable` thanks to the uses of `AssertSrcIsIntoBytes`,
            //   `AssertSrcIsImmutable`, `AssertDstIsFromBytes`, and
            //   `AssertDstIsImmutable` above.
            // - We know that `size_of::<Src>() == size_of::<Dst>()` thanks to
            //   the use of `assert_size_eq!` above.
            // - We know that `align_of::<Src>() >= align_of::<Dst>()` thanks to
            //   the use of `assert_align_gt_eq!` above.
            let u = unsafe { $crate::util::macro_util::transmute_ref(e) };
            $crate::util::macro_util::must_use(u)
        }
    }}
}

/// Safely transmutes a mutable reference of one type to a mutable reference of
/// another type of the same size and compatible alignment.
///
/// This macro behaves like an invocation of this function:
///
/// ```ignore
/// const fn transmute_mut<'src, 'dst, Src, Dst>(src: &'src mut Src) -> &'dst mut Dst
/// where
///     'src: 'dst,
///     Src: FromBytes + IntoBytes,
///     Dst: FromBytes + IntoBytes,
///     size_of::<Src>() == size_of::<Dst>(),
///     align_of::<Src>() >= align_of::<Dst>(),
/// {
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// However, unlike a function, this macro can only be invoked when the types of
/// `Src` and `Dst` are completely concrete. The types `Src` and `Dst` are
/// inferred from the calling context; they cannot be explicitly specified in
/// the macro invocation.
///
/// # Examples
///
/// ```
/// # use zerocopy::transmute_mut;
/// let mut one_dimensional: [u8; 8] = [0, 1, 2, 3, 4, 5, 6, 7];
///
/// let two_dimensional: &mut [[u8; 4]; 2] = transmute_mut!(&mut one_dimensional);
///
/// assert_eq!(two_dimensional, &[[0, 1, 2, 3], [4, 5, 6, 7]]);
///
/// two_dimensional.reverse();
///
/// assert_eq!(one_dimensional, [4, 5, 6, 7, 0, 1, 2, 3]);
/// ```
///
/// # Use in `const` contexts
///
/// This macro can be invoked in `const` contexts.
///
/// # Alignment increase error message
///
/// Because of limitations on macros, the error message generated when
/// `transmute_mut!` is used to transmute from a type of lower alignment to a
/// type of higher alignment is somewhat confusing. For example, the following
/// code:
///
/// ```compile_fail
/// const INCREASE_ALIGNMENT: &mut u16 = zerocopy::transmute_mut!(&mut [0u8; 2]);
/// ```
///
/// ...generates the following error:
///
/// ```text
/// error[E0512]: cannot transmute between types of different sizes, or dependently-sized types
///  --> src/lib.rs:1524:34
///   |
/// 5 | const INCREASE_ALIGNMENT: &mut u16 = zerocopy::transmute_mut!(&mut [0u8; 2]);
///   |                                      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
///   |
///   = note: source type: `AlignOf<[u8; 2]>` (8 bits)
///   = note: target type: `MaxAlignsOf<[u8; 2], u16>` (16 bits)
///   = note: this error originates in the macro `$crate::assert_align_gt_eq` which comes from the expansion of the macro `transmute_mut` (in Nightly builds, run with -Z macro-backtrace for more info)
/// ```
///
/// This is saying that `max(align_of::<T>(), align_of::<U>()) !=
/// align_of::<T>()`, which is equivalent to `align_of::<T>() <
/// align_of::<U>()`.
#[macro_export]
macro_rules! transmute_mut {
    ($e:expr) => {{
        // NOTE: This must be a macro (rather than a function with trait bounds)
        // because there's no way, in a generic context, to enforce that two
        // types have the same size or alignment.

        // Ensure that the source type is a mutable reference.
        let e: &mut _ = $e;

        #[allow(unused, clippy::diverging_sub_expression)]
        if false {
            // This branch, though never taken, ensures that the type of `e` is
            // `&mut T` where `T: 't + Sized + FromBytes + IntoBytes` and that
            // the type of this macro expression is `&mut U` where `U: 'u +
            // Sized + FromBytes + IntoBytes`.

            // We use immutable references here rather than mutable so that, if
            // this macro is used in a const context (in which, as of this
            // writing, mutable references are banned), the error message
            // appears to originate in the user's code rather than in the
            // internals of this macro.
            struct AssertSrcIsSized<'a, T: ::core::marker::Sized>(&'a T);
            struct AssertSrcIsFromBytes<'a, T: ?::core::marker::Sized + $crate::FromBytes>(&'a T);
            struct AssertSrcIsIntoBytes<'a, T: ?::core::marker::Sized + $crate::IntoBytes>(&'a T);
            struct AssertDstIsSized<'a, T: ::core::marker::Sized>(&'a T);
            struct AssertDstIsFromBytes<'a, T: ?::core::marker::Sized + $crate::FromBytes>(&'a T);
            struct AssertDstIsIntoBytes<'a, T: ?::core::marker::Sized + $crate::IntoBytes>(&'a T);

            if true {
                let _ = AssertSrcIsSized(&*e);
            } else if true {
                let _ = AssertSrcIsFromBytes(&*e);
            } else {
                let _ = AssertSrcIsIntoBytes(&*e);
            }

            if true {
                #[allow(unused, unreachable_code)]
                let u = AssertDstIsSized(loop {});
                &mut *u.0
            } else if true {
                #[allow(unused, unreachable_code)]
                let u = AssertDstIsFromBytes(loop {});
                &mut *u.0
            } else {
                #[allow(unused, unreachable_code)]
                let u = AssertDstIsIntoBytes(loop {});
                &mut *u.0
            }
        } else if false {
            // This branch, though never taken, ensures that `size_of::<T>() ==
            // size_of::<U>()` and that that `align_of::<T>() >=
            // align_of::<U>()`.

            // `t` is inferred to have type `T` because it's assigned to `e` (of
            // type `&mut T`) as `&mut t`.
            let mut t = loop {};
            e = &mut t;

            // `u` is inferred to have type `U` because it's used as `&mut u` as
            // the value returned from this branch.
            let u;

            $crate::assert_size_eq!(t, u);
            $crate::assert_align_gt_eq!(t, u);

            &mut u
        } else {
            // SAFETY: For source type `Src` and destination type `Dst`:
            // - We know that `size_of::<Src>() == size_of::<Dst>()` thanks to
            //   the use of `assert_size_eq!` above.
            // - We know that `align_of::<Src>() >= align_of::<Dst>()` thanks to
            //   the use of `assert_align_gt_eq!` above.
            let u = unsafe { $crate::util::macro_util::transmute_mut(e) };
            $crate::util::macro_util::must_use(u)
        }
    }}
}

/// Conditionally transmutes a value of one type to a value of another type of
/// the same size.
///
/// This macro behaves like an invocation of this function:
///
/// ```ignore
/// fn try_transmute<Src, Dst>(src: Src) -> Result<Dst, ValidityError<Src, Dst>>
/// where
///     Src: IntoBytes,
///     Dst: TryFromBytes,
///     size_of::<Src>() == size_of::<Dst>(),
/// {
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// However, unlike a function, this macro can only be invoked when the types of
/// `Src` and `Dst` are completely concrete. The types `Src` and `Dst` are
/// inferred from the calling context; they cannot be explicitly specified in
/// the macro invocation.
///
/// Note that the `Src` produced by the expression `$e` will *not* be dropped.
/// Semantically, its bits will be copied into a new value of type `Dst`, the
/// original `Src` will be forgotten, and the value of type `Dst` will be
/// returned.
///
/// # Examples
///
/// ```
/// # use zerocopy::*;
/// // 0u8 → bool = false
/// assert_eq!(try_transmute!(0u8), Ok(false));
///
/// // 1u8 → bool = true
///  assert_eq!(try_transmute!(1u8), Ok(true));
///
/// // 2u8 → bool = error
/// assert!(matches!(
///     try_transmute!(2u8),
///     Result::<bool, _>::Err(ValidityError { .. })
/// ));
/// ```
#[macro_export]
macro_rules! try_transmute {
    ($e:expr) => {{
        // NOTE: This must be a macro (rather than a function with trait bounds)
        // because there's no way, in a generic context, to enforce that two
        // types have the same size. `core::mem::transmute` uses compiler magic
        // to enforce this so long as the types are concrete.

        let e = $e;
        if false {
            // Check that the sizes of the source and destination types are
            // equal.

            // SAFETY: This code is never executed.
            Ok(unsafe {
                // Clippy: We can't annotate the types; this macro is designed
                // to infer the types from the calling context.
                #[allow(clippy::missing_transmute_annotations)]
                $crate::util::macro_util::core_reexport::mem::transmute(e)
            })
        } else {
            $crate::util::macro_util::try_transmute::<_, _>(e)
        }
    }}
}

/// Conditionally transmutes a mutable or immutable reference of one type to an
/// immutable reference of another type of the same size and compatible
/// alignment.
///
/// This macro behaves like an invocation of this function:
///
/// ```ignore
/// fn try_transmute_ref<Src, Dst>(src: &Src) -> Result<&Dst, ValidityError<&Src, Dst>>
/// where
///     Src: IntoBytes + Immutable,
///     Dst: TryFromBytes + Immutable,
///     size_of::<Src>() == size_of::<Dst>(),
///     align_of::<Src>() >= align_of::<Dst>(),
/// {
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// However, unlike a function, this macro can only be invoked when the types of
/// `Src` and `Dst` are completely concrete. The types `Src` and `Dst` are
/// inferred from the calling context; they cannot be explicitly specified in
/// the macro invocation.
///
/// # Examples
///
/// ```
/// # use zerocopy::*;
/// // 0u8 → bool = false
/// assert_eq!(try_transmute_ref!(&0u8), Ok(&false));
///
/// // 1u8 → bool = true
///  assert_eq!(try_transmute_ref!(&1u8), Ok(&true));
///
/// // 2u8 → bool = error
/// assert!(matches!(
///     try_transmute_ref!(&2u8),
///     Result::<&bool, _>::Err(ValidityError { .. })
/// ));
/// ```
///
/// # Alignment increase error message
///
/// Because of limitations on macros, the error message generated when
/// `try_transmute_ref!` is used to transmute from a type of lower alignment to
/// a type of higher alignment is somewhat confusing. For example, the following
/// code:
///
/// ```compile_fail
/// let increase_alignment: Result<&u16, _> = zerocopy::try_transmute_ref!(&[0u8; 2]);
/// ```
///
/// ...generates the following error:
///
/// ```text
/// error[E0512]: cannot transmute between types of different sizes, or dependently-sized types
///  --> example.rs:1:47
///   |
/// 1 |     let increase_alignment: Result<&u16, _> = zerocopy::try_transmute_ref!(&[0u8; 2]);
///   |                                               ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
///   |
///   = note: source type: `AlignOf<[u8; 2]>` (8 bits)
///   = note: target type: `MaxAlignsOf<[u8; 2], u16>` (16 bits)
///   = note: this error originates in the macro `$crate::assert_align_gt_eq` which comes from the expansion of the macro `zerocopy::try_transmute_ref` (in Nightly builds, run with -Z macro-backtrace for more info)/// ```
/// ```
///
/// This is saying that `max(align_of::<T>(), align_of::<U>()) !=
/// align_of::<T>()`, which is equivalent to `align_of::<T>() <
/// align_of::<U>()`.
#[macro_export]
macro_rules! try_transmute_ref {
    ($e:expr) => {{
        // NOTE: This must be a macro (rather than a function with trait bounds)
        // because there's no way, in a generic context, to enforce that two
        // types have the same size. `core::mem::transmute` uses compiler magic
        // to enforce this so long as the types are concrete.

        // Ensure that the source type is a reference or a mutable reference
        // (note that mutable references are implicitly reborrowed here).
        let e: &_ = $e;

        #[allow(unreachable_code, unused, clippy::diverging_sub_expression)]
        if false {
            // This branch, though never taken, ensures that `size_of::<T>() ==
            // size_of::<U>()` and that that `align_of::<T>() >=
            // align_of::<U>()`.

            // `t` is inferred to have type `T` because it's assigned to `e` (of
            // type `&T`) as `&t`.
            let mut t = loop {};
            e = &t;

            // `u` is inferred to have type `U` because it's used as `Ok(&u)` as
            // the value returned from this branch.
            let u;

            $crate::assert_size_eq!(t, u);
            $crate::assert_align_gt_eq!(t, u);

            Ok(&u)
        } else {
            $crate::util::macro_util::try_transmute_ref::<_, _>(e)
        }
    }}
}

/// Conditionally transmutes a mutable reference of one type to a mutable
/// reference of another type of the same size and compatible alignment.
///
/// This macro behaves like an invocation of this function:
///
/// ```ignore
/// fn try_transmute_mut<Src, Dst>(src: &mut Src) -> Result<&mut Dst, ValidityError<&mut Src, Dst>>
/// where
///     Src: FromBytes + IntoBytes,
///     Dst: TryFromBytes + IntoBytes,
///     size_of::<Src>() == size_of::<Dst>(),
///     align_of::<Src>() >= align_of::<Dst>(),
/// {
/// # /*
///     ...
/// # */
/// }
/// ```
///
/// However, unlike a function, this macro can only be invoked when the types of
/// `Src` and `Dst` are completely concrete. The types `Src` and `Dst` are
/// inferred from the calling context; they cannot be explicitly specified in
/// the macro invocation.
///
/// # Examples
///
/// ```
/// # use zerocopy::*;
/// // 0u8 → bool = false
/// let src = &mut 0u8;
/// assert_eq!(try_transmute_mut!(src), Ok(&mut false));
///
/// // 1u8 → bool = true
/// let src = &mut 1u8;
///  assert_eq!(try_transmute_mut!(src), Ok(&mut true));
///
/// // 2u8 → bool = error
/// let src = &mut 2u8;
/// assert!(matches!(
///     try_transmute_mut!(src),
///     Result::<&mut bool, _>::Err(ValidityError { .. })
/// ));
/// ```
///
/// # Alignment increase error message
///
/// Because of limitations on macros, the error message generated when
/// `try_transmute_ref!` is used to transmute from a type of lower alignment to
/// a type of higher alignment is somewhat confusing. For example, the following
/// code:
///
/// ```compile_fail
/// let src = &mut [0u8; 2];
/// let increase_alignment: Result<&mut u16, _> = zerocopy::try_transmute_mut!(src);
/// ```
///
/// ...generates the following error:
///
/// ```text
/// error[E0512]: cannot transmute between types of different sizes, or dependently-sized types
///  --> example.rs:2:51
///   |
/// 2 |     let increase_alignment: Result<&mut u16, _> = zerocopy::try_transmute_mut!(src);
///   |                                                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
///   |
///   = note: source type: `AlignOf<[u8; 2]>` (8 bits)
///   = note: target type: `MaxAlignsOf<[u8; 2], u16>` (16 bits)
///   = note: this error originates in the macro `$crate::assert_align_gt_eq` which comes from the expansion of the macro `zerocopy::try_transmute_mut` (in Nightly builds, run with -Z macro-backtrace for more info)
/// ```
///
/// This is saying that `max(align_of::<T>(), align_of::<U>()) !=
/// align_of::<T>()`, which is equivalent to `align_of::<T>() <
/// align_of::<U>()`.
#[macro_export]
macro_rules! try_transmute_mut {
    ($e:expr) => {{
        // NOTE: This must be a macro (rather than a function with trait bounds)
        // because there's no way, in a generic context, to enforce that two
        // types have the same size. `core::mem::transmute` uses compiler magic
        // to enforce this so long as the types are concrete.

        // Ensure that the source type is a mutable reference.
        let e: &mut _ = $e;

        #[allow(unreachable_code, unused, clippy::diverging_sub_expression)]
        if false {
            // This branch, though never taken, ensures that `size_of::<T>() ==
            // size_of::<U>()` and that that `align_of::<T>() >=
            // align_of::<U>()`.

            // `t` is inferred to have type `T` because it's assigned to `e` (of
            // type `&mut T`) as `&mut t`.
            let mut t = loop {};
            e = &mut t;

            // `u` is inferred to have type `U` because it's used as `Ok(&mut
            // u)` as the value returned from this branch.
            let u;

            $crate::assert_size_eq!(t, u);
            $crate::assert_align_gt_eq!(t, u);

            Ok(&mut u)
        } else {
            $crate::util::macro_util::try_transmute_mut::<_, _>(e)
        }
    }}
}

/// Includes a file and safely transmutes it to a value of an arbitrary type.
///
/// The file will be included as a byte array, `[u8; N]`, which will be
/// transmuted to another type, `T`. `T` is inferred from the calling context,
/// and must implement [`FromBytes`].
///
/// The file is located relative to the current file (similarly to how modules
/// are found). The provided path is interpreted in a platform-specific way at
/// compile time. So, for instance, an invocation with a Windows path containing
/// backslashes `\` would not compile correctly on Unix.
///
/// `include_value!` is ignorant of byte order. For byte order-aware types, see
/// the [`byteorder`] module.
///
/// [`FromBytes`]: crate::FromBytes
/// [`byteorder`]: crate::byteorder
///
/// # Examples
///
/// Assume there are two files in the same directory with the following
/// contents:
///
/// File `data` (no trailing newline):
///
/// ```text
/// abcd
/// ```
///
/// File `main.rs`:
///
/// ```rust
/// use zerocopy::include_value;
/// # macro_rules! include_value {
/// # ($file:expr) => { zerocopy::include_value!(concat!("../testdata/include_value/", $file)) };
/// # }
///
/// fn main() {
///     let as_u32: u32 = include_value!("data");
///     assert_eq!(as_u32, u32::from_ne_bytes([b'a', b'b', b'c', b'd']));
///     let as_i32: i32 = include_value!("data");
///     assert_eq!(as_i32, i32::from_ne_bytes([b'a', b'b', b'c', b'd']));
/// }
/// ```
///
/// # Use in `const` contexts
///
/// This macro can be invoked in `const` contexts.
#[doc(alias("include_bytes", "include_data", "include_type"))]
#[macro_export]
macro_rules! include_value {
    ($file:expr $(,)?) => {
        $crate::transmute!(*::core::include_bytes!($file))
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! cryptocorrosion_derive_traits {
    (
        #[repr($repr:ident)]
        $(#[$attr:meta])*
        $vis:vis struct $name:ident $(<$($tyvar:ident),*>)?
        $(
            (
                $($tuple_field_vis:vis $tuple_field_ty:ty),*
            );
        )?

        $(
            {
                $($field_vis:vis $field_name:ident: $field_ty:ty,)*
            }
        )?
    ) => {
        $crate::cryptocorrosion_derive_traits!(@assert_allowed_struct_repr #[repr($repr)]);

        $(#[$attr])*
        #[repr($repr)]
        $vis struct $name $(<$($tyvar),*>)?
        $(
            (
                $($tuple_field_vis $tuple_field_ty),*
            );
        )?

        $(
            {
                $($field_vis $field_name: $field_ty,)*
            }
        )?

        // SAFETY: See inline.
        unsafe impl $(<$($tyvar),*>)? $crate::TryFromBytes for $name$(<$($tyvar),*>)?
        where
            $(
                $($tuple_field_ty: $crate::FromBytes,)*
            )?

            $(
                $($field_ty: $crate::FromBytes,)*
            )?
        {
            fn is_bit_valid<A>(_c: $crate::Maybe<'_, Self, A>) -> bool
            where
                A: $crate::pointer::invariant::Reference
            {
                // SAFETY: This macro only accepts `#[repr(C)]` and
                // `#[repr(transparent)]` structs, and this `impl` block
                // requires all field types to be `FromBytes`. Thus, all
                // initialized byte sequences constitutes valid instances of
                // `Self`.
                true
            }

            fn only_derive_is_allowed_to_implement_this_trait() {}
        }

        // SAFETY: This macro only accepts `#[repr(C)]` and
        // `#[repr(transparent)]` structs, and this `impl` block requires all
        // field types to be `FromBytes`, which is a sub-trait of `FromZeros`.
        unsafe impl $(<$($tyvar),*>)? $crate::FromZeros for $name$(<$($tyvar),*>)?
        where
            $(
                $($tuple_field_ty: $crate::FromBytes,)*
            )?

            $(
                $($field_ty: $crate::FromBytes,)*
            )?
        {
            fn only_derive_is_allowed_to_implement_this_trait() {}
        }

        // SAFETY: This macro only accepts `#[repr(C)]` and
        // `#[repr(transparent)]` structs, and this `impl` block requires all
        // field types to be `FromBytes`.
        unsafe impl $(<$($tyvar),*>)? $crate::FromBytes for $name$(<$($tyvar),*>)?
        where
            $(
                $($tuple_field_ty: $crate::FromBytes,)*
            )?

            $(
                $($field_ty: $crate::FromBytes,)*
            )?
        {
            fn only_derive_is_allowed_to_implement_this_trait() {}
        }

        // SAFETY: This macro only accepts `#[repr(C)]` and
        // `#[repr(transparent)]` structs, this `impl` block requires all field
        // types to be `IntoBytes`, and a padding check is used to ensures that
        // there are no padding bytes.
        unsafe impl $(<$($tyvar),*>)? $crate::IntoBytes for $name$(<$($tyvar),*>)?
        where
            $(
                $($tuple_field_ty: $crate::IntoBytes,)*
            )?

            $(
                $($field_ty: $crate::IntoBytes,)*
            )?

            (): $crate::util::macro_util::PaddingFree<
                Self,
                {
                    $crate::cryptocorrosion_derive_traits!(
                        @struct_padding_check #[repr($repr)]
                        $(($($tuple_field_ty),*))?
                        $({$($field_ty),*})?
                    )
                },
            >,
        {
            fn only_derive_is_allowed_to_implement_this_trait() {}
        }

        // SAFETY: This macro only accepts `#[repr(C)]` and
        // `#[repr(transparent)]` structs, and this `impl` block requires all
        // field types to be `Immutable`.
        unsafe impl $(<$($tyvar),*>)? $crate::Immutable for $name$(<$($tyvar),*>)?
        where
            $(
                $($tuple_field_ty: $crate::Immutable,)*
            )?

            $(
                $($field_ty: $crate::Immutable,)*
            )?
        {
            fn only_derive_is_allowed_to_implement_this_trait() {}
        }
    };
    (@assert_allowed_struct_repr #[repr(transparent)]) => {};
    (@assert_allowed_struct_repr #[repr(C)]) => {};
    (@assert_allowed_struct_repr #[$_attr:meta]) => {
        compile_error!("repr must be `#[repr(transparent)]` or `#[repr(C)]`");
    };
    (
        @struct_padding_check #[repr(transparent)]
        $(($($tuple_field_ty:ty),*))?
        $({$($field_ty:ty),*})?
    ) => {
        // SAFETY: `#[repr(transparent)]` structs cannot have the same layout as
        // their single non-zero-sized field, and so cannot have any padding
        // outside of that field.
        false
    };
    (
        @struct_padding_check #[repr(C)]
        $(($($tuple_field_ty:ty),*))?
        $({$($field_ty:ty),*})?
    ) => {
        $crate::struct_has_padding!(
            Self,
            [
                $($($tuple_field_ty),*)?
                $($($field_ty),*)?
            ]
        )
    };
    (
        #[repr(C)]
        $(#[$attr:meta])*
        $vis:vis union $name:ident {
            $(
                $field_name:ident: $field_ty:ty,
            )*
        }
    ) => {
        $(#[$attr])*
        #[repr(C)]
        $vis union $name {
            $(
                $field_name: $field_ty,
            )*
        }

        // SAFETY: See inline.
        unsafe impl $crate::TryFromBytes for $name
        where
            $(
                $field_ty: $crate::FromBytes,
            )*
        {
            fn is_bit_valid<A>(_c: $crate::Maybe<'_, Self, A>) -> bool
            where
                A: $crate::pointer::invariant::Reference
            {
                // SAFETY: This macro only accepts `#[repr(C)]` unions, and this
                // `impl` block requires all field types to be `FromBytes`.
                // Thus, all initialized byte sequences constitutes valid
                // instances of `Self`.
                true
            }

            fn only_derive_is_allowed_to_implement_this_trait() {}
        }

        // SAFETY: This macro only accepts `#[repr(C)]` unions, and this `impl`
        // block requires all field types to be `FromBytes`, which is a
        // sub-trait of `FromZeros`.
        unsafe impl $crate::FromZeros for $name
        where
            $(
                $field_ty: $crate::FromBytes,
            )*
        {
            fn only_derive_is_allowed_to_implement_this_trait() {}
        }

        // SAFETY: This macro only accepts `#[repr(C)]` unions, and this `impl`
        // block requires all field types to be `FromBytes`.
        unsafe impl $crate::FromBytes for $name
        where
            $(
                $field_ty: $crate::FromBytes,
            )*
        {
            fn only_derive_is_allowed_to_implement_this_trait() {}
        }

        // SAFETY: This macro only accepts `#[repr(C)]` unions, this `impl`
        // block requires all field types to be `IntoBytes`, and a padding check
        // is used to ensures that there are no padding bytes before or after
        // any field.
        unsafe impl $crate::IntoBytes for $name
        where
            $(
                $field_ty: $crate::IntoBytes,
            )*
            (): $crate::util::macro_util::PaddingFree<
                Self,
                {
                    $crate::union_has_padding!(
                        Self,
                        [$($field_ty),*]
                    )
                },
            >,
        {
            fn only_derive_is_allowed_to_implement_this_trait() {}
        }

        // SAFETY: This macro only accepts `#[repr(C)]` unions, and this `impl`
        // block requires all field types to be `Immutable`.
        unsafe impl $crate::Immutable for $name
        where
            $(
                $field_ty: $crate::Immutable,
            )*
        {
            fn only_derive_is_allowed_to_implement_this_trait() {}
        }
    };
}

#[cfg(test)]
mod tests {
    use crate::util::testutil::*;
    use crate::*;

    #[test]
    fn test_transmute() {
        // Test that memory is transmuted as expected.
        let array_of_u8s = [0u8, 1, 2, 3, 4, 5, 6, 7];
        let array_of_arrays = [[0, 1], [2, 3], [4, 5], [6, 7]];
        let x: [[u8; 2]; 4] = transmute!(array_of_u8s);
        assert_eq!(x, array_of_arrays);
        let x: [u8; 8] = transmute!(array_of_arrays);
        assert_eq!(x, array_of_u8s);

        // Test that the source expression's value is forgotten rather than
        // dropped.
        #[derive(IntoBytes)]
        #[repr(transparent)]
        struct PanicOnDrop(());
        impl Drop for PanicOnDrop {
            fn drop(&mut self) {
                panic!("PanicOnDrop::drop");
            }
        }
        #[allow(clippy::let_unit_value)]
        let _: () = transmute!(PanicOnDrop(()));

        // Test that `transmute!` is legal in a const context.
        const ARRAY_OF_U8S: [u8; 8] = [0u8, 1, 2, 3, 4, 5, 6, 7];
        const ARRAY_OF_ARRAYS: [[u8; 2]; 4] = [[0, 1], [2, 3], [4, 5], [6, 7]];
        const X: [[u8; 2]; 4] = transmute!(ARRAY_OF_U8S);
        assert_eq!(X, ARRAY_OF_ARRAYS);

        // Test that `transmute!` works with `!Immutable` types.
        let x: usize = transmute!(UnsafeCell::new(1usize));
        assert_eq!(x, 1);
        let x: UnsafeCell<usize> = transmute!(1usize);
        assert_eq!(x.into_inner(), 1);
        let x: UnsafeCell<isize> = transmute!(UnsafeCell::new(1usize));
        assert_eq!(x.into_inner(), 1);
    }

    #[test]
    fn test_transmute_ref() {
        // Test that memory is transmuted as expected.
        let array_of_u8s = [0u8, 1, 2, 3, 4, 5, 6, 7];
        let array_of_arrays = [[0, 1], [2, 3], [4, 5], [6, 7]];
        let x: &[[u8; 2]; 4] = transmute_ref!(&array_of_u8s);
        assert_eq!(*x, array_of_arrays);
        let x: &[u8; 8] = transmute_ref!(&array_of_arrays);
        assert_eq!(*x, array_of_u8s);

        // Test that `transmute_ref!` is legal in a const context.
        const ARRAY_OF_U8S: [u8; 8] = [0u8, 1, 2, 3, 4, 5, 6, 7];
        const ARRAY_OF_ARRAYS: [[u8; 2]; 4] = [[0, 1], [2, 3], [4, 5], [6, 7]];
        #[allow(clippy::redundant_static_lifetimes)]
        const X: &'static [[u8; 2]; 4] = transmute_ref!(&ARRAY_OF_U8S);
        assert_eq!(*X, ARRAY_OF_ARRAYS);

        // Test that it's legal to transmute a reference while shrinking the
        // lifetime (note that `X` has the lifetime `'static`).
        let x: &[u8; 8] = transmute_ref!(X);
        assert_eq!(*x, ARRAY_OF_U8S);

        // Test that `transmute_ref!` supports decreasing alignment.
        let u = AU64(0);
        let array = [0, 0, 0, 0, 0, 0, 0, 0];
        let x: &[u8; 8] = transmute_ref!(&u);
        assert_eq!(*x, array);

        // Test that a mutable reference can be turned into an immutable one.
        let mut x = 0u8;
        #[allow(clippy::useless_transmute)]
        let y: &u8 = transmute_ref!(&mut x);
        assert_eq!(*y, 0);
    }

    #[test]
    fn test_try_transmute() {
        // Test that memory is transmuted with `try_transmute` as expected.
        let array_of_bools = [false, true, false, true, false, true, false, true];
        let array_of_arrays = [[0, 1], [0, 1], [0, 1], [0, 1]];
        let x: Result<[[u8; 2]; 4], _> = try_transmute!(array_of_bools);
        assert_eq!(x, Ok(array_of_arrays));
        let x: Result<[bool; 8], _> = try_transmute!(array_of_arrays);
        assert_eq!(x, Ok(array_of_bools));

        // Test that `try_transmute!` works with `!Immutable` types.
        let x: Result<usize, _> = try_transmute!(UnsafeCell::new(1usize));
        assert_eq!(x.unwrap(), 1);
        let x: Result<UnsafeCell<usize>, _> = try_transmute!(1usize);
        assert_eq!(x.unwrap().into_inner(), 1);
        let x: Result<UnsafeCell<isize>, _> = try_transmute!(UnsafeCell::new(1usize));
        assert_eq!(x.unwrap().into_inner(), 1);

        #[derive(FromBytes, IntoBytes, Debug, PartialEq)]
        #[repr(transparent)]
        struct PanicOnDrop<T>(T);

        impl<T> Drop for PanicOnDrop<T> {
            fn drop(&mut self) {
                panic!("PanicOnDrop dropped");
            }
        }

        // Since `try_transmute!` semantically moves its argument on failure,
        // the `PanicOnDrop` is not dropped, and thus this shouldn't panic.
        let x: Result<usize, _> = try_transmute!(PanicOnDrop(1usize));
        assert_eq!(x, Ok(1));

        // Since `try_transmute!` semantically returns ownership of its argument
        // on failure, the `PanicOnDrop` is returned rather than dropped, and
        // thus this shouldn't panic.
        let y: Result<bool, _> = try_transmute!(PanicOnDrop(2u8));
        // We have to use `map_err` instead of comparing against
        // `Err(PanicOnDrop(2u8))` because the latter would create and then drop
        // its `PanicOnDrop` temporary, which would cause a panic.
        assert_eq!(y.as_ref().map_err(|p| &p.src.0), Err::<&bool, _>(&2u8));
        mem::forget(y);
    }

    #[test]
    fn test_try_transmute_ref() {
        // Test that memory is transmuted with `try_transmute_ref` as expected.
        let array_of_bools = &[false, true, false, true, false, true, false, true];
        let array_of_arrays = &[[0, 1], [0, 1], [0, 1], [0, 1]];
        let x: Result<&[[u8; 2]; 4], _> = try_transmute_ref!(array_of_bools);
        assert_eq!(x, Ok(array_of_arrays));
        let x: Result<&[bool; 8], _> = try_transmute_ref!(array_of_arrays);
        assert_eq!(x, Ok(array_of_bools));

        // Test that it's legal to transmute a reference while shrinking the
        // lifetime.
        {
            let x: Result<&[[u8; 2]; 4], _> = try_transmute_ref!(array_of_bools);
            assert_eq!(x, Ok(array_of_arrays));
        }

        // Test that `try_transmute_ref!` supports decreasing alignment.
        let u = AU64(0);
        let array = [0u8, 0, 0, 0, 0, 0, 0, 0];
        let x: Result<&[u8; 8], _> = try_transmute_ref!(&u);
        assert_eq!(x, Ok(&array));

        // Test that a mutable reference can be turned into an immutable one.
        let mut x = 0u8;
        #[allow(clippy::useless_transmute)]
        let y: Result<&u8, _> = try_transmute_ref!(&mut x);
        assert_eq!(y, Ok(&0));
    }

    #[test]
    fn test_try_transmute_mut() {
        // Test that memory is transmuted with `try_transmute_mut` as expected.
        let array_of_u8s = &mut [0u8, 1, 0, 1, 0, 1, 0, 1];
        let array_of_arrays = &mut [[0u8, 1], [0, 1], [0, 1], [0, 1]];
        let x: Result<&mut [[u8; 2]; 4], _> = try_transmute_mut!(array_of_u8s);
        assert_eq!(x, Ok(array_of_arrays));

        let array_of_bools = &mut [false, true, false, true, false, true, false, true];
        let array_of_arrays = &mut [[0u8, 1], [0, 1], [0, 1], [0, 1]];
        let x: Result<&mut [bool; 8], _> = try_transmute_mut!(array_of_arrays);
        assert_eq!(x, Ok(array_of_bools));

        // Test that it's legal to transmute a reference while shrinking the
        // lifetime.
        let array_of_bools = &mut [false, true, false, true, false, true, false, true];
        let array_of_arrays = &mut [[0u8, 1], [0, 1], [0, 1], [0, 1]];
        {
            let x: Result<&mut [bool; 8], _> = try_transmute_mut!(array_of_arrays);
            assert_eq!(x, Ok(array_of_bools));
        }

        // Test that `try_transmute_mut!` supports decreasing alignment.
        let u = &mut AU64(0);
        let array = &mut [0u8, 0, 0, 0, 0, 0, 0, 0];
        let x: Result<&mut [u8; 8], _> = try_transmute_mut!(u);
        assert_eq!(x, Ok(array));

        // Test that a mutable reference can be turned into an immutable one.
        let mut x = 0u8;
        #[allow(clippy::useless_transmute)]
        let y: Result<&mut u8, _> = try_transmute_mut!(&mut x);
        assert_eq!(y, Ok(&mut 0));
    }

    #[test]
    fn test_transmute_mut() {
        // Test that memory is transmuted as expected.
        let mut array_of_u8s = [0u8, 1, 2, 3, 4, 5, 6, 7];
        let mut array_of_arrays = [[0, 1], [2, 3], [4, 5], [6, 7]];
        let x: &mut [[u8; 2]; 4] = transmute_mut!(&mut array_of_u8s);
        assert_eq!(*x, array_of_arrays);
        let x: &mut [u8; 8] = transmute_mut!(&mut array_of_arrays);
        assert_eq!(*x, array_of_u8s);

        {
            // Test that it's legal to transmute a reference while shrinking the
            // lifetime.
            let x: &mut [u8; 8] = transmute_mut!(&mut array_of_arrays);
            assert_eq!(*x, array_of_u8s);
        }
        // Test that `transmute_mut!` supports decreasing alignment.
        let mut u = AU64(0);
        let array = [0, 0, 0, 0, 0, 0, 0, 0];
        let x: &[u8; 8] = transmute_mut!(&mut u);
        assert_eq!(*x, array);

        // Test that a mutable reference can be turned into an immutable one.
        let mut x = 0u8;
        #[allow(clippy::useless_transmute)]
        let y: &u8 = transmute_mut!(&mut x);
        assert_eq!(*y, 0);
    }

    #[test]
    fn test_macros_evaluate_args_once() {
        let mut ctr = 0;
        #[allow(clippy::useless_transmute)]
        let _: usize = transmute!({
            ctr += 1;
            0usize
        });
        assert_eq!(ctr, 1);

        let mut ctr = 0;
        let _: &usize = transmute_ref!({
            ctr += 1;
            &0usize
        });
        assert_eq!(ctr, 1);

        let mut ctr: usize = 0;
        let _: &mut usize = transmute_mut!({
            ctr += 1;
            &mut ctr
        });
        assert_eq!(ctr, 1);

        let mut ctr = 0;
        #[allow(clippy::useless_transmute)]
        let _: usize = try_transmute!({
            ctr += 1;
            0usize
        })
        .unwrap();
        assert_eq!(ctr, 1);
    }

    #[test]
    fn test_include_value() {
        const AS_U32: u32 = include_value!("../testdata/include_value/data");
        assert_eq!(AS_U32, u32::from_ne_bytes([b'a', b'b', b'c', b'd']));
        const AS_I32: i32 = include_value!("../testdata/include_value/data");
        assert_eq!(AS_I32, i32::from_ne_bytes([b'a', b'b', b'c', b'd']));
    }

    #[test]
    #[allow(non_camel_case_types, unreachable_pub, dead_code)]
    fn test_cryptocorrosion_derive_traits() {
        // Test the set of invocations added in
        // https://github.com/cryptocorrosion/cryptocorrosion/pull/85

        fn assert_impls<T: FromBytes + IntoBytes + Immutable>() {}

        cryptocorrosion_derive_traits! {
            #[repr(C)]
            #[derive(Clone, Copy)]
            pub union vec128_storage {
                d: [u32; 4],
                q: [u64; 2],
            }
        }

        assert_impls::<vec128_storage>();

        cryptocorrosion_derive_traits! {
            #[repr(transparent)]
            #[derive(Copy, Clone, Debug, PartialEq)]
            pub struct u32x4_generic([u32; 4]);
        }

        assert_impls::<u32x4_generic>();

        cryptocorrosion_derive_traits! {
            #[repr(transparent)]
            #[derive(Copy, Clone, Debug, PartialEq)]
            pub struct u64x2_generic([u64; 2]);
        }

        assert_impls::<u64x2_generic>();

        cryptocorrosion_derive_traits! {
            #[repr(transparent)]
            #[derive(Copy, Clone, Debug, PartialEq)]
            pub struct u128x1_generic([u128; 1]);
        }

        assert_impls::<u128x1_generic>();

        cryptocorrosion_derive_traits! {
            #[repr(transparent)]
            #[derive(Copy, Clone, Default)]
            #[allow(non_camel_case_types)]
            pub struct x2<W, G>(pub [W; 2], PhantomData<G>);
        }

        enum NotZerocopy {}
        assert_impls::<x2<(), NotZerocopy>>();

        cryptocorrosion_derive_traits! {
            #[repr(transparent)]
            #[derive(Copy, Clone, Default)]
            #[allow(non_camel_case_types)]
            pub struct x4<W>(pub [W; 4]);
        }

        assert_impls::<x4<()>>();

        #[cfg(feature = "simd")]
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        {
            #[cfg(target_arch = "x86")]
            use core::arch::x86::{__m128i, __m256i};
            #[cfg(target_arch = "x86_64")]
            use core::arch::x86_64::{__m128i, __m256i};

            cryptocorrosion_derive_traits! {
                #[repr(C)]
                #[derive(Copy, Clone)]
                pub struct X4(__m128i, __m128i, __m128i, __m128i);
            }

            assert_impls::<X4>();

            cryptocorrosion_derive_traits! {
                #[repr(C)]
                /// Generic wrapper for unparameterized storage of any of the possible impls.
                /// Converting into and out of this type should be essentially free, although it may be more
                /// aligned than a particular impl requires.
                #[allow(non_camel_case_types)]
                #[derive(Copy, Clone)]
                pub union vec128_storage {
                    u32x4: [u32; 4],
                    u64x2: [u64; 2],
                    u128x1: [u128; 1],
                    sse2: __m128i,
                }
            }

            assert_impls::<vec128_storage>();

            cryptocorrosion_derive_traits! {
                #[repr(transparent)]
                #[allow(non_camel_case_types)]
                #[derive(Copy, Clone)]
                pub struct vec<S3, S4, NI> {
                    x: __m128i,
                    s3: PhantomData<S3>,
                    s4: PhantomData<S4>,
                    ni: PhantomData<NI>,
                }
            }

            assert_impls::<vec<NotZerocopy, NotZerocopy, NotZerocopy>>();

            cryptocorrosion_derive_traits! {
                #[repr(transparent)]
                #[derive(Copy, Clone)]
                pub struct u32x4x2_avx2<NI> {
                    x: __m256i,
                    ni: PhantomData<NI>,
                }
            }

            assert_impls::<u32x4x2_avx2<NotZerocopy>>();
        }

        // Make sure that our derive works for `#[repr(C)]` structs even though
        // cryptocorrosion doesn't currently have any.
        cryptocorrosion_derive_traits! {
            #[repr(C)]
            #[derive(Copy, Clone, Debug, PartialEq)]
            pub struct ReprC(u8, u8, u16);
        }
    }
}
