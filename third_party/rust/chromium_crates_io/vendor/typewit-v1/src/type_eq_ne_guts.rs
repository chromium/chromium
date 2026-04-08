// $type_cmp is either TypeEq or TypeNe
macro_rules! declare_zip_helper {($_:tt $type_cmp_ty:ident) => {
    macro_rules! zip_impl {
        // Using more specific patterns for $L and $R to prevent usage of macros,
        // which can expand to different values on each use
        ($_( $type_cmp:ident [$_($L:ident)::*, $_($R:ident)::*] ),* $_(,)*) => {
            $_(
                let _te: $type_cmp_ty<$_($L)::*, $_($R)::*> = $type_cmp;
            )*

            // SAFETY: 
            // `$type_cmp_ty<$_($L)::*, $_($R)::*>` for every passed `$type_cmp`
            // implies `$type_cmp_ty<(L0, L1, ...), (R0, R1, ...)>`
            unsafe {
                $type_cmp_ty::<($_($_($L)::*,)*), ($_($_($R)::*,)*)>::new_unchecked()
            }
        }
    }

}} pub(crate) use declare_zip_helper;

// $type_cmp is either TypeEq or TypeNe
macro_rules! declare_helpers {($_:tt $type_cmp_ty:ident $tyfn:ident $callfn:ident) => {
    macro_rules! projected_type_cmp {
        ($type_cmp:expr, $L:ty, $R:ty, $F:ty) => ({
            // Safety(TypeEq): 
            // this takes a `TypeEq<$L, $R>`,
            // which implies `TypeEq<CallFn<$tyfn, $L>, CallFn<$tyfn, $R>>`.
            //
            // Safety(TypeNe): 
            // this takes a `TypeNe<$L, $R>`,
            // and requires `$tyfn: InjTypeFn<$L> + InjTypeFn<$R>`,
            // (`InjTypeFn` guarantees that unequal arguments map to unequal return values),
            // which implies `TypeNe<CallInjFn<$tyfn, $L>, CallInjFn<$tyfn, $R>>`.
            unsafe {
                __ProjectVars::<$F, $L, $R> {
                    te: $type_cmp,
                    projected_te: $type_cmp_ty::new_unchecked(),
                }.projected_te
            }
        })
    } use projected_type_cmp;

    struct __ProjectVars<F, L: ?Sized, R: ?Sized> 
    where
        InvokeAlias<F>: $tyfn<L> + $tyfn<R>
    {
        #[allow(dead_code)]
        te: $type_cmp_ty<L, R>,

        //         $type_cmp_ty<L, R> 
        // implies $type_cmp_ty<$callfn<F, L>, $callfn<F, R>>
        projected_te: $type_cmp_ty<$callfn<InvokeAlias<F>, L>, $callfn<InvokeAlias<F>, R>>,
    }

    macro_rules! unprojected_type_cmp {
        ($type_cmp:expr, $L:ty, $R:ty, $F:ty) => ({
            // safety: 
            // This macro takes a `$type_cmp_ty<$L, $R>` value,
            // which implies `$type_cmp_ty<UncallFn<F, $L>, UncallFn<F, $R>>`
            //  
            // The properties section of RevTypeFn guarantees this for 
            // both TypeEq and TypeNe
            unsafe {
                __UnprojectVars::<$F, $L, $R> {
                    te: $type_cmp,
                    unprojected_te: $type_cmp_ty::new_unchecked(),
                }.unprojected_te
            }
        })
    }

    struct __UnprojectVars<F, L: ?Sized, R: ?Sized> 
    where
        InvokeAlias<F>: crate::RevTypeFn<L> + crate::RevTypeFn<R>
    {
        #[allow(dead_code)]
        te: $type_cmp_ty<L, R>,

        //         $type_cmp_ty<L, R> 
        // implies $type_cmp_ty<UncallFn<F, L>, UncallFn<F, R>>
        //  
        // The properties section of RevTypeFn guarantees this for 
        // both TypeEq and TypeNe
        unprojected_te: $type_cmp_ty<UncallFn<InvokeAlias<F>, L>, UncallFn<InvokeAlias<F>, R>>,
    }

}} pub(crate) use declare_helpers;

