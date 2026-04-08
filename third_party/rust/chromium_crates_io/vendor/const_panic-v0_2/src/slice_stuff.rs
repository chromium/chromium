use crate::{
    fmt::{FmtArg, PackedFmtArg, PanicFmt},
    panic_val::{PanicVal, PanicVariant},
    utils::Packed,
    StdWrapper,
};

macro_rules! impl_panicfmt_array {
    ($(($variant:ident, $panicval_ctor:ident, $ty:ty)),* $(,)*) => {

        #[derive(Copy, Clone)]
        #[repr(packed)]
        pub(crate) struct Slice<'s> {
            pub(crate) fmtarg: PackedFmtArg,
            pub(crate) vari: SliceV<'s>,
        }

        #[repr(u8)]
        #[derive(Copy, Clone)]
        pub(crate) enum SliceV<'s> {
            $(
                $variant(Packed<&'s [$ty]>),
            )*
        }


        impl<'s> Slice<'s> {
            // length in elements
            pub(crate) const fn arr_len(self) -> usize {
                match self.vari {
                    $(
                        SliceV::$variant(Packed(arr)) => arr.len(),
                    )*
                }
            }
        }

        impl<'s> SliceV<'s> {
            const fn get(self, index: usize, fmtarg: FmtArg) -> PanicVal<'s> {
                match self {
                    $(
                        SliceV::$variant(Packed(arr)) => {
                            let elem: &'s <$ty as PanicFmt>::This = &arr[index];
                            StdWrapper(elem).to_panicval(fmtarg)
                        },
                    )*
                }
            }
        }

        #[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
        impl<'s> PanicVal<'s> {
            $(
                /// Constructs a `PanicVal` from a slice.
                pub const fn $panicval_ctor(this: &'s [$ty], mut fmtarg: FmtArg) -> PanicVal<'s> {
                    fmtarg = fmtarg.indent();
                    if this.is_empty() {
                        fmtarg = fmtarg.set_alternate(false);
                    }
                    PanicVal::__new(
                        PanicVariant::Slice(Slice{
                            fmtarg: fmtarg.pack(),
                            vari: SliceV::$variant(Packed(this)),
                        })
                    )
                }
            )*
        }

        $(
            impl<'s> PanicFmt for [$ty] {
                type This = Self;
                type Kind = crate::fmt::IsStdType;
                const PV_COUNT: usize = 1;
            }
            impl<'s, const LEN: usize> PanicFmt for [$ty; LEN] {
                type This = Self;
                type Kind = crate::fmt::IsStdType;
                const PV_COUNT: usize = 1;
            }

            #[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
            impl<'s> StdWrapper<&'s [$ty]> {
                /// Converts the slice to a single-element `PanicVal` array.
                pub const fn to_panicvals(self: Self, f:FmtArg) -> [PanicVal<'s>;1] {
                    [PanicVal::$panicval_ctor(self.0, f)]
                }
                /// Converts the slice to a `PanicVal`.
                pub const fn to_panicval(self: Self, f:FmtArg) -> PanicVal<'s> {
                    PanicVal::$panicval_ctor(self.0, f)
                }
            }

            #[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
            impl<'s, const LEN: usize> StdWrapper<&'s [$ty; LEN]> {
                /// Converts the array to a single-element `PanicVal` array.
                pub const fn to_panicvals(self: Self, f:FmtArg) -> [PanicVal<'s>;1] {
                    [PanicVal::$panicval_ctor(self.0, f)]
                }
                /// Converts the array to a `PanicVal`.
                pub const fn to_panicval(self: Self, f:FmtArg) -> PanicVal<'s> {
                    PanicVal::$panicval_ctor(self.0, f)
                }
            }
        )*

    };
}

impl_panicfmt_array! {
    (U8, from_slice_u8, u8),
    (U16, from_slice_u16, u16),
    (U32, from_slice_u32, u32),
    (U64, from_slice_u64, u64),
    (U128, from_slice_u128, u128),
    (Usize, from_slice_usize, usize),
    (I8, from_slice_i8, i8),
    (I16, from_slice_i16, i16),
    (I32, from_slice_i32, i32),
    (I64, from_slice_i64, i64),
    (I128, from_slice_i128, i128),
    (Isize, from_slice_isize, isize),
    (Bool, from_slice_bool, bool),
    (Char, from_slice_char, char),
    (Str, from_slice_str, &'s str),
}

#[derive(Copy, Clone)]
pub(crate) struct SliceIter<'s> {
    slice: SliceV<'s>,
    fmtarg: FmtArg,
    state: IterState,
    arr_len: u32,
}

#[derive(Copy, Clone, PartialEq, Eq)]
struct IterState(u32);

#[allow(non_upper_case_globals)]
impl IterState {
    const Start: Self = Self(u32::MAX - 1);
    const End: Self = Self(u32::MAX);
}

impl<'s> Slice<'s> {
    pub(crate) const fn iter<'b>(&'b self) -> SliceIter<'s> {
        SliceIter {
            slice: self.vari,
            fmtarg: self.fmtarg.unpack(),
            state: IterState::Start,
            arr_len: self.arr_len() as u32,
        }
    }
}

impl<'s> SliceIter<'s> {
    pub(crate) const fn next(mut self) -> ([PanicVal<'s>; 2], Option<Self>) {
        let fmtarg = self.fmtarg;

        let ret = match self.state {
            IterState::Start => {
                self.state = if self.arr_len == 0 {
                    IterState::End
                } else {
                    IterState(0)
                };

                [crate::fmt::OpenBracket.to_panicval(fmtarg), PanicVal::EMPTY]
            }
            IterState::End => {
                let close_brace = crate::fmt::CloseBracket.to_panicval(fmtarg.unindent());
                return ([close_brace, PanicVal::EMPTY], None);
            }
            IterState(x) => {
                let comma = if x + 1 == self.arr_len {
                    self.state = IterState::End;
                    crate::fmt::COMMA_TERM
                } else {
                    self.state = IterState(x + 1);
                    crate::fmt::COMMA_SEP
                }
                .to_panicval(fmtarg);

                [self.slice.get(x as usize, fmtarg), comma]
            }
        };

        (ret, Some(self))
    }
}
