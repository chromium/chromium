use crate::{
    fmt::{self as cfmt, ComputePvCount, FmtArg, FmtKind, PanicFmt},
    PanicVal, StdWrapper,
};

use core::ffi::{FromBytesUntilNulError, FromBytesWithNulError};

#[cfg_attr(
    feature = "docsrs",
    doc(cfg(all(feature = "non_basic", feature = "rust_1_88")))
)]
impl PanicFmt for FromBytesWithNulError {
    type This = Self;
    type Kind = crate::fmt::IsStdType;
    const PV_COUNT: usize = ComputePvCount {
        field_amount: 1,
        summed_pv_count: <usize>::PV_COUNT,
        delimiter: cfmt::TypeDelim::Braced,
    }
    .call();
}

#[cfg_attr(
    feature = "docsrs",
    doc(cfg(all(feature = "non_basic", feature = "rust_1_88")))
)]
impl StdWrapper<&FromBytesWithNulError> {
    /// Formats a `FromBytesWithNulError`.
    pub const fn to_panicvals(
        self,
        fmtarg: FmtArg,
    ) -> [PanicVal<'static>; FromBytesWithNulError::PV_COUNT] {
        match (fmtarg.fmt_kind, *self.0) {
            (FmtKind::Display, FromBytesWithNulError::InteriorNul { position }) => {
                crate::utils::flatten_panicvals(&[&[
                    PanicVal::write_str("data provided contains an interior nul byte at byte pos "),
                    PanicVal::from_usize(position, fmtarg),
                ]])
            }
            (FmtKind::Display, FromBytesWithNulError::NotNulTerminated) => {
                crate::utils::flatten_panicvals(&[&[PanicVal::write_str(
                    "data provided is not nul terminated",
                )]])
            }
            (FmtKind::Debug, FromBytesWithNulError::InteriorNul { position }) => {
                flatten_panicvals! {fmtarg;
                    "InteriorNul",
                    open: cfmt::OpenBrace,
                        "position: ", usize => position, cfmt::COMMA_TERM,
                    close: cfmt::CloseBrace,
                }
            }
            (FmtKind::Debug, FromBytesWithNulError::NotNulTerminated) => {
                crate::utils::flatten_panicvals(&[&[PanicVal::write_str("NotNulTerminated")]])
            }
        }
    }
}

#[cfg_attr(
    feature = "docsrs",
    doc(cfg(all(feature = "non_basic", feature = "rust_1_88")))
)]
impl PanicFmt for FromBytesUntilNulError {
    type This = Self;
    type Kind = crate::fmt::IsStdType;
    const PV_COUNT: usize = ComputePvCount {
        field_amount: 1,
        summed_pv_count: <()>::PV_COUNT,
        delimiter: cfmt::TypeDelim::Tupled,
    }
    .call();
}

#[cfg_attr(
    feature = "docsrs",
    doc(cfg(all(feature = "non_basic", feature = "rust_1_88")))
)]
impl StdWrapper<&FromBytesUntilNulError> {
    /// Formats a `FromBytesUntilNulError`.
    pub const fn to_panicvals(
        self,
        fmtarg: FmtArg,
    ) -> [PanicVal<'static>; FromBytesUntilNulError::PV_COUNT] {
        match fmtarg.fmt_kind {
            FmtKind::Display => crate::utils::flatten_panicvals(&[&[PanicVal::write_str(
                "data provided does not contain a nul",
            )]]),
            FmtKind::Debug => flatten_panicvals! {fmtarg;
                "FromBytesUntilNulError",
                open: cfmt::OpenParen,
                    () => (), cfmt::COMMA_TERM,
                close: cfmt::CloseParen,
            },
        }
    }
}
