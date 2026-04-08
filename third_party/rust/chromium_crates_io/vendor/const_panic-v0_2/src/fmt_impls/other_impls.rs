use crate::{FmtArg, PanicVal};

use core::{
    marker::{PhantomData, PhantomPinned},
    ptr::NonNull,
};

use core as std;

macro_rules! ptr_impls {
    ($ty:ty) => {
        primitive_static_panicfmt! {
            fn[T: ?Sized](&self: $ty, _f) {
                PanicVal::write_str("<pointer>")
            }
        }
    };
}

ptr_impls! {*const T}

ptr_impls! {*mut T}

ptr_impls! {NonNull<T>}

impl_for_option! {
    (for[T], 'static, NonNull<T>, NonNull<T>)
}

primitive_static_panicfmt! {
    fn[T: ?Sized](&self: PhantomData<T>, _f) {
        PanicVal::write_str("PhantomData")
    }
}

primitive_static_panicfmt! {
    fn[](&self: PhantomPinned, _f) {
        PanicVal::write_str("PhantomPinned")
    }
}

primitive_static_panicfmt! {
    fn[](&self: (), _f) {
        PanicVal::write_str("()")
    }
}

impl_for_option! {
    (for[], 'static, core::cmp::Ordering, core::cmp::Ordering)
}
primitive_static_panicfmt! {
    fn[](&self: std::cmp::Ordering, _f) {
        let v = match self.0 {
            std::cmp::Ordering::Less => "Less",
            std::cmp::Ordering::Equal => "Equal",
            std::cmp::Ordering::Greater => "Greater",
        };
        PanicVal::write_str(v)
    }
}

primitive_static_panicfmt! {
    fn[](&self: std::sync::atomic::Ordering, _f) {
        use std::sync::atomic::Ordering;
        let v = match self.0 {
            Ordering::Relaxed => "Relaxed",
            Ordering::Release => "Release",
            Ordering::Acquire => "Acquire",
            Ordering::AcqRel => "AcqRel",
            Ordering::SeqCst => "SeqCst",
            _ => "<std::sync::atomic::Ordering>",
        };
        PanicVal::write_str(v)
    }
}

#[allow(unreachable_code)]
const _: () = {
    primitive_static_panicfmt! {
        fn[](&self: std::convert::Infallible, _f) {
            match *self.0 {}
        }
    }
};
