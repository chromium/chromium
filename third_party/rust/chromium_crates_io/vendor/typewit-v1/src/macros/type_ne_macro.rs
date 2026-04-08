
/// Constructs a [`TypeNe`](crate::TypeNe)
/// of types that are statically known to be different.
/// 
/// This macro is syntactic sugar for calling 
/// [`TypeNe::with_fn`](crate::TypeNe::with_fn) with a private 
/// [`InjTypeFn`](crate::InjTypeFn) implementor.
/// 
/// # Syntax 
/// 
/// This macro takes this syntax:
/// 
/// ```text
/// $( < $($generic_param:generic_param),* $(,)? > )?  $left_type:ty, $right_type:ty
/// $(where $($where_cause:tt)*)?
/// ```
/// 
/// # Limitations
/// 
/// This macro can't use generic parameters from the surrounding scope,
/// they must be redeclared within the macro to be used.
/// 
/// # Example
/// 
/// ```rust
/// use typewit::TypeNe;
/// 
/// const _: TypeNe<u8, (u8,)> = foo();
/// 
/// const fn foo<T>() -> TypeNe<T, (T,)> 
/// where
///     (T,): Copy,
/// {
///     typewit::type_ne!{
///         <X>  X, (X,) 
///         where (X,): Copy
///     }
/// }
/// 
/// 
/// ```
#[macro_export]
macro_rules! type_ne {
    (< $($generics:tt)* ) => {
        $crate::__::__parse_in_generics! {
            ($crate::__tyne_parsed_capture_generics !())
            [] [] [$($generics)*]
        }
    };
    ($left_ty:ty, $($rem:tt)*) => {
        $crate::__tyne_parsed_capture_generics! {
            []
            []
            []
            $left_ty, $($rem)*
        }
    };
    ($($rem:tt)*) => {
        $crate::__::compile_error!{"invalid arguments for `type_ne` macro"}
    }
}


#[doc(hidden)]
#[macro_export]
macro_rules! __tyne_parsed_capture_generics {
    (
        [$(($gen_arg:tt ($($($gen_phantom:tt)+)?) $($gen_rem:tt)*))*]
        [$(($($gen_params:tt)*))*]
        $deleted_markers:tt

        $left_ty:ty, $right_ty:ty $(,)?

        $(where $($where:tt)*)?
    ) => ({
        struct __TypeNeParameterizer<$($($gen_params)*,)*>(
            $crate::__::PhantomData<(
                $($($crate::__::PhantomData<$($gen_phantom)+>,)?)*
            )>
        )$( where $($where)* )?;

        impl<$($($gen_params)*,)*> __TypeNeParameterizer<$($gen_arg,)*> 
        $( where $($where)* )?
        {
            const NEW: Self = Self($crate::__::PhantomData);
        }

        $crate::__impl_with_span! {
            $left_ty // span
            () // impl attrs
            ( <$($($gen_params)*,)*> $crate::TypeFn<$crate::type_ne::LeftArg> )
            // for
            (__TypeNeParameterizer<$($gen_arg,)*>)
            ( $( where $($where)* )? )
            (
                type Output = $left_ty;

                const TYPE_FN_ASSERTS: () = { 
                    let _: $crate::CallInjFn<Self, $crate::type_ne::LeftArg>; 
                };
            )
        }
        
        $crate::__impl_with_span! {
            $left_ty // span
            () // impl attrs
            ( <$($($gen_params)*,)*> $crate::RevTypeFn<$left_ty> )
            // for
            (__TypeNeParameterizer<$($gen_arg,)*>)
            ( $( where $($where)* )? )
            (
                type Arg = $crate::type_ne::LeftArg;
            )
        }

        $crate::__impl_with_span! {
            $right_ty // span
            () // impl attrs
            ( <$($($gen_params)*,)*> $crate::TypeFn<$crate::type_ne::RightArg> )
            // for
            (__TypeNeParameterizer<$($gen_arg,)*>)
            ( $( where $($where)* )? )
            (
                type Output = $right_ty;

                const TYPE_FN_ASSERTS: () = { 
                    let _: $crate::CallInjFn<Self, $crate::type_ne::RightArg>; 
                };
            )
        }

        $crate::__impl_with_span! {
            $right_ty // span
            () // impl attrs
            ( <$($($gen_params)*,)*> $crate::RevTypeFn<$right_ty> )
            // for
            (__TypeNeParameterizer<$($gen_arg,)*>)
            ( $( where $($where)* )? )
            (
                type Arg = $crate::type_ne::RightArg;
            )
        }

        $crate::TypeNe::with_fn(
            __TypeNeParameterizer::NEW
        )
    });
}