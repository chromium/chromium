use crate::fmt::{FmtArg, FmtKind, NumberFmt};

/// A version of FmtArg which occupies less space, but needs to be unpacked to be used.
#[derive(Copy, Clone)]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub struct PackedFmtArg {
    indentation: u8,
    bitfields: u8,
}

const FMT_KIND_OFFSET: u8 = 1;
const NUMBER_FMT_OFFSET: u8 = FMT_KIND_OFFSET + FmtKind::BITS;

impl FmtArg {
    /// Converts this `FmtArg` into a `PackedFmtArg`,
    /// which is smaller but can only be converted back into a `FmtArg`.
    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
    pub const fn pack(self) -> PackedFmtArg {
        let Self {
            indentation,
            is_alternate,
            fmt_kind,
            number_fmt,
        } = self;

        PackedFmtArg {
            indentation,
            bitfields: is_alternate as u8
                | ((fmt_kind as u8) << FMT_KIND_OFFSET)
                | ((number_fmt as u8) << NUMBER_FMT_OFFSET),
        }
    }
}

impl PackedFmtArg {
    /// Converts this `PackedFmtArg` back into a `FmtArg`.
    pub const fn unpack(self) -> FmtArg {
        let indentation = self.indentation;
        let is_alternate = (self.bitfields & 1) != 0;
        let fmt_kind = FmtKind::from_prim(self.bitfields >> FMT_KIND_OFFSET);
        let number_fmt = NumberFmt::from_prim(self.bitfields >> NUMBER_FMT_OFFSET);

        FmtArg {
            indentation,
            is_alternate,
            fmt_kind,
            number_fmt,
        }
    }
}

macro_rules! enum_prim {
    (
        $type:ident, $bits:expr;
        default $default:ident,
        $($variant:ident),*
        $(,)?
    ) => (
        #[allow(non_upper_case_globals)]
        const _: () = {
            $(const $variant: u8 = $type::$variant as u8;)*
            const __MASK: u8 = 2u8.pow($bits) - 1;

            match $type::$default {
                $($type::$variant => (),)*
                $type::$default => (),
            }

            impl $type {
                #[allow(dead_code)]
                const BITS: u8 = $bits;

                const fn from_prim(n: u8) -> Self {
                    match n & __MASK {
                        $($variant => Self::$variant,)*
                        _ => Self::$default,
                    }
                }
            }
        };
    )
}
use enum_prim;

enum_prim! {
    FmtKind, 2;
    default Debug,
    Display,
}

enum_prim! {
    NumberFmt, 2;
    default Decimal,
    Binary,
    Hexadecimal,
}
