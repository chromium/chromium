#![cfg(target_feature = "avx512ifma")]

use super::*;

// IFMA is a small feature: it only adds the two `vpmadd52` instructions, each
// in the three register widths. Every lane is a `u64` holding a 52-bit value,
// because the multiplier reuses the hardware of the double-precision float
// unit, whose mantissa is 52 bits wide.
//
// These are `add_mul` rather than `mul_add` because the accumulator is the
// *first* operand (`a + (b * c)`), unlike the floating point
// `fused_mul_add_m128` (`(a * b) + c`). There's no `fused` prefix because
// integer multiply-add is always exact, so there's no rounding behaviour that
// a fused version would distinguish itself by.

//
// ADD MUL LOW
//

/// Lanewise `a + ((b * c) mod 2^52)`, reading only the low 52 bits of each
/// lane of `b` and `c`.
///
/// The product of two 52-bit values is 104 bits, and this keeps the low half
/// of it. Use [`add_mul_high_u52_m128i`] for the other half.
/// ```
/// # use safe_arch::*;
/// let a = m128i::from([1_u64; 2]);
/// let b = m128i::from([2_u64; 2]);
/// let c = m128i::from([3_u64; 2]);
/// let d: [u64; 2] = add_mul_low_u52_m128i(a, b, c).into();
/// assert_eq!(d, [7_u64; 2]);
///
/// // `2^26 * 2^26` is exactly `2^52`, so the low 52 bits of the product are 0.
/// let big = m128i::from([1_u64 << 26; 2]);
/// let e: [u64; 2] = add_mul_low_u52_m128i(a, big, big).into();
/// assert_eq!(e, [1_u64; 2]);
/// ```
/// * **Intrinsic:** [`_mm_madd52lo_epu64`]
/// * **Assembly:** `vpmadd52luq xmm, xmm, xmm`
#[must_use]
#[inline(always)]
#[cfg(all(target_feature = "avx512ifma", target_feature = "avx512vl"))]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512ifma", target_feature = "avx512vl"))))]
pub fn add_mul_low_u52_m128i(a: m128i, b: m128i, c: m128i) -> m128i {
  m128i(unsafe { _mm_madd52lo_epu64(a.0, b.0, c.0) })
}

/// Lanewise `a + ((b * c) mod 2^52)`, reading only the low 52 bits of each
/// lane of `b` and `c`.
///
/// The product of two 52-bit values is 104 bits, and this keeps the low half
/// of it. Use [`add_mul_high_u52_m256i`] for the other half.
/// ```
/// # use safe_arch::*;
/// let a = m256i::from([1_u64; 4]);
/// let b = m256i::from([2_u64; 4]);
/// let c = m256i::from([3_u64; 4]);
/// let d: [u64; 4] = add_mul_low_u52_m256i(a, b, c).into();
/// assert_eq!(d, [7_u64; 4]);
/// ```
/// * **Intrinsic:** [`_mm256_madd52lo_epu64`]
/// * **Assembly:** `vpmadd52luq ymm, ymm, ymm`
#[must_use]
#[inline(always)]
#[cfg(all(target_feature = "avx512ifma", target_feature = "avx512vl"))]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512ifma", target_feature = "avx512vl"))))]
pub fn add_mul_low_u52_m256i(a: m256i, b: m256i, c: m256i) -> m256i {
  m256i(unsafe { _mm256_madd52lo_epu64(a.0, b.0, c.0) })
}

/// Lanewise `a + ((b * c) mod 2^52)`, reading only the low 52 bits of each
/// lane of `b` and `c`.
///
/// The product of two 52-bit values is 104 bits, and this keeps the low half
/// of it. Use [`add_mul_high_u52_m512i`] for the other half.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_u64; 8]);
/// let b = m512i::from([2_u64; 8]);
/// let c = m512i::from([3_u64; 8]);
/// let d: [u64; 8] = add_mul_low_u52_m512i(a, b, c).into();
/// assert_eq!(d, [7_u64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_madd52lo_epu64`]
/// * **Assembly:** `vpmadd52luq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512ifma")))]
pub fn add_mul_low_u52_m512i(a: m512i, b: m512i, c: m512i) -> m512i {
  m512i(unsafe { _mm512_madd52lo_epu64(a.0, b.0, c.0) })
}

//
// ADD MUL HIGH
//

/// Lanewise `a + ((b * c) >> 52)`, reading only the low 52 bits of each lane
/// of `b` and `c`.
///
/// The product of two 52-bit values is 104 bits, and this keeps the high half
/// of it. Use [`add_mul_low_u52_m128i`] for the other half.
/// ```
/// # use safe_arch::*;
/// let a = m128i::from([1_u64; 2]);
/// let b = m128i::from([2_u64; 2]);
/// let c = m128i::from([3_u64; 2]);
/// // `6` fits in 52 bits, so the high half of the product is 0.
/// let d: [u64; 2] = add_mul_high_u52_m128i(a, b, c).into();
/// assert_eq!(d, [1_u64; 2]);
///
/// // `2^26 * 2^26` is exactly `2^52`, so the high 52 bits of the product are 1.
/// let big = m128i::from([1_u64 << 26; 2]);
/// let e: [u64; 2] = add_mul_high_u52_m128i(a, big, big).into();
/// assert_eq!(e, [2_u64; 2]);
/// ```
/// * **Intrinsic:** [`_mm_madd52hi_epu64`]
/// * **Assembly:** `vpmadd52huq xmm, xmm, xmm`
#[must_use]
#[inline(always)]
#[cfg(all(target_feature = "avx512ifma", target_feature = "avx512vl"))]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512ifma", target_feature = "avx512vl"))))]
pub fn add_mul_high_u52_m128i(a: m128i, b: m128i, c: m128i) -> m128i {
  m128i(unsafe { _mm_madd52hi_epu64(a.0, b.0, c.0) })
}

/// Lanewise `a + ((b * c) >> 52)`, reading only the low 52 bits of each lane
/// of `b` and `c`.
///
/// The product of two 52-bit values is 104 bits, and this keeps the high half
/// of it. Use [`add_mul_low_u52_m256i`] for the other half.
/// ```
/// # use safe_arch::*;
/// let a = m256i::from([1_u64; 4]);
/// let big = m256i::from([1_u64 << 26; 4]);
/// let d: [u64; 4] = add_mul_high_u52_m256i(a, big, big).into();
/// assert_eq!(d, [2_u64; 4]);
/// ```
/// * **Intrinsic:** [`_mm256_madd52hi_epu64`]
/// * **Assembly:** `vpmadd52huq ymm, ymm, ymm`
#[must_use]
#[inline(always)]
#[cfg(all(target_feature = "avx512ifma", target_feature = "avx512vl"))]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512ifma", target_feature = "avx512vl"))))]
pub fn add_mul_high_u52_m256i(a: m256i, b: m256i, c: m256i) -> m256i {
  m256i(unsafe { _mm256_madd52hi_epu64(a.0, b.0, c.0) })
}

/// Lanewise `a + ((b * c) >> 52)`, reading only the low 52 bits of each lane
/// of `b` and `c`.
///
/// The product of two 52-bit values is 104 bits, and this keeps the high half
/// of it. Use [`add_mul_low_u52_m512i`] for the other half.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_u64; 8]);
/// let big = m512i::from([1_u64 << 26; 8]);
/// let d: [u64; 8] = add_mul_high_u52_m512i(a, big, big).into();
/// assert_eq!(d, [2_u64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_madd52hi_epu64`]
/// * **Assembly:** `vpmadd52huq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512ifma")))]
pub fn add_mul_high_u52_m512i(a: m512i, b: m512i, c: m512i) -> m512i {
  m512i(unsafe { _mm512_madd52hi_epu64(a.0, b.0, c.0) })
}
