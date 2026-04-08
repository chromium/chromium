use crate::{FmtArg, PanicVal};

use core::num::{
    NonZeroI128, NonZeroI16, NonZeroI32, NonZeroI64, NonZeroI8, NonZeroIsize, NonZeroU128,
    NonZeroU16, NonZeroU32, NonZeroU64, NonZeroU8, NonZeroUsize,
};

macro_rules! nonzero_impls {
    ($(($int_ctor:ident, $ty:ty))*) => (
        $(
            primitive_static_panicfmt!{
                fn[](&self: $ty, fmtarg) {
                    PanicVal::$int_ctor(self.0.get(), fmtarg)
                }
            }
        )*

        impl_for_option!{
            $((for[], 'static, $ty, $ty))*
        }
    )
}

nonzero_impls! {
    (from_u8, NonZeroU8)
    (from_i8, NonZeroI8)
    (from_u16, NonZeroU16)
    (from_i16, NonZeroI16)
    (from_u32, NonZeroU32)
    (from_i32, NonZeroI32)
    (from_u64, NonZeroU64)
    (from_i64, NonZeroI64)
    (from_u128, NonZeroU128)
    (from_i128, NonZeroI128)
    (from_usize, NonZeroUsize)
    (from_isize, NonZeroIsize)
}
