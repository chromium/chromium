/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

#![cfg(all(feature = "x86", any(target_arch = "x86", target_arch = "x86_64")))]
//! This module provides unsafe ways to do some things
#![allow(clippy::wildcard_imports)]

#[cfg(target_arch = "x86")]
use core::arch::x86::*;
#[cfg(target_arch = "x86_64")]
use core::arch::x86_64::*;
use core::ops::{Add, AddAssign, Mul, MulAssign, Sub};

/// An abstraction of an AVX ymm register that
///allows some things to not look ugly
#[derive(Clone, Copy)]
pub struct YmmRegister {
    /// An AVX register
    pub(crate) mm256: __m256i
}

impl Add for YmmRegister {
    type Output = YmmRegister;

    #[inline]
    fn add(self, rhs: Self) -> Self::Output {
        unsafe {
            return YmmRegister {
                mm256: _mm256_add_epi32(self.mm256, rhs.mm256)
            };
        }
    }
}

impl Add<i32> for YmmRegister {
    type Output = YmmRegister;

    #[inline]
    fn add(self, rhs: i32) -> Self::Output {
        unsafe {
            let tmp = _mm256_set1_epi32(rhs);

            return YmmRegister {
                mm256: _mm256_add_epi32(self.mm256, tmp)
            };
        }
    }
}

impl Sub for YmmRegister {
    type Output = YmmRegister;

    #[inline]
    fn sub(self, rhs: Self) -> Self::Output {
        unsafe {
            return YmmRegister {
                mm256: _mm256_sub_epi32(self.mm256, rhs.mm256)
            };
        }
    }
}

impl AddAssign for YmmRegister {
    #[inline]
    fn add_assign(&mut self, rhs: Self) {
        unsafe {
            self.mm256 = _mm256_add_epi32(self.mm256, rhs.mm256);
        }
    }
}

impl AddAssign<i32> for YmmRegister {
    #[inline]
    fn add_assign(&mut self, rhs: i32) {
        unsafe {
            let tmp = _mm256_set1_epi32(rhs);

            self.mm256 = _mm256_add_epi32(self.mm256, tmp);
        }
    }
}

impl Mul for YmmRegister {
    type Output = YmmRegister;

    #[inline]
    fn mul(self, rhs: Self) -> Self::Output {
        unsafe {
            YmmRegister {
                mm256: _mm256_mullo_epi32(self.mm256, rhs.mm256)
            }
        }
    }
}

impl Mul<i32> for YmmRegister {
    type Output = YmmRegister;

    #[inline]
    fn mul(self, rhs: i32) -> Self::Output {
        unsafe {
            let tmp = _mm256_set1_epi32(rhs);

            YmmRegister {
                mm256: _mm256_mullo_epi32(self.mm256, tmp)
            }
        }
    }
}

impl MulAssign for YmmRegister {
    #[inline]
    fn mul_assign(&mut self, rhs: Self) {
        unsafe {
            self.mm256 = _mm256_mullo_epi32(self.mm256, rhs.mm256);
        }
    }
}

impl MulAssign<i32> for YmmRegister {
    #[inline]
    fn mul_assign(&mut self, rhs: i32) {
        unsafe {
            let tmp = _mm256_set1_epi32(rhs);

            self.mm256 = _mm256_mullo_epi32(self.mm256, tmp);
        }
    }
}

impl MulAssign<__m256i> for YmmRegister {
    #[inline]
    fn mul_assign(&mut self, rhs: __m256i) {
        unsafe {
            self.mm256 = _mm256_mullo_epi32(self.mm256, rhs);
        }
    }
}

type Reg = YmmRegister;

/// Transpose an array of 8 by 8 i32's using avx intrinsics
///
/// This was translated from [here](https://newbedev.com/transpose-an-8x8-float-using-avx-avx2)
#[allow(unused_parens, clippy::too_many_arguments)]
#[target_feature(enable = "avx2")]
#[inline]
pub unsafe fn transpose(
    v0: &mut Reg, v1: &mut Reg, v2: &mut Reg, v3: &mut Reg, v4: &mut Reg, v5: &mut Reg,
    v6: &mut Reg, v7: &mut Reg
) {
    let t0 = _mm256_unpacklo_epi32(v0.mm256, v1.mm256);
    let t1 = _mm256_unpackhi_epi32(v0.mm256, v1.mm256);
    let t2 = _mm256_unpacklo_epi32(v2.mm256, v3.mm256);
    let t3 = _mm256_unpackhi_epi32(v2.mm256, v3.mm256);
    let t4 = _mm256_unpacklo_epi32(v4.mm256, v5.mm256);
    let t5 = _mm256_unpackhi_epi32(v4.mm256, v5.mm256);
    let t6 = _mm256_unpacklo_epi32(v6.mm256, v7.mm256);
    let t7 = _mm256_unpackhi_epi32(v6.mm256, v7.mm256);

    let x0 = _mm256_unpacklo_epi64(t0, t2);
    let x1 = _mm256_unpackhi_epi64(t0, t2);
    let x2 = _mm256_unpacklo_epi64(t1, t3);
    let x3 = _mm256_unpackhi_epi64(t1, t3);
    let x4 = _mm256_unpacklo_epi64(t4, t6);
    let x5 = _mm256_unpackhi_epi64(t4, t6);
    let x6 = _mm256_unpacklo_epi64(t5, t7);
    let x7 = _mm256_unpackhi_epi64(t5, t7);

    v0.mm256 = _mm256_permute2x128_si256::<0x20>(x0, x4);
    v1.mm256 = _mm256_permute2x128_si256::<0x20>(x1, x5);
    v2.mm256 = _mm256_permute2x128_si256::<0x20>(x2, x6);
    v3.mm256 = _mm256_permute2x128_si256::<0x20>(x3, x7);
    v4.mm256 = _mm256_permute2x128_si256::<0x31>(x0, x4);
    v5.mm256 = _mm256_permute2x128_si256::<0x31>(x1, x5);
    v6.mm256 = _mm256_permute2x128_si256::<0x31>(x2, x6);
    v7.mm256 = _mm256_permute2x128_si256::<0x31>(x3, x7);
}
