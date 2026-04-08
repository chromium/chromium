
/// Declares an
/// [injective type-level function](crate::type_fn::InjTypeFn)
/// 
/// This macro takes in the exact same syntax as the [`type_fn`] macro.
/// 
/// This macro generates the same items as the `type_fn` macro,
/// in addition to implementing [`RevTypeFn`], 
/// so that the function implements [`InjTypeFn`].
/// 
/// 
/// # Example
/// 
/// This macro is also demonstrated in 
/// `TypeNe::{`[`map`]`, `[`project`]`, `[`unmap`]`, `[`unproject`]`}`.
/// 
/// [`map`]: crate::TypeNe::map
/// [`project`]: crate::TypeNe::project
/// [`unmap`]: crate::TypeNe::unmap
/// [`unproject`]: crate::TypeNe::unproject
/// 
/// ### Basic
/// 
/// ```rust
/// use typewit::{CallFn, UncallFn, inj_type_fn};
/// 
/// // Calls the `ToSigned` function with `u64` as the argument.
/// let _: CallFn<ToSigned, u64> = 3i64;
/// 
/// // Gets the argument of the `ToSigned` function from the `i8` return value.
/// let _: UncallFn<ToSigned, i8> = 5u8;
/// 
/// inj_type_fn!{
///     struct ToSigned;
/// 
///     impl u128 => i128;
///     impl u64 => i64;
///     impl u32 => i32;
///     impl u16 => i16;
///     impl u8 => i8;
/// }
/// ```
/// 
/// <details>
/// <summary>
/// <p>
/// 
/// the above `inj_type_fn` macro invocation roughly expands to this code
/// </p>
/// </summary>
///
/// ```rust
/// struct ToSigned;
/// 
/// impl ToSigned {
///     const NEW: Self = Self;
/// }
/// 
/// impl ::typewit::TypeFn<u128> for ToSigned {
///     type Output = i128;
/// }
/// 
/// impl ::typewit::RevTypeFn<i128> for ToSigned {
///     type Arg = u128;
/// }
/// 
/// impl ::typewit::TypeFn<u64> for ToSigned {
///     type Output = i64;
/// }
/// 
/// impl ::typewit::RevTypeFn<i64> for ToSigned {
///     type Arg = u64;
/// }
/// 
/// impl ::typewit::TypeFn<u32> for ToSigned {
///     type Output = i32;
/// }
/// 
/// impl ::typewit::RevTypeFn<i32> for ToSigned {
///     type Arg = u32;
/// }
/// 
/// impl ::typewit::TypeFn<u16> for ToSigned {
///     type Output = i16;
/// }
/// 
/// impl ::typewit::RevTypeFn<i16> for ToSigned {
///     type Arg = u16;
/// }
/// 
/// impl ::typewit::TypeFn<u8> for ToSigned {
///     type Output = i8;
/// }
/// 
/// impl ::typewit::RevTypeFn<i8> for ToSigned {
///     type Arg = u8;
/// }
/// ```
/// </details>
/// 
/// [`type_fn`]: macro@crate::type_fn
/// [`TypeFn`]: crate::type_fn::TypeFn
/// [`InjTypeFn`]: crate::type_fn::InjTypeFn
/// [`RevTypeFn`]: crate::type_fn::RevTypeFn
#[macro_export]
macro_rules! inj_type_fn {
    ($($args:tt)*) => {
        $crate::__type_fn!{
            __tyfn_injtypefn_impl
            $($args)*
        }
    }
}


#[doc(hidden)]
#[macro_export]
macro_rules! __tyfn_injtypefn_impl {
    (
        (
            $(#[$attrs:meta])*
            $impl:ident[$(($($fn_gen_param:tt)*))*] $ty_arg:ty => $ret_ty:ty
            where[ $($where_preds:tt)* ] 
        )

        $function_name:ident

        [$(($capt_gen_args:tt $($rem_0:tt)*))*]
        [
            $(($capt_lt:lifetime $($capt_lt_rem:tt)*))*
            $(($capt_tcp:ident $($capt_tcp_rem:tt)*))*
        ]
        where [$($capt_where:tt)*]
        
    ) => {
        $crate::__impl_with_span! {
            $ty_arg // span
            ( $(#[$attrs])* #[allow(unused_parens)] )
            (
                <
                    $($capt_lt $($capt_lt_rem)*,)*
                    $($($fn_gen_param)*,)*
                    $($capt_tcp $($capt_tcp_rem)*,)*
                > $crate::TypeFn<$ty_arg> 
            )
            // for
            ( $function_name<$($capt_gen_args),*> )
            (
                where
                    $($capt_where)*
                    $($where_preds)*
            )
            (
                type Output = $ret_ty;

                const TYPE_FN_ASSERTS: () = { let _: $crate::CallInjFn<Self, $ty_arg>; };
            )
        }

        $crate::__impl_with_span! {
            $ret_ty // span
            ( $(#[$attrs])* #[allow(unused_parens)] )
            (
                <
                    $($capt_lt $($capt_lt_rem)*,)*
                    $($($fn_gen_param)*,)*
                    $($capt_tcp $($capt_tcp_rem)*,)*
                > $crate::type_fn::RevTypeFn<$ret_ty> 
            )
            // for
            ( $function_name<$($capt_gen_args),*> )
            (
                where
                    $($capt_where)*
                    $($where_preds)*
            )
            (
                type Arg = $ty_arg;
            )
        }
    };
}