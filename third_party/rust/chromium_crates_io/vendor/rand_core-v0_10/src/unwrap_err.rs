use crate::{Infallible, TryCryptoRng, TryRng};

/// Wrapper around [`TryRng`] implementation which implements [`Rng`][crate::Rng]
/// by panicking on potential errors.
///
/// # Examples
///
/// ```rust
/// # use rand_core::{UnwrapErr, TryRng, Rng};
/// fn with_try_rng<R: TryRng>(mut rng: R) {
///     // rng does not impl Rng:
///     let _ = rng.try_next_u32(); // okay
///     // let _ = rng.next_u32(); // error
///
///     // An adapter borrowing rng:
///     let _ = UnwrapErr(&mut rng).next_u32();
///
///     // An adapter moving rng:
///     let mut rng = UnwrapErr(rng);
///     let _ = rng.next_u32();
/// }
///
/// fn call_with_unsized_try_rng<R: TryRng + ?Sized>(rng: &mut R) {
///     // R is unsized, thus we must use &mut R:
///     let mut rng = UnwrapErr(rng);
///     let _ = rng.next_u32();
/// }
/// ```
#[derive(Debug, Default, Clone, Copy, Eq, PartialEq, Hash)]
pub struct UnwrapErr<R: TryRng>(pub R);

impl<R: TryRng> TryRng for UnwrapErr<R> {
    type Error = Infallible;

    #[inline]
    fn try_next_u32(&mut self) -> Result<u32, Self::Error> {
        self.0.try_next_u32().map_err(panic_msg)
    }

    #[inline]
    fn try_next_u64(&mut self) -> Result<u64, Self::Error> {
        self.0.try_next_u64().map_err(panic_msg)
    }

    #[inline]
    fn try_fill_bytes(&mut self, dst: &mut [u8]) -> Result<(), Self::Error> {
        self.0.try_fill_bytes(dst).map_err(panic_msg)
    }
}

fn panic_msg(err: impl core::error::Error) -> Infallible {
    panic!("rand_core::UnwrapErr: failed to unwrap: {err}")
}

impl<R: TryCryptoRng> TryCryptoRng for UnwrapErr<R> {}

impl<'r, R: TryRng + ?Sized> UnwrapErr<&'r mut R> {
    /// Reborrow with a new lifetime
    ///
    /// Rust allows references like `&T` or `&mut T` to be "reborrowed" through
    /// coercion: essentially, the pointer is copied under a new, shorter, lifetime.
    /// Until rfcs#1403 lands, reborrows on user types require a method call.
    #[inline(always)]
    pub fn re<'b>(&'b mut self) -> UnwrapErr<&'b mut R>
    where
        'r: 'b,
    {
        UnwrapErr(self.0)
    }
}
