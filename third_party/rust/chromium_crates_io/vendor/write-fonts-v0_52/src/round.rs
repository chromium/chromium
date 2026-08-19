//! Rounding whose behavior is defined by the
//! [font specification](https://learn.microsoft.com/en-us/typography/opentype/spec/otff).

/// Floating-point rounding per the [OpenType spec][spec].
///
/// <https://github.com/fonttools/fonttools/issues/1248#issuecomment-383198166> captures the rationale
/// for the current implementation.
///
/// Copied from <https://github.com/simoncozens/rust-font-tools/blob/105436d3a617ddbebd25f790b041ff506bd90d44/otmath/src/lib.rs#L17>,
/// which is in turn copied from <https://github.com/fonttools/fonttools/blob/a55a545b12a9735e303568a9d4c7e75fe6dbd2be/Lib/fontTools/misc/roundTools.py#L23>.
///
/// [spec]: https://docs.microsoft.com/en-us/typography/opentype/spec/otvaroverview#coordinate-scales-and-normalization
pub trait OtRound<U, T = Self> {
    fn ot_round(self) -> U;
}

impl OtRound<i16> for f64 {
    #[inline]
    fn ot_round(self) -> i16 {
        (self + 0.5).floor() as i16
    }
}

impl OtRound<i16> for f32 {
    #[inline]
    fn ot_round(self) -> i16 {
        (self + 0.5).floor() as i16
    }
}

impl OtRound<u16> for f64 {
    #[inline]
    fn ot_round(self) -> u16 {
        (self + 0.5).floor() as u16
    }
}

impl OtRound<u16> for f32 {
    #[inline]
    fn ot_round(self) -> u16 {
        (self + 0.5).floor() as u16
    }
}

impl OtRound<f64> for f64 {
    #[inline]
    fn ot_round(self) -> f64 {
        (self + 0.5).floor()
    }
}

impl OtRound<f32> for f32 {
    #[inline]
    fn ot_round(self) -> f32 {
        (self + 0.5).floor()
    }
}

impl OtRound<(i16, i16)> for kurbo::Point {
    #[inline]
    fn ot_round(self) -> (i16, i16) {
        (self.x.ot_round(), self.y.ot_round())
    }
}

impl OtRound<kurbo::Vec2> for kurbo::Vec2 {
    #[inline]
    fn ot_round(self) -> kurbo::Vec2 {
        kurbo::Vec2::new((self.x + 0.5).floor(), (self.y + 0.5).floor())
    }
}
