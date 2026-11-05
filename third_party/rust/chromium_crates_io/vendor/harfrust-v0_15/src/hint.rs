//! Branch hints for lookups that only fail on malformed input.

/// Marks the `None` outcome of a lookup as unlikely.
///
/// Branching on an `Option` otherwise looks to the optimizer like an even
/// split, so a few early returns in a row make the code after them look
/// rarely executed and keep it from being inlined or laid out as hot.
pub(crate) trait Likely {
    fn likely(self) -> Self;
}

impl<T> Likely for Option<T> {
    #[inline(always)]
    fn likely(self) -> Self {
        if self.is_none() {
            cold();
        }
        self
    }
}

/// Converts a `Result` to an `Option`, marking the error as unlikely.
pub(crate) trait LikelyOk<T> {
    fn likely_ok(self) -> Option<T>;
}

impl<T, E> LikelyOk<T> for Result<T, E> {
    #[inline(always)]
    fn likely_ok(self) -> Option<T> {
        if let Ok(value) = self {
            Some(value)
        } else {
            cold();
            None
        }
    }
}

// The body keeps the call from being optimized away along with its hint.
#[cold]
#[inline(never)]
fn cold() {
    core::hint::black_box(());
}
