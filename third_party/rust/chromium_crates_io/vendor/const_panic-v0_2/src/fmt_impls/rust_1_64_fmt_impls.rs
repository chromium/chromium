use crate::{
    fmt::{FmtArg, FmtKind, PanicFmt},
    PanicVal, StdWrapper,
};

use core::str::Utf8Error;

#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
impl PanicFmt for Utf8Error {
    type This = Self;
    type Kind = crate::fmt::IsStdType;
    const PV_COUNT: usize = 5;
}

#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
impl StdWrapper<&Utf8Error> {
    /// Formats a `Utf8Error` (supports both Debug and Display formatting).
    pub const fn to_panicvals(self, fmtarg: FmtArg) -> [PanicVal<'static>; Utf8Error::PV_COUNT] {
        let this = *self.0;
        match fmtarg.fmt_kind {
            FmtKind::Display => {
                let [pv0, pv1, pv2] = match this.error_len() {
                    Some(x) => [
                        PanicVal::write_str("invalid utf-8 sequence of "),
                        PanicVal::from_usize(x, fmtarg),
                        PanicVal::write_str(" bytes "),
                    ],
                    None => [
                        PanicVal::write_str("incomplete utf-8 byte sequence "),
                        PanicVal::EMPTY,
                        PanicVal::EMPTY,
                    ],
                };

                [
                    pv0,
                    pv1,
                    pv2,
                    PanicVal::write_str("from index "),
                    PanicVal::from_usize(this.valid_up_to(), fmtarg),
                ]
            }
            FmtKind::Debug => {
                let [pv0, pv1, pv2] = match this.error_len() {
                    Some(x) => [
                        PanicVal::write_str(", error_len: Some("),
                        PanicVal::from_usize(x, fmtarg),
                        PanicVal::write_str(") }"),
                    ],
                    None => [
                        PanicVal::write_str(", error_len: None }"),
                        PanicVal::EMPTY,
                        PanicVal::EMPTY,
                    ],
                };

                [
                    PanicVal::write_str("Utf8Error { valid_up_to: "),
                    PanicVal::from_usize(this.valid_up_to(), fmtarg),
                    pv0,
                    pv1,
                    pv2,
                ]
            }
        }
    }
}
