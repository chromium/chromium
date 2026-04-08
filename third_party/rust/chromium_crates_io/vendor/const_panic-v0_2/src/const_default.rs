/// Default values for const parameters
///
/// This trait is sealed so that it can be potentially replaced with
/// an external `ConstDefault` trait when a potential future feature is enabled.
#[doc(hidden)]
pub trait ConstDefault: sealed::Sealed {
    const DEFAULT: Self;
}

mod sealed {
    pub trait Sealed {}
}

macro_rules! impl_constdefault {
    ($($ty:ty = $val:expr),* $(,)?) => (
        $(
            impl sealed::Sealed for $ty {}

            impl ConstDefault for $ty {
                const DEFAULT: Self = $val;
            }
        )*
    )
}

impl_constdefault! {
    u8 = 0,
    u16 = 0,
    u32 = 0,
    u64 = 0,
    u128 = 0,
    usize = 0,
    i8 = 0,
    i16 = 0,
    i32 = 0,
    i64 = 0,
    i128 = 0,
    isize = 0,
    bool = false,
    char = '\0',
    &str = "",
}
