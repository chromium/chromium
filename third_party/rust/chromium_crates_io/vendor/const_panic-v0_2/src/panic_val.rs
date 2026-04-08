use crate::{
    fmt::{FmtArg, FmtKind, NumberFmt},
    utils::{string_cap, Packed, PreFmtString, RangedBytes, Sign, TailShortString, WasTruncated},
};

#[cfg(feature = "non_basic")]
use crate::{
    array_string::TinyString,
    fmt::{IsLast, ShortString},
};

/// An opaque enum of the values that this crate knows how to format,
/// along with some formatting metadata.
///
/// This has constructor functions to make a `PanicVal` from:
/// - `bool`
/// - Integers
/// - `&str`
/// - Arrays/Slices of primitives (with the "non_basic" feature, enabled by default)
/// - [`ShortString`](crate::fmt::ShortString)
/// (with the "non_basic" feature, enabled by default)
///
#[derive(Copy, Clone)]
pub struct PanicVal<'a> {
    pub(crate) var: PanicVariant<'a>,
}

#[derive(Copy, Clone)]
pub(crate) enum PanicVariant<'a> {
    Str(StrFmt, Packed<&'a str>),
    #[cfg(feature = "non_basic")]
    ShortString(StrFmt, TinyString<{ string_cap::TINY }>),
    PreFmt(PreFmtString),
    Int(IntVal),
    #[cfg(feature = "non_basic")]
    Slice(crate::slice_stuff::Slice<'a>),
}

pub(crate) enum PanicClass<'a> {
    PreFmt(RangedBytes<&'a [u8]>),
    Int(IntVal),
    #[cfg(feature = "non_basic")]
    Slice(crate::slice_stuff::Slice<'a>),
}

#[derive(Copy, Clone)]
pub(crate) struct StrFmt {
    pub(crate) leftpad: u8,
    pub(crate) rightpad: u8,
    pub(crate) fmt_kind: FmtKind,
}

impl StrFmt {
    const DISPLAY: Self = Self {
        leftpad: 0,
        rightpad: 0,
        fmt_kind: FmtKind::Display,
    };

    pub const fn new(fmtarg: FmtArg) -> Self {
        Self {
            leftpad: 0,
            rightpad: 0,
            fmt_kind: fmtarg.fmt_kind,
        }
    }
}

impl<'a> PanicVal<'a> {
    /// A `PanicVal` that formats to nothing.
    pub const EMPTY: Self = PanicVal::write_str("");

    /// How many spaces are printed before this
    pub const fn leftpad(&self) -> u8 {
        use self::PanicVariant as PV;

        match self.var {
            PV::Str(strfmt, ..) => strfmt.leftpad,
            #[cfg(feature = "non_basic")]
            PV::ShortString(strfmt, ..) => strfmt.leftpad,
            _ => 0,
        }
    }
    /// How many spaces are printed after this
    pub const fn rightpad(&self) -> u8 {
        use self::PanicVariant as PV;

        match self.var {
            PV::Str(strfmt, ..) => strfmt.rightpad,
            #[cfg(feature = "non_basic")]
            PV::ShortString(strfmt, ..) => strfmt.rightpad,
            _ => 0,
        }
    }
}

macro_rules! mutate_strfmt {
    ($self:ident, |$strfmt:ident| $mutator:expr) => {
        match $self.var {
            PanicVariant::Str(mut $strfmt, str) => {
                $mutator;
                PanicVal {
                    var: PanicVariant::Str($strfmt, str),
                }
            }
            #[cfg(feature = "non_basic")]
            PanicVariant::ShortString(mut $strfmt, str) => {
                $mutator;
                PanicVal {
                    var: PanicVariant::ShortString($strfmt, str),
                }
            }
            var => PanicVal { var },
        }
    };
}

impl<'a> PanicVal<'a> {
    /// Sets the amount of spaces printed before this to `fmtarg.indentation`.
    ///
    /// Note that only strings can be padded.
    pub const fn with_leftpad(self, fmtarg: FmtArg) -> Self {
        mutate_strfmt! {self, |strfmt| strfmt.leftpad = fmtarg.indentation}
    }

    /// Sets the amount of spaces printed after this to `fmtarg.indentation`.
    ///
    /// Note that only strings can be padded.
    pub const fn with_rightpad(self, fmtarg: FmtArg) -> Self {
        mutate_strfmt! {self, |strfmt| strfmt.rightpad = fmtarg.indentation}
    }

    /// Constructs a PanicVal which outputs the contents of `string` verbatim.
    ///
    /// Equivalent to `PanicVal::from_str(string, FmtArg::DISPLAY)`
    pub const fn write_str(string: &'a str) -> Self {
        PanicVal {
            var: PanicVariant::Str(StrFmt::DISPLAY, Packed(string)),
        }
    }

    /// Constructs a PanicVal from a [`ShortString`], which outputs the string verbatim.
    #[cfg(feature = "non_basic")]
    pub const fn write_short_str(string: ShortString) -> Self {
        Self {
            var: PanicVariant::ShortString(StrFmt::DISPLAY, string.to_compact()),
        }
    }

    /// Constructs a `PanicVal` usable as a separator between fields or elements.
    ///
    /// This is sensitive to the [`fmtarg.is_alternate`] flag,
    /// for more details on that you can look at the docs for
    /// [`Separator::to_panicval`](crate::fmt::Separator#method.to_panicval)
    ///
    /// # Panics
    ///
    /// This panics if `string.len()` is greater than 12.
    ///
    /// [`fmtarg.is_alternate`]: crate::FmtArg#structfield.is_alternate
    #[cfg(feature = "non_basic")]
    pub const fn from_element_separator(
        separator: &str,
        is_last_field: IsLast,
        fmtarg: FmtArg,
    ) -> Self {
        let (concat, rightpad) = match (is_last_field, fmtarg.is_alternate) {
            (IsLast::No, false) => (ShortString::concat(&[separator, " "]), 0),
            (IsLast::Yes, false) => (ShortString::new(""), 0),
            (IsLast::No, true) => (ShortString::concat(&[separator, "\n"]), fmtarg.indentation),
            (IsLast::Yes, true) => (ShortString::concat(&[separator, "\n"]), 0),
        };

        let strfmt = StrFmt {
            leftpad: 0,
            rightpad,
            fmt_kind: FmtKind::Display,
        };
        Self {
            var: PanicVariant::ShortString(strfmt, concat.to_compact()),
        }
    }

    #[inline(always)]
    pub(crate) const fn __new(var: PanicVariant<'a>) -> Self {
        Self { var }
    }

    pub(crate) const fn to_class(&self) -> (StrFmt, PanicClass<'_>) {
        match &self.var {
            &PanicVariant::Str(strfmt, Packed(str)) => {
                let ranged = RangedBytes {
                    start: 0,
                    end: str.len(),
                    bytes: str.as_bytes(),
                };

                (strfmt, PanicClass::PreFmt(ranged))
            }
            #[cfg(feature = "non_basic")]
            PanicVariant::ShortString(strfmt, str) => (*strfmt, PanicClass::PreFmt(str.ranged())),
            PanicVariant::PreFmt(str) => (StrFmt::DISPLAY, PanicClass::PreFmt(str.ranged())),
            PanicVariant::Int(int) => (StrFmt::DISPLAY, PanicClass::Int(*int)),
            #[cfg(feature = "non_basic")]
            PanicVariant::Slice(slice) => (
                StrFmt::new(slice.fmtarg.unpack()),
                PanicClass::Slice(*slice),
            ),
        }
    }

    pub(crate) const fn to_class_truncated(
        &self,
        mut truncate_to: usize,
    ) -> (StrFmt, PanicClass<'_>, WasTruncated) {
        let (mut strfmt, class) = self.to_class();

        if strfmt.leftpad as usize > truncate_to {
            return (
                StrFmt {
                    leftpad: strfmt.leftpad - truncate_to as u8,
                    rightpad: 0,
                    fmt_kind: FmtKind::Display,
                },
                PanicClass::PreFmt(RangedBytes::EMPTY),
                WasTruncated::Yes(0),
            );
        } else {
            truncate_to -= strfmt.leftpad as usize;
        };

        let was_trunc: WasTruncated;
        let orig_len: usize;

        match class {
            PanicClass::PreFmt(str) => {
                was_trunc = if let PanicVariant::PreFmt(pfmt) = self.var {
                    if pfmt.len() <= truncate_to {
                        WasTruncated::No
                    } else {
                        WasTruncated::Yes(0)
                    }
                } else {
                    if let FmtKind::Display = strfmt.fmt_kind {
                        crate::utils::truncated_str_len(str, truncate_to)
                    } else {
                        crate::utils::truncated_debug_str_len(str, truncate_to)
                    }
                };
                orig_len = str.len();
            }
            PanicClass::Int(int) => {
                strfmt.fmt_kind = FmtKind::Display;
                was_trunc = if int.len() <= truncate_to {
                    WasTruncated::No
                } else {
                    WasTruncated::Yes(0)
                };
                orig_len = int.len();
            }
            #[cfg(feature = "non_basic")]
            PanicClass::Slice(_) => {
                was_trunc = WasTruncated::No;
                orig_len = 0;
            }
        }
        truncate_to -= was_trunc.get_length(orig_len);

        strfmt.rightpad = crate::utils::min_usize(strfmt.rightpad as usize, truncate_to) as u8;

        (strfmt, class, was_trunc)
    }
}

#[derive(Copy, Clone)]
pub(crate) struct IntVal {
    sign: Sign,
    number_fmt: NumberFmt,
    is_alternate: bool,
    // the size of the integer in bits
    bits: u8,
    // the length of the integer in bytes, once written.
    len: u8,

    value: Packed<u128>,
}

impl IntVal {
    pub(crate) const fn from_u128(n: u128, bits: u8, f: FmtArg) -> PanicVal<'static> {
        Self::new(Sign::Positive, n, bits, f)
    }
    pub(crate) const fn from_i128(n: i128, bits: u8, f: FmtArg) -> PanicVal<'static> {
        let is_neg = if n < 0 {
            Sign::Negative
        } else {
            Sign::Positive
        };
        Self::new(is_neg, n.unsigned_abs(), bits, f)
    }

    const fn new(sign: Sign, n: u128, bits: u8, fmtarg: FmtArg) -> PanicVal<'static> {
        use crate::int_formatting::compute_len;

        let len = compute_len(sign, n, bits, fmtarg);

        let this = IntVal {
            sign,
            number_fmt: fmtarg.number_fmt,
            is_alternate: fmtarg.is_alternate,
            bits,
            len,
            value: Packed(n),
        };

        let var = if len as usize <= string_cap::PREFMT {
            PanicVariant::PreFmt(this.fmt::<{ string_cap::PREFMT }>())
        } else {
            PanicVariant::Int(this)
        };
        PanicVal { var }
    }

    pub(crate) const fn fmt<const N: usize>(self) -> TailShortString<N> {
        use crate::int_formatting::{fmt_binary, fmt_decimal, fmt_hexadecimal};

        let IntVal {
            sign,
            number_fmt,
            is_alternate,
            len: _,
            bits,
            value: Packed(n),
        } = self;

        match number_fmt {
            NumberFmt::Decimal => fmt_decimal::<N>(sign, n),
            NumberFmt::Binary => {
                let masked = apply_mask(sign, n, bits);
                fmt_binary::<N>(masked, is_alternate)
            }
            NumberFmt::Hexadecimal => {
                let masked = apply_mask(sign, n, bits);
                fmt_hexadecimal::<N>(masked, is_alternate)
            }
        }
    }

    pub(crate) const fn len(&self) -> usize {
        self.len as usize
    }
}

const fn apply_mask(sign: Sign, n: u128, bits: u8) -> u128 {
    if let Sign::Negative = sign {
        let mask: u128 = if bits == 128 { !0 } else { (1 << bits) - 1 };

        (n as i128).wrapping_neg() as u128 & mask
    } else {
        n
    }
}

impl crate::PanicFmt for PanicVal<'_> {
    type This = Self;
    type Kind = crate::fmt::IsCustomType;

    const PV_COUNT: usize = 1;
}

impl<'a> PanicVal<'a> {
    /// Wraps this `PanicVal` in a single-element array.
    pub const fn to_panicvals(&self, _: FmtArg) -> [PanicVal<'a>; 1] {
        [*self]
    }
    /// Returns a copy of this `PanicVal`.
    pub const fn to_panicval(&self, _: FmtArg) -> PanicVal<'a> {
        *self
    }
}
