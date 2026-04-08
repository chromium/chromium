use crate::{FmtArg, PanicFmt, PanicVal};

/// Note: there is only `to_panicvals` methods for `Option`s of standard library types
/// for now.
///
impl<T> PanicFmt for Option<T>
where
    T: PanicFmt,
{
    type This = Self;
    type Kind = crate::fmt::IsStdType;
    const PV_COUNT: usize = 4 + T::PV_COUNT;
}

macro_rules! impl_for_option {
    (
        $((for[$($generics:tt)*],$lt:lifetime, $ty:ty, $unref:ty))*
    ) => (
        $(
            impl<'s, $($generics)*> crate::StdWrapper<&'s Option<$ty>> {
                #[doc = concat!(
                    "Converts this `Option<",
                    stringify!($ty),
                    ">` to a `PanicVal` array."
                )]
                pub const fn to_panicvals(
                    self: Self,
                    mut fmtarg: FmtArg,
                ) -> [PanicVal<$lt>; 5] {
                    use crate::{PanicVal, StdWrapper, __::EPV, fmt};

                    match self.0 {
                        Some(x) => [
                            PanicVal::write_str("Some"),
                            {fmtarg = fmtarg.indent(); fmt::OpenParen.to_panicval(fmtarg)},
                            StdWrapper::<&$unref>(x).to_panicval(fmtarg),
                            fmt::COMMA_TERM.to_panicval(fmtarg),
                            {fmtarg = fmtarg.unindent(); fmt::CloseParen.to_panicval(fmtarg)},
                        ],
                        None => [PanicVal::write_str("None"), EPV, EPV, EPV, EPV],
                    }
                }
            }
        )*
    )
}

macro_rules! impl_for_option_outer {
    (
        $(($lt:lifetime, $ty:ty, $unref:ty))*
    ) => (
        impl_for_option!{
            $(
                (for[], $lt, $ty, $unref)
                (for[const N: usize], 's, [$ty; N], [$ty; N])
                (for[const N: usize], 's, &'s [$ty; N], [$ty; N])
                (for[], 's, &'s [$ty], [$ty])
            )*
        }
    )
}

impl_for_option_outer! {
    ('static, bool, bool)
    ('static, u8, u8)
    ('static, u16, u16)
    ('static, u32, u32)
    ('static, u64, u64)
    ('static, u128, u128)
    ('static, i8, i8)
    ('static, i16, i16)
    ('static, i32, i32)
    ('static, i64, i64)
    ('static, i128, i128)
    ('static, isize, isize)
    ('static, usize, usize)
    ('s, &'s str, str)
}
