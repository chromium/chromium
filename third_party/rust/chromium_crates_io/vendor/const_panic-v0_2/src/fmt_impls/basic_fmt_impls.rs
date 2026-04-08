use crate::{
    panic_val::{IntVal, PanicVal, PanicVariant, StrFmt},
    utils::Packed,
    FmtArg, PanicFmt, StdWrapper,
};

macro_rules! primitive_static_panicfmt {
    (
        fn[$($impl:tt)*](&$self:ident: $ty:ty, $f:ident) {
            $($content:tt)*
        }
    ) => {
        impl<$($impl)*> crate::PanicFmt for $ty {
            type This = Self;
            type Kind = crate::fmt::IsStdType;
            const PV_COUNT: usize = 1;
        }

        impl<$($impl)*> crate::StdWrapper<&$ty> {
            #[doc = concat!(
                "Converts this `", stringify!($ty), "` to a single-element `PanicVal` array."
            )]
            pub const fn to_panicvals($self, $f: crate::FmtArg) -> [PanicVal<'static>; 1] {
                [{
                    $($content)*
                }]
            }

            #[doc = concat!(
                "Converts this `", stringify!($ty), "` to a `PanicVal`."
            )]
            pub const fn to_panicval($self, $f: crate::FmtArg) -> PanicVal<'static> {
                $($content)*
            }
        }
    }
}
pub(crate) use primitive_static_panicfmt;

macro_rules! impl_panicfmt_panicval_array {
    (
        PV_COUNT = $pv_len:expr;
        fn[$($impl:tt)*](&$self:ident: $ty:ty) -> $ret:ty {
            $($content:tt)*
        }
    ) => (
        impl<$($impl)*> PanicFmt for $ty {
            type This = Self;
            type Kind = crate::fmt::IsStdType;
            const PV_COUNT: usize = $pv_len;
        }

        impl<'s, $($impl)*> StdWrapper<&'s $ty> {
            ///
            pub const fn to_panicvals($self: Self, _: FmtArg) -> $ret {
                $($content)*
            }
        }
    )
}

macro_rules! impl_panicfmt_panicarg {
    (
        fn $panic_arg_ctor:ident[$($impl:tt)*](
            $this:ident:
            $ty:ty,
            $f:ident
        ) -> PanicVal<$pa_lt:lifetime>
        $panic_args:block


    )=>{
        impl PanicVal<'_> {
            #[doc = concat!("Constructs a `PanicVal` from a `", stringify!($ty), "` .")]
            pub const fn $panic_arg_ctor<$($impl)*>($this: $ty, $f: FmtArg) -> PanicVal<$pa_lt>
            $panic_args
        }

        impl<$($impl)*> PanicFmt for $ty {
            type This = Self;
            type Kind = crate::fmt::IsStdType;

            const PV_COUNT: usize = 1;
        }

        impl<'s, $($impl)*> StdWrapper<&'s $ty> {
            #[doc = concat!(
                "Formats this `", stringify!($ty),
                "` into a single-`PanicVal` array",
            )]
            pub const fn to_panicvals(self: Self, f: FmtArg) -> [PanicVal<$pa_lt>;1] {
                [PanicVal::$panic_arg_ctor(*self.0, f)]
            }
            #[doc = concat!("Formats this `", stringify!($ty), "` into a `PanicVal`")]
            pub const fn to_panicval(self: Self, f: FmtArg) -> PanicVal<$pa_lt> {
                PanicVal::$panic_arg_ctor(*self.0, f)
            }
        }
    }
}

macro_rules! impl_panicfmt_int {
    ($panic_arg_ctor:ident, $intarg_contructor:ident, $ty:ty) => {
        impl PanicVal<'_> {
            /// Constructs this `PanicVal` from an integer.
            pub const fn $panic_arg_ctor(this: $ty, f: FmtArg) -> PanicVal<'static> {
                const BITS: u8 = core::mem::size_of::<$ty>() as u8 * 8;
                IntVal::$intarg_contructor(this as _, BITS, f)
            }
        }

        primitive_static_panicfmt! {
            fn[](&self: $ty, f) {
                PanicVal::$panic_arg_ctor(*self.0, f)
            }
        }
    };
}

impl_panicfmt_int! {from_u8, from_u128, u8}
impl_panicfmt_int! {from_u16, from_u128, u16}
impl_panicfmt_int! {from_u32, from_u128, u32}
impl_panicfmt_int! {from_u64, from_u128, u64}
impl_panicfmt_int! {from_u128, from_u128, u128}
impl_panicfmt_int! {from_usize, from_u128, usize}

impl_panicfmt_int! {from_i8, from_i128, i8}
impl_panicfmt_int! {from_i16, from_i128, i16}
impl_panicfmt_int! {from_i32, from_i128, i32}
impl_panicfmt_int! {from_i64, from_i128, i64}
impl_panicfmt_int! {from_i128, from_i128, i128}
impl_panicfmt_int! {from_isize, from_i128, isize}

impl_panicfmt_panicarg! {
    fn from_bool[](this: bool, _f) -> PanicVal<'static> {
        PanicVal::write_str(if this { "true" } else { "false" })
    }
}

impl<'a> PanicVal<'a> {
    /// Constructs a `PanicVal` from a `&str`
    pub const fn from_str(this: &'a str, f: FmtArg) -> PanicVal<'a> {
        PanicVal::__new(PanicVariant::Str(StrFmt::new(f), Packed(this)))
    }
}

impl PanicFmt for str {
    type This = Self;
    type Kind = crate::fmt::IsStdType;
    const PV_COUNT: usize = 1;
}

impl<'a> StdWrapper<&'a str> {
    /// Formats this `&str` into a single-`PanicVal` array
    pub const fn to_panicvals(self: Self, f: FmtArg) -> [PanicVal<'a>; 1] {
        [PanicVal::from_str(self.0, f)]
    }
    /// Formats this `&str` into a `PanicVal`
    pub const fn to_panicval(self: Self, f: FmtArg) -> PanicVal<'a> {
        PanicVal::from_str(self.0, f)
    }
}

impl_panicfmt_panicval_array! {
    PV_COUNT = N;
    fn['a, const N: usize](&self: [PanicVal<'a>; N]) -> &'s [PanicVal<'a>; N] {
        self.0
    }
}

impl_panicfmt_panicval_array! {
    PV_COUNT = usize::MAX;
    fn['a](&self: [PanicVal<'a>]) -> &'s [PanicVal<'a>] {
        self.0
    }
}

impl<'a, 'b> StdWrapper<&'a &'b [PanicVal<'b>]> {
    /// Coerces a `&&[PanicVal<'_>]` into a `&[PanicVal<'_>]`
    pub const fn deref_panic_vals(self) -> &'b [PanicVal<'b>] {
        *self.0
    }
}
impl<'a, 'b, const N: usize> StdWrapper<&'a &'b [PanicVal<'b>; N]> {
    /// Coerces a `&&[PanicVal<'_>; N]` into a `&[PanicVal<'_>]`
    pub const fn deref_panic_vals(self) -> &'b [PanicVal<'b>] {
        *self.0
    }
}
impl<'b, const N: usize> StdWrapper<&'b [PanicVal<'b>; N]> {
    /// Coerces a `&[PanicVal<'_>; N]` into a `&[PanicVal<'_>]`
    pub const fn deref_panic_vals(self) -> &'b [PanicVal<'b>] {
        self.0
    }
}
