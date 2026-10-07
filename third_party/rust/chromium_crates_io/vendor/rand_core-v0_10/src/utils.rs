//! Utilties to aid trait implementations
//!
//! ## Portability
//!
//! For cross-platform reproducibility, Little-Endian order (least-significant
//! part first) has been chosen as the standard for inter-type conversion.
//! For example, [`next_u64_via_u32`] generates two `u32` values `x, y`,
//! then outputs `(y << 32) | x`.
//!
//! Byte-swapping (like the std `to_le` functions) is only needed to convert
//! to/from byte sequences, and since its purpose is reproducibility,
//! non-reproducible sources (e.g. `OsRng`) need not bother with it.
//!
//! ## Implementing [`TryRng`]
//!
//! Usually an implementation of [`TryRng`] will implement one of the three
//! methods over its internal source. The following helpers are provided for
//! the remaining implementations.
//!
//! **`fn try_next_u32`:**
//! -   `self.next_u64() as u32`
//! -   `(self.next_u64() >> 32) as u32`
//! -   <code>[next_word_via_fill][](self)</code>
//!
//! **`fn try_next_u64`:**
//! -   <code>[next_u64_via_u32][](self)</code>
//! -   <code>[next_word_via_fill][](self)</code>
//!
//! **`fn try_fill_bytes`:**
//! -   <code>[fill_bytes_via_next_word][](self, dest)</code>
//!
//! ## Implementing [`SeedableRng`]
//!
//! In many cases, [`SeedableRng::Seed`] must be converted to `[u32; _]` or
//! `[u64; _]`. [`read_words`] may be used for this.
//!
//! [`SeedableRng`]: crate::SeedableRng
//! [`SeedableRng::Seed`]: crate::SeedableRng::Seed
//!
//! ## Example
//!
//! We demonstrate a simple multiplicative congruential generator (MCG), taken
//! from M.E. O'Neill's blog post [Does It Beat the Minimal Standard?][0].
//!
//! [0]: https://www.pcg-random.org/posts/does-it-beat-the-minimal-standard.html
//!
//! ```
//! use core::convert::Infallible;
//! use rand_core::{Rng, SeedableRng, TryRng, utils};
//!
//! pub struct Mcg128(u128);
//!
//! impl SeedableRng for Mcg128 {
//!     type Seed = [u8; 16];
//!
//!     #[inline]
//!     fn from_seed(seed: Self::Seed) -> Self {
//!         // Always use little-endian byte order to ensure portable results
//!         Self(u128::from_le_bytes(seed))
//!     }
//! }
//!
//! impl TryRng for Mcg128 {
//!     type Error = Infallible;
//!
//!     #[inline]
//!     fn try_next_u32(&mut self) -> Result<u32, Infallible> {
//!         Ok((self.next_u64() >> 32) as u32)
//!     }
//!
//!     #[inline]
//!     fn try_next_u64(&mut self) -> Result<u64, Infallible> {
//!         self.0 = self.0.wrapping_mul(0x0fc94e3bf4e9ab32866458cd56f5e605);
//!         Ok((self.0 >> 64) as u64)
//!     }
//!
//!     #[inline]
//!     fn try_fill_bytes(&mut self, dst: &mut [u8]) -> Result<(), Infallible> {
//!         utils::fill_bytes_via_next_word(dst, || self.try_next_u64())
//!     }
//! }
//! #
//! # let mut rng = Mcg128::seed_from_u64(42);
//! # assert_eq!(rng.next_u32(), 3443086493);
//! # assert_eq!(rng.next_u64(), 3462997187007721903);
//! # let mut buf = [0u8; 5];
//! # rng.fill_bytes(&mut buf);
//! # assert_eq!(buf, [154, 23, 43, 68, 75]);
//! ```

use crate::TryRng;
pub use crate::word::Word;

/// Generate a `u64` using `next_u32`, little-endian order.
#[inline]
pub fn next_u64_via_u32<R: TryRng + ?Sized>(rng: &mut R) -> Result<u64, R::Error> {
    // Use LE; we explicitly generate one value before the next.
    let x = u64::from(rng.try_next_u32()?);
    let y = u64::from(rng.try_next_u32()?);
    Ok((y << 32) | x)
}

/// Fill `dst` with bytes using `next_word`
///
/// This may be used to implement `fill_bytes` over `next_u32` or
/// `next_u64`. Words are used in order of generation. The last word may be
/// partially discarded.
#[inline]
pub fn fill_bytes_via_next_word<E, W: Word>(
    dst: &mut [u8],
    mut next_word: impl FnMut() -> Result<W, E>,
) -> Result<(), E> {
    let mut chunks = dst.chunks_exact_mut(size_of::<W>());
    for chunk in &mut chunks {
        let val = next_word()?;
        chunk.copy_from_slice(val.to_le_bytes().as_ref());
    }
    let rem = chunks.into_remainder();
    if !rem.is_empty() {
        let val = next_word()?.to_le_bytes();
        rem.copy_from_slice(&val.as_ref()[..rem.len()]);
    }
    Ok(())
}

/// Generate a `u32` or `u64` word using `fill_bytes`
pub fn next_word_via_fill<W: Word, R: TryRng>(rng: &mut R) -> Result<W, R::Error> {
    let mut buf: W::Bytes = Default::default();
    rng.try_fill_bytes(buf.as_mut())?;
    Ok(W::from_le_bytes(buf))
}

/// Reads an array of words from a byte slice
///
/// Words are read from `src` in order, using LE conversion from bytes.
///
/// # Panics
///
/// Panics if `size_of_val(src) != size_of::<[W; N]>()`.
#[inline(always)]
pub fn read_words<W: Word, const N: usize>(src: &[u8]) -> [W; N] {
    assert_eq!(size_of_val(src), size_of::<[W; N]>());
    let mut dst = [W::from_usize(0); N];
    let chunks = src.chunks_exact(size_of::<W>());
    for (out, chunk) in dst.iter_mut().zip(chunks) {
        let mut buf: W::Bytes = Default::default();
        buf.as_mut().copy_from_slice(chunk);
        *out = W::from_le_bytes(buf);
    }
    dst
}
