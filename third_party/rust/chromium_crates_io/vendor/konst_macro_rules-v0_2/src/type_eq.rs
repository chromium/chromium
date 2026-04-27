use core::marker::PhantomData;

mod __ {
    use super::*;

    /// Value-level proof that `L` is the same type as `R`
    ///
    /// Type type can be used to prove that `L` and `R` are the same type,
    /// because it can only be constructed with `TypeEq::<L, L>::NEW`,
    /// where both type arguments are the same type.
    pub struct TypeEq<L: ?Sized, R: ?Sized>(
        PhantomData<(
            fn(PhantomData<L>) -> PhantomData<L>,
            fn(PhantomData<R>) -> PhantomData<R>,
        )>,
    );

    impl<L: ?Sized> TypeEq<L, L> {
        /// Constructs a `TypeEq<L, L>`.
        pub const NEW: Self = TypeEq(PhantomData);
    }
}
pub use __::TypeEq;

impl<L: ?Sized, R: ?Sized> Copy for TypeEq<L, R> {}

impl<L: ?Sized, R: ?Sized> Clone for TypeEq<L, R> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<L, R> TypeEq<L, R> {
    /// Whether `L` is the same type as `R`.
    const ARE_SAME_TYPE: Amb = {
        // hacky way to emulate a lifetime-unaware
        // `TypeId::of<L>() == TypeId::of<R>()`
        let approx_same_type = {
            core::mem::size_of::<L>() == core::mem::size_of::<R>()
                && core::mem::align_of::<L>() == core::mem::align_of::<R>()
                && core::mem::size_of::<Option<L>>() == core::mem::size_of::<Option<R>>()
                && core::mem::align_of::<Option<L>>() == core::mem::align_of::<Option<R>>()
        };

        if approx_same_type {
            Amb::Indefinite
        } else {
            Amb::No
        }
    };

    /// Hints to the compiler that holding a `TypeEq<L, R>` where `L != R` is impossible.
    #[inline(always)]
    pub const fn reachability_hint(self) {
        if let Amb::No = Self::ARE_SAME_TYPE {
            // safety: it's impossible to have a `TypeEq<L, R>` value,
            // where `L` and `R` are not the same type
            #[cfg(feature = "rust_1_57")]
            unsafe {
                core::hint::unreachable_unchecked()
            }
            #[cfg(not(feature = "rust_1_57"))]
            #[allow(unreachable_code)]
            unsafe {
                match crate::__priv_transmute!((), core::convert::Infallible, ()) {}
            }
        }
    }

    /// Transforms `L` to `R` given a `TypeEq<L, R>`
    /// (the `TypeEq` value proves that `L` and `R` are the same type)
    #[inline(always)]
    pub const fn to_right(self, from: L) -> R {
        self.reachability_hint();

        unsafe { crate::__priv_transmute!(L, R, from) }
    }
}

enum Amb {
    // indefinitely false/true
    Indefinite,
    // definitely false
    No,
}
