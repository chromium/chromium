use core::{marker::PhantomData, mem::ManuallyDrop};

pub mod range_into_iter;
pub mod slice_into_iter;

pub trait IntoIterKind {
    type Kind;
}

pub struct IsStdKind {}

pub struct IsNonIteratorKind {}

pub struct IsIteratorKind {}

///
#[repr(transparent)]
pub struct IntoIterWrapper<I, K> {
    pub iter: ManuallyDrop<I>,
    pub marker: IsIntoIterKind<I, K>,
}

mod is_into_iter_kind {
    use super::*;

    pub struct IsIntoIterKind<T, K>(PhantomData<(fn() -> PhantomData<T>, fn() -> K)>);

    impl<T> IsIntoIterKind<T, T::Kind>
    where
        T: IntoIterKind,
    {
        pub const NEW: Self = Self(PhantomData);
    }
}
pub use is_into_iter_kind::IsIntoIterKind;

impl<T> IntoIterWrapper<T, IsStdKind> {
    #[inline(always)]
    pub const fn coerce(self) -> Self {
        self
    }
}

impl<T> IntoIterWrapper<T, IsNonIteratorKind> {
    #[inline(always)]
    pub const fn coerce(self) -> T {
        ManuallyDrop::into_inner(self.iter)
    }
}

impl<T> IntoIterWrapper<T, IsIteratorKind> {
    #[inline(always)]
    pub const fn coerce(self) -> Self {
        self
    }

    #[inline(always)]
    pub const fn const_into_iter(self) -> T {
        ManuallyDrop::into_inner(self.iter)
    }
}

////////////////////////////////////////////////////////////////////////////////

pub struct EmptyIter;

impl EmptyIter {
    #[inline(always)]
    pub const fn next(self) -> Option<(core::convert::Infallible, Self)> {
        None
    }
}

impl IntoIterKind for EmptyIter {
    type Kind = IsIteratorKind;
}

////////////////////////////////////////////////////////////////////////////////

#[macro_export]
macro_rules! into_iter_macro {
    ($iter:expr) => {
        $crate::__::IntoIterWrapper {
            iter: $crate::__::ManuallyDrop::new($iter),
            marker: $crate::__::IsIntoIterKind::NEW,
        }
        .coerce()
        .const_into_iter()
    };
}
