/// Implements the [`PanicFmt`](crate::fmt::PanicFmt)
/// trait and the `to_panicvals` method it requires.
///
/// For a derive macro alternative, there's the [`PanicFmt`](derive@crate::PanicFmt) derive,
/// which requires the `"derive"` feature(disabled by default).
///
/// # Syntax
///
/// This macro roughly takes a type definition and a (conditionally required) list of impls,
/// [for an example demonstrating all the parts of the syntax look here](#all-the-syntax).
///
/// This macro has these optional attributes that go above the item definition
/// (and must go in this order):
///
/// - `#[pfmt(display_fmt = $display_fmt:expr)]`[**(example below)**](#display-example):
/// Tells the macro to use the `$display_fmt` function to Display-format the type.
///
///
/// - `#[pfmt(panicvals_lower_bound = $panicvals_lower_bound:expr)]`:
/// Tells the macro to use at least `$panicvals_lower_bound` [`PanicVal`]s for
/// formatting the type, useful for Display formatting with the
/// `#[pfmt(display_fmt = ...)]` attribute.
///
///
/// # Limitations
///
/// ### Type parameters
///
/// Types with type parameters can't be generically formatted, which has two workarounds.
///
/// The first workaround is marking a type parameter as ignored with an `ignore` prefix,
/// if the type parameter(s) are only used in marker types (eg: `PhantomData`).
/// [example of this workaround](#phantom-type-parameter-example)
///
/// The second workaround is to implement panic formatting with concrete type arguments,
/// using trailing `(impl Foo<Bar>)`s.
/// [example of this workaround](#type-parameter-example)
///
/// This limitation is caused by:
/// - the lack of trait bound support in stable const fns.
/// - the need to [have a concrete type argument](#concrete-pv-count)
///
/// [example below](#type-parameter-example)
///
/// ### Const parameters
///
/// Const parameters must not affect the value of the `PanicFmt::PV_COUNT`  of this type,
/// since the const parameter [must be replaceable with a concrete value](#concrete-pv-count).
/// <br>Note that arrays have a `PV_COUNT` of `1` for all lengths.
///
/// <a id = "concrete-pv-count"></a>
/// ### Concrete `Self` type for `PanicFmt::PV_COUNT`
///
/// The `to_panicvals` method that this macro generates roughly returns a
/// ```text
/// [PanicVal<'_>; <Self as PanicFmt>::PV_COUNT]
/// ```
///
/// Because of limitations in stable const generics,
/// the generic arguments of `Self` in the above code must be replaced with concrete arguments,
/// requiring:
/// - Lifetime arguments to be replaced with `'_`
/// - Type arguments to be replaced with concrete types (usually `()`)
/// - Const arguments to be replaced with concrete values (usually the default value for the type)
///
/// # Examples
///
/// ### Struct formatting
///
/// ```rust
/// use const_panic::{ArrayString, FmtArg, impl_panicfmt};
///
/// fn main(){
///     const FOO: Foo = Foo {
///         x: &[3, 5, 8, 13],
///         y: 21,
///         z: Bar(false, true),
///     };
///     
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG; FOO),
///         "Foo { x: [3, 5, 8, 13], y: 21, z: Bar(false, true) }",
///     );
///     assert_eq!(
///         const_panic::concat_!(FmtArg::ALT_DEBUG; FOO),
///         concat!(
///             "Foo {\n",
///             "    x: [\n",
///             "        3,\n",
///             "        5,\n",
///             "        8,\n",
///             "        13,\n",
///             "    ],\n",
///             "    y: 21,\n",
///             "    z: Bar(\n",
///             "        false,\n",
///             "        true,\n",
///             "    ),\n",
///             "}",
///         ),
///     );
/// }
///
///
/// struct Foo<'a> {
///     x: &'a [u8],
///     y: u8,
///     z: Bar,
/// }
///
/// // Implementing `PanicFmt` and the `to_panicvals` method for `Foo<'a>`
/// impl_panicfmt!{
///     struct Foo<'a> {
///         x: &'a [u8],
///         y: u8,
///         z: Bar,
///     }
/// }
///
///
/// struct Bar(bool, bool);
///
/// impl_panicfmt!{
///     struct Bar(bool, bool);
/// }
///
/// ```
///
/// ### Enum Formatting
///
/// ```rust
/// use const_panic::{ArrayString, FmtArg, impl_panicfmt};
///
/// fn main() {
///     const UP: Qux<u8> = Qux::Up;
///     // Debug formatting the Up variant
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG; UP),
///         "Up",
///     );
///
///
///     const DOWN: Qux<u16> = Qux::Down { x: 21, y: 34, z: 55 };
///     // Debug formatting the Down variant
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG; DOWN),
///         "Down { x: 21, y: 34, z: 55 }",
///     );
///     // Alternate-Debug formatting the Down variant
///     assert_eq!(
///         const_panic::concat_!(FmtArg::ALT_DEBUG; DOWN),
///         concat!(
///             "Down {\n",
///             "    x: 21,\n",
///             "    y: 34,\n",
///             "    z: 55,\n",
///             "}",
///         )
///     );
///
///
///     const LEFT: Qux<u32> = Qux::Left(89);
///     // Debug formatting the Left variant
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG; LEFT),
///         "Left(89)",
///     );
///     // Alternate-Debug formatting the Left variant
///     assert_eq!(
///         const_panic::concat_!(FmtArg::ALT_DEBUG; LEFT),
///         concat!(
///             "Left(\n",
///             "    89,\n",
///             ")",
///         )
///     );
/// }
///
/// enum Qux<T> {
///     Up,
///     Down { x: T, y: T, z: T },
///     Left(u64),
/// }
///
///
/// // Because of limitations of stable const evaluation,
/// // `Qux` can't generically implement panic formatting,
/// // so this macro invocation implements panic formatting for these specifically:
/// // - `Qux<u8>`
/// // - `Qux<u16>`
/// // - `Qux<u32>`
/// impl_panicfmt!{
///     enum Qux<T> {
///         Up,
///         Down { x: T, y: T, z: T },
///         Left(u64),
///     }
///
///     (impl Qux<u8>)
///     (impl Qux<u16>)
///     (impl Qux<u32>)
/// }
/// ```
///
/// <a id = "type-parameter-example"></a>
/// ### Type parameters
///
/// This example demonstrates support for types with type parameters.
///
/// ```rust
/// use const_panic::{ArrayString, FmtArg, impl_panicfmt};
///
/// use std::marker::PhantomData;
///
/// {
///     const WITH_INT: Foo<&str, u8> = Foo {
///         value: 100u8,
///         _marker: PhantomData,
///     };
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG; WITH_INT),
///         "Foo { value: 100, _marker: PhantomData }",
///     );
/// }
/// {
///     const WITH_STR: Foo<bool, &str> = Foo {
///         value: "hello",
///         _marker: PhantomData,
///     };
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG; WITH_STR),
///         r#"Foo { value: "hello", _marker: PhantomData }"#,
///     );
/// }
///
/// #[derive(Debug)]
/// pub struct Foo<A, B> {
///     value: B,
///     _marker: PhantomData<A>,
/// }
///
/// impl_panicfmt!{
///     // `ignore` here tells the macro that this type parameter is not formatted.
///     struct Foo<ignore A, B> {
///         value: B,
///         _marker: PhantomData<A>,
///     }
///     
///     // Because type parameters can't be generically formatted,
///     // you need to list impls with concrete `B` type arguments.
///     //
///     // the generic parameters to the impl block go inside `[]`
///     (impl[A] Foo<A, u8>)
///     // the bounds in where clauses also go inside `[]`
///     (impl[A] Foo<A, &str> where[A: 'static])
/// }
///
///
///
/// ```
///
///
/// <a id = "phantom-type-parameter-example"></a>
/// ### Phantom Type parameters
///
/// This example demonstrates how type parameters can be ignored.
///
/// ```rust
/// use const_panic::{ArrayString, FmtArg, impl_panicfmt};
///
/// use std::marker::PhantomData;
///
/// {
///     const WITH_INT: Foo<u8, bool, 100> = Foo{
///         value: 5,
///         _marker: PhantomData,
///     };
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG; WITH_INT),
///         "Foo { value: 5, _marker: PhantomData }",
///     );
/// }
/// {
///     const WITH_STR: Foo<str, char, 200> = Foo {
///         value: 8,
///         _marker: PhantomData,
///     };
///     assert_eq!(
///         const_panic::concat_!(FmtArg::DEBUG; WITH_STR),
///         r#"Foo { value: 8, _marker: PhantomData }"#,
///     );
/// }
///
/// #[derive(Debug)]
/// pub struct Foo<A: ?Sized, B, const X: u32> {
///     value: u32,
///     _marker: PhantomData<(PhantomData<A>, B)>,
/// }
///
/// impl_panicfmt!{
///     // `ignore` here tells the macro that this type parameter is not formatted.
///     //
///     // `ignore(u8)` tells the macro to use `u8` as the `B` type parameter for
///     // `<Foo<....> as PanicFmt>::PV_COUNT` in the generated `to_panicvals` method.
///     struct Foo<ignore A, ignore(u8) B, const X: u32>
///     // bounds must be written in the where clause, like this:
///     where[ A: ?Sized ]
///     {
///         value: u32,
///         _marker: PhantomData<(PhantomData<A>, B)>,
///     }
/// }
/// ```
///
/// <a id = "display-example"></a>
/// ### Display formatting
///
/// ```rust
/// use const_panic::{impl_panicfmt, FmtArg, PanicFmt, PanicVal};
///
/// assert_eq!(const_panic::concat_!(debug: Foo([3, 5, 8])), "Foo([3, 5, 8])");
/// assert_eq!(const_panic::concat_!(display: Foo([3, 5, 8])), "3 5 8");
///
/// struct Foo([u8; 3]);
///
/// impl_panicfmt! {
///     // these (optional) attributes are the only supported struct-level attributes and
///     // can only go in this order
///     #[pfmt(display_fmt = Self::display_fmt)]
///     // need this attribute to output more PanicVals in Display formatting than
///     // in Debug formatting.
///     #[pfmt(panicvals_lower_bound = 10)]
///     struct Foo([u8; 3]);
/// }
///
/// impl Foo {
///     const fn display_fmt(&self, fmtarg: FmtArg) -> [PanicVal<'_>; Foo::PV_COUNT] {
///         let [a, b, c] = self.0;
///
///         const_panic::flatten_panicvals!{fmtarg, Foo::PV_COUNT;
///             a, " ", b, " ", c
///         }
///     }
/// }
///
/// ```
///
/// <a id = "all-the-syntax"></a>
/// ### All the syntax
///
/// ```rust
/// # use const_panic::{impl_panicfmt, PanicFmt, PanicVal};
/// # use const_panic::fmt::FmtArg;
/// #
/// # use std::marker::PhantomData;
/// #
/// # #[derive(Debug)]
/// # pub struct Foo<'a, 'b, C, D, E, const X: u32>
/// # where
/// #    C: ?Sized,
/// #    D: ?Sized,
/// #    E: ?Sized,
/// # {
/// #     _lifetimes: PhantomData<(&'a (), &'b ())>,
/// #     _marker: PhantomData<(PhantomData<C>, PhantomData<D>, PhantomData<E>)>,
/// # }
/// impl_panicfmt!{
///     // these are the only supported struct-level attributes and can only go in this order
///     #[pfmt(display_fmt = Self::display_fmt)]
///     #[pfmt(panicvals_lower_bound = 100)]
///     struct Foo<
///         'a,
///         'b,
///         // For type parameters that aren't formatted.
///         // Removes the `PanicFmt` bound on this type parameter
///         // and uses `()` as the type argument for this to get
///         // `<Foo<....> as PanicFmt>::PV_COUNT` in the generated `to_panicvals` methods.
///         ignore C,
///         // same as `C`, using `u8` instead of `()`
///         ignore(u8) D,
///         // un-`ignore`d type parameters must be replaced with concrete types in the impls below
///         E,
///         const X: u32,
///     >
///     // bounds must be written in the where clause, like this:
///     where[ C: ?Sized, D: ?Sized, E: ?Sized ]
///     {
///         _lifetimes: PhantomData<(&'a (), &'b ())>,
///         _marker: PhantomData<(PhantomData<C>, PhantomData<D>, PhantomData<E>)>,
///     }
///
///     // The impls for this type, this is required for types with un-`ignore`d type parameters.
///     // Otherwise, a generic impl is automatically generated.
///     (
///         impl['a, 'b, D, E: ?Sized, const X: u32] Foo<'a, 'b, D, E, u32, X>
///         where[D: ?Sized]
///     )
///     (
///         impl['a, 'b, D, E: ?Sized, const X: u32] Foo<'a, 'b, D, E, &str, X>
///         where[D: ?Sized]
///     )
/// }
///
/// impl<'a, 'b, C, D, E, const X: u32> Foo<'a, 'b, C, D, E, X>
/// where
///     C: ?Sized,
///     D: ?Sized,
///     E: ?Sized,
/// {
///     const fn display_fmt(
///         &self,
///         fmt: FmtArg,
///     ) -> [PanicVal<'_>; <Foo<'_, '_, (), u8, u32, 0>>::PV_COUNT] {
///         const_panic::flatten_panicvals!{fmt, <Foo<'_, '_, (), u8, u32, 0>>::PV_COUNT;
///             "Foo: ", X
///         }
///     }
/// }
///
/// ```
///
/// [`PanicVal`]: crate::PanicVal
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
#[macro_export]
macro_rules! impl_panicfmt {
    (
        $(# $attrs:tt)*
        $kind:ident $typename:ident < $($rem:tt)*
    ) => (
        const _: () = {
            // for intra-doc links
            use $crate::{self as __cp_bCj7dq3Pud};

            $crate::__impl_panicfmt_step_aaa!{
                ($(# $attrs)* $kind $typename)
                ()
                ($($rem)*)
            }
        };
    );
    (
        $(# $attrs:tt)*
        $kind:ident $typename:ident $($rem:tt)*
    ) => (
        const _: () = {
            // for intra-doc links
            use $crate::{self as __cp_bCj7dq3Pud};

            $crate::__impl_panicfmt_step_aaa!{
                ($(# $attrs)* $kind $typename)
                ()
                (> $($rem)*)
            }
        };
    );
}

#[doc(hidden)]
#[macro_export]
macro_rules! __impl_panicfmt_step_aaa {
    (
        ($(# $attrs:tt)* struct $struct_name:ident)
        $generic_params:tt
        (
            $(,)? >
            $(( $($tupled:tt)* ))?
            $(where[$($where_preds:tt)*])?;

            $($rem:tt)*
        )
    ) => (
        $crate::__impl_panicfmt_step_ccc!{
            [
                $(# $attrs)*
                struct $struct_name
                $generic_params
                ($($($where_preds)*)?)
                $($rem)*
            ]
            []
            [
                $struct_name

                ( $($($tupled)*)? )
            ]
        }
    );
    (
        ($(# $attrs:tt)* struct $struct_name:ident)
        $generic_params:tt
        (
            $(,)? >
            $(where[$($where_preds:tt)*])?
            $(( $($tupled:tt)* ))?;

            $($rem:tt)*
        )
    ) => (
        compile_error!{"the where clause must come after the tuple struct fields"}
    );
    (
        ($(# $attrs:tt)* struct $struct_name:ident)
        $generic_params:tt
        (
            $(,)?>
            $(where[$($where_preds:tt)*])?
            { $($braced:tt)* }

            $($rem:tt)*
        )
    ) => (
        $crate::__impl_panicfmt_step_ccc!{
            [
                $(# $attrs)*
                struct $struct_name
                $generic_params
                ($($($where_preds)*)?)
                $($rem)*
            ]
            []
            [
                $struct_name

                { $($braced)* }
            ]
        }
    );
    (
        ($(# $attrs:tt)* enum $enum_name:ident)
        $generic_params:tt
        (
            $(,)?>
            $(where[$($where_preds:tt)*])?
            {
                $(
                    $variant:ident $({ $($braced:tt)* })? $(( $($tupled:tt)* ))?
                ),*
                $(,)?
            }

            $($rem:tt)*
        )
    ) => (
        $crate::__impl_panicfmt_step_ccc!{
            [
                $(# $attrs)*
                enum $enum_name
                $generic_params
                ($($($where_preds)*)?)
                $($rem)*
            ]
            []
            [
                $(
                    $variant $({ $($braced)* })? $(( $($tupled)* ))?,
                )*
            ]
        }
    );


    ////////////////////////////////////

    (
        $fixed:tt
        ($($prev_params:tt)*)
        ($(,)? $(ignore)? $lifetime:lifetime  $($rem:tt)*)
    ) => (
        $crate::__impl_panicfmt_step_aaa!{
            $fixed
            ($($prev_params)* ($lifetime ($lifetime) ignore ('_)) )
            ($($rem)*)
        }
    );
    (
        $fixed:tt
        $prev_params:tt
        ($(,)? $(ignore $(($($const_def:tt)*))? )? const $const_param:ident: $const_ty:ty , $($rem:tt)*)
    ) => (
        $crate::__impl_panicfmt_step_aaa_const!{
            $fixed
            ($(ignore $(($($const_def)*))? )?)
            $prev_params
            ($const_param: $const_ty)
            ($($rem)*)
        }
    );
    (
        $fixed:tt
        $prev_params:tt
        ($(,)? $(ignore $(($($const_def:tt)*))? )? const $const_param:ident: $const_ty:ty > $($rem:tt)*)
    ) => (
        $crate::__impl_panicfmt_step_aaa_const!{
            $fixed
            ($(ignore $(($($const_def)*))? )?)
            $prev_params
            ($const_param: $const_ty)
            (> $($rem)*)
        }
    );
    (
        $fixed:tt
        ($($prev_params:tt)*)
        ($(,)? ignore $(($ignore_ty:ty))? $type_param:ident $($rem:tt)*)
    ) => (
        $crate::__impl_panicfmt_step_aaa!{
            $fixed
            ($($prev_params)* ($type_param ($type_param) ignore (($($ignore_ty)?)) ) )
            ($($rem)*)
        }
    );
    (
        $fixed:tt
        ($($prev_params:tt)*)
        ($(,)? $type_param:ident $($rem:tt)*)
    ) => (
        $crate::__impl_panicfmt_step_aaa!{
            $fixed
            (kept_type[$type_param] $($prev_params)* ($type_param ($type_param) kept ()) )
            ($($rem)*)
        }
    );
    (
        $fixed:tt
        $prev_params:tt
        ($($rem:tt)+)
    ) => (
        $crate::__::compile_error!{concat!(
            "could not parse this in the generic parameter(s) of the type definition:",
            stringify!($($rem:tt)+)
        )}
    );
}

#[doc(hidden)]
#[macro_export]
macro_rules!  __impl_panicfmt_step_aaa_const {
    (
        $fixed:tt
        (ignore($const_def:expr))
        ($($prev_params:tt)*)
        ($const_param:ident: $const_ty:ty)
        $rem:tt
    ) => {
        $crate::__impl_panicfmt_step_aaa!{
            $fixed
            (
                $($prev_params)*
                (
                    $const_param
                    (const $const_param: $const_ty)
                    ignore ({$const_def})
                )
            )
            $rem
        }
    };
    (
        $fixed:tt
        ($(ignore)?)
        ($($prev_params:tt)*)
        ($const_param:ident: $const_ty:ty)
        $rem:tt
    ) => {
        $crate::__impl_panicfmt_step_aaa!{
            $fixed
            (
                $($prev_params)*
                (
                    $const_param
                    (const $const_param: $const_ty)
                    ignore ({$crate::__::ConstDefault::DEFAULT})
                )
            )
            $rem
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules!  __impl_panicfmt_step_ccc {
    (
        $kept:tt
        $prev_variants:tt
        [
            $variant:ident
            $($(@$is_brace:tt@)? {$($br_field:ident: $br_ty:ty),* $(,)*})?
            $($(@$is_tuple:tt@)? ( $($tup_ty:ty),* $(,)* ))?
            $(,$($rem_variants:tt)*)?
        ]
    ) => {
        $crate::zip_counter_and_last!{
            $crate::__impl_panicfmt_step_ccc_inner!{
                $kept
                $prev_variants
                $variant
                (
                    $($($is_brace)? Braced)?
                    $($($is_tuple)? Tupled)?
                    Braced
                )
                [$($($rem_variants)*)?]
            }
            ($($(($br_field, $br_ty))*)? $($((, $tup_ty))*)?)
            (
                (0 fi0) (1 fi1) (2 fi2) (3 fi3) (4 fi4) (5 fi5) (6 fi6) (7 fi7)
                (8 fi8) (9 fi9) (10 fi10) (11 fi11) (12 fi12) (13 fi13) (14 fi14) (15 fi15)
                (16 fi16) (17 fi17) (18 fi18) (19 fi19) (20 fi20) (21 fi21) (22 fi22) (23 fi23)
                (24 fi24) (25 fi25) (26 fi26) (27 fi27) (28 fi28) (29 fi29) (30 fi30) (31 fi31)
                (32 fi32) (33 fi33) (34 fi34) (35 fi35) (36 fi36) (37 fi37) (38 fi38) (39 fi39)
                (40 fi40) (41 fi41) (42 fi42) (43 fi43) (44 fi44) (45 fi45) (46 fi46) (47 fi47)
                (48 fi48) (49 fi49) (50 fi50) (51 fi51) (52 fi52) (53 fi53) (54 fi54) (55 fi55)
                (56 fi56) (57 fi57) (58 fi58) (59 fi59) (60 fi60) (61 fi61) (62 fi62) (63 fi63)
            )
        }
    };
    // Parsing unit variants / structs
    (
        $kept:tt
        [$($prev_variants:tt)*]
        [
            $variant:ident
            $(, $($rem_variants:tt)*)?
        ]
    ) => {
        $crate::__impl_panicfmt_step_ccc!{
            $kept
            [$($prev_variants)* ($variant Braced) ]
            [$($($rem_variants)*)?]
        }
    };
    // Finished parsing variants/structs,
    //
    (
        $kept:tt
        $variants:tt
        []
    ) => {
        $crate::__impl_panicfmt_step__panicfmt_impl!{
            $kept
            variants $variants

            $kept
            $variants
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules!  __impl_panicfmt_step_ccc_inner {
    (
        $kept:tt
        [$($prev_variants:tt)*]
        $variant:ident
        ($delim:ident $($ignore0:tt)*)
        [$($rem_variants:tt)*]

        $(prefix (($($p_fname:ident)?, $p_ty:ty) ($p_index:tt $p_fi_index:tt)))*
        $(last (($($l_fname:ident)?, $l_ty:ty) ($l_index:tt $l_fi_index:tt)) )?
    ) => {
        $crate::__impl_panicfmt_step_ccc!{
            $kept
            [
                $($prev_variants)*
                (
                    $variant
                    $delim
                    ($($l_index + 1,)? 0,)
                    =>
                    $(prefix (($($p_fname)? $p_index), ($($p_fname)? $p_fi_index), $p_ty))*
                    $(last (($($l_fname)? $l_index), ($($l_fname)? $l_fi_index), $l_ty))?
                )
            ]
            [$($rem_variants)*]
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules!  __impl_panicfmt_step__panicfmt_impl {
    (

        [
            $(#[pfmt(display_fmt = $__display_fmt:expr)])?
            $(#[pfmt(panicvals_lower_bound = $panicvals_lower_bound:expr)])?
            $type_kind:ident $type_name:ident
            (
                $($kept_type:ident [$kept_type_:ident])*
                $((
                    $gp_arg:tt
                    ($($gp_param:tt)*)
                    $ignorance:tt
                    ($($gp_arg_concrete:tt)*)
                ))*
            )
            ($($where_preds:tt)*)

            $($to_panicval_impls:tt)*
        ]

        variants[$(
            (
                $variant:ident
                $delimiter:ident
                ($field_amount:expr, $($ignore2:tt)*)
                =>
                $(
                    $is_last_field:ident
                    (
                        ($fpati:tt $($ignore3:tt)?),
                        ($fname:tt $($ignore4:tt)?),
                        $ty:ty
                    )
                )*
            )
        )*]

        $kept:tt
        $variants:tt
    ) => (
        impl<$($($gp_param)*),*> $crate::PanicFmt for $type_name<$($gp_arg),*>
        where
            $($kept_type_: $crate::PanicFmt,)*
            $($where_preds)*
        {
            type This = Self;
            type Kind = $crate::fmt::IsCustomType;

            const PV_COUNT: $crate::__::usize = $crate::utils::slice_max_usize(&[
                $(
                    $crate::fmt::ComputePvCount{
                        field_amount: $field_amount,
                        summed_pv_count: 0 $( + <$ty as $crate::PanicFmt>::PV_COUNT )*,
                        delimiter: $crate::fmt::TypeDelim::$delimiter
                    }.call(),
                )*
                $($panicvals_lower_bound)?
            ]);
        }

        macro_rules! __assert_type_name__ {
            ($type_name) => ()
        }

        $crate::__impl_to_panicvals!{
            [$($kept_type)*]
            [$($ignorance ($($gp_arg_concrete)*))*]
            [
                $type_name
                [$($where_preds)*]
                $((
                    ($($gp_param)*)
                    $gp_arg
                ))*
            ]
            [$($to_panicval_impls)*]
            $kept
            $variants
        }
    );
}

#[doc(hidden)]
#[macro_export]
macro_rules! __impl_to_panicvals {
    (
        [$(kept_type)*]
        $ignorances:tt
        $gp_param:tt
        [$($to_panicval_impls:tt)+]
        $kept:tt
        $variants:tt
    ) => (
        $(
            $crate::__impl_to_panicvals_step_aaa!{
                $ignorances
                $to_panicval_impls
                $to_panicval_impls
                $kept
                $variants
            }
        )*
    );
    (
        []
        $ignorances:tt
        [$type_name:ident [$($where_preds:tt)*] $(( ($($gp_param:tt)*) $gp_arg:tt ))* ]
        []
        $kept:tt
        $variants:tt
    ) => (
        $crate::__impl_to_panicvals_step_aaa!{
            $ignorances
            (
                impl[$($($gp_param)*),*] $type_name<$($gp_arg),*>
                where[$($where_preds)*]
            )
            (
                impl[$($($gp_param)*),*] $type_name<$($gp_arg),*>
                where[$($where_preds)*]
            )
            $kept
            $variants
        }
    );
    ([$(kept_type)+] $ignorances:tt $gp_param:tt [] $kept:tt $variants:tt) => (
        $crate::__::compile_error!{"\
            type parameters must either be:\n\
            - all prefixed with ignore\n\
            - be replaced with concrete type arguments in \
            `(impl Foo<Bar>)`s after the type definition\n\
        "}
    );
}

#[doc(hidden)]
#[macro_export]
macro_rules! __impl_to_panicvals_step_aaa {
    (
        $ignorances:tt
        (impl $([$($impl_param:tt)*])? $type_name:ident $(where $where_preds:tt)?)
        $impl:tt
        $kept:tt
        $variants:tt
    ) => {
        __assert_type_name__!{$type_name}

        $crate::__impl_to_panicvals_step_bbb!{
            ([$($($impl_param)*)?] $type_name $kept $variants)
            $ignorances
            []
            [> $(where $where_preds)?]
        }
    };
    (
        $ignorances:tt
        (impl $([$($impl_param:tt)*])? $type_name:ident <$($args:tt)*)
        (impl $([$($impl_paramb:tt)*])? $type:path $(where $where_preds:tt)? )
        $kept:tt
        $variants:tt
    ) => {
        __assert_type_name__!{$type_name}

        $crate::__impl_to_panicvals_step_bbb!{
            ([$($($impl_param)*)?] $type $kept $variants)
            $ignorances
            []
            [$($args)*]
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __impl_to_panicvals_step_bbb {
    // finished parsing
    (
        $kept:tt
        []
        $cself:tt
        [> $(where[$($where_preds:tt)*])?]
    ) => {
        $crate::__impl_to_panicvals_finish!{
            $kept
            [$($($where_preds)*)?]
            $cself
        }
    };



    // const or lifetime argument
    ($kept:tt $ignorance:tt $prev_cself:tt [$(,)? $gen_arg:tt , $($rem:tt)*]) => {
        $crate::__impl_to_panicvals_step_bbb_inner!{
            $kept
            $ignorance
            [$gen_arg]
            $prev_cself
            [$($rem)*]
        }
    };
    ($kept:tt $ignorance:tt $prev_cself:tt [$(,)? $gen_arg:tt > $($rem:tt)*]) => {
        $crate::__impl_to_panicvals_step_bbb_inner!{
            $kept
            $ignorance
            [$gen_arg]
            $prev_cself
            [> $($rem)*]
        }
    };

    // type argument
    ($kept:tt $ignorance:tt $prev_cself:tt [$(,)? $ty_arg:ty , $($rem:tt)* ]) => {
        $crate::__impl_to_panicvals_step_bbb_inner!{
            $kept
            $ignorance
            [$ty_arg]
            $prev_cself
            [$($rem)*]
        }
    };
    ($kept:tt $ignorance:tt $prev_cself:tt [$(,)? $ty_arg:ty > $($rem:tt)*]) => {
        $crate::__impl_to_panicvals_step_bbb_inner!{
            $kept
            $ignorance
            [$ty_arg]
            $prev_cself
            [> $($rem)*]
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __impl_to_panicvals_step_bbb_inner {
    (
        $kept:tt
        [kept $gp_arg_concrete:tt $($rem_ignorance:tt)*]
        [$gen_arg:tt]
        [$($prev_cself:tt)*]
        $rem:tt
    ) => {
        $crate::__impl_to_panicvals_step_bbb!{
            $kept
            [$($rem_ignorance)*]
            [$($prev_cself)* $gen_arg,]
            $rem
        }
    };
    (
        $kept:tt
        [ignore ($($gp_arg_concrete:tt)*) $($rem_ignorance:tt)*]
        $gen_arg:tt
        [$($prev_cself:tt)*]
        $rem:tt
    ) => {
        $crate::__impl_to_panicvals_step_bbb!{
            $kept
            [$($rem_ignorance)*]
            [$($prev_cself)* $($gp_arg_concrete)*,]
            $rem
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __impl_to_panicvals_finish {
    // finished parsing
    (
        (
            [$($impl_param:tt)*]
            $type:ty

            [
                $(#[pfmt(display_fmt = $display_fmt:expr)])?
                $(#[pfmt(panicvals_lower_bound = $__panicvals_lower_bound:expr)])?
                $type_kind:ident $type_name:ident
                $generics:tt
                $type_where_preds:tt

                $($to_panicval_impls:tt)*
            ]

            [$(
                (
                    $variant:ident
                    $delimiter:ident
                    ($field_amount:expr, $($ignore2:tt)*)
                    =>
                    $(
                        $is_last_field:ident
                        (
                            ($fpati:tt $($ignore3:tt)?),
                            ($fname:tt $($ignore4:tt)?),
                            $ty:ty
                        )
                    )*
                )
            )*]

        )
        [ $($where_preds:tt)* ]
        $cself:tt
    ) => {
        /// Provides the required `to_panicvals` method for
        /// [`const_panic::fmt::PanicFmt` trait](trait@__cp_bCj7dq3Pud::fmt::PanicFmt)
        #[automatically_derived]
        impl<$($impl_param)*> $type
        where
            $($where_preds)*
        {
            /// Gets the `PanicVal`s for
            /// [`const_panic`](__cp_bCj7dq3Pud)-based
            /// formatting of this value.
            pub const fn to_panicvals(
                &self,
                mut fmt: $crate::FmtArg,
            ) -> [$crate::PanicVal<'_>; $crate::__ipm_cself!($type_name $cself)] {
                $(
                    if let $crate::fmt::FmtKind::Display = fmt.fmt_kind {
                        $display_fmt(self, fmt)
                    } else
                )? {
                    match self {
                        $(
                            $crate::__ipm_pattern!($type_kind $variant{$($fpati: $fname,)* ..}) =>
                                $crate::__ipm_fmt!{
                                    ($crate::__ipm_cself!($type_name $cself))
                                    $delimiter
                                    $variant
                                    fmt
                                    ( $($is_last_field ($fname, $ty))* )
                                },
                        )*
                    }
                }
            }
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules!  __ipm_pattern {
    (struct $name:ident {$($patterns:tt)*}) => {
        $name {$($patterns)*}
    };
    (enum $name:ident {$($patterns:tt)*}) => {
        Self::$name {$($patterns)*}
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules!  __ipm_fmt {
    (
        ($count:expr) $delimiter:ident $typename:ident $fmt:ident
        ( $($is_last_field:ident ($fname:ident, $ty:ty))+ )
    ) => ({
        let (open, close) = $crate::fmt::TypeDelim::$delimiter.get_open_and_close();

        $crate::__::flatten_panicvals::<{$count}>(&[
            &[
                $crate::PanicVal::write_str($crate::__::stringify!($typename)),
                {
                    $fmt = $fmt.indent();
                    open.to_panicval($fmt)
                }
            ],
            $(
                $crate::__ipm_pv_fmt_field_name!($delimiter $fname),
                &$crate::PanicFmt::PROOF
                    .infer($fname)
                    .coerce($fname)
                    .to_panicvals($fmt),
                &$crate::__ipm_pv_comma!($is_last_field)
                    .to_panicvals($fmt),
            )*
            &[
                {
                    $fmt = $fmt.unindent();
                    close.to_panicval($fmt)
                }
            ],
        ])
    });
    (
        ($count:expr) $delimiter:ident $typename:ident $fmt:ident
        ()
    ) => {
        $crate::__::flatten_panicvals::<{$count}>(&[
            &[$crate::PanicVal::write_str($crate::__::stringify!($typename))]
        ])
    }
}

#[doc(hidden)]
#[macro_export]
macro_rules! __ipm_pv_fmt_field_name {
    (Tupled $field_name:ident) => {
        &[]
    };
    (Braced $field_name:ident) => {
        &[$crate::PanicVal::write_str($crate::__::concat!(
            $crate::__::stringify!($field_name),
            ": "
        ))]
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __ipm_pv_comma {
    (prefix) => {
        $crate::fmt::COMMA_SEP
    };
    (last) => {
        $crate::fmt::COMMA_TERM
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __ipm_cself {
    ($type_name:ident [$($cself:tt)*]) => {
        <$type_name<$($cself)*> as $crate::PanicFmt>::PV_COUNT
    };
}
