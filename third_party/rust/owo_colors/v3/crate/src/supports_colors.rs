use core::fmt;

#[cfg(feature = "supports-colors")]
/// A display wrapper which applies a transformation based on if the given stream supports
/// colored terminal output
pub struct SupportsColorsDisplay<'a, InVal, Out, ApplyFn>(
    pub(crate) &'a InVal,
    pub(crate) ApplyFn,
    pub(crate) supports_color::Stream,
)
where
    InVal: ?Sized,
    ApplyFn: Fn(&'a InVal) -> Out;

use crate::OVERRIDE;

macro_rules! impl_fmt_for {
    ($($trait:path),* $(,)?) => {
        $(
            impl<'a, In, Out, F> $trait for SupportsColorsDisplay<'a, In, Out, F>
                where In: $trait,
                      Out: $trait,
                      F: Fn(&'a In) -> Out,
            {
                #[inline(always)]
                fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                    let (force_enabled, force_disabled) = OVERRIDE.is_force_enabled_or_disabled();
                    if force_enabled || (
                        supports_color::on_cached(self.2)
                            .map(|level| level.has_basic)
                            .unwrap_or(false)
                        && !force_disabled
                    ) {
                        <Out as $trait>::fmt(&self.1(self.0), f)
                    } else {
                        <In as $trait>::fmt(self.0, f)
                    }
                }
            }
        )*
    };
}

impl_fmt_for! {
    fmt::Display,
    fmt::Debug,
    fmt::UpperHex,
    fmt::LowerHex,
    fmt::Binary,
    fmt::UpperExp,
    fmt::LowerExp,
    fmt::Octal,
    fmt::Pointer,
}
