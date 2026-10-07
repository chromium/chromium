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

#[cfg(feature = "std")]
#[inline]
pub(crate) fn pow(b: f64, p: f64) -> f64 {
    b.powf(p)
}

#[cfg(not(feature = "std"))]
#[inline]
pub(crate) fn pow(b: f64, p: f64) -> f64 {
    libm::pow(b, p)
}

#[cfg(feature = "std")]
#[inline]
pub(crate) fn round(x: f64) -> f64 {
    x.round()
}

#[cfg(not(feature = "std"))]
#[inline]
pub(crate) fn round(x: f64) -> f64 {
    libm::round(x)
}
