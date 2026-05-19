#[cfg(feature = "std")]
#[inline]
pub(crate) fn ln(x: f64) -> f64 {
    x.ln()
}

#[cfg(not(feature = "std"))]
#[inline]
pub(crate) fn ln(x: f64) -> f64 {
    libm::log(x)
}

#[cfg(feature = "std")]
#[inline]
pub(crate) fn ceil(x: f64) -> f64 {
    x.ceil()
}

#[cfg(not(feature = "std"))]
#[inline]
pub(crate) fn ceil(x: f64) -> f64 {
    libm::ceil(x)
}
