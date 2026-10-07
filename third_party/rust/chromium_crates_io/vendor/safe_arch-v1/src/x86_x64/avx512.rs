#![cfg(target_feature = "avx512f")]
#![allow(non_camel_case_types)]
use super::*;

#[cfg(target_arch = "x86")]
use ::core::arch::x86::*;
#[cfg(target_arch = "x86_64")]
use ::core::arch::x86_64::*;

/// Mask type for 8-element operations
pub type mmask8 = u8;
/// Mask type for 16-element operations
pub type mmask16 = __mmask16;
/// Mask type for 32-element operations
pub type mmask32 = __mmask32;
/// Mask type for 64-element operations
pub type mmask64 = __mmask64;

/// Turns an integer comparison operator token into the appropriate
#[macro_export]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
macro_rules! cmp_int_op {
    (Eq) => {{
        #[cfg(target_arch = "x86")]
        use ::core::arch::x86::_MM_CMPINT_EQ;
        #[cfg(target_arch = "x86_64")]
        use ::core::arch::x86_64::_MM_CMPINT_EQ;
        _MM_CMPINT_EQ
    }};
    (Lt) => {{
        #[cfg(target_arch = "x86")]
        use ::core::arch::x86::_MM_CMPINT_LT;
        #[cfg(target_arch = "x86_64")]
        use ::core::arch::x86_64::_MM_CMPINT_LT;
        _MM_CMPINT_LT
    }};
    (Le) => {{
        #[cfg(target_arch = "x86")]
        use ::core::arch::x86::_MM_CMPINT_LE;
        #[cfg(target_arch = "x86_64")]
        use ::core::arch::x86_64::_MM_CMPINT_LE;
        _MM_CMPINT_LE
    }};
    (Ne) => {{
        #[cfg(target_arch = "x86")]
        use ::core::arch::x86::_MM_CMPINT_NE;
        #[cfg(target_arch = "x86_64")]
        use ::core::arch::x86_64::_MM_CMPINT_NE;
        _MM_CMPINT_NE
    }};
    (Nlt) => {{
        #[cfg(target_arch = "x86")]
        use ::core::arch::x86::_MM_CMPINT_NLT;
        #[cfg(target_arch = "x86_64")]
        use ::core::arch::x86_64::_MM_CMPINT_NLT;
        _MM_CMPINT_NLT
    }};
    (Nle) => {{
        #[cfg(target_arch = "x86")]
        use ::core::arch::x86::_MM_CMPINT_NLE;
        #[cfg(target_arch = "x86_64")]
        use ::core::arch::x86_64::_MM_CMPINT_NLE;
        _MM_CMPINT_NLE
    }};
    (True) => {{
        #[cfg(target_arch = "x86")]
        use ::core::arch::x86::_MM_CMPINT_TRUE;
        #[cfg(target_arch = "x86_64")]
        use ::core::arch::x86_64::_MM_CMPINT_TRUE;
        _MM_CMPINT_TRUE
    }};
    ($unknown:tt) => {
        compile_error!("`cmp_int_op!` got an unknown integer-compare token");
    };
}

// Constructors and basic operations

/// Zeroed `m512i`
/// ```
/// # use safe_arch::*;
/// let a = zeroed_m512i();
/// let b: [i32; 16] = a.into();
/// assert_eq!(b, [0; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_setzero_si512`]
/// * **Assembly:** `vpxorq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn zeroed_m512i() -> m512i {
  m512i(unsafe { _mm512_setzero_si512() })
}

/// Zeroed `m512d`
/// ```
/// # use safe_arch::*;
/// let a = zeroed_m512d();
/// let b: [f64; 8] = a.into();
/// assert_eq!(b, [0.0; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_setzero_pd`]
/// * **Assembly:** `vxorpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn zeroed_m512d() -> m512d {
    m512d(unsafe { _mm512_setzero_pd() })
}

/// Zeroed `m512`
/// ```
/// # use safe_arch::*;
/// let a = zeroed_m512();
/// let b: [f32; 16] = a.into();
/// assert_eq!(b, [0.0; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_setzero_ps`]
/// * **Assembly:** `vxorps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn zeroed_m512() -> m512 {
    m512(unsafe { _mm512_setzero_ps() })
}

/// Shuffle the `f64` lanes from `a` and `b` together using an immediate control
/// value, across all eight double-precision lanes in the ZMM register.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = m512d::from([1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]);
/// let b = m512d::from([10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0]);
/// // IMM = 0 selects A0,B0, A2,B2, A4,B4, A6,B6
/// let c: [f64; 8] = shuffle_m512d::<0>(a, b).into();
/// assert_eq!(c, [1.0, 10.0, 3.0, 12.0, 5.0, 14.0, 7.0, 16.0]);
/// ```
/// * **Intrinsic:** [`_mm512_shuffle_pd`]
/// * **Assembly:** `vshufpd zmm, zmm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shuffle_m512d<const IMM: i32>(a: m512d, b: m512d) -> m512d {
    m512d(unsafe { _mm512_shuffle_pd(a.0, b.0, IMM) })
}

/// Shuffle the `f32` lanes from `a` and `b` together using an immediate control
/// value, across all sixteen single-precision lanes in the ZMM register.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = m512::from([
///     1.0, 2.0, 3.0, 4.0,   5.0, 6.0, 7.0, 8.0,
///     9.0, 10.0,11.0,12.0,  13.0,14.0,15.0,16.0,
/// ]);
/// let b = m512::from([
///     10.0,11.0,12.0,13.0,  14.0,15.0,16.0,17.0,
///     18.0,19.0,20.0,21.0,  22.0,23.0,24.0,25.0,
/// ]);
/// // IMM = 0: each 4-lane block produces [a0,a0,b0,b0]
/// let c: [f32; 16] = shuffle_m512::<0>(a, b).into();
/// assert_eq!(&c[0..4], &[1.0, 1.0, 10.0, 10.0]);
/// assert_eq!(&c[4..8], &[5.0, 5.0, 14.0, 14.0]);
/// assert_eq!(&c[8..12], &[9.0, 9.0, 18.0, 18.0]);
/// assert_eq!(&c[12..16], &[13.0,13.0,22.0,22.0]);
/// ```
/// * **Intrinsic:** [`_mm512_shuffle_ps`]
/// * **Assembly:** `vshufps zmm, zmm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shuffle_m512<const IMM: i32>(a: m512, b: m512) -> m512 {
    m512(unsafe { _mm512_shuffle_ps(a.0, b.0, IMM) })
}

/// Sets all `i8` lanes to the value given.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(5);
/// let b: [i8; 64] = a.into();
/// assert_eq!(b, [5_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_set1_epi8`]
/// * **Assembly:** `vpbroadcastb zmm, xmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn set_splat_i8_m512i(i: i8) -> m512i {
  m512i(unsafe { _mm512_set1_epi8(i) })
}

/// Sets all `i16` lanes to the value given.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(5);
/// let b: [i16; 32] = a.into();
/// assert_eq!(b, [5_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_set1_epi16`]
/// * **Assembly:** `vpbroadcastw zmm, xmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn set_splat_i16_m512i(i: i16) -> m512i {
  m512i(unsafe { _mm512_set1_epi16(i) })
}

/// Sets all `i32` lanes to the value given.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(5);
/// let b: [i32; 16] = a.into();
/// assert_eq!(b, [5_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_set1_epi32`]
/// * **Assembly:** `vpbroadcastd zmm, xmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn set_splat_i32_m512i(i: i32) -> m512i {
  m512i(unsafe { _mm512_set1_epi32(i) })
}

/// Splat an `i64` value into all 8 lanes of an `m512i`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(5);
/// let b: [i64; 8] = a.into();
/// assert_eq!(b, [5_i64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_set1_epi64`]
/// * **Assembly:** `vpbroadcastq zmm, r/m64`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn set_splat_i64_m512i(i: i64) -> m512i {
    m512i(unsafe { _mm512_set1_epi64(i) })
}

/// Splat an `f64` value into all 8 lanes of an `m512d`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(5.0);
/// let b: [f64; 8] = a.into();
/// assert_eq!(b, [5.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_set1_pd`]
/// * **Assembly:** `vbroadcastsd zmm, r/m64`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn set_splat_m512d(f: f64) -> m512d {
    m512d(unsafe { _mm512_set1_pd(f) })
}

/// Sets all `f32` lanes to the value given.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(5.0);
/// let b: [f32; 16] = a.into();
/// assert_eq!(b, [5.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_set1_ps`]
/// * **Assembly:** `vbroadcastss zmm, xmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn set_splat_m512(f: f32) -> m512 {
  m512(unsafe { _mm512_set1_ps(f) })
}

/// Load data from memory into a register.
/// ```
/// # use safe_arch::*;
/// let a = [1.0_f32; 16];
/// let b = load_m512(&a);
/// let c: [f32; 16] = b.into();
/// assert_eq!(c, [1.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_loadu_ps`]
/// * **Assembly:** `vmovups zmm, m512`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn load_m512(a: &[f32; 16]) -> m512 {
  m512(unsafe { _mm512_loadu_ps(a.as_ptr()) })
}

/// Load `f64` data from memory into a register.
/// ```
/// # use safe_arch::*;
/// let a = [1.0_f64; 8];
/// let b = load_m512d(&a);
/// let c: [f64; 8] = b.into();
/// assert_eq!(c, [1.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_loadu_pd`]
/// * **Assembly:** `vmovupd zmm, m512`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn load_m512d(a: &[f64; 8]) -> m512d {
    m512d(unsafe { _mm512_loadu_pd(a.as_ptr()) })
}

/// Load data from memory into a register.
/// ```
/// # use safe_arch::*;
/// let a = [1_i32; 16];
/// let b = load_m512i(&a);
/// let c: [i32; 16] = b.into();
/// assert_eq!(c, [1_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_loadu_si512`]
/// * **Assembly:** `vmovdqu64 zmm, m512`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn load_m512i(a: &[i32; 16]) -> m512i {
  m512i(unsafe { _mm512_loadu_si512(a.as_ptr() as *const __m512i) })
}

/// Store a register into memory.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(5.0);
/// let mut b = [0.0_f32; 16];
/// store_m512(&mut b, a);
/// assert_eq!(b, [5.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_storeu_ps`]
/// * **Assembly:** `vmovups m512, zmm`
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn store_m512(addr: &mut [f32; 16], a: m512) {
  unsafe { _mm512_storeu_ps(addr.as_mut_ptr(), a.0) }
}

/// Store a `m512d` register into memory.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(5.0);
/// let mut b = [0.0_f64; 8];
/// store_m512d(&mut b, a);
/// assert_eq!(b, [5.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_storeu_pd`]
/// * **Assembly:** `vmovupd m512, zmm`
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn store_m512d(addr: &mut [f64; 8], a: m512d) {
    unsafe { _mm512_storeu_pd(addr.as_mut_ptr(), a.0) }
}

/// Store a register into memory.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(5);
/// let mut b = m512i::default();
/// store_m512i(&mut b, a);
/// let c: [i32; 16] = b.into();
/// assert_eq!(c, [5_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_storeu_si512`]
/// * **Assembly:** `vmovdqu64 m512, zmm`
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn store_m512i(addr: &mut m512i, a: m512i) {
  unsafe { _mm512_storeu_si512(addr as *mut m512i as *mut __m512i, a.0) }
}

// Arithmetic operations

/// Lanewise `a + b` with lanes as `i8`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(5);
/// let b = set_splat_i8_m512i(10);
/// let c: [i8; 64] = add_i8_m512i(a, b).into();
/// assert_eq!(c, [15_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_add_epi8`]
/// * **Assembly:** `vpaddb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn add_i8_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_add_epi8(a.0, b.0) })
}

/// Lanewise `a + b` with lanes as `i16`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(5);
/// let b = set_splat_i16_m512i(10);
/// let c: [i16; 32] = add_i16_m512i(a, b).into();
/// assert_eq!(c, [15_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_add_epi16`]
/// * **Assembly:** `vpaddw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn add_i16_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_add_epi16(a.0, b.0) })
}

/// Lanewise `a + b` with lanes as `i32`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(5);
/// let b = set_splat_i32_m512i(10);
/// let c: [i32; 16] = add_i32_m512i(a, b).into();
/// assert_eq!(c, [15_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_add_epi32`]
/// * **Assembly:** `vpaddd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn add_i32_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_add_epi32(a.0, b.0) })
}

/// Lanewise `a + b` with lanes as `i64`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(5);
/// let b = set_splat_i64_m512i(10);
/// let c: [i64; 8] = add_i64_m512i(a, b).into();
/// assert_eq!(c, [15_i64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_add_epi64`]
/// * **Assembly:** `vpaddd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn add_i64_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_add_epi64(a.0, b.0) })
}

/// Lanewise `a + b` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(5.0);
/// let b = set_splat_m512(10.0);
/// let c: [f32; 16] = add_m512(a, b).into();
/// assert_eq!(c, [15.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_add_ps`]
/// * **Assembly:** `vaddps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn add_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_add_ps(a.0, b.0) })
}

/// Lanewise `a + b` with lanes as `f64`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(5.0);
/// let b = set_splat_m512d(10.0);
/// let c: [f64; 8] = add_m512d(a, b).into();
/// assert_eq!(c, [15.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_add_pd`]
/// * **Assembly:** `vaddpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn add_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_add_pd(a.0, b.0) })
}

/// Lanewise `a - b` with lanes as `i8`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(5);
/// let b = set_splat_i8_m512i(10);
/// let c: [i8; 64] = sub_i8_m512i(a, b).into();
/// assert_eq!(c, [-5_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_sub_epi8`]
/// * **Assembly:** `vpsubb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sub_i8_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_sub_epi8(a.0, b.0) })
}

/// Lanewise `a - b` with lanes as `i16`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(5);
/// let b = set_splat_i16_m512i(10);
/// let c: [i16; 32] = sub_i16_m512i(a, b).into();
/// assert_eq!(c, [-5_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_sub_epi16`]
/// * **Assembly:** `vpsubw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sub_i16_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_sub_epi16(a.0, b.0) })
}

/// Lanewise `a - b` with lanes as `i32`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(5);
/// let b = set_splat_i32_m512i(10);
/// let c: [i32; 16] = sub_i32_m512i(a, b).into();
/// assert_eq!(c, [-5_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_sub_epi32`]
/// * **Assembly:** `vpsubd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sub_i32_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_sub_epi32(a.0, b.0) })
}

/// Lanewise `a - b` with lanes as `i64`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(5);
/// let b = set_splat_i64_m512i(10);
/// let c: [i64; 8] = sub_i64_m512i(a, b).into();
/// assert_eq!(c, [-5_i64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_sub_epi64`]
/// * **Assembly:** `vpsubd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sub_i64_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_sub_epi64(a.0, b.0) })
}

/// Lanewise `a - b` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(5.0);
/// let b = set_splat_m512(10.0);
/// let c: [f32; 16] = sub_m512(a, b).into();
/// assert_eq!(c, [-5.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_sub_ps`]
/// * **Assembly:** `vsubps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sub_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_sub_ps(a.0, b.0) })
}

/// Lanewise `a - b` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(5.0);
/// let b = set_splat_m512d(10.0);
/// let c: [f64; 8] = sub_m512d(a, b).into();
/// assert_eq!(c, [-5.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_sub_ps`]
/// * **Assembly:** `vsubpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sub_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_sub_pd(a.0, b.0) })
}

/// Lanewise saturating `a + b` with lanes as signed `i8`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(126);
/// let b = set_splat_i8_m512i(125);
/// let c: [i8; 64] = add_saturating_i8_m512i(a, b).into();
/// // 126 + 125 = 251, but saturates to 127 (i8::MAX)
/// assert_eq!(c, [127_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_adds_epi8`]
/// * **Assembly:** `vpaddsb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn add_saturating_i8_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_adds_epi8(a.0, b.0) })
}

/// Lanewise saturating `a + b` with lanes as signed `i16`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(32_700);
/// let b = set_splat_i16_m512i(32_000);
/// let c: [i16; 32] = add_saturating_i16_m512i(a, b).into();
/// // 32700 + 32000 = 64700, but saturates to 32767 (i16::MAX)
/// assert_eq!(c, [32767_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_adds_epi16`]
/// * **Assembly:** `vpaddsw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn add_saturating_i16_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_adds_epi16(a.0, b.0) })
}

/// Lanewise saturating `a + b` with lanes as unsigned `u8`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(200_u8 as i8);
/// let b = set_splat_i8_m512i(100);
/// let c: [u8; 64] = add_saturating_u8_m512i(a, b).into();
/// // 200 + 100 = 300, but saturates to 255 (u8::MAX)
/// assert_eq!(c, [255_u8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_adds_epu8`]
/// * **Assembly:** `vpaddusb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn add_saturating_u8_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_adds_epu8(a.0, b.0) })
}

/// Lanewise saturating `a + b` with lanes as unsigned `u16`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(60_000_u16 as i16);
/// let b = set_splat_i16_m512i(10_000);
/// let c: [u16; 32] = add_saturating_u16_m512i(a, b).into();
/// // 60000 + 10000 = 70000, saturates to 65535 (u16::MAX)
/// assert_eq!(c, [65535_u16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_adds_epu16`]
/// * **Assembly:** `vpaddusw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn add_saturating_u16_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_adds_epu16(a.0, b.0) })
}

/// Lanewise saturating `a - b` with lanes as signed `i8`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(-120);
/// let b = set_splat_i8_m512i(100);
/// let c: [i8; 64] = sub_saturating_i8_m512i(a, b).into();
/// // -120 - 100 = -220, saturates to -128 (i8::MIN)
/// assert_eq!(c, [-128_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_subs_epi8`]
/// * **Assembly:** `vpsubsb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn sub_saturating_i8_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_subs_epi8(a.0, b.0) })
}

/// Lanewise saturating `a - b` with lanes as signed `i16`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(-30_000);
/// let b = set_splat_i16_m512i(10_000);
/// let c: [i16; 32] = sub_saturating_i16_m512i(a, b).into();
/// // -30000 - 10000 = -40000, saturates to -32768 (i16::MIN)
/// assert_eq!(c, [-32768_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_subs_epi16`]
/// * **Assembly:** `vpsubsw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn sub_saturating_i16_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_subs_epi16(a.0, b.0) })
}

/// Lanewise saturating `a - b` with lanes as unsigned `u8`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(50);
/// let b = set_splat_i8_m512i(100);
/// let c: [u8; 64] = sub_saturating_u8_m512i(a, b).into();
/// // 50 - 100 = -50, saturates to 0 (u8::MIN)
/// assert_eq!(c, [0_u8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_subs_epu8`]
/// * **Assembly:** `vpsubusb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn sub_saturating_u8_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_subs_epu8(a.0, b.0) })
}

/// Lanewise saturating `a - b` with lanes as unsigned `u16`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(5_000);
/// let b = set_splat_i16_m512i(10_000);
/// let c: [u16; 32] = sub_saturating_u16_m512i(a, b).into();
/// // 5000 - 10000 = -5000, saturates to 0 (u16::MIN)
/// assert_eq!(c, [0_u16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_subs_epu16`]
/// * **Assembly:** `vpsubusw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn sub_saturating_u16_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_subs_epu16(a.0, b.0) })
}

/// Lanewise `a * b` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(5.0);
/// let b = set_splat_m512(10.0);
/// let c: [f32; 16] = mul_m512(a, b).into();
/// assert_eq!(c, [50.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_mul_ps`]
/// * **Assembly:** `vmulps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn mul_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_mul_ps(a.0, b.0) })
}

/// Lanewise `a * b` with lanes as `f64`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(5.0);
/// let b = set_splat_m512d(10.0);
/// let c: [f64; 8] = mul_m512d(a, b).into();
/// assert_eq!(c, [50.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_mul_ps`]
/// * **Assembly:** `vmulpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn mul_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_mul_pd(a.0, b.0) })
}

/// Multiply the `i16` lanes and keep the low half of each 32-bit output.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(5);
/// let b = set_splat_i16_m512i(10);
/// let c: [i16; 32] = mul_i16_keep_low_m512i(a, b).into();
/// assert_eq!(c, [50_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_mullo_epi16`]
/// * **Assembly:** `vpmullw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn mul_i16_keep_low_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_mullo_epi16(a.0, b.0) })
}

/// Multiply the `i32` lanes and keep the low half of each 64-bit output.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(5);
/// let b = set_splat_i32_m512i(10);
/// let c: [i32; 16] = mul_i32_keep_low_m512i(a, b).into();
/// assert_eq!(c, [50_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_mullo_epi32`]
/// * **Assembly:** `vpmulld zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn mul_i32_keep_low_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_mullo_epi32(a.0, b.0) })
}

/// Signed widening multiply of the 32-bit lanes → 64-bit lanes.
///
/// * **Intrinsic:** [`_mm512_mul_epi32`]
/// * **Assembly:** `vpmulldq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn mul_i32_wide_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_mul_epi32(a.0, b.0) })
}

/// Unsigned widening multiply of the 32-bit lanes → 64-bit lanes.
///
/// * **Intrinsic:** [`_mm512_mul_epu32`]
/// * **Assembly:** `vpmuludq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn mul_u32_wide_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_mul_epu32(a.0, b.0) })
}

/// Multiply the `i16` lanes and keep the high half of each 32‐bit product.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// // 0x4000×0x4000 = 0x1000_0000 → high 16 bits = 0x1000 (4096)
/// let a = set_splat_i16_m512i(0x4000);
/// let b = set_splat_i16_m512i(0x4000);
/// let c: [i16; 32] = mul_i16_keep_high_m512i(a, b).into();
/// assert_eq!(c, [0x1000_i16; 32]);
///
/// // Test a negative case: -0x4000×0x4000 = -0x1000_0000 → high 16 bits = 0xF000 (-4096)
/// let a2 = set_splat_i16_m512i(-0x4000);
/// let c2: [i16; 32] = mul_i16_keep_high_m512i(a2, b).into();
/// assert_eq!(c2, [(-0x1000_i16); 32]);
/// ```
/// * **Intrinsic:** [`_mm512_mulhi_epi16`]
/// * **Assembly:** `vpmulhw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn mul_i16_keep_high_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_mulhi_epi16(a.0, b.0) })
}

/// Multiply the `u16` lanes and keep the high half of each 32‐bit product.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// // 0x8000×0x8000 = 0x4000_0000 → high 16 bits = 0x4000 (16384)
/// let a = set_splat_i16_m512i(0x8000u16 as i16);
/// let b = set_splat_i16_m512i(0x8000u16 as i16);
/// let c: [u16; 32] = mul_u16_keep_high_m512i(a, b).into();
/// assert_eq!(c, [0x4000_u16; 32]);
///
/// // A mixed‐value test:
/// let a2 = set_splat_i16_m512i(0x1234);
/// let b2 = set_splat_i16_m512i(0x00FF);
/// // 0x1234×0x00FF = 0x1234 × 255 = 0x1234×0x00FF = 0x1234×0x00FF = 0x1234×0x00FF = 0x2FE * 0x100 + ...
/// // actually 0x1234=4660, ×255=1_188_300 = 0x122A6C → high16 = 0x0012 (18)
/// let c2: [u16; 32] = mul_u16_keep_high_m512i(a2, b2).into();
/// assert_eq!(c2, [0x0012_u16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_mulhi_epu16`]
/// * **Assembly:** `vpmulhuw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn mul_u16_keep_high_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_mulhi_epu16(a.0, b.0) })
}

/// Lanewise `a / b` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(50.0);
/// let b = set_splat_m512(10.0);
/// let c: [f32; 16] = div_m512(a, b).into();
/// assert_eq!(c, [5.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_div_ps`]
/// * **Assembly:** `vdivps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn div_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_div_ps(a.0, b.0) })
}

/// Lanewise `a / b` with lanes as `f64`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(50.0);
/// let b = set_splat_m512d(10.0);
/// let c: [f64; 8] = div_m512d(a, b).into();
/// assert_eq!(c, [5.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_div_pd`]
/// * **Assembly:** `vdivps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn div_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_div_pd(a.0, b.0) })
}

/// Fused multiply-add. Computes `(a * b) + c` with a single rounding.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(2.0);
/// let b = set_splat_m512(3.0);
/// let c = set_splat_m512(1.0);
/// let d: [f32; 16] = fused_mul_add_m512(a, b, c).into();
/// assert_eq!(d, [7.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_fmadd_ps`]
/// * **Assembly:** `vfmadd132ps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_add_m512(a: m512, b: m512, c: m512) -> m512 {
  m512(unsafe { _mm512_fmadd_ps(a.0, b.0, c.0) })
}

/// Fused multiply-add. Computes `(a * b) + c` with a single rounding.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(2.0);
/// let b = set_splat_m512d(3.0);
/// let c = set_splat_m512d(1.0);
/// let d: [f64; 8] = fused_mul_add_m512d(a, b, c).into();
/// assert_eq!(d, [7.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_fmadd_pd`]
/// * **Assembly:** `vfmadd132pd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_add_m512d(a: m512d, b: m512d, c: m512d) -> m512d {
  m512d(unsafe { _mm512_fmadd_pd(a.0, b.0, c.0) })
}

/// Fused multiply-subtract. Computes `(a * b) - c` with a single rounding.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(2.0);
/// let b = set_splat_m512(3.0);
/// let c = set_splat_m512(1.0);
/// let d: [f32; 16] = fused_mul_sub_m512(a, b, c).into();
/// assert_eq!(d, [5.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_fmsub_ps`]
/// * **Assembly:** one of
///   * `vfmsub132ps zmm, zmm, zmm`
///   * `vfmsub213ps zmm, zmm, zmm`
///   * `vfmsub231ps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_sub_m512(a: m512, b: m512, c: m512) -> m512 {
  m512(unsafe { _mm512_fmsub_ps(a.0, b.0, c.0) })
}

/// Fused multiply-subtract. Computes `(a * b) - c` with a single rounding.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(2.0);
/// let b = set_splat_m512d(3.0);
/// let c = set_splat_m512d(1.0);
/// let d: [f64; 8] = fused_mul_sub_m512d(a, b, c).into();
/// assert_eq!(d, [5.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_fmsub_pd`]
/// * **Assembly:** one of
///   * `vfmsub132pd zmm, zmm, zmm`
///   * `vfmsub213pd zmm, zmm, zmm`
///   * `vfmsub231pd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_sub_m512d(a: m512d, b: m512d, c: m512d) -> m512d {
  m512d(unsafe { _mm512_fmsub_pd(a.0, b.0, c.0) })
}

/// Lanewise fused `-(a * b) + c`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(2.0);
/// let b = set_splat_m512(3.0);
/// let c = set_splat_m512(1.0);
/// let d: [f32; 16] = fused_mul_neg_add_m512(a, b, c).into();
/// assert_eq!(d, [-5.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_fnmadd_ps`]
/// * **Assembly:** one of
///   * `vfnmadd132ps zmm, zmm, zmm`
///   * `vfnmadd213ps zmm, zmm, zmm`
///   * `vfnmadd231ps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_neg_add_m512(a: m512, b: m512, c: m512) -> m512 {
  m512(unsafe { _mm512_fnmadd_ps(a.0, b.0, c.0) })
}

/// Lanewise fused `-(a * b) + c`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(2.0);
/// let b = set_splat_m512d(3.0);
/// let c = set_splat_m512d(1.0);
/// let d: [f64; 8] = fused_mul_neg_add_m512d(a, b, c).into();
/// assert_eq!(d, [-5.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_fnmadd_pd`]
/// * **Assembly:** one of
///   * `vfnmadd132pd zmm, zmm, zmm`
///   * `vfnmadd213pd zmm, zmm, zmm`
///   * `vfnmadd231pd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_neg_add_m512d(a: m512d, b: m512d, c: m512d) -> m512d {
  m512d(unsafe { _mm512_fnmadd_pd(a.0, b.0, c.0) })
}

/// Lanewise fused `-(a * b) - c`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(2.0);
/// let b = set_splat_m512(3.0);
/// let c = set_splat_m512(1.0);
/// let d: [f32; 16] = fused_mul_neg_sub_m512(a, b, c).into();
/// assert_eq!(d, [-7.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_fnmsub_ps`]
/// * **Assembly:** one of
///   * `vfnmsub132ps zmm, zmm, zmm`
///   * `vfnmsub213ps zmm, zmm, zmm`
///   * `vfnmsub231ps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_neg_sub_m512(a: m512, b: m512, c: m512) -> m512 {
  m512(unsafe { _mm512_fnmsub_ps(a.0, b.0, c.0) })
}

/// Lanewise fused `-(a * b) - c`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(2.0);
/// let b = set_splat_m512d(3.0);
/// let c = set_splat_m512d(1.0);
/// let d: [f64; 8] = fused_mul_neg_sub_m512d(a, b, c).into();
/// assert_eq!(d, [-7.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_fnmsub_pd`]
/// * **Assembly:** one of
///   * `vfnmsub132pd zmm, zmm, zmm`
///   * `vfnmsub213pd zmm, zmm, zmm`
///   * `vfnmsub231pd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_neg_sub_m512d(a: m512d, b: m512d, c: m512d) -> m512d {
  m512d(unsafe { _mm512_fnmsub_pd(a.0, b.0, c.0) })
}

/// Alternating fused multiply add/sub: even lanes `(a*b)+c`, odd lanes `(a*b)-c`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(2.0);
/// let b = set_splat_m512(3.0);
/// let c = set_splat_m512(1.0);
/// let d: [f32; 16] = fused_mul_add_sub_m512(a, b, c).into();
/// assert_eq!(d, [5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0]);
/// ```
/// * **Intrinsic:** [`_mm512_fmaddsub_ps`]
/// * **Assembly:** `vfmaddsub132ps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_add_sub_m512(a: m512, b: m512, c: m512) -> m512 {
  m512(unsafe { _mm512_fmaddsub_ps(a.0, b.0, c.0) })
}

/// Alternating fused multiply add/sub: even lanes `(a*b)+c`, odd lanes `(a*b)-c`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(2.0);
/// let b = set_splat_m512d(3.0);
/// let c = set_splat_m512d(1.0);
/// let d: [f64; 8] = fused_mul_add_sub_m512d(a, b, c).into();
/// assert_eq!(d, [5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0]);
/// ```
/// * **Intrinsic:** [`_mm512_fmaddsub_pd`]
/// * **Assembly:** `vfmaddsub132pd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_add_sub_m512d(a: m512d, b: m512d, c: m512d) -> m512d {
  m512d(unsafe { _mm512_fmaddsub_pd(a.0, b.0, c.0) })
}

/// Alternating fused multiply sub/add: even lanes `(a*b)-c`, odd lanes `(a*b)+c`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(2.0);
/// let b = set_splat_m512(3.0);
/// let c = set_splat_m512(1.0);
/// let d: [f32; 16] = fused_mul_sub_add_m512(a, b, c).into();
/// assert_eq!(d, [7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0]);
/// ```
/// * **Intrinsic:** [`_mm512_fmsubadd_ps`]
/// * **Assembly:** `vfmsubadd132ps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_sub_add_m512(a: m512, b: m512, c: m512) -> m512 {
  m512(unsafe { _mm512_fmsubadd_ps(a.0, b.0, c.0) })
}

/// Alternating fused multiply sub/add: even lanes `(a*b)-c`, odd lanes `(a*b)+c`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(2.0);
/// let b = set_splat_m512d(3.0);
/// let c = set_splat_m512d(1.0);
/// let d: [f64; 8] = fused_mul_sub_add_m512d(a, b, c).into();
/// assert_eq!(d, [7.0,5.0,7.0,5.0,7.0,5.0,7.0,5.0]);
/// ```
/// * **Intrinsic:** [`_mm512_fmsubadd_pd`]
/// * **Assembly:** `vfmsubadd132pd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn fused_mul_sub_add_m512d(a: m512d, b: m512d, c: m512d) -> m512d {
  m512d(unsafe { _mm512_fmsubadd_pd(a.0, b.0, c.0) })
}

// Comparison operations

/// Compare `i8` lanes under `OP`, returning a 64-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i8_m512i(5);
/// let b = set_splat_i8_m512i(5);
/// let m = cmp_op_mask_i8::<{ _MM_CMPINT_EQ }>(a, b);
/// assert_eq!(m, u64::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_epi8_mask`
/// * **Assembly:** `VPCMPB k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn cmp_op_mask_i8<const OP: i32>(a: m512i, b: m512i) -> mmask64 {
    unsafe { _mm512_cmp_epi8_mask(a.0, b.0, OP) }
}

/// Compare `u8` lanes under `OP`, returning a 64-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i8_m512i(3);
/// let b = set_splat_i8_m512i(5);
/// // unsigned <  → 3<5
/// let m = cmp_op_mask_u8::<{ _MM_CMPINT_LT }>(a, b);
/// assert_eq!(m, u64::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_epu8_mask`
/// * **Assembly:** `VPCMPUB k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn cmp_op_mask_u8<const OP: i32>(a: m512i, b: m512i) -> mmask64 {
    unsafe { _mm512_cmp_epu8_mask(a.0, b.0, OP) }
}

/// Compare `i16` lanes under `OP`, returning a 32-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i16_m512i(5);
/// let b = set_splat_i16_m512i(5);
/// let m = cmp_op_mask_i16::<{ _MM_CMPINT_EQ }>(a, b);
/// assert_eq!(m, u32::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_epi16_mask`
/// * **Assembly:** `VPCMPW k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn cmp_op_mask_i16<const OP: i32>(a: m512i, b: m512i) -> mmask32 {
    unsafe { _mm512_cmp_epi16_mask(a.0, b.0, OP) }
}

/// Compare `u16` lanes under `OP`, returning a 32-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i16_m512i(3);
/// let b = set_splat_i16_m512i(5);
/// // unsigned <= → 3<=5
/// let m = cmp_op_mask_u16::<{ _MM_CMPINT_LE }>(a, b);
/// assert_eq!(m, u32::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_epu16_mask`
/// * **Assembly:** `VPCMPUW k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn cmp_op_mask_u16<const OP: i32>(a: m512i, b: m512i) -> mmask32 {
    unsafe { _mm512_cmp_epu16_mask(a.0, b.0, OP) }
}

/// Compare `i32` lanes under `OP`, returning a 16-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i32_m512i(5);
/// let b = set_splat_i32_m512i(2);
/// // signed > → 5>2
/// let m = cmp_op_mask_i32::<{ _MM_CMPINT_LT }>(b, a);
/// assert_eq!(m, u16::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_epi32_mask`
/// * **Assembly:** `VPCMPD k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_i32<const OP: i32>(a: m512i, b: m512i) -> mmask16 {
    unsafe { _mm512_cmp_epi32_mask(a.0, b.0, OP) }
}

/// Compare `u32` lanes under `OP`, returning a 16-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i32_m512i(2);
/// let b = set_splat_i32_m512i(5);
/// // unsigned < → 2<5
/// let m = cmp_op_mask_u32::<{ _MM_CMPINT_LT }>(a, b);
/// assert_eq!(m, u16::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_epu32_mask`
/// * **Assembly:** `VPCMPUD k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_u32<const OP: i32>(a: m512i, b: m512i) -> mmask16 {
    unsafe { _mm512_cmp_epu32_mask(a.0, b.0, OP) }
}

/// Compare `i64` lanes under `OP`, returning an 8-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i64_m512i(5);
/// let b = set_splat_i64_m512i(5);
/// let m = cmp_op_mask_i64::<{ _MM_CMPINT_EQ }>(a, b);
/// assert_eq!(m, u8::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_epi64_mask`
/// * **Assembly:** `VPCMPQ k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_i64<const OP: i32>(a: m512i, b: m512i) -> mmask8 {
    unsafe { _mm512_cmp_epi64_mask(a.0, b.0, OP) }
}

/// Compare `u64` lanes under `OP`, returning an 8-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i64_m512i(3);
/// let b = set_splat_i64_m512i(5);
/// // unsigned <= → 3<=5
/// let m = cmp_op_mask_u64::<{ _MM_CMPINT_LE }>(a, b);
/// assert_eq!(m, u8::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_epu64_mask`
/// * **Assembly:** `VPCMPUQ k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_u64<const OP: i32>(a: m512i, b: m512i) -> mmask8 {
    unsafe { _mm512_cmp_epu64_mask(a.0, b.0, OP) }
}

/// Compare `f32` lanes under `OP`, returning a 16-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_m512(3.0);
/// let b = set_splat_m512(5.0);
/// // < : 3<5
/// let m = cmp_op_mask_f32::<{ _MM_CMPINT_LT }>(a, b);
/// assert_eq!(m, u16::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_ps_mask`
/// * **Assembly:** `VPCMPPS k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_f32<const OP: i32>(a: m512, b: m512) -> mmask16 {
    unsafe { _mm512_cmp_ps_mask(a.0, b.0, OP) }
}

/// Compare `f64` lanes under `OP`, returning an 8-bit mask.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_m512d(3.0);
/// let b = set_splat_m512d(3.0);
/// let m = cmp_op_mask_f64::<{ _MM_CMPINT_EQ }>(a, b);
/// assert_eq!(m, u8::MAX);
/// ```
/// * **Intrinsic:** `_mm512_cmp_pd_mask`
/// * **Assembly:** `VPCMPPD k, zmm, zmm, imm8`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_f64<const OP: i32>(a: m512d, b: m512d) -> mmask8 {
    unsafe { _mm512_cmp_pd_mask(a.0, b.0, OP) }
}

//
// 2) Full-width vector versions
//

/// `i8` version: expands your `mmask64` into a `m512i` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i8_m512i(5);
/// let b = set_splat_i8_m512i(5);
/// let v = cmp_op_mask_i8_m512i::<{ _MM_CMPINT_EQ }>(a, b);
/// assert_eq!(v, set_splat_i8_m512i(-1));
/// ```
/// * **Intrinsic:** `_mm512_cmp_epi8_mask`, `_mm512_maskz_mov_epi8`
/// * **Assembly:** `VPCMPB k, zmm, zmm, imm8` + `VPMOVM2B zmm, k`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn cmp_op_mask_i8_m512i<const OP: i32>(a: m512i, b: m512i) -> m512i {
    let m = cmp_op_mask_i8::<OP>(a, b);
    m512i(unsafe { _mm512_maskz_mov_epi8(m, _mm512_set1_epi8(-1)) })
}

/// `u8` version: expands your `mmask64` into a `m512i` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i8_m512i(3);
/// let b = set_splat_i8_m512i(5);
/// let v = cmp_op_mask_u8_m512i::<{ _MM_CMPINT_LT }>(a, b);
/// assert_eq!(v, set_splat_i8_m512i(-1));
/// ```
/// * **Intrinsic:** `_mm512_cmp_epu8_mask`, `_mm512_maskz_mov_epi8`
/// * **Assembly:** `VPCMPUB k, zmm, zmm, imm8` + `VPMOVM2B zmm, k`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn cmp_op_mask_u8_m512i<const OP: i32>(a: m512i, b: m512i) -> m512i {
    let m = cmp_op_mask_u8::<OP>(a, b);
    m512i(unsafe { _mm512_maskz_mov_epi8(m, _mm512_set1_epi8(-1)) })
}

/// `i16` version: expands your `mmask32` into a `m512i` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i16_m512i(5);
/// let b = set_splat_i16_m512i(5);
/// let v = cmp_op_mask_i16_m512i::<{ _MM_CMPINT_EQ }>(a, b);
/// assert_eq!(v, set_splat_i16_m512i(-1));
/// ```
/// * **Intrinsic:** `_mm512_cmp_epi16_mask`, `_mm512_maskz_mov_epi16`
/// * **Assembly:** `VPCMPW k, zmm, zmm, imm8` + `VPMOVM2W zmm, k`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn cmp_op_mask_i16_m512i<const OP: i32>(a: m512i, b: m512i) -> m512i {
    let m = cmp_op_mask_i16::<OP>(a, b);
    m512i(unsafe { _mm512_maskz_mov_epi16(m, _mm512_set1_epi16(-1)) })
}

/// `u16` version: expands your `mmask32` into a `m512i` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i16_m512i(3);
/// let b = set_splat_i16_m512i(5);
/// let v = cmp_op_mask_u16_m512i::<{ _MM_CMPINT_LE }>(a, b);
/// assert_eq!(v, set_splat_i16_m512i(-1));
/// ```
/// * **Intrinsic:** `_mm512_cmp_epu16_mask`, `_mm512_maskz_mov_epi16`
/// * **Assembly:** `VPCMPUW k, zmm, zmm, imm8` + `VPMOVM2W zmm, k`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn cmp_op_mask_u16_m512i<const OP: i32>(a: m512i, b: m512i) -> m512i {
    let m = cmp_op_mask_u16::<OP>(a, b);
    m512i(unsafe { _mm512_maskz_mov_epi16(m, _mm512_set1_epi16(-1)) })
}

/// `i32` version: expands your `mmask16` into a `m512i` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i32_m512i(5);
/// let b = set_splat_i32_m512i(2);
/// let v = cmp_op_mask_i32_m512i::<{ _MM_CMPINT_LT }>(b, a);
/// assert_eq!(v, set_splat_i32_m512i(-1));
/// ```
/// * **Intrinsic:** `_mm512_cmp_epi32_mask`, `_mm512_maskz_mov_epi32`
/// * **Assembly:** `VPCMPD k, zmm, zmm, imm8` + `VPMOVM2D zmm, k`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_i32_m512i<const OP: i32>(a: m512i, b: m512i) -> m512i {
    let m = cmp_op_mask_i32::<OP>(a, b);
    m512i(unsafe { _mm512_maskz_mov_epi32(m, _mm512_set1_epi32(-1)) })
}

/// `u32` version: expands your `mmask16` into a `m512i` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i32_m512i(2);
/// let b = set_splat_i32_m512i(5);
/// let v = cmp_op_mask_u32_m512i::<{ _MM_CMPINT_LT }>(a, b);
/// assert_eq!(v, set_splat_i32_m512i(-1));
/// ```
/// * **Intrinsic:** `_mm512_cmp_epu32_mask`, `_mm512_maskz_mov_epi32`
/// * **Assembly:** `VPCMPUD k, zmm, zmm, imm8` + `VPMOVM2D zmm, k`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_u32_m512i<const OP: i32>(a: m512i, b: m512i) -> m512i {
    let m = cmp_op_mask_u32::<OP>(a, b);
    m512i(unsafe { _mm512_maskz_mov_epi32(m, _mm512_set1_epi32(-1)) })
}

/// `i64` version: expands your `mmask8` into a `m512i` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i64_m512i(5);
/// let b = set_splat_i64_m512i(5);
/// let v = cmp_op_mask_i64_m512i::<{ _MM_CMPINT_EQ }>(a, b);
/// assert_eq!(v, set_splat_i64_m512i(-1));
/// ```
/// * **Intrinsic:** `_mm512_cmp_epi64_mask`, `_mm512_maskz_mov_epi64`
/// * **Assembly:** `VPCMPQ k, zmm, zmm, imm8` + `VPMOVM2Q zmm, k`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_i64_m512i<const OP: i32>(a: m512i, b: m512i) -> m512i {
    let m = cmp_op_mask_i64::<OP>(a, b);
    m512i(unsafe { _mm512_maskz_mov_epi64(m, _mm512_set1_epi64(-1)) })
}

/// `u64` version: expands your `mmask8` into a `m512i` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_i64_m512i(3);
/// let b = set_splat_i64_m512i(5);
/// let v = cmp_op_mask_u64_m512i::<{ _MM_CMPINT_LE }>(a, b);
/// assert_eq!(v, set_splat_i64_m512i(-1));
/// ```
/// * **Intrinsic:** `_mm512_cmp_epu64_mask`, `_mm512_maskz_mov_epi64`
/// * **Assembly:** `VPCMPUQ k, zmm, zmm, imm8` + `VPMOVM2Q zmm, k`
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_u64_m512i<const OP: i32>(a: m512i, b: m512i) -> m512i {
    let m = cmp_op_mask_u64::<OP>(a, b);
    m512i(unsafe { _mm512_maskz_mov_epi64(m, _mm512_set1_epi64(-1)) })
}

/// `f32` version: expands your `mmask16` into a `m512` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_m512(3.0);
/// let b = set_splat_m512(5.0);
/// let v = cmp_op_mask_m512::<{ _MM_CMPINT_LT }>(a, b);
/// assert_eq!(v.to_bits(), [u32::MAX; 16]);
/// ```
/// * **Intrinsic:** `_mm512_cmp_ps_mask`, `_mm512_maskz_mov_ps`
/// * **Assembly:** `VCMPPS k, zmm, zmm, imm8` + masked move
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_m512<const OP: i32>(a: m512, b: m512) -> m512 {
    let m = unsafe { _mm512_cmp_ps_mask(a.0, b.0, OP) };
    m512(unsafe {
        let ones = _mm512_castsi512_ps(_mm512_set1_epi32(-1));
        _mm512_maskz_mov_ps(m, ones)
    })
}

/// `f64` version: expands your `mmask8` into a `m512d` of all-ones or zeros.
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = set_splat_m512d(3.0);
/// let b = set_splat_m512d(3.0);
/// let v = cmp_op_mask_m512d::<{ _MM_CMPINT_EQ }>(a, b);
/// assert_eq!(v.to_bits(), [u64::MAX; 8]);
/// ```
/// * **Intrinsic:** `_mm512_cmp_pd_mask`, `_mm512_maskz_mov_pd`
/// * **Assembly:** `VCMPPD k, zmm, zmm, imm8` + masked move
#[must_use] #[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn cmp_op_mask_m512d<const OP: i32>(a: m512d, b: m512d) -> m512d {
    let m = unsafe { _mm512_cmp_pd_mask(a.0, b.0, OP) };
    m512d(unsafe {
        let ones = _mm512_castsi512_pd(_mm512_set1_epi64(-1));
        _mm512_maskz_mov_pd(m, ones)
    })
}

// Bitwise operations

/// Bitwise `a & b`.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([0_i64, 0, 1, 1, 0, 0, 1, 1]);
/// let b = m512i::from([0_i64, 1, 0, 1, 0, 1, 0, 1]);
/// let c: [i64; 8] = bitand_m512i(a, b).into();
/// assert_eq!(c, [0_i64, 0, 0, 1, 0, 0, 0, 1]);
/// ```
/// * **Intrinsic:** [`_mm512_and_si512`]
/// * **Assembly:** `vpandq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn bitand_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_and_si512(a.0, b.0) })
}

/// Bitwise `a & b` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = m512::from([1.0_f32; 16]);
/// let b = m512::from([1.0_f32; 16]);
/// let c: [f32; 16] = bitand_m512(a, b).into();
/// assert_eq!(c, [1.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_and_ps`]
/// * **Assembly:** `vandps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn bitand_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_and_ps(a.0, b.0) })
}

/// Bitwise `a & b` with lanes as `f64`.
/// ```
/// # use safe_arch::*;
/// let a = m512d::from([1.0_f64; 8]);
/// let b = m512d::from([1.0_f64; 8]);
/// let c: [f64; 8] = bitand_m512d(a, b).into();
/// assert_eq!(c, [1.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_and_pd`]
/// * **Assembly:** `vandpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn bitand_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_and_pd(a.0, b.0) })
}

/// Bitwise `(!a) & b`.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([0_i64, 0, 1, 1, 0, 0, 1, 1]);
/// let b = m512i::from([0_i64, 1, 0, 1, 0, 1, 0, 1]);
/// let c: [i64; 8] = bitandnot_m512i(a, b).into();
/// assert_eq!(c, [0_i64, 1, 0, 0, 0, 1, 0, 0]);
/// ```
/// * **Intrinsic:** [`_mm512_andnot_si512`]
/// * **Assembly:** `vpandnq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn bitandnot_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_andnot_si512(a.0, b.0) })
}

/// Bitwise `(!a) & b` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = m512::from([0.0_f32; 16]);
/// let b = m512::from([1.0_f32; 16]);
/// let c: [f32; 16] = bitandnot_m512(a, b).into();
/// // The result is not 1.0 due to floating point bit patterns
/// ```
/// * **Intrinsic:** [`_mm512_andnot_ps`]
/// * **Assembly:** `vandnps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn bitandnot_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_andnot_ps(a.0, b.0) })
}

/// Bitwise `(!a) & b` with lanes as `f64`.
/// ```
/// # use safe_arch::*;
/// let a = m512d::from([0.0_f64; 8]);
/// let b = m512d::from([1.0_f64; 8]);
/// let c: [f64; 8] = bitandnot_m512d(a, b).into();
/// // The result is not 1.0 due to floating point bit patterns
/// ```
/// * **Intrinsic:** [`_mm512_andnot_pd`]
/// * **Assembly:** `vandnpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn bitandnot_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_andnot_pd(a.0, b.0) })
}

/// Average `u8` lanes (unsigned 8-bit integers) in two `m512i` vectors.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([100_u8; 64]);
/// let b = m512i::from([120_u8; 64]);
/// let c: [u8; 64] = average_u8_m512i(a, b).into();
/// assert_eq!(c, [110_u8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_avg_epu8`]
/// * **Assembly:** `vpavgb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn average_u8_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_avg_epu8(a.0, b.0) })
}

/// Average `u16` lanes in two `m512i` vectors (unsigned 16-bit integers).
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([100_u16; 32]);
/// let b = m512i::from([120_u16; 32]);
/// let c: [u16; 32] = average_u16_m512i(a, b).into();
/// assert_eq!(c, [110_u16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_avg_epu16`]
/// * **Assembly:** `vpavgw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn average_u16_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_avg_epu16(a.0, b.0) })
}

/// Bitwise `a | b`.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([0_i64, 0, 1, 1, 0, 0, 1, 1]);
/// let b = m512i::from([0_i64, 1, 0, 1, 0, 1, 0, 1]);
/// let c: [i64; 8] = bitor_m512i(a, b).into();
/// assert_eq!(c, [0_i64, 1, 1, 1, 0, 1, 1, 1]);
/// ```
/// * **Intrinsic:** [`_mm512_or_si512`]
/// * **Assembly:** `vporq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn bitor_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_or_si512(a.0, b.0) })
}

/// Bitwise `a | b` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = m512::from([0.0_f32; 16]);
/// let b = m512::from([1.0_f32; 16]);
/// let c: [f32; 16] = bitor_m512(a, b).into();
/// assert_eq!(c, [1.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_or_ps`]
/// * **Assembly:** `vorps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn bitor_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_or_ps(a.0, b.0) })
}

/// Bitwise `a | b` with lanes as `f64`.
/// ```
/// # use safe_arch::*;
/// let a = m512d::from([0.0_f64; 8]);
/// let b = m512d::from([1.0_f64; 8]);
/// let c: [f64; 8] = bitor_m512d(a, b).into();
/// assert_eq!(c, [1.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_or_pd`]
/// * **Assembly:** `vorpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn bitor_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_or_pd(a.0, b.0) })
}

/// Bitwise `a ^ b`.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([0_i64, 0, 1, 1, 0, 0, 1, 1]);
/// let b = m512i::from([0_i64, 1, 0, 1, 0, 1, 0, 1]);
/// let c: [i64; 8] = bitxor_m512i(a, b).into();
/// assert_eq!(c, [0_i64, 1, 1, 0, 0, 1, 1, 0]);
/// ```
/// * **Intrinsic:** [`_mm512_xor_si512`]
/// * **Assembly:** `vpxorq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn bitxor_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_xor_si512(a.0, b.0) })
}

/// Bitwise `a ^ b` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = m512::from([1.0_f32; 16]);
/// let b = m512::from([1.0_f32; 16]);
/// let c: [f32; 16] = bitxor_m512(a, b).into();
/// assert_eq!(c, [0.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_xor_ps`]
/// * **Assembly:** `vxorps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn bitxor_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_xor_ps(a.0, b.0) })
}

/// Bitwise `a ^ b` with lanes as `f64`.
/// ```
/// # use safe_arch::*;
/// let a = m512d::from([1.0_f64; 8]);
/// let b = m512d::from([1.0_f64; 8]);
/// let c: [f64; 8] = bitxor_m512d(a, b).into();
/// assert_eq!(c, [0.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_xor_pd`]
/// * **Assembly:** `vxorpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn bitxor_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_xor_pd(a.0, b.0) })
}

// Blend operations

/// Blend `i8` values using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(10);
/// let b = set_splat_i8_m512i(20);
/// let mask = 0xAAAAAAAAAAAAAAAA;
/// let c: [i8; 64] = blend_varying_i8_m512i(a, b, mask).into();
/// for (i, &val) in c.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 20 } else { 10 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_blend_epi8`]
/// * **Assembly:** `vpblendmb zmm {k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn blend_varying_i8_m512i(a: m512i, b: m512i, mask: mmask64) -> m512i {
  m512i(unsafe { _mm512_mask_blend_epi8(mask, a.0, b.0) })
}

/// Blend `i16` values using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(10);
/// let b = set_splat_i16_m512i(20);
/// let mask = 0xAAAAAAAA;
/// let c: [i16; 32] = blend_varying_i16_m512i(a, b, mask).into();
/// for (i, &val) in c.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 20 } else { 10 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_blend_epi16`]
/// * **Assembly:** `vpblendmw zmm {k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn blend_varying_i16_m512i(a: m512i, b: m512i, mask: mmask32) -> m512i {
  m512i(unsafe { _mm512_mask_blend_epi16(mask, a.0, b.0) })
}

/// Blend `i32` values using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(10);
/// let b = set_splat_i32_m512i(20);
/// let mask = 0xAAAA;
/// let c: [i32; 16] = blend_varying_i32_m512i(a, b, mask).into();
/// for (i, &val) in c.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 20 } else { 10 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_blend_epi32`]
/// * **Assembly:** `vpblendmd zmm {k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn blend_varying_i32_m512i(a: m512i, b: m512i, mask: mmask16) -> m512i {
  m512i(unsafe { _mm512_mask_blend_epi32(mask, a.0, b.0) })
}

/// Blend `f32` values using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(10.0);
/// let b = set_splat_m512(20.0);
/// let mask = 0xAAAA;
/// let c: [f32; 16] = blend_varying_m512(a, b, mask).into();
/// for (i, &val) in c.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 20.0 } else { 10.0 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_blend_ps`]
/// * **Assembly:** `vblendmps zmm {k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn blend_varying_m512(a: m512, b: m512, mask: mmask16) -> m512 {
  m512(unsafe { _mm512_mask_blend_ps(mask, a.0, b.0) })
}

/// Blend `f64` values using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(10.0);
/// let b = set_splat_m512d(20.0);
/// let mask = 0xAA;
/// let c: [f64; 8] = blend_varying_m512d(a, b, mask).into();
/// for (i, &val) in c.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 20.0 } else { 10.0 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_blend_pd`]
/// * **Assembly:** `vblendmpd zmm {k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn blend_varying_m512d(a: m512d, b: m512d, mask: mmask8) -> m512d {
  m512d(unsafe { _mm512_mask_blend_pd(mask, a.0, b.0) })
}

/// Sets the lowest `i8` lane of an `m128i` as all lanes of an `m512i`.
/// ```
/// # use safe_arch::*;
/// let a = m128i::from(7_i8 as i128);
/// let b: [i8; 64] = set_splat_i8_m128i_s_m512i(a).into();
/// assert_eq!(b, [7_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_broadcastb_epi8`]
/// * **Assembly:** `vpbroadcastb zmm, xmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512bw", target_feature = "avx512vl"))))]
pub fn set_splat_i8_m128i_s_m512i(a: m128i) -> m512i {
    m512i(unsafe { _mm512_broadcastb_epi8(a.0) })
}

/// Sets the lowest `i16` lane of an `m128i` as all lanes of an `m512i`.
/// ```
/// # use safe_arch::*;
/// let a = m128i::from(42_i16 as i128);
/// let b: [i16; 32] = set_splat_i16_m128i_s_m512i(a).into();
/// assert_eq!(b, [42_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_broadcastw_epi16`]
/// * **Assembly:** `vpbroadcastw zmm, xmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512bw", target_feature = "avx512vl"))))]
pub fn set_splat_i16_m128i_s_m512i(a: m128i) -> m512i {
    m512i(unsafe { _mm512_broadcastw_epi16(a.0) })
}

/// Sets the lowest `i32` lane of an `m128i` as all lanes of an `m512i`.
/// ```
/// # use safe_arch::*;
/// let a = m128i::from(123_i32 as i128);
/// let b: [i32; 16] = set_splat_i32_m128i_s_m512i(a).into();
/// assert_eq!(b, [123_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_broadcastd_epi32`]
/// * **Assembly:** `vpbroadcastd zmm, xmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn set_splat_i32_m128i_s_m512i(a: m128i) -> m512i {
    m512i(unsafe { _mm512_broadcastd_epi32(a.0) })
}

/// Sets the lowest `i64` lane of an `m128i` as all lanes of an `m512i`.
/// ```
/// # use safe_arch::*;
/// let a = m128i::from(99_i64 as i128);
/// let b: [i64; 8] = set_splat_i64_m128i_s_m512i(a).into();
/// assert_eq!(b, [99_i64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_broadcastq_epi64`]
/// * **Assembly:** `vpbroadcastq zmm, xmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn set_splat_i64_m128i_s_m512i(a: m128i) -> m512i {
    m512i(unsafe { _mm512_broadcastq_epi64(a.0) })
}

// Conversion operations

/// Convert `i8` values to `i16` values.
/// ```
/// # use safe_arch::*;
/// let a = m256i::from([-5_i8; 32]);
/// let b: [i16; 32] = convert_to_i16_m512i_from_i8_m256i(a).into();
/// assert_eq!(b, [-5_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtepi8_epi16`]
/// * **Assembly:** `vpmovsxbw zmm, ymm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn convert_to_i16_m512i_from_i8_m256i(a: m256i) -> m512i {
  m512i(unsafe { _mm512_cvtepi8_epi16(a.0) })
}

/// Convert `u8` values to `i16` values.
/// ```
/// # use safe_arch::*;
/// let a = m256i::from([5_u8; 32]);
/// let b: [i16; 32] = convert_to_i16_m512i_from_u8_m256i(a).into();
/// assert_eq!(b, [5_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtepu8_epi16`]
/// * **Assembly:** `vpmovzxbw zmm, ymm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn convert_to_i16_m512i_from_u8_m256i(a: m256i) -> m512i {
  m512i(unsafe { _mm512_cvtepu8_epi16(a.0) })
}

/// Convert `u8` values to `u16` values (zero-extend).
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// // 0xFF_u8 → 255 → as u16 still 255
/// let a = m256i::from([0xFFu8 as i8; 32]);
/// let b: [u16; 32] = convert_to_u16_m512i_from_u8_m256i(a).into();
/// assert_eq!(b, [0x00FFu16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtepu8_epi16`]
/// * **Assembly:** `vpmovzxbw zmm, ymm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn convert_to_u16_m512i_from_u8_m256i(a: m256i) -> m512i {
    m512i(unsafe { _mm512_cvtepu8_epi16(a.0) })
}

/// Convert `i16` values to `i32` values.
/// ```
/// # use safe_arch::*;
/// let a = m256i::from([-5_i16; 16]);
/// let b: [i32; 16] = convert_to_i32_m512i_from_i16_m256i(a).into();
/// assert_eq!(b, [-5_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtepi16_epi32`]
/// * **Assembly:** `vpmovsxwd zmm, ymm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn convert_to_i32_m512i_from_i16_m256i(a: m256i) -> m512i {
  m512i(unsafe { _mm512_cvtepi16_epi32(a.0) })
}

/// Convert `u16` values to `u32` values (zero-extend).
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// // 0xFFFFu16 → 65535 → as u32 still 65535
/// let a = m256i::from([0xFFFFu16 as i16; 16]);
/// let b: [u32; 16] = convert_to_u32_m512i_from_u16_m256i(a).into();
/// assert_eq!(b, [0x0000_FFFFu32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtepu16_epi32`]
/// * **Assembly:** `vpmovzxwd zmm, ymm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn convert_to_u32_m512i_from_u16_m256i(a: m256i) -> m512i {
    unsafe { m512i(_mm512_cvtepu16_epi32(a.0)) }
}

/// Convert `i16` values to `i8` values, saturating.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([5_i16; 32]);
/// let b: [i8; 32] = convert_to_i8_m256i_from_i16_m512i(a).into();
/// assert_eq!(b, [5_i8; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtepi16_epi8`]
/// * **Assembly:** `vpmovwb ymm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn convert_to_i8_m256i_from_i16_m512i(a: m512i) -> m256i {
  m256i(unsafe { _mm512_cvtepi16_epi8(a.0) })
}

/// Convert `f64` values to `i64` values.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(5.5);
/// let b: [i64; 8] = convert_to_i64_m512i_from_m512d(a).into();
/// assert_eq!(b, [6_i64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtpd_epi64`]
/// * **Assembly:** `vcvtpd2dq zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn convert_to_i64_m512i_from_m512d(a: m512d) -> m512i {
  m512i(unsafe { _mm512_cvtpd_epi64(a.0) })
}

/// Convert `f32` values to `i32` values.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(5.5);
/// let b: [i32; 16] = convert_to_i32_m512i_from_m512(a).into();
/// assert_eq!(b, [6_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtps_epi32`]
/// * **Assembly:** `vcvtps2dq zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn convert_to_i32_m512i_from_m512(a: m512) -> m512i {
  m512i(unsafe { _mm512_cvtps_epi32(a.0) })
}

/// Convert `f32` values to `i32` values with truncation.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(5.9);
/// let b: [i32; 16] = convert_truncate_m512_i32_m512i(a).into();
/// assert_eq!(b, [5_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_cvttps_epi32`]
/// * **Assembly:** `vcvttps2dq zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn convert_truncate_m512_i32_m512i(a: m512) -> m512i {
  m512i(unsafe { _mm512_cvttps_epi32(a.0) })
}

/// Convert `f64` values to `i64` values with truncation.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(5.9);
/// let b: [i64; 8] = convert_truncate_m512d_i64_m512i(a).into();
/// assert_eq!(b, [5_i64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_cvttpd_epi64`]
/// * **Assembly:** `vcvttps2dq zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn convert_truncate_m512d_i64_m512i(a: m512d) -> m512i {
  m512i(unsafe { _mm512_cvttpd_epi64(a.0) })
}

/// Convert `i32` lanes to `f64` lanes in a 512-bit vector.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// // eight 32-bit integers → eight 64-bit doubles
/// let a = m256i::from([3_i32; 8]);
/// let b: [f64; 8] = convert_to_m512d_from_i32_m256i(a).into();
/// assert_eq!(b, [3.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtepi32_pd`]
/// * **Assembly:** `vcvtdq2pd zmm, ymm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn convert_to_m512d_from_i32_m256i(a: m256i) -> m512d {
    m512d(unsafe { _mm512_cvtepi32_pd(a.0) })
}

/// Convert `i32` lanes to `f32` lanes in a 512-bit vector.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = m512i::from([3_i32; 16]);
/// let b: [f32; 16] = convert_to_m512_from_i32_m512i(a).into();
/// assert_eq!(b, [3.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_cvtepi32_ps`]
/// * **Assembly:** `vcvtdq2ps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn convert_to_m512_from_i32_m512i(a: m512i) -> m512 {
    m512(unsafe { _mm512_cvtepi32_ps(a.0) })
}

// Pack operations

/// Saturating convert `i32` to `i16`, and pack the values.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_i32; 16]);
/// let b = m512i::from([2_i32; 16]);
/// let c: [i16; 32] = pack_i32_to_i16_m512i(a, b).into();
/// assert_eq!(c, [
///   1, 1, 1, 1,
///   2, 2, 2, 2,
///   1, 1, 1, 1,
///   2, 2, 2, 2,
///   1, 1, 1, 1,
///   2, 2, 2, 2,
///   1, 1, 1, 1,
///   2, 2, 2, 2,
/// ]);
/// ```
/// * **Intrinsic:** [`_mm512_packs_epi32`]
/// * **Assembly:** `vpackssdw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn pack_i32_to_i16_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_packs_epi32(a.0, b.0) })
}

/// Saturating convert `i16` to `u8`, and pack the values.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_i16; 32]);
/// let b = m512i::from([2_i16; 32]);
/// let c: [u8; 64] = pack_i16_to_u8_m512i(a, b).into();
/// assert_eq!(c, [
///   1, 1, 1, 1, 1, 1, 1, 1,
///   2, 2, 2, 2, 2, 2, 2, 2,
///   1, 1, 1, 1, 1, 1, 1, 1,
///   2, 2, 2, 2, 2, 2, 2, 2,
///   1, 1, 1, 1, 1, 1, 1, 1,
///   2, 2, 2, 2, 2, 2, 2, 2,
///   1, 1, 1, 1, 1, 1, 1, 1,
///   2, 2, 2, 2, 2, 2, 2, 2
/// ]);
/// ```
/// * **Intrinsic:** [`_mm512_packus_epi16`]
/// * **Assembly:** `vpackuswb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn pack_i16_to_u8_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_packus_epi16(a.0, b.0) })
}

// Unpack operations

/// Unpack and interleave high `i8` lanes of `a` and `b`.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_i8; 64]);
/// let b = m512i::from([2_i8; 64]);
/// let c: [i8; 64] = unpack_high_i8_m512i(a, b).into();
/// // Unpacking happens within each 128-bit lane
/// ```
/// * **Intrinsic:** [`_mm512_unpackhi_epi8`]
/// * **Assembly:** `vpunpckhbw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn unpack_high_i8_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_unpackhi_epi8(a.0, b.0) })
}

/// Unpack and interleave high `i16` lanes of `a` and `b`.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_i16; 32]);
/// let b = m512i::from([2_i16; 32]);
/// let c: [i16; 32] = unpack_high_i16_m512i(a, b).into();
/// // Unpacking happens within each 128-bit lane
/// ```
/// * **Intrinsic:** [`_mm512_unpackhi_epi16`]
/// * **Assembly:** `vpunpckhwd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn unpack_high_i16_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_unpackhi_epi16(a.0, b.0) })
}

/// Unpack and interleave low `i8` lanes of `a` and `b`.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_i8; 64]);
/// let b = m512i::from([2_i8; 64]);
/// let c: [i8; 64] = unpack_low_i8_m512i(a, b).into();
/// // Unpacking happens within each 128-bit lane
/// ```
/// * **Intrinsic:** [`_mm512_unpacklo_epi8`]
/// * **Assembly:** `vpunpcklbw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn unpack_low_i8_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_unpacklo_epi8(a.0, b.0) })
}

/// Unpack and interleave low `i16` lanes of `a` and `b`.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_i16; 32]);
/// let b = m512i::from([2_i16; 32]);
/// let c: [i16; 32] = unpack_low_i16_m512i(a, b).into();
/// // Unpacking happens within each 128-bit lane
/// ```
/// * **Intrinsic:** [`_mm512_unpacklo_epi16`]
/// * **Assembly:** `vpunpcklwd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn unpack_low_i16_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_unpacklo_epi16(a.0, b.0) })
}

/// Unpack and interleave high `i32` lanes of `a` and `b`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = m512i::from([1_i32; 16]);
/// let b = m512i::from([2_i32; 16]);
/// let c: [i32; 16] = unpack_high_i32_m512i(a, b).into();
/// // Unpacking happens within each 128-bit lane
/// ```
/// * **Intrinsic:** [`_mm512_unpackhi_epi32`]
/// * **Assembly:** `vpunpckhdq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn unpack_high_i32_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_unpackhi_epi32(a.0, b.0) })
}

/// Unpack and interleave low `i32` lanes of `a` and `b`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = m512i::from([1_i32; 16]);
/// let b = m512i::from([2_i32; 16]);
/// let c: [i32; 16] = unpack_low_i32_m512i(a, b).into();
/// // Unpacking happens within each 128-bit lane
/// ```
/// * **Intrinsic:** [`_mm512_unpacklo_epi32`]
/// * **Assembly:** `vpunpckldq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn unpack_low_i32_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_unpacklo_epi32(a.0, b.0) })
}

/// Unpack and interleave high `i64` lanes of `a` and `b`.
///
/// Note that this works within each 128-bit lane, so it interleaves the *odd*
/// lanes of the whole register rather than its high half.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = m512i::from([0_i64, 1, 2, 3, 4, 5, 6, 7]);
/// let b = m512i::from([10_i64, 11, 12, 13, 14, 15, 16, 17]);
/// let c: [i64; 8] = unpack_high_i64_m512i(a, b).into();
/// assert_eq!(c, [1, 11, 3, 13, 5, 15, 7, 17]);
/// ```
/// * **Intrinsic:** [`_mm512_unpackhi_epi64`]
/// * **Assembly:** `vpunpckhqdq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn unpack_high_i64_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_unpackhi_epi64(a.0, b.0) })
}

/// Unpack and interleave low `i64` lanes of `a` and `b`.
///
/// Note that this works within each 128-bit lane, so it interleaves the *even*
/// lanes of the whole register rather than its low half.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = m512i::from([0_i64, 1, 2, 3, 4, 5, 6, 7]);
/// let b = m512i::from([10_i64, 11, 12, 13, 14, 15, 16, 17]);
/// let c: [i64; 8] = unpack_low_i64_m512i(a, b).into();
/// assert_eq!(c, [0, 10, 2, 12, 4, 14, 6, 16]);
/// ```
/// * **Intrinsic:** [`_mm512_unpacklo_epi64`]
/// * **Assembly:** `vpunpcklqdq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn unpack_low_i64_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_unpacklo_epi64(a.0, b.0) })
}

// Shift operations

/// Lanewise `u16` shift left by the matching `u16` lane in `count`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(1);
/// let count = set_splat_i16_m512i(2);
/// let b: [u16; 32] = shl_each_u16_m512i(a, count).into();
/// assert_eq!(b, [4_u16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_sllv_epi16`]
/// * **Assembly:** `vpsllvw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn shl_each_u16_m512i(a: m512i, count: m512i) -> m512i {
  m512i(unsafe { _mm512_sllv_epi16(a.0, count.0) })
}

/// Lanewise `u32` shift left by the matching `u32` lane in `count`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(1);
/// let count = set_splat_i32_m512i(2);
/// let b: [u32; 16] = shl_each_u32_m512i(a, count).into();
/// assert_eq!(b, [4_u32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_sllv_epi32`]
/// * **Assembly:** `vpsllvd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shl_each_u32_m512i(a: m512i, count: m512i) -> m512i {
  m512i(unsafe { _mm512_sllv_epi32(a.0, count.0) })
}

/// Lanewise `u64` shift left by the matching `u64` lane in `count`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(1);
/// let count = set_splat_i64_m512i(2);
/// let b: [u64; 8] = shl_each_u64_m512i(a, count).into();
/// assert_eq!(b, [4_u64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_sllv_epi64`]
/// * **Assembly:** `vpdllvd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shl_each_u64_m512i(a: m512i, count: m512i) -> m512i {
  m512i(unsafe { _mm512_sllv_epi64(a.0, count.0) })
}

/// Lanewise logical right shift for `u16` lanes by the matching `u16` count lane.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(0x8000_u16 as i16);
/// let count = set_splat_i16_m512i(15);
/// let b: [u16; 32] = shr_each_u16_m512i(a, count).into();
/// // 0x8000 >> 15 = 1
/// assert_eq!(b, [1_u16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_srlv_epi16`]
/// * **Assembly:** `vpsrlvw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn shr_each_u16_m512i(a: m512i, count: m512i) -> m512i {
    m512i(unsafe { _mm512_srlv_epi16(a.0, count.0) })
}

/// Lanewise logical right shift for `u32` lanes by the matching `u32` count lane.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(0x8000_0000_u32 as i32);
/// let count = set_splat_i32_m512i(31);
/// let b: [u32; 16] = shr_each_u32_m512i(a, count).into();
/// // 0x8000_0000 >> 31 = 1
/// assert_eq!(b, [1_u32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_srlv_epi32`]
/// * **Assembly:** `vpsrlvd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shr_each_u32_m512i(a: m512i, count: m512i) -> m512i {
    m512i(unsafe { _mm512_srlv_epi32(a.0, count.0) })
}

/// Lanewise logical right shift for `u64` lanes by the matching `u64` count lane.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(0x8000_0000_0000_0000_u64 as i64);
/// let count = set_splat_i64_m512i(63);
/// let b: [u64; 8] = shr_each_u64_m512i(a, count).into();
/// // 0x8000_0000_0000_0000 >> 63 = 1
/// assert_eq!(b, [1_u64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_srlv_epi64`]
/// * **Assembly:** `vpsrlvq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shr_each_u64_m512i(a: m512i, count: m512i) -> m512i {
    m512i(unsafe { _mm512_srlv_epi64(a.0, count.0) })
}

// Immediate shifts (same shift for all lanes)

/// Lanewise logical left shift for all `u16` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(1);
/// let b: [u16; 32] = shl_all_u16_m512i(a, 3).into();
/// assert_eq!(b, [8_u16; 32]);
/// ```
/// * **Implementation:** broadcast `count` and call `shl_each_u16_m512i`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn shl_all_u16_m512i(a: m512i, count: u16) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi16(count as i16) });
    shl_each_u16_m512i(a, cnt)
}

/// Lanewise logical left shift for all `i16` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(1);
/// let b: [i16; 32] = shl_all_i16_m512i(a, 3).into();
/// assert_eq!(b, [8_i16; 32]);
/// ```
/// * **Implementation:** broadcast `count` and call [`shl_each_u16_m512i`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn shl_all_i16_m512i(a: m512i, count: u16) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi16(count as i16) });
    shl_each_u16_m512i(a, cnt)
}

/// Lanewise arithmetic right shift for all `i16` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(-4);
/// let b: [i16; 32] = shr_all_i16_m512i(a, 1).into();
/// assert_eq!(b, [-2_i16; 32]);
/// ```
/// * **Implementation:** broadcast `count` and call [`_mm512_srav_epi16`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn shr_all_i16_m512i(a: m512i, count: u16) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi16(count as i16) });
    m512i(unsafe { _mm512_srav_epi16(a.0, cnt.0) })
}

/// Lanewise logical left shift for all `i32` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(1);
/// let b: [i32; 16] = shl_all_i32_m512i(a, 4).into();
/// assert_eq!(b, [16_i32; 16]);
/// ```
/// * **Implementation:** broadcast `count` and call [`shl_each_u32_m512i`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shl_all_i32_m512i(a: m512i, count: u32) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi32(count as i32) });
    shl_each_u32_m512i(a, cnt)
}

/// Lanewise arithmetic right shift for all `i32` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(-16);
/// let b: [i32; 16] = shr_all_i32_m512i(a, 2).into();
/// assert_eq!(b, [-4_i32; 16]);
/// ```
/// * **Implementation:** broadcast `count` and call [`_mm512_srav_epi32`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shr_all_i32_m512i(a: m512i, count: u32) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi32(count as i32) });
    m512i(unsafe { _mm512_srav_epi32(a.0, cnt.0) })
}

/// Lanewise logical left shift for all `i64` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(1);
/// let b: [i64; 8] = shl_all_i64_m512i(a, 5).into();
/// assert_eq!(b, [32_i64; 8]);
/// ```
/// * **Implementation:** broadcast `count` and call [`shl_each_u64_m512i`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shl_all_i64_m512i(a: m512i, count: u64) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi64(count as i64) });
    shl_each_u64_m512i(a, cnt)
}

/// Lanewise arithmetic right shift for all `i64` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(-32);
/// let b: [i64; 8] = shr_all_i64_m512i(a, 3).into();
/// assert_eq!(b, [-4_i64; 8]);
/// ```
/// * **Implementation:** broadcast `count` and call [`_mm512_srav_epi64`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shr_all_i64_m512i(a: m512i, count: u64) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi64(count as i64) });
    m512i(unsafe { _mm512_srav_epi64(a.0, cnt.0) })
}

/// Lanewise logical right shift for all `u16` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(0x8000_u16 as i16);
/// let b: [u16; 32] = shr_all_u16_m512i(a, 15).into();
/// assert_eq!(b, [1_u16; 32]);
/// ```
/// * **Implementation:** broadcast `count` and call `shr_each_u16_m512i`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn shr_all_u16_m512i(a: m512i, count: u16) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi16(count as i16) });
    shr_each_u16_m512i(a, cnt)
}

/// Lanewise logical left shift for all `u32` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(1);
/// let b: [u32; 16] = shl_all_u32_m512i(a, 4).into();
/// assert_eq!(b, [16_u32; 16]);
/// ```
/// * **Implementation:** broadcast `count` and call `shl_each_u32_m512i`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shl_all_u32_m512i(a: m512i, count: u32) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi32(count as i32) });
    shl_each_u32_m512i(a, cnt)
}

/// Lanewise logical right shift for all `u32` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(0x8000_0000_u32 as i32);
/// let b: [u32; 16] = shr_all_u32_m512i(a, 31).into();
/// assert_eq!(b, [1_u32; 16]);
/// ```
/// * **Implementation:** broadcast `count` and call `shr_each_u32_m512i`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shr_all_u32_m512i(a: m512i, count: u32) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi32(count as i32) });
    shr_each_u32_m512i(a, cnt)
}

/// Lanewise logical left shift for all `u64` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(1);
/// let b: [u64; 8] = shl_all_u64_m512i(a, 5).into();
/// assert_eq!(b, [32_u64; 8]);
/// ```
/// * **Implementation:** broadcast `count` and call `shl_each_u64_m512i`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shl_all_u64_m512i(a: m512i, count: u64) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi64(count as i64) });
    shl_each_u64_m512i(a, cnt)
}

/// Lanewise logical right shift for all `u64` lanes by the same runtime count.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(0x8000_0000_0000_0000_u64 as i64);
/// let b: [u64; 8] = shr_all_u64_m512i(a, 63).into();
/// assert_eq!(b, [1_u64; 8]);
/// ```
/// * **Implementation:** broadcast `count` and call `shr_each_u64_m512i`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shr_all_u64_m512i(a: m512i, count: u64) -> m512i {
    let cnt = m512i(unsafe { _mm512_set1_epi64(count as i64) });
    shr_each_u64_m512i(a, cnt)
}

/// Absolute value of `i8` lanes in a 512-bit integer vector.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(-7);
/// let b: [i8; 64] = abs_i8_m512i(a).into();
/// assert_eq!(b, [7_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_abs_epi8`]
/// * **Assembly:** `vpabsb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn abs_i8_m512i(a: m512i) -> m512i {
    m512i(unsafe { _mm512_abs_epi8(a.0) })
}

/// Absolute value of `i16` lanes in a 512-bit integer vector.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(-1234);
/// let b: [i16; 32] = abs_i16_m512i(a).into();
/// assert_eq!(b, [1234_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_abs_epi16`]
/// * **Assembly:** `vpabsw zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn abs_i16_m512i(a: m512i) -> m512i {
    m512i(unsafe { _mm512_abs_epi16(a.0) })
}

/// Absolute value of `i32` lanes in a 512-bit integer vector.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(-100000);
/// let b: [i32; 16] = abs_i32_m512i(a).into();
/// assert_eq!(b, [100000_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_abs_epi32`]
/// * **Assembly:** `vpabsd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn abs_i32_m512i(a: m512i) -> m512i {
    m512i(unsafe { _mm512_abs_epi32(a.0) })
}

// Extract and insert operations

/// Extracts a 64-bit mask from each of the 64 `i8` lanes’ MSB.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// // build a vector whose lanes are either 0 or –1
/// let a = set_splat_i8_m512i(-1);
/// let m: mmask64 = movepi8_mask_m512i(a);
/// assert_eq!(m, !0u64);
/// ```
/// * **Intrinsic:** [`_mm512_movepi8_mask`]
/// * **Assembly:** `vpmovmb2q k, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn movepi8_mask_m512i(a: m512i) -> mmask64 {
    unsafe { _mm512_movepi8_mask(a.0) }
}

/// Extracts a 32-bit mask from each of the 32 `i16` lanes’ MSB.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(-1);
/// let m: mmask32 = movepi16_mask_m512i(a);
/// assert_eq!(m, !0u32);
/// ```
/// * **Intrinsic:** [`_mm512_movepi16_mask`]
/// * **Assembly:** `vpmovmw2d k, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn movepi16_mask_m512i(a: m512i) -> mmask32 {
    unsafe { _mm512_movepi16_mask(a.0) }
}

/// Extracts a 16-bit mask from each of the 16 `i32` lanes’ MSB.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(-1);
/// let m: mmask16 = movepi32_mask_m512i(a);
/// assert_eq!(m, !0u16);
/// ```
/// * **Intrinsic:** [`_mm512_movepi32_mask`]
/// * **Assembly:** `vpmovmd2w k, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn movepi32_mask_m512i(a: m512i) -> mmask16 {
    unsafe { _mm512_movepi32_mask(a.0) }
}

/// Extracts an 8-bit mask from each of the 8 `i64` lanes’ MSB.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(-1);
/// let m: mmask8 = movepi64_mask_m512i(a);
/// assert_eq!(m, !0u8);
/// ```
/// * **Intrinsic:** [`_mm512_movepi64_mask`]
/// * **Assembly:** `vpmovmq2d k, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn movepi64_mask_m512i(a: m512i) -> mmask8 {
    unsafe { _mm512_movepi64_mask(a.0) }
}

/// Extracts a 16-bit mask from each of the 16 `f32` lanes’ MSB.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// // Build a vector of all -0.0f32 (sign bit set)
/// let a = set_splat_m512(-0.0);
/// let m: mmask16 = movepi32_mask_m512(a);
/// assert_eq!(m, !0u16);
///
/// // And with +0.0 (no sign-bits)
/// let b = set_splat_m512(0.0);
/// let m2: mmask16 = movepi32_mask_m512(b);
/// assert_eq!(m2, 0);
/// ```
/// * **Intrinsic:** [`_mm512_movepi32_mask`]
/// * **Assembly:** `vpmovmd2w k, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn movepi32_mask_m512(a: m512) -> mmask16 {
    let ai: __m512i = unsafe { _mm512_castps_si512(a.0) };
    unsafe { _mm512_movepi32_mask(ai) }
}

/// Extracts an 8-bit mask from each of the 8 `f64` lanes’ MSB.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// // All lanes have the sign bit set (−0.0)
/// let a = set_splat_m512d(-0.0);
/// let m: mmask8 = movepi64_mask_m512d(a);
/// assert_eq!(m, !0u8);
///
/// // All lanes positive zero — no sign bits
/// let b = set_splat_m512d(0.0);
/// let m2: mmask8 = movepi64_mask_m512d(b);
/// assert_eq!(m2, 0);
/// ```
/// * **Intrinsic:** [`_mm512_movepi64_mask`]
/// * **Assembly:** `vpmovmq2d k, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn movepi64_mask_m512d(a: m512d) -> mmask8 {
    let ai: __m512i = unsafe { _mm512_castpd_si512(a.0) };
    unsafe { _mm512_movepi64_mask(ai) }
}

/// Compare only the low `f32` lane according to `OP`, returning a mask (bit 0).
///
/// * Operators are according to the `cmp_op!` macro (pass as a const generic).
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512(2.0);
/// let b = set_splat_m512(1.0);
/// // low lane: 2.0 > 1.0 => bit 0 set; others ignored
/// let m: mmask16 = cmp_op_mask_m512_s::<{ cmp_op!(GreaterThanOrdered) }>(a, b);
/// assert_eq!(m, 0x0001);
/// ```
/// * **Intrinsic:** [`_mm512_mask_cmp_ps_mask`]
/// * **Assembly:** `vcmpps k, zmm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cmp_op_mask_m512_s<const OP: i32>(a: m512, b: m512) -> mmask16 {
  unsafe { _mm512_mask_cmp_ps_mask(0x0001u16, a.0, b.0, OP) }
}

/// Compare only the low `f64` lane according to `OP`, returning a mask (bit 0).
///
/// * Operators are according to the `cmp_op!` macro (pass as a const generic).
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512d(2.0);
/// let b = set_splat_m512d(3.0);
/// // low lane: 2.0 < 3.0 => bit 0 set; others ignored
/// let m: mmask8 = cmp_op_mask_m512d_s::<{ cmp_op!(LessThanOrdered) }>(a, b);
/// assert_eq!(m, 0x01);
/// ```
/// * **Intrinsic:** [`_mm512_mask_cmp_pd_mask`]
/// * **Assembly:** `vcmppd k, zmm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cmp_op_mask_m512d_s<const OP: i32>(a: m512d, b: m512d) -> mmask8 {
  unsafe { _mm512_mask_cmp_pd_mask(0x01u8, a.0, b.0, OP) }
}

/// Multiply `i16` lanes producing `i32` values, horizontal add pairs of `i32`
/// values to produce the final output.
/// ```rust
/// # use safe_arch::*;
/// let a = m512i::from([1_i16, 2, 3, 4, -1, -2, -3, -4, 12, 13, -14, -15, 100, 200, 300, -400, -1, 2, 3, 4, -1, -2, -3, -4, 12, 13, -14, -15, 100, 200, 300, -400]);
/// let b = m512i::from([5_i16, 6, 7, 8, -15, -26, -37, 48, 50, 60, 70, -80, 90, 100, 12, -80, 5, 6, 7, 8, -15, -26, -37, 48, 50, 60, 70, -80, 90, 100, 12, -80]);
/// let c: [i32; 16] = mul_i16_horizontal_add_m512i(a, b).into();
/// assert_eq!(c, [17, 53, 67, -81, 1380, 220, 29000, 35600, 7, 53, 67, -81, 1380, 220, 29000, 35600]);
/// ```
/// * **Intrinsic:** [`_mm512_madd_epi16`]
/// * **Assembly:** `vpmaddwd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn mul_i16_horizontal_add_m512i(a: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_madd_epi16(a.0, b.0) })
}

/// Low-lane add: result lane 0 = `a0 + b0`, other lanes unchanged.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512(1.0);
/// let b = set_splat_m512(2.0);
/// let out: [f32; 16] = add_m512_s(a, b).into();
/// assert_eq!(out, [3.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
///                   1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_add_ps`] (merge to `a`)
/// * **Assembly:** `vaddps zmm{k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn add_m512_s(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_mask_add_ps(a.0, 0x0001u16, a.0, b.0) })
}

/// Low-lane add for `f64`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512d(1.0);
/// let b = set_splat_m512d(2.0);
/// let out: [f64; 8] = add_m512d_s(a, b).into();
/// assert_eq!(out, [3.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_add_pd`] (merge to `a`)
/// * **Assembly:** `vaddpd zmm{k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn add_m512d_s(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_mask_add_pd(a.0, 0x01u8, a.0, b.0) })
}

/// Low-lane subtract: result lane 0 = `a0 - b0`, other lanes unchanged.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512(3.0);
/// let b = set_splat_m512(1.0);
/// let out: [f32; 16] = sub_m512_s(a, b).into();
/// assert_eq!(out, [2.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0,
///                   3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_sub_ps`] (merge to `a`)
/// * **Assembly:** `vsubps zmm{k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sub_m512_s(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_mask_sub_ps(a.0, 0x0001u16, a.0, b.0) })
}

/// Low-lane subtract for `f64`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512d(3.0);
/// let b = set_splat_m512d(1.0);
/// let out: [f64; 8] = sub_m512d_s(a, b).into();
/// assert_eq!(out, [2.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_sub_pd`] (merge to `a`)
/// * **Assembly:** `vsubpd zmm{k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sub_m512d_s(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_mask_sub_pd(a.0, 0x01u8, a.0, b.0) })
}

/// Low-lane multiply: result lane 0 = `a0 * b0`, other lanes unchanged.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512(2.0);
/// let b = set_splat_m512(4.0);
/// let out: [f32; 16] = mul_m512_s(a, b).into();
/// assert_eq!(out, [8.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0,
///                   2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_mul_ps`] (merge to `a`)
/// * **Assembly:** `vmulps zmm{k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn mul_m512_s(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_mask_mul_ps(a.0, 0x0001u16, a.0, b.0) })
}

/// Low-lane multiply for `f64`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512d(2.0);
/// let b = set_splat_m512d(4.0);
/// let out: [f64; 8] = mul_m512d_s(a, b).into();
/// assert_eq!(out, [8.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_mul_pd`] (merge to `a`)
/// * **Assembly:** `vmulpd zmm{k}, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn mul_m512d_s(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_mask_mul_pd(a.0, 0x01u8, a.0, b.0) })
}

/// Extract 256-bit integer from `a` at the specified index.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_i64, 2, 3, 4, 5, 6, 7, 8]);
/// let b: [i64; 4] = extract_m256i_from_m512i::<0>(a).into();
/// assert_eq!(b, [1, 2, 3, 4]);
/// let c: [i64; 4] = extract_m256i_from_m512i::<1>(a).into();
/// assert_eq!(c, [5, 6, 7, 8]);
/// ```
/// * **Intrinsic:** [`_mm512_extracti64x4_epi64`]
/// * **Assembly:** `vextracti64x4 ymm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn extract_m256i_from_m512i<const LANE: i32>(a: m512i) -> m256i {
    m256i(unsafe { _mm512_extracti64x4_epi64(a.0, LANE) })
}

/// Extract 256-bit float from `a` at the specified index.
/// ```
/// # use safe_arch::*;
/// let a = m512::from([1.0_f32, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
///                     9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]);
/// let b: [f32; 8] = extract_m256_from_m512::<0>(a).into();
/// assert_eq!(b, [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]);
/// let c: [f32; 8] = extract_m256_from_m512::<1>(a).into();
/// assert_eq!(c, [9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]);
/// ```
/// * **Intrinsic:** [`_mm512_extractf32x8_ps`]
/// * **Assembly:** `vextractf32x8 ymm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn extract_m256_from_m512<const LANE: i32>(a: m512) -> m256 {
    m256(unsafe { _mm512_extractf32x8_ps(a.0, LANE) })
}

/// Extract 256-bit double-precision float from `a` at the specified index.
/// ```
/// # use safe_arch::*;
/// let a = m512d::from([1.0_f64, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]);
/// let b: [f64; 4] = extract_m256d_from_m512d::<0>(a).into();
/// assert_eq!(b, [1.0, 2.0, 3.0, 4.0]);
/// let c: [f64; 4] = extract_m256d_from_m512d::<1>(a).into();
/// assert_eq!(c, [5.0, 6.0, 7.0, 8.0]);
/// ```
/// * **Intrinsic:** [`_mm512_extractf64x4_pd`]
/// * **Assembly:** `vextractf64x4 ymm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn extract_m256d_from_m512d<const LANE: i32>(a: m512d) -> m256d {
    m256d(unsafe { _mm512_extractf64x4_pd(a.0, LANE) })
}

/// Extracts a 256-bit integer vector of eight `i32` lanes from `a` at the specified index.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = m512i::from([
///     1_i32, 2, 3, 4,     // low half
///     5, 6, 7, 8,         // low half
///     9, 10, 11, 12,      // high half
///     13, 14, 15, 16,     // high half
/// ]);
/// let lo: [i32; 8] = extract_m256i32_from_m512i::<0>(a).into();
/// assert_eq!(lo, [1, 2, 3, 4, 5, 6, 7, 8]);
/// let hi: [i32; 8] = extract_m256i32_from_m512i::<1>(a).into();
/// assert_eq!(hi, [9, 10, 11, 12, 13, 14, 15, 16]);
/// ```
/// * **Intrinsic:** [`_mm512_extracti32x8_epi32`]
/// * **Assembly:** `vextracti32x8 ymm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn extract_m256i32_from_m512i<const LANE: i32>(a: m512i) -> m256i {
    m256i(unsafe { _mm512_extracti32x8_epi32(a.0, LANE) })
}

/// Inserts a 256-bit integer vector of eight `i32` lanes `b` into `a` at the specified index.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = m512i::from([1_i32; 16]);
/// let b = m256i::from([10_i32, 11, 12, 13, 14, 15, 16, 17]);
/// let c: [i32; 16] = insert_m256i32_to_m512i::<1>(a, b).into();
/// // low half unchanged, high half replaced by `b`
/// assert_eq!(c, [1,1,1,1,1,1,1,1,10,11,12,13,14,15,16,17]);
/// ```
/// * **Intrinsic:** [`_mm512_inserti32x8`]
/// * **Assembly:** `vinserti32x8 zmm, zmm, ymm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512dq")))]
pub fn insert_m256i32_to_m512i<const LANE: i32>(a: m512i, b: m256i) -> m512i {
    m512i(unsafe { _mm512_inserti32x8(a.0, b.0, LANE) })
}

/// Insert 256-bit integer into `a` at the specified index.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_i64, 2, 3, 4, 5, 6, 7, 8]);
/// let b = m256i::from([10_i64, 11, 12, 13]);
/// let c: [i64; 8] = insert_m256i_to_m512i::<1>(a, b).into();
/// assert_eq!(c, [1, 2, 3, 4, 10, 11, 12, 13]);
/// ```
/// * **Intrinsic:** [`_mm512_inserti64x4`]
/// * **Assembly:** `vinserti64x4 zmm, zmm, ymm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn insert_m256i_to_m512i<const LANE: i32>(a: m512i, b: m256i) -> m512i {
    m512i(unsafe { _mm512_inserti64x4(a.0, b.0, LANE) })
}

/// Insert 256-bit single-precision float into `a` at the specified index.
/// ```
/// # use safe_arch::*;
/// let a = m512::from([1.0_f32, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
///                     9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]);
/// let b = m256::from([100.0, 101.0, 102.0, 103.0, 104.0, 105.0, 106.0, 107.0]);
/// let c: [f32; 16] = insert_m256_to_m512::<1>(a, b).into();
/// assert_eq!(c[8..], [100.0, 101.0, 102.0, 103.0, 104.0, 105.0, 106.0, 107.0]);
/// ```
/// * **Intrinsic:** [`_mm512_insertf32x8`]
/// * **Assembly:** `vinsertf32x8 zmm, zmm, ymm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn insert_m256_to_m512<const LANE: i32>(a: m512, b: m256) -> m512 {
    m512(unsafe { _mm512_insertf32x8(a.0, b.0, LANE) })
}

/// Insert 256-bit double-precision float into `a` at the specified index.
/// ```
/// # use safe_arch::*;
/// let a = m512d::from([1.0_f64, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]);
/// let b = m256d::from([10.0, 11.0, 12.0, 13.0]);
/// let c: [f64; 8] = insert_m256d_to_m512d::<1>(a, b).into();
/// assert_eq!(c, [1.0, 2.0, 3.0, 4.0, 10.0, 11.0, 12.0, 13.0]);
/// ```
/// * **Intrinsic:** [`_mm512_insertf64x4`]
/// * **Assembly:** `vinsertf64x4 zmm, zmm, ymm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn insert_m256d_to_m512d<const LANE: i32>(a: m512d, b: m256d) -> m512d {
    m512d(unsafe { _mm512_insertf64x4(a.0, b.0, LANE) })
}

// Cast operations

/// Expand a `__mmask16` into a full-width `__m512` mask vector for `f32` lanes.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let full = maskz_mov_f32_m512(!0u16);
/// assert_eq!(full.to_bits(), [u32::MAX; 16]);
/// let none = maskz_mov_f32_m512(0);
/// assert_eq!(none, set_splat_m512(0.0));
/// ```
/// * **Intrinsic:** `_mm512_maskz_mov_ps`
/// * **Assembly:** `VMOVDQU32 zmm{dest}{mask}{z}, zmmones`
#[must_use]
#[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn maskz_mov_f32_m512(mask: mmask16) -> m512 {
    let ones: __m512 = unsafe { _mm512_castsi512_ps(_mm512_set1_epi32(-1)) };
    m512(unsafe { _mm512_maskz_mov_ps(mask, ones) })
}

/// Expand a `__mmask16` into a full-width `__m512d` mask vector for `f64` lanes.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let full = maskz_mov_f64_m512d(!0u8);
/// assert_eq!(full.to_bits(), [u64::MAX; 8]);
/// let none = maskz_mov_f64_m512d(0);
/// assert_eq!(none, set_splat_m512d(0.0));
/// ```
/// * **Intrinsic:** `_mm512_maskz_mov_pd`
/// * **Assembly:** `VMOVDQU64 zmm{dest}{mask}{z}, zmmones`
#[must_use]
#[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn maskz_mov_f64_m512d(mask: mmask8) -> m512d {
    let ones: __m512d = unsafe { _mm512_castsi512_pd(_mm512_set1_epi64(-1)) };
    m512d(unsafe { _mm512_maskz_mov_pd(mask, ones) })
}

/// Expand a `mmask8` into a full-width `__m512i` mask vector for 8 lanes of `i64`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let full = maskz_mov_i64_m512i(!0u8);
/// assert_eq!(full, set_splat_i64_m512i(-1));
/// let none = maskz_mov_i64_m512i(0);
/// assert_eq!(none, set_splat_i64_m512i(0));
/// ```
/// * **Intrinsic:** `_mm512_maskz_mov_epi64`
/// * **Assembly:** `VMOVDQU64 zmm{dest}{mask}{z}, zmmones`
#[must_use]
#[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn maskz_mov_i64_m512i(mask: mmask8) -> m512i {
    let ones: __m512i = unsafe { _mm512_set1_epi64(-1) };
    m512i(unsafe { _mm512_maskz_mov_epi64(mask, ones) })
}

/// Expand a `mmask16` into a full-width `__m512i` mask vector for 16 lanes of `i32`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let full = maskz_mov_i32_m512i(!0u16);
/// assert_eq!(full, set_splat_i32_m512i(-1));
/// let none = maskz_mov_i32_m512i(0);
/// assert_eq!(none, set_splat_i32_m512i(0));
/// ```
/// * **Intrinsic:** `_mm512_maskz_mov_epi32`
/// * **Assembly:** `VMOVDQU32 zmm{dest}{mask}{z}, zmmones`
#[must_use]
#[inline(always)]
#[cfg(target_feature = "avx512f")]
pub fn maskz_mov_i32_m512i(mask: mmask16) -> m512i {
    let ones: __m512i = unsafe { _mm512_set1_epi32(-1) };
    m512i(unsafe { _mm512_maskz_mov_epi32(mask, ones) })
}

/// Expand a `mmask32` into a full-width `__m512i` mask vector for 32 lanes of `i16`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let full = maskz_mov_i16_m512i(!0u32);
/// assert_eq!(full.to_array(), [-1_i32; 16]);
/// let none = maskz_mov_i16_m512i(0);
/// assert_eq!(none.to_array(), [0; 16]);
/// ```
/// * **Intrinsic:** `_mm512_maskz_mov_epi16`
/// * **Assembly:** `VMOVDQU16 zmm{dest}{mask}{z}, zmmones`
#[must_use]
#[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn maskz_mov_i16_m512i(mask: mmask32) -> m512i {
    let ones: __m512i = unsafe { _mm512_set1_epi16(-1) };
    m512i(unsafe { _mm512_maskz_mov_epi16(mask, ones) })
}

/// Expand a `mmask64` into a full-width `__m512i` mask vector for 64 lanes of `i8`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let full = maskz_mov_i8_m512i(!0u64);
/// assert_eq!(full, set_splat_i8_m512i(-1));
/// let none = maskz_mov_i8_m512i(0);
/// assert_eq!(none, set_splat_i8_m512i(0));
/// ```
/// * **Intrinsic:** `_mm512_maskz_mov_epi8`
/// * **Assembly:** `VMOVDQU8 zmm{dest}{mask}{z}, zmmones`
#[must_use]
#[inline(always)]
#[cfg(target_feature = "avx512bw")]
pub fn maskz_mov_i8_m512i(mask: mmask64) -> m512i {
    let ones: __m512i = unsafe { _mm512_set1_epi8(-1) };
    m512i(unsafe { _mm512_maskz_mov_epi8(mask, ones) })
}

/// Cast `m256i` to `m512i` (no conversion, upper bits undefined).
/// ```
/// # use safe_arch::*;
/// let a = m256i::from([1_i64; 4]);
/// let b = cast_m256i_to_m512i(a);
/// // Lower 256 bits are preserved, upper 256 bits are undefined
/// ```
/// * **Intrinsic:** [`_mm512_castsi256_si512`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_m256i_to_m512i(a: m256i) -> m512i {
  m512i(unsafe { _mm512_castsi256_si512(a.0) })
}

/// Cast `m256d` to `m512d` (no conversion, upper bits undefined).
/// ```
/// # use safe_arch::*;
/// let a = m256d::from([1.0_f64; 4]);
/// let b = cast_m256d_to_m512d(a);
/// // Lower 256 bits are preserved, upper 256 bits are undefined
/// ```
/// * **Intrinsic:** [`_mm512_castpd256_pd512`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_m256d_to_m512d(a: m256d) -> m512d {
    m512d(unsafe { _mm512_castpd256_pd512(a.0) })
}

/// Cast `m256` to `m512` (no conversion, upper bits undefined).
/// ```
/// # use safe_arch::*;
/// let a = m256::from([1.0_f32; 8]);
/// let b = cast_m256_to_m512(a);
/// // Lower 256 bits are preserved, upper 256 bits are undefined
/// ```
/// * **Intrinsic:** [`_mm512_castps256_ps512`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_m256_to_m512(a: m256) -> m512 {
    m512(unsafe { _mm512_castps256_ps512(a.0) })
}

/// Cast `m512i` to `m256i` (truncate to lower 256 bits).
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([1_i64, 2, 3, 4, 5, 6, 7, 8]);
/// let b: [i64; 4] = cast_m512i_to_m256i(a).into();
/// assert_eq!(b, [1, 2, 3, 4]);
/// ```
/// * **Intrinsic:** [`_mm512_castsi512_si256`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_m512i_to_m256i(a: m512i) -> m256i {
  m256i(unsafe { _mm512_castsi512_si256(a.0) })
}

/// Cast `m512` to `m256` (truncate to lower 256 bits).
/// ```
/// # use safe_arch::*;
/// let a = m512::from([1.0_f32, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
///                     9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]);
/// let b: [f32; 8] = cast_m512_to_m256(a).into();
/// assert_eq!(b, [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]);
/// ```
/// * **Intrinsic:** [`_mm512_castps512_ps256`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_m512_to_m256(a: m512) -> m256 {
    m256(unsafe { _mm512_castps512_ps256(a.0) })
}

/// Cast `m512d` to `m256d` (truncate to lower 256 bits).
/// ```
/// # use safe_arch::*;
/// let a = m512d::from([1.0_f64, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]);
/// let b: [f64; 4] = cast_m512d_to_m256d(a).into();
/// assert_eq!(b, [1.0, 2.0, 3.0, 4.0]);
/// ```
/// * **Intrinsic:** [`_mm512_castpd512_pd256`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_m512d_to_m256d(a: m512d) -> m256d {
    m256d(unsafe { _mm512_castpd512_pd256(a.0) })
}

// Permutation operations

/// Shuffle the 32-bit lanes within each 128-bit chunk of a 512-bit vector.
///
/// This is the AVX-512 version of AVX2’s `_mm256_shuffle_epi32`, operating
/// in four-lane groups inside the ZMM register.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// // [a0,a1,a2,a3,  a4,a5,a6,a7,  …]
/// let a = m512i::from([0,1,2,3,  4,5,6,7,  8,9,10,11, 12,13,14,15]);
/// // IMM = 0b10_11_00_01 = 0xB1
/// //   for each 4-lane chunk pick lanes [1,0,3,2]
/// let c: [i32;16] = shuffle_i32_m512i::<0xB1>(a).into();
/// assert_eq!(&c[0..4], &[1,0,3,2]);
/// assert_eq!(&c[4..8], &[5,4,7,6]);
/// ```
/// * **Intrinsic:** [`_mm512_shuffle_epi32`]
/// * **Assembly:** `vpshufd zmm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shuffle_i32_m512i<const IMM: i32>(a: m512i) -> m512i {
    m512i(unsafe { _mm512_shuffle_epi32(a.0, IMM) })
}

/// Shuffle `i32` values between `a` and `b` using variable indices.
///
/// Each index selects from the concatenation of `a` and `b`: `0..4` picks a
/// lane of `a` and `4..8` picks a lane of `b`. Only the low 3 bits of each
/// index are read.
/// ```
/// # use safe_arch::*;
/// let a = m128i::from([0, 1, 2, 3]);
/// let b = m128i::from([10, 11, 12, 13]);
/// let idx = m128i::from([0, 4, 1, 5]);
/// let c: [i32; 4] = shuffle_abv_i32_all_m128i(a, idx, b).into();
/// assert_eq!(c, [0, 10, 1, 11]);
/// ```
/// * **Intrinsic:** [`_mm_permutex2var_epi32`]
/// * **Assembly:** `vpermt2d xmm, xmm, xmm`
#[must_use]
#[inline(always)]
#[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))))]
pub fn shuffle_abv_i32_all_m128i(a: m128i, idx: m128i, b: m128i) -> m128i {
  m128i(unsafe { _mm_permutex2var_epi32(a.0, idx.0, b.0) })
}

/// Shuffle `i64` values between `a` and `b` using variable indices.
///
/// Each index selects from the concatenation of `a` and `b`: `0..2` picks a
/// lane of `a` and `2..4` picks a lane of `b`. Only the low 2 bits of each
/// index are read.
/// ```
/// # use safe_arch::*;
/// let a = m128i::from([0_i64, 1]);
/// let b = m128i::from([10_i64, 11]);
/// let idx = m128i::from([2_i64, 1]);
/// let c: [i64; 2] = shuffle_abv_i64_all_m128i(a, idx, b).into();
/// assert_eq!(c, [10, 1]);
/// ```
/// * **Intrinsic:** [`_mm_permutex2var_epi64`]
/// * **Assembly:** `vpermt2q xmm, xmm, xmm`
#[must_use]
#[inline(always)]
#[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))))]
pub fn shuffle_abv_i64_all_m128i(a: m128i, idx: m128i, b: m128i) -> m128i {
  m128i(unsafe { _mm_permutex2var_epi64(a.0, idx.0, b.0) })
}

/// Shuffle `i32` values between `a` and `b` using variable indices.
///
/// Each index selects from the concatenation of `a` and `b`: `0..8` picks a
/// lane of `a` and `8..16` picks a lane of `b`. Only the low 4 bits of each
/// index are read.
/// ```
/// # use safe_arch::*;
/// let a = m256i::from([0, 1, 2, 3, 4, 5, 6, 7]);
/// let b = m256i::from([10, 11, 12, 13, 14, 15, 16, 17]);
/// let idx = m256i::from([0, 8, 1, 9, 2, 10, 3, 11]);
/// let c: [i32; 8] = shuffle_abv_i32_all_m256i(a, idx, b).into();
/// assert_eq!(c, [0, 10, 1, 11, 2, 12, 3, 13]);
/// ```
/// * **Intrinsic:** [`_mm256_permutex2var_epi32`]
/// * **Assembly:** `vpermt2d ymm, ymm, ymm`
#[must_use]
#[inline(always)]
#[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))))]
pub fn shuffle_abv_i32_all_m256i(a: m256i, idx: m256i, b: m256i) -> m256i {
  m256i(unsafe { _mm256_permutex2var_epi32(a.0, idx.0, b.0) })
}

/// Shuffle `i64` values between `a` and `b` using variable indices.
///
/// Each index selects from the concatenation of `a` and `b`: `0..4` picks a
/// lane of `a` and `4..8` picks a lane of `b`. Only the low 3 bits of each
/// index are read.
/// ```
/// # use safe_arch::*;
/// let a = m256i::from([0_i64, 1, 2, 3]);
/// let b = m256i::from([10_i64, 11, 12, 13]);
/// let idx = m256i::from([0_i64, 4, 1, 5]);
/// let c: [i64; 4] = shuffle_abv_i64_all_m256i(a, idx, b).into();
/// assert_eq!(c, [0, 10, 1, 11]);
/// ```
/// * **Intrinsic:** [`_mm256_permutex2var_epi64`]
/// * **Assembly:** `vpermt2q ymm, ymm, ymm`
#[must_use]
#[inline(always)]
#[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))))]
pub fn shuffle_abv_i64_all_m256i(a: m256i, idx: m256i, b: m256i) -> m256i {
  m256i(unsafe { _mm256_permutex2var_epi64(a.0, idx.0, b.0) })
}

/// Shuffle `i32` values between `a` and `b` using variable indices.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([0_i32; 16]);
/// let b = m512i::from([16_i32; 16]);
/// let idx = m512i::from([16_i32; 16]); // All select from b[0]
/// let c: [i32; 16] = shuffle_abv_i32_all_m512i(a, idx, b).into();
/// assert_eq!(c, [16_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_permutex2var_epi32`]
/// * **Assembly:** `vpermt2d zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shuffle_abv_i32_all_m512i(a: m512i, idx: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_permutex2var_epi32(a.0, idx.0, b.0) })
}

/// Shuffle `i64` values between `a` and `b` using variable indices.
///
/// Each index selects from the concatenation of `a` and `b`: `0..8` picks a
/// lane of `a` and `8..16` picks a lane of `b`. Only the low 4 bits of each
/// index are read.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([0_i64, 1, 2, 3, 4, 5, 6, 7]);
/// let b = m512i::from([10_i64, 11, 12, 13, 14, 15, 16, 17]);
/// let idx = m512i::from([0_i64, 8, 1, 9, 2, 10, 3, 11]);
/// let c: [i64; 8] = shuffle_abv_i64_all_m512i(a, idx, b).into();
/// assert_eq!(c, [0, 10, 1, 11, 2, 12, 3, 13]);
/// ```
/// * **Intrinsic:** [`_mm512_permutex2var_epi64`]
/// * **Assembly:** `vpermt2q zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn shuffle_abv_i64_all_m512i(a: m512i, idx: m512i, b: m512i) -> m512i {
  m512i(unsafe { _mm512_permutex2var_epi64(a.0, idx.0, b.0) })
}

/// Shuffle `i64` values in `a` using variable indices.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([0_i64, 1, 2, 3, 4, 5, 6, 7]);
/// let idx = m512i::from([7_i64, 6, 5, 4, 3, 2, 1, 0]);
/// let b: [i64; 8] = permute_i64_m512i(idx, a).into();
/// assert_eq!(b, [7, 6, 5, 4, 3, 2, 1, 0]);
/// ```
/// * **Intrinsic:** [`_mm512_permutexvar_epi64`]
/// * **Assembly:** `vpermq zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn permute_i64_m512i(idx: m512i, a: m512i) -> m512i {
  m512i(unsafe { _mm512_permutexvar_epi64(idx.0, a.0) })
}

/// Shuffle `i32` values in `a` using variable indices.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([0_i32, 1, 2, 3, 4, 5, 6, 7,
///                      8, 9, 10, 11, 12, 13, 14, 15]);
/// let idx = m512i::from([15_i32, 14, 13, 12, 11, 10, 9, 8,
///                        7, 6, 5, 4, 3, 2, 1, 0]);
/// let b: [i32; 16] = permute_i32_m512i(idx, a).into();
/// assert_eq!(b, [15, 14, 13, 12, 11, 10, 9, 8,
///                7, 6, 5, 4, 3, 2, 1, 0]);
/// ```
/// * **Intrinsic:** [`_mm512_permutexvar_epi32`]
/// * **Assembly:** `vpermd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn permute_i32_m512i(idx: m512i, a: m512i) -> m512i {
    m512i(unsafe { _mm512_permutexvar_epi32(idx.0, a.0) })
}

/// Shuffle `i8` values in `a` using variable byte indices.
///
/// Each output lane `i` is `a[idx[i] & 63]` (the index is taken modulo 64;
/// out-of-range indices do not zero the lane).
/// ```
/// # use safe_arch::*;
/// # #[cfg(target_arch = "x86")]
/// # use ::core::arch::x86::*;
/// # #[cfg(target_arch = "x86_64")]
/// # use ::core::arch::x86_64::*;
/// let mut table = [0_i8; 64];
/// let mut indices = [0_i8; 64];
/// for i in 0..64 {
///   table[i] = i as i8;
///   indices[i] = (63 - i) as i8;
/// }
/// let a = m512i::from(table);
/// let idx = m512i::from(indices);
/// let b: [i8; 64] = permute_i8_m512i(idx, a).into();
/// for i in 0..64 {
///   assert_eq!(b[i], (63 - i) as i8);
/// }
/// ```
/// * **Intrinsic:** [`_mm512_permutexvar_epi8`]
/// * **Assembly:** `vpermb zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg(target_feature = "avx512vbmi")]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512vbmi")))]
pub fn permute_i8_m512i(idx: m512i, a: m512i) -> m512i {
    m512i(unsafe { _mm512_permutexvar_epi8(idx.0, a.0) })
}

/// Shuffle `i8` values in `a` using variable byte indices.
///
/// Each output lane `i` is `a[idx[i] & 31]` (the index is taken modulo 32;
/// out-of-range indices do not zero the lane).
/// ```
/// # use safe_arch::*;
/// # #[cfg(target_arch = "x86")]
/// # use ::core::arch::x86::*;
/// # #[cfg(target_arch = "x86_64")]
/// # use ::core::arch::x86_64::*;
/// let mut table = [0_i8; 32];
/// let mut indices = [0_i8; 32];
/// for i in 0..32 {
///   table[i] = i as i8;
///   indices[i] = (31 - i) as i8;
/// }
/// let a = m256i::from(table);
/// let idx = m256i::from(indices);
/// let b: [i8; 32] = permute_i8_m256i(idx, a).into();
/// for i in 0..32 {
///   assert_eq!(b[i], (31 - i) as i8);
/// }
/// ```
/// * **Intrinsic:** [`_mm256_permutexvar_epi8`]
/// * **Assembly:** `vpermb ymm, ymm, ymm`
#[must_use]
#[inline(always)]
#[cfg(all(target_feature = "avx512vbmi", target_feature = "avx512vl"))]
#[cfg_attr(docsrs, doc(cfg(all(target_feature = "avx512vbmi", target_feature = "avx512vl"))))]
pub fn permute_i8_m256i(idx: m256i, a: m256i) -> m256i {
    m256i(unsafe { _mm256_permutexvar_epi8(idx.0, a.0) })
}

/// Rounds each lane of a 512-bit vector of double-precision floats (`f64`) according to `OP`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = m512d::from([
///     1.3,  2.7, -1.3, -2.7,
///     3.5, -3.5,  4.1, -4.9,
/// ]);
/// // Round to nearest, suppress exceptions
/// let r_nearest: [f64; 8] = round_m512d::<{ _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC }>(a).into();
/// assert_eq!(r_nearest, [1.0, 3.0, -1.0, -3.0, 4.0, -4.0, 4.0, -5.0]);
///
/// // Round toward zero, suppress exceptions
/// let r_zero: [f64; 8] = round_m512d::<{ _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC }>(a).into();
/// assert_eq!(r_zero, [1.0, 2.0, -1.0, -2.0, 3.0, -3.0, 4.0, -4.0]);
/// ```
/// * **Intrinsic:** [`_mm512_roundscale_pd`]
/// * **Assembly:** `vrndscalepd zmm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn round_m512d<const OP: i32>(a: m512d) -> m512d {
    m512d(unsafe { _mm512_roundscale_pd(a.0, OP) })
}

/// Rounds each lane of a 512-bit vector of single-precision floats (`f32`) according to `OP`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// #[cfg(target_arch = "x86")]
/// use ::core::arch::x86::*;
/// #[cfg(target_arch = "x86_64")]
/// use ::core::arch::x86_64::*;
/// let a = m512::from([
///     1.3,  2.7, -1.3, -2.7,
///     3.5, -3.5,  4.1, -4.9,
///     5.2, -5.2,  6.8, -6.8,
///     7.9, -7.9,  8.4, -8.4,
/// ]);
/// // Round to nearest, suppress exceptions
/// let r_nearest: [f32; 16] = round_m512::<{ _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC }>(a).into();
/// assert_eq!(&r_nearest[0..4], &[1.0, 3.0, -1.0, -3.0]);
///
/// // Round toward zero, suppress exceptions
/// let r_zero: [f32; 16] = round_m512::<{ _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC }>(a).into();
/// assert_eq!(&r_zero[0..4], &[1.0, 2.0, -1.0, -2.0]);
/// ```
/// * **Intrinsic:** [`_mm512_roundscale_ps`]
/// * **Assembly:** `vrndscaleps zmm, zmm, imm8`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn round_m512<const OP: i32>(a: m512) -> m512 {
    m512(unsafe { _mm512_roundscale_ps(a.0, OP) })
}

/// Permute `i32` values from `a` and `b` using index vector `idx`.
/// ```
/// # use safe_arch::*;
/// let a = m512i::from([0_i32, 1, 2, 3, 4, 5, 6, 7,
///                      8, 9, 10, 11, 12, 13, 14, 15]);
/// let b = m512i::from([100, 101, 102, 103, 104, 105, 106, 107,
///                      108, 109, 110, 111, 112, 113, 114, 115]);
/// // Even indices select from `a`, odd indices from `b`
/// let idx = m512i::from([0, 17, 2, 19, 4, 21, 6, 23,
///                        8, 25, 10, 27, 12, 29, 14, 31]);
/// let c: [i32; 16] = permute2_i32_m512i(a, idx, b).into();
/// assert_eq!(c, [0, 101, 2, 103, 4, 105, 6, 107,
///                8, 109, 10, 111, 12, 113, 14, 115]);
/// ```
/// * **Intrinsic:** [`_mm512_permutex2var_epi32`]
/// * **Assembly:** `vpermt2d zmm1, zmm2, zmm3`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512vl,avx512f")))]
pub fn permute2_i32_m512i(a: m512i, idx: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_permutex2var_epi32(a.0, idx.0, b.0) })
}

// Reduction operations

/// Reduce by adding all `f32` lanes together.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(1.0);
/// let sum = reduce_add_m512(a);
/// assert_eq!(sum, 16.0);
/// ```
/// * **Intrinsic:** [`_mm512_reduce_add_ps`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn reduce_add_m512(a: m512) -> f32 {
  unsafe { _mm512_reduce_add_ps(a.0) }
}

/// Reduce by adding all `f64` lanes together.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(1.0);
/// let sum = reduce_add_m512d(a);
/// assert_eq!(sum, 8.0);
/// ```
/// * **Intrinsic:** [`_mm512_reduce_add_pd`]
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn reduce_add_m512d(a: m512d) -> f64 {
    unsafe { _mm512_reduce_add_pd(a.0) }
}

// Max/min operations

/// Lanewise maximum for signed `i8` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(1);
/// let b = set_splat_i8_m512i(5);
/// let c: [i8; 64] = max_i8_m512i(a, b).into();
/// assert_eq!(c, [5_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_max_epi8`]
/// * **Assembly:** `vpmaxsb zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn max_i8_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_max_epi8(a.0, b.0) })
}

/// Lanewise maximum for unsigned `u8` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(1);
/// let b = set_splat_i8_m512i(5);
/// let c: [u8; 64] = max_u8_m512i(a, b).into();
/// assert_eq!(c, [5_u8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_max_epu8`]
/// * **Assembly:** `vpmaxub zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn max_u8_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_max_epu8(a.0, b.0) })
}

/// Lanewise maximum for signed `i16` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(1);
/// let b = set_splat_i16_m512i(5);
/// let c: [i16; 32] = max_i16_m512i(a, b).into();
/// assert_eq!(c, [5_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_max_epi16`]
/// * **Assembly:** `vpmaxsw zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn max_i16_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_max_epi16(a.0, b.0) })
}

/// Lanewise maximum for unsigned `u16` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(1);
/// let b = set_splat_i16_m512i(5);
/// let c: [u16; 32] = max_u16_m512i(a, b).into();
/// assert_eq!(c, [5_u16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_max_epu16`]
/// * **Assembly:** `vpmaxuw zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn max_u16_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_max_epu16(a.0, b.0) })
}

/// Lanewise maximum for signed `i32` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(1);
/// let b = set_splat_i32_m512i(5);
/// let c: [i32; 16] = max_i32_m512i(a, b).into();
/// assert_eq!(c, [5_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_max_epi32`]
/// * **Assembly:** `vpmaxsd zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn max_i32_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_max_epi32(a.0, b.0) })
}

/// Lanewise maximum for unsigned `u32` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(1);
/// let b = set_splat_i32_m512i(5);
/// let c: [u32; 16] = max_u32_m512i(a, b).into();
/// assert_eq!(c, [5_u32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_max_epu32`]
/// * **Assembly:** `vpmaxud zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn max_u32_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_max_epu32(a.0, b.0) })
}

/// Lanewise maximum for signed `i64` lanes.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(-5);
/// let b = set_splat_i64_m512i( 2);
/// let c: [i64; 8] = max_i64_m512i(a, b).into();
/// assert_eq!(c, [2_i64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_max_epi64`] :contentReference[oaicite:0]{index=0}
/// * **Assembly:** `vpmaxsq zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn max_i64_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_max_epi64(a.0, b.0) })
}

/// Lanewise maximum for unsigned `u64` lanes.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(1);
/// let b = set_splat_i64_m512i(5);
/// let c: [u64; 8] = max_u64_m512i(a, b).into();
/// assert_eq!(c, [5_u64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_max_epu64`] :contentReference[oaicite:1]{index=1}
/// * **Assembly:** `vpmaxuq zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn max_u64_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_max_epu64(a.0, b.0) })
}

/// Lanewise minimum for signed `i8` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(1);
/// let b = set_splat_i8_m512i(5);
/// let c: [i8; 64] = min_i8_m512i(a, b).into();
/// assert_eq!(c, [1_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_min_epi8`]
/// * **Assembly:** `vpminsb zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn min_i8_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_min_epi8(a.0, b.0) })
}

/// Lanewise minimum for unsigned `u8` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(1);
/// let b = set_splat_i8_m512i(5);
/// let c: [u8; 64] = min_u8_m512i(a, b).into();
/// assert_eq!(c, [1_u8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_min_epu8`]
/// * **Assembly:** `vpminub zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn min_u8_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_min_epu8(a.0, b.0) })
}

/// Lanewise minimum for signed `i16` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(1);
/// let b = set_splat_i16_m512i(5);
/// let c: [i16; 32] = min_i16_m512i(a, b).into();
/// assert_eq!(c, [1_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_min_epi16`]
/// * **Assembly:** `vpminsw zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn min_i16_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_min_epi16(a.0, b.0) })
}

/// Lanewise minimum for unsigned `u16` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(1);
/// let b = set_splat_i16_m512i(5);
/// let c: [u16; 32] = min_u16_m512i(a, b).into();
/// assert_eq!(c, [1_u16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_min_epu16`]
/// * **Assembly:** `vpminuw zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn min_u16_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_min_epu16(a.0, b.0) })
}

/// Lanewise minimum for signed `i32` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(1);
/// let b = set_splat_i32_m512i(5);
/// let c: [i32; 16] = min_i32_m512i(a, b).into();
/// assert_eq!(c, [1_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_min_epi32`]
/// * **Assembly:** `vpminsd zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn min_i32_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_min_epi32(a.0, b.0) })
}

/// Lanewise minimum for unsigned `u32` lanes.
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(1);
/// let b = set_splat_i32_m512i(5);
/// let c: [u32; 16] = min_u32_m512i(a, b).into();
/// assert_eq!(c, [1_u32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_min_epu32`]
/// * **Assembly:** `vpminud zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn min_u32_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_min_epu32(a.0, b.0) })
}

/// Lanewise minimum for signed `i64` lanes.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(-5);
/// let b = set_splat_i64_m512i( 2);
/// let c: [i64; 8] = min_i64_m512i(a, b).into();
/// assert_eq!(c, [-5_i64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_min_epi64`] :contentReference[oaicite:2]{index=2}
/// * **Assembly:** `vpminsq zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn min_i64_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_min_epi64(a.0, b.0) })
}

/// Lanewise minimum for unsigned `u64` lanes.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(1);
/// let b = set_splat_i64_m512i(5);
/// let c: [u64; 8] = min_u64_m512i(a, b).into();
/// assert_eq!(c, [1_u64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_min_epu64`] :contentReference[oaicite:3]{index=3}
/// * **Assembly:** `vpminuq zmm, zmm, zmm`
#[must_use] #[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn min_u64_m512i(a: m512i, b: m512i) -> m512i {
    m512i(unsafe { _mm512_min_epu64(a.0, b.0) })
}

/// Lanewise `max(a, b)` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(1.0);
/// let b = set_splat_m512(2.0);
/// let c: [f32; 16] = max_m512(a, b).into();
/// assert_eq!(c, [2.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_max_ps`]
/// * **Assembly:** `vmaxps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn max_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_max_ps(a.0, b.0) })
}

/// Lanewise `max(a, b)` with lanes as `f64`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(1.0);
/// let b = set_splat_m512d(2.0);
/// let c: [f64; 8] = max_m512d(a, b).into();
/// assert_eq!(c, [2.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_max_ps`]
/// * **Assembly:** `vmaxpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn max_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_max_pd(a.0, b.0) })
}

/// Lanewise `min(a, b)` with lanes as `f32`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(1.0);
/// let b = set_splat_m512(2.0);
/// let c: [f32; 16] = min_m512(a, b).into();
/// assert_eq!(c, [1.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_min_ps`]
/// * **Assembly:** `vminps zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn min_m512(a: m512, b: m512) -> m512 {
  m512(unsafe { _mm512_min_ps(a.0, b.0) })
}

/// Lanewise `min(a, b)` with lanes as `f64`.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(1.0);
/// let b = set_splat_m512d(2.0);
/// let c: [f64; 8] = min_m512d(a, b).into();
/// assert_eq!(c, [1.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_min_pd`]
/// * **Assembly:** `vminpd zmm, zmm, zmm`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn min_m512d(a: m512d, b: m512d) -> m512d {
  m512d(unsafe { _mm512_min_pd(a.0, b.0) })
}

// Masked load/store operations

/// Load `i8` values from memory using a mask.
/// ```
/// # use safe_arch::*;
/// let src = set_splat_i8_m512i(1);
/// let data = [5_i8; 64];
/// let mask = 0xFFFFFFFFFFFFFFFF;
/// let a: [i8; 64] = load_masked_i8_m512i(src, mask, &data).into();
/// assert_eq!(a, [5_i8; 64]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_loadu_epi8`]
/// * **Assembly:** `vmovdqu8 zmm {k}, m512`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn load_masked_i8_m512i(src: m512i, mask: mmask64, mem_addr: &[i8; 64]) -> m512i {
  m512i(unsafe { _mm512_mask_loadu_epi8(src.0, mask, mem_addr.as_ptr() as *const i8) })
}

/// Load `i16` values from memory using a mask.
/// ```
/// # use safe_arch::*;
/// let src = set_splat_i16_m512i(1);
/// let data = [5_i16; 32];
/// let mask = 0xFFFFFFFF;
/// let a: [i16; 32] = load_masked_i16_m512i(src, mask, &data).into();
/// assert_eq!(a, [5_i16; 32]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_loadu_epi16`]
/// * **Assembly:** `vmovdqu16 zmm {k}, m512`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn load_masked_i16_m512i(src: m512i, mask: mmask32, mem_addr: &[i16; 32]) -> m512i {
  m512i(unsafe { _mm512_mask_loadu_epi16(src.0, mask, mem_addr.as_ptr() as *const i16) })
}

/// Load `i32` values from memory using a mask.
/// ```
/// # use safe_arch::*;
/// let src = set_splat_i32_m512i(1);
/// let data = [5_i32; 16];
/// let mask = 0xFFFF;
/// let a: [i32; 16] = load_masked_i32_m512i(src, mask, &data).into();
/// assert_eq!(a, [5_i32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_loadu_epi32`]
/// * **Assembly:** `vmovdqu32 zmm {k}, m512`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn load_masked_i32_m512i(src: m512i, mask: mmask16, mem_addr: &[i32; 16]) -> m512i {
  m512i(unsafe { _mm512_mask_loadu_epi32(src.0, mask, mem_addr.as_ptr() as *const i32) })
}

/// Load `f32` values from memory using a mask.
/// ```
/// # use safe_arch::*;
/// let src = set_splat_m512(1.0);
/// let data = [5.0_f32; 16];
/// let mask = 0xFFFF;
/// let a: [f32; 16] = load_masked_m512(src, mask, &data).into();
/// assert_eq!(a, [5.0_f32; 16]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_loadu_ps`]
/// * **Assembly:** `vmovups zmm {k}, m512`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn load_masked_m512(src: m512, mask: mmask16, mem_addr: &[f32; 16]) -> m512 {
  m512(unsafe { _mm512_mask_loadu_ps(src.0, mask, mem_addr.as_ptr() as *const f32) })
}

/// Load `f64` values from memory using a mask.
/// ```
/// # use safe_arch::*;
/// let src = set_splat_m512d(1.0);
/// let data = [5.0_f64; 8];
/// let mask = 0xFF;
/// let a: [f64; 8] = load_masked_m512d(src, mask, &data).into();
/// assert_eq!(a, [5.0_f64; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_mask_loadu_pd`]
/// * **Assembly:** `vmovupd zmm {k}, m512`
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn load_masked_m512d(src: m512d, mask: mmask8, mem_addr: &[f64; 8]) -> m512d {
    m512d(unsafe { _mm512_mask_loadu_pd(src.0, mask, mem_addr.as_ptr() as *const f64) })
}

/// Store `i8` values to memory using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i8_m512i(5);
/// let mut mem = [0_i8; 64];
/// let mask = 0xAAAAAAAAAAAAAAAA;
/// store_masked_i8_m512i(&mut mem, mask, a);
/// for (i, &val) in mem.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 5 } else { 0 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_storeu_epi8`]
/// * **Assembly:** `vmovdqu8 m512 {k}, zmm`
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn store_masked_i8_m512i(mem_addr: &mut [i8; 64], mask: mmask64, a: m512i) {
  unsafe { _mm512_mask_storeu_epi8(mem_addr.as_mut_ptr() as *mut i8, mask, a.0) }
}

/// Store `i16` values to memory using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i16_m512i(5);
/// let mut mem = [0_i16; 32];
/// let mask = 0xAAAAAAAA;
/// store_masked_i16_m512i(&mut mem, mask, a);
/// for (i, &val) in mem.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 5 } else { 0 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_storeu_epi16`]
/// * **Assembly:** `vmovdqu16 m512 {k}, zmm`
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512bw")))]
pub fn store_masked_i16_m512i(mem_addr: &mut [i16; 32], mask: mmask32, a: m512i) {
  unsafe { _mm512_mask_storeu_epi16(mem_addr.as_mut_ptr() as *mut i16, mask, a.0) }
}

/// Store `i32` values to memory using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(5);
/// let mut mem = [0_i32; 16];
/// let mask = 0xAAAA;
/// store_masked_i32_m512i(&mut mem, mask, a);
/// for (i, &val) in mem.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 5 } else { 0 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_storeu_epi32`]
/// * **Assembly:** `vmovdqu32 m512 {k}, zmm`
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn store_masked_i32_m512i(mem_addr: &mut [i32; 16], mask: mmask16, a: m512i) {
  unsafe { _mm512_mask_storeu_epi32(mem_addr.as_mut_ptr() as *mut i32, mask, a.0) }
}

/// Store `f32` values to memory using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(5.0);
/// let mut mem = [0.0_f32; 16];
/// let mask = 0xAAAA;
/// store_masked_m512(&mut mem, mask, a);
/// for (i, &val) in mem.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 5.0 } else { 0.0 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_storeu_ps`]
/// * **Assembly:** `vmovups m512 {k}, zmm`
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn store_masked_m512(mem_addr: &mut [f32; 16], mask: mmask16, a: m512) {
  unsafe { _mm512_mask_storeu_ps(mem_addr.as_mut_ptr() as *mut f32, mask, a.0) }
}

/// Store `f64` values to memory using a mask.
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(5.0);
/// let mut mem = [0.0_f64; 8];
/// let mask = 0b10101010;
/// store_masked_m512d(&mut mem, mask, a);
/// for (i, &val) in mem.iter().enumerate() {
///   assert_eq!(val, if (mask >> i) & 1 == 1 { 5.0 } else { 0.0 });
/// }
/// ```
/// * **Intrinsic:** [`_mm512_mask_storeu_pd`]
/// * **Assembly:** `vmovupd m512 {k}, zmm`
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn store_masked_m512d(mem_addr: &mut [f64; 8], mask: mmask8, a: m512d) {
    unsafe { _mm512_mask_storeu_pd(mem_addr.as_mut_ptr() as *mut f64, mask, a.0) }
}

/// Lanewise `sqrt` on `f64` lanes.
/// ```
/// # use safe_arch::*;
/// let input = m512d::from([1.0_f64, 4.0, 9.0, 16.0, 25.0, 36.0, 49.0, 64.0]);
/// let output: [f64; 8] = sqrt_m512d(input).into();
/// assert_eq!(output, [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]);
/// ```
/// * **Intrinsic:** [`_mm512_sqrt_pd`]
/// * **Assembly:**
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sqrt_m512d(a: m512d) -> m512d {
  m512d(unsafe { _mm512_sqrt_pd(a.0) })
}

/// Lanewise `sqrt` on `f32` lanes.
/// ```
/// # use safe_arch::*;
/// let input = m512::from([1.0_f32, 4.0, 9.0, 16.0, 25.0, 36.0, 49.0, 64.0,
///                         81.0, 100.0, 121.0, 144.0, 169.0, 196.0, 225.0, 256.0]);
/// let output: [f32; 16] = sqrt_m512(input).into();
/// assert_eq!(output, [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
///                     9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]);
/// ```
/// * **Intrinsic:** [`_mm512_sqrt_ps`]
/// * **Assembly:**
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn sqrt_m512(a: m512) -> m512 {
  m512(unsafe { _mm512_sqrt_ps(a.0) })
}

/// Cast from `m512i` to `m512` (reinterpret bits).
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(0x3F800000_i32); // 1.0f32 in bits
/// let b = cast_to_m512_from_m512i(a);
/// let arr: [f32; 16] = b.into();
/// assert_eq!(arr[0], 1.0_f32);
/// ```
/// * **Intrinsic:** [`_mm512_castsi512_ps`]
/// * **Assembly:** (no-op, just reinterpretation)
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_to_m512_from_m512i(a: m512i) -> m512 {
  unsafe { m512(_mm512_castsi512_ps(a.0)) }
}

/// Cast from `m512i` to `m512d` (reinterpret bits).
/// ```
/// # use safe_arch::*;
/// let a = set_splat_i64_m512i(0x3FF0000000000000_i64); // 1.0f64 in bits
/// let b = cast_to_m512d_from_m512i(a);
/// let arr: [f64; 8] = b.into();
/// assert_eq!(arr[0], 1.0_f64);
/// ```
/// * **Intrinsic:** [`_mm512_castsi512_pd`]
/// * **Assembly:** (no-op, just reinterpretation)
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_to_m512d_from_m512i(a: m512i) -> m512d {
  unsafe { m512d(_mm512_castsi512_pd(a.0)) }
}

/// Cast from `m512` to `m512i` (reinterpret bits).
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(1.0_f32);
/// let b = cast_to_m512i_from_m512(a);
/// let arr: [i32; 16] = b.into();
/// assert_eq!(arr[0], 0x3F800000_i32);
/// ```
/// * **Intrinsic:** [`_mm512_castps_si512`]
/// * **Assembly:** (no-op, just reinterpretation)
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_to_m512i_from_m512(a: m512) -> m512i {
  unsafe { m512i(_mm512_castps_si512(a.0)) }
}

/// Cast from `m512d` to `m512i` (reinterpret bits).
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(1.0_f64);
/// let b = cast_to_m512i_from_m512d(a);
/// let arr: [i64; 8] = b.into();
/// assert_eq!(arr[0], 0x3FF0000000000000_i64);
/// ```
/// * **Intrinsic:** [`_mm512_castpd_si512`]
/// * **Assembly:** (no-op, just reinterpretation)
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_to_m512i_from_m512d(a: m512d) -> m512i {
  unsafe { m512i(_mm512_castpd_si512(a.0)) }
}

/// Cast from `m512` to `m512d` (reinterpret bits).
/// Note: This does NOT convert float values to double values!
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512(1.0_f32);
/// let b = cast_to_m512d_from_m512(a);
/// // b now contains garbage values, not 1.0_f64!
/// ```
/// * **Intrinsic:** [`_mm512_castps_pd`]
/// * **Assembly:** (no-op, just reinterpretation)
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_to_m512d_from_m512(a: m512) -> m512d {
  unsafe { m512d(_mm512_castps_pd(a.0)) }
}

/// Cast from `m512d` to `m512` (reinterpret bits).
/// Note: This does NOT convert double values to float values!
/// ```
/// # use safe_arch::*;
/// let a = set_splat_m512d(1.0_f64);
/// let b = cast_to_m512_from_m512d(a);
/// // b now contains garbage values, not 1.0_f32!
/// ```
/// * **Intrinsic:** [`_mm512_castpd_ps`]
/// * **Assembly:** (no-op, just reinterpretation)
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_to_m512_from_m512d(a: m512d) -> m512 {
  unsafe { m512(_mm512_castpd_ps(a.0)) }
}

/// Bit-preserving cast to `m256` from `m512`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512(3.25);
/// let lo: [f32; 8] = cast_to_m256_from_m512(a).into();
/// assert_eq!(lo, [3.25_f32; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_castps512_ps256`]
/// * **Assembly:** *(none – no-op cast)*
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_to_m256_from_m512(a: m512) -> m256 {
  m256(unsafe { _mm512_castps512_ps256(a.0) })
}

/// Bit-preserving cast to `m256d` from `m512d`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_m512d(-1.5);
/// let lo: [f64; 4] = cast_to_m256d_from_m512d(a).into();
/// assert_eq!(lo, [-1.5_f64; 4]);
/// ```
/// * **Intrinsic:** [`_mm512_castpd512_pd256`]
/// * **Assembly:** *(none – no-op cast)*
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_to_m256d_from_m512d(a: m512d) -> m256d {
  m256d(unsafe { _mm512_castpd512_pd256(a.0) })
}

/// Bit-preserving cast to `m256i` from `m512i`.
///
/// # Examples
/// ```rust
/// # use safe_arch::*;
/// let a = set_splat_i32_m512i(42);
/// let lo: [i32; 8] = cast_to_m256i_from_m512i(a).into();
/// assert_eq!(lo, [42_i32; 8]);
/// ```
/// * **Intrinsic:** [`_mm512_castsi512_si256`]
/// * **Assembly:** *(none – no-op cast)*
#[must_use]
#[inline(always)]
#[cfg_attr(docsrs, doc(cfg(target_feature = "avx512f")))]
pub fn cast_to_m256i_from_m512i(a: m512i) -> m256i {
  m256i(unsafe { _mm512_castsi512_si256(a.0) })
}

// m512i implementations
impl Not for m512i {
  type Output = Self;
  /// Not a direct intrinsic, but it's very useful and the implementation is
  /// simple enough.
  ///
  /// Negates the bits by performing an `xor` with an all-1s bit pattern.
  /// ```
  /// # use safe_arch::*;
  /// let a = m512i::from([0_u128, 0, 0, 0]);
  /// let c: [u128; 4] = (!a).into();
  /// assert_eq!(c, [u128::MAX, u128::MAX, u128::MAX, u128::MAX]);
  /// ```
  #[inline(always)]
  fn not(self) -> Self {
    let all_bits = set_splat_i16_m512i(-1);
    self ^ all_bits
  }
}

impl BitAnd for m512i {
  type Output = Self;
  /// ```
  /// # use safe_arch::*;
  /// let a = m512i::from([0_i64, 0, 1, 1, 0, 0, 1, 1]);
  /// let b = m512i::from([0_i64, 1, 0, 1, 0, 1, 0, 1]);
  /// let c: [i64; 8] = (a & b).into();
  /// assert_eq!(c, [0_i64, 0, 0, 1, 0, 0, 0, 1]);
  /// ```
  #[inline(always)]
  fn bitand(self, rhs: Self) -> Self {
    bitand_m512i(self, rhs)
  }
}
impl BitAndAssign for m512i {
  #[inline(always)]
  fn bitand_assign(&mut self, rhs: Self) {
    *self = *self & rhs;
  }
}

impl BitOr for m512i {
  type Output = Self;
  /// ```
  /// # use safe_arch::*;
  /// let a = m512i::from([0_i64, 0, 1, 1, 0, 0, 1, 1]);
  /// let b = m512i::from([0_i64, 1, 0, 1, 0, 1, 0, 1]);
  /// let c: [i64; 8] = (a | b).into();
  /// assert_eq!(c, [0_i64, 1, 1, 1, 0, 1, 1, 1]);
  /// ```
  #[inline(always)]
  fn bitor(self, rhs: Self) -> Self {
    bitor_m512i(self, rhs)
  }
}
impl BitOrAssign for m512i {
  #[inline(always)]
  fn bitor_assign(&mut self, rhs: Self) {
    *self = *self | rhs;
  }
}

impl BitXor for m512i {
  type Output = Self;
  /// ```
  /// # use safe_arch::*;
  /// let a = m512i::from([0_i64, 0, 1, 1, 0, 0, 1, 1]);
  /// let b = m512i::from([0_i64, 1, 0, 1, 0, 1, 0, 1]);
  /// let c: [i64; 8] = (a ^ b).into();
  /// assert_eq!(c, [0_i64, 1, 1, 0, 0, 1, 1, 0]);
  /// ```
  #[inline(always)]
  fn bitxor(self, rhs: Self) -> Self {
    bitxor_m512i(self, rhs)
  }
}
impl BitXorAssign for m512i {
  #[inline(always)]
  fn bitxor_assign(&mut self, rhs: Self) {
    *self = *self ^ rhs;
  }
}

impl PartialEq for m512i {
  #[inline(always)]
  /// ```
  /// # use safe_arch::*;
  /// let a = m512i::from([0_i64, 0, 1, 1, 0, 0, 1, 1]);
  /// let b = m512i::from([0_i64, 1, 0, 1, 0, 1, 0, 1]);
  /// assert_eq!(a, a);
  /// assert_ne!(a, b);
  /// ```
  fn eq(&self, other: &Self) -> bool {
    let mask = cmp_op_mask_i32::<_MM_CMPINT_EQ>(*self, *other);
    mask == 0xFFFF_u16
  }
}

impl Eq for m512i {}

// m512 (f32) implementations
impl Not for m512 {
  type Output = Self;
  /// Bitwise NOT operation on `m512`.
  /// ```
  /// # use safe_arch::*;
  /// let a = m512::from([0.0_f32; 16]);
  /// let c = !a;
  /// // Note: This is a bitwise NOT, not a logical NOT
  /// ```
  #[inline(always)]
  fn not(self) -> Self {
    let all_bits = cast_to_m512_from_m512i(set_splat_i32_m512i(-1));
    self ^ all_bits
  }
}

impl BitAnd for m512 {
  type Output = Self;
  /// Bitwise AND operation on `m512`.
  /// ```
  /// # use safe_arch::*;
  /// let a = m512::from_bits([0xFFFFFFFF_u32; 16]);
  /// let b = m512::from_bits([0x00000000_u32; 16]);
  /// let c = a & b;
  /// assert_eq!(c.to_bits(), [0x00000000_u32; 16]);
  /// ```
  #[inline(always)]
  fn bitand(self, rhs: Self) -> Self {
    bitand_m512(self, rhs)
  }
}
impl BitAndAssign for m512 {
  #[inline(always)]
  fn bitand_assign(&mut self, rhs: Self) {
    *self = *self & rhs;
  }
}

impl BitOr for m512 {
  type Output = Self;
  /// Bitwise OR operation on `m512`.
  /// ```
  /// # use safe_arch::*;
  /// let a = m512::from_bits([0xFFFFFFFF_u32; 16]);
  /// let b = m512::from_bits([0x00000000_u32; 16]);
  /// let c = a | b;
  /// assert_eq!(c.to_bits(), [0xFFFFFFFF_u32; 16]);
  /// ```
  #[inline(always)]
  fn bitor(self, rhs: Self) -> Self {
    bitor_m512(self, rhs)
  }
}
impl BitOrAssign for m512 {
  #[inline(always)]
  fn bitor_assign(&mut self, rhs: Self) {
    *self = *self | rhs;
  }
}

impl BitXor for m512 {
  type Output = Self;
  /// Bitwise XOR operation on `m512`.
  /// ```
  /// # use safe_arch::*;
  /// let a = m512::from_bits([0xFFFFFFFF_u32; 16]);
  /// let b = m512::from_bits([0xFFFFFFFF_u32; 16]);
  /// let c = a ^ b;
  /// assert_eq!(c.to_bits(), [0x00000000_u32; 16]);
  /// ```
  #[inline(always)]
  fn bitxor(self, rhs: Self) -> Self {
    bitxor_m512(self, rhs)
  }
}
impl BitXorAssign for m512 {
  #[inline(always)]
  fn bitxor_assign(&mut self, rhs: Self) {
    *self = *self ^ rhs;
  }
}

impl PartialEq for m512 {
  #[inline(always)]
  /// ```
  /// # use safe_arch::*;
  /// let a = m512::from([1.0_f32; 16]);
  /// let b = m512::from([2.0_f32; 16]);
  /// assert_eq!(a, a);
  /// assert_ne!(a, b);
  /// ```
  fn eq(&self, other: &Self) -> bool {
    let mask = cmp_op_mask_f32::<_MM_CMPINT_EQ>(*self, *other);
    mask == 0xFFFF
  }
}

// m512d (f64) implementations
impl Not for m512d {
  type Output = Self;
  /// Bitwise NOT operation on `m512d`.
  /// ```
  /// # use safe_arch::*;
  /// let a = m512d::from([0.0_f64; 8]);
  /// let c = !a;
  /// // Note: This is a bitwise NOT, not a logical NOT
  /// ```
  #[inline(always)]
  fn not(self) -> Self {
    let all_bits = cast_to_m512d_from_m512i(set_splat_i64_m512i(-1));
    self ^ all_bits
  }
}

impl BitAnd for m512d {
  type Output = Self;
  /// Bitwise AND operation on `m512d`.
  /// ```
  /// # use safe_arch::*;
  /// let a = m512d::from_bits([0xFFFFFFFFFFFFFFFF_u64; 8]);
  /// let b = m512d::from_bits([0x0000000000000000_u64; 8]);
  /// let c = a & b;
  /// assert_eq!(c.to_bits(), [0x0000000000000000_u64; 8]);
  /// ```
  #[inline(always)]
  fn bitand(self, rhs: Self) -> Self {
    bitand_m512d(self, rhs)
  }
}
impl BitAndAssign for m512d {
  #[inline(always)]
  fn bitand_assign(&mut self, rhs: Self) {
    *self = *self & rhs;
  }
}

impl BitOr for m512d {
  type Output = Self;
  /// Bitwise OR operation on `m512d`.
  /// ```
  /// # use safe_arch::*;
  /// let a = m512d::from_bits([0xFFFFFFFFFFFFFFFF_u64; 8]);
  /// let b = m512d::from_bits([0x0000000000000000_u64; 8]);
  /// let c = a | b;
  /// assert_eq!(c.to_bits(), [0xFFFFFFFFFFFFFFFF_u64; 8]);
  /// ```
  #[inline(always)]
  fn bitor(self, rhs: Self) -> Self {
    bitor_m512d(self, rhs)
  }
}
impl BitOrAssign for m512d {
  #[inline(always)]
  fn bitor_assign(&mut self, rhs: Self) {
    *self = *self | rhs;
  }
}

impl BitXor for m512d {
  type Output = Self;
  /// Bitwise XOR operation on `m512d`.
  /// ```
  /// # use safe_arch::*;
  /// let a = m512d::from_bits([0xFFFFFFFFFFFFFFFF_u64; 8]);
  /// let b = m512d::from_bits([0xFFFFFFFFFFFFFFFF_u64; 8]);
  /// let c = a ^ b;
  /// assert_eq!(c.to_bits(), [0x0000000000000000_u64; 8]);
  /// ```
  #[inline(always)]
  fn bitxor(self, rhs: Self) -> Self {
    bitxor_m512d(self, rhs)
  }
}
impl BitXorAssign for m512d {
  #[inline(always)]
  fn bitxor_assign(&mut self, rhs: Self) {
    *self = *self ^ rhs;
  }
}

impl PartialEq for m512d {
  #[inline(always)]
  /// ```
  /// # use safe_arch::*;
  /// let a = m512d::from([1.0_f64; 8]);
  /// let b = m512d::from([2.0_f64; 8]);
  /// assert_eq!(a, a);
  /// assert_ne!(a, b);
  /// ```
  fn eq(&self, other: &Self) -> bool {
    let mask = cmp_op_mask_f64::<_MM_CMPINT_EQ>(*self, *other);
    mask == 0xFF
  }
}
