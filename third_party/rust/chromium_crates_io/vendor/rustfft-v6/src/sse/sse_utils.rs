use core::arch::x86_64::*;

//  __  __       _   _               _________  _     _ _
// |  \/  | __ _| |_| |__           |___ /___ \| |__ (_) |_
// | |\/| |/ _` | __| '_ \   _____    |_ \ __) | '_ \| | __|
// | |  | | (_| | |_| | | | |_____|  ___) / __/| |_) | | |_
// |_|  |_|\__,_|\__|_| |_|         |____/_____|_.__/|_|\__|
//

pub struct Rotate90F32 {
    //sign_lo: __m128,
    sign_hi: __m128,
    sign_both: __m128,
}

impl Rotate90F32 {
    pub fn new(positive: bool) -> Self {
        // There doesn't seem to be any need for rotating just the first element, but let's keep the code just in case
        //let sign_lo = unsafe {
        //    if positive {
        //        _mm_set_ps(0.0, 0.0, 0.0, -0.0)
        //    }
        //    else {
        //        _mm_set_ps(0.0, 0.0, -0.0, 0.0)
        //    }
        //};
        let sign_hi = unsafe {
            if positive {
                _mm_set_ps(0.0, -0.0, 0.0, 0.0)
            } else {
                _mm_set_ps(-0.0, 0.0, 0.0, 0.0)
            }
        };
        let sign_both = unsafe {
            if positive {
                _mm_set_ps(0.0, -0.0, 0.0, -0.0)
            } else {
                _mm_set_ps(-0.0, 0.0, -0.0, 0.0)
            }
        };
        Self {
            //sign_lo,
            sign_hi,
            sign_both,
        }
    }

    #[inline(always)]
    pub unsafe fn rotate_hi(&self, values: __m128) -> __m128 {
        let temp = _mm_shuffle_ps(values, values, 0xB4);
        _mm_xor_ps(temp, self.sign_hi)
    }

    // There doesn't seem to be any need for rotating just the first element, but let's keep the code just in case
    //#[inline(always)]
    //pub unsafe fn rotate_lo(&self, values: __m128) -> __m128 {
    //    let temp = _mm_shuffle_ps(values, values, 0xE1);
    //    _mm_xor_ps(temp, self.sign_lo)
    //}

    #[inline(always)]
    pub unsafe fn rotate_both(&self, values: __m128) -> __m128 {
        let temp = _mm_shuffle_ps(values, values, 0xB1);
        _mm_xor_ps(temp, self.sign_both)
    }

    #[inline(always)]
    pub unsafe fn rotate_both_45(&self, values: __m128) -> __m128 {
        let rotated = self.rotate_both(values);
        let sum = _mm_add_ps(rotated, values);
        _mm_mul_ps(sum, _mm_set1_ps(0.5f32.sqrt()))
    }

    #[inline(always)]
    pub unsafe fn rotate_both_135(&self, values: __m128) -> __m128 {
        let rotated = self.rotate_both(values);
        let diff = _mm_sub_ps(rotated, values);
        _mm_mul_ps(diff, _mm_set1_ps(0.5f32.sqrt()))
    }

    #[inline(always)]
    pub unsafe fn rotate_both_225(&self, values: __m128) -> __m128 {
        let rotated = self.rotate_both(values);
        let diff = _mm_add_ps(rotated, values);
        _mm_mul_ps(diff, _mm_set1_ps(-(0.5f32.sqrt())))
    }
}

// Pack low (1st) complex
// left: r1.re, r1.im, r2.re, r2.im
// right: l1.re, l1.im, l2.re, l2.im
// --> r1.re, r1.im, l1.re, l1.im
#[inline(always)]
pub unsafe fn extract_lo_lo_f32(left: __m128, right: __m128) -> __m128 {
    //_mm_shuffle_ps(left, right, 0x44)
    _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(left), _mm_castps_pd(right)))
}

// Pack high (2nd) complex
// left: r1.re, r1.im, r2.re, r2.im
// right: l1.re, l1.im, l2.re, l2.im
// --> r2.re, r2.im, l2.re, l2.im
#[inline(always)]
pub unsafe fn extract_hi_hi_f32(left: __m128, right: __m128) -> __m128 {
    _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(left), _mm_castps_pd(right)))
}

// Pack low (1st) and high (2nd) complex
// left: r1.re, r1.im, r2.re, r2.im
// right: l1.re, l1.im, l2.re, l2.im
// --> r1.re, r1.im, l2.re, l2.im
#[inline(always)]
pub unsafe fn extract_lo_hi_f32(left: __m128, right: __m128) -> __m128 {
    _mm_blend_ps(left, right, 0x0C)
}

// Pack  high (2nd) and low (1st) complex
// left: r1.re, r1.im, r2.re, r2.im
// right: l1.re, l1.im, l2.re, l2.im
// --> r2.re, r2.im, l1.re, l1.im
#[inline(always)]
pub unsafe fn extract_hi_lo_f32(left: __m128, right: __m128) -> __m128 {
    _mm_shuffle_ps(left, right, 0x4E)
}

// Reverse complex
// values: a.re, a.im, b.re, b.im
// --> b.re, b.im, a.re, a.im
#[inline(always)]
pub unsafe fn reverse_complex_elements_f32(values: __m128) -> __m128 {
    _mm_shuffle_ps(values, values, 0x4E)
}

// Invert sign of high (2nd) complex
// values: a.re, a.im, b.re, b.im
// -->  a.re, a.im, -b.re, -b.im
#[inline(always)]
pub unsafe fn negate_hi_f32(values: __m128) -> __m128 {
    _mm_xor_ps(values, _mm_set_ps(-0.0, -0.0, 0.0, 0.0))
}

// Duplicate low (1st) complex
// values: a.re, a.im, b.re, b.im
// --> a.re, a.im, a.re, a.im
#[inline(always)]
pub unsafe fn duplicate_lo_f32(values: __m128) -> __m128 {
    _mm_shuffle_ps(values, values, 0x44)
}

// Duplicate high (2nd) complex
// values: a.re, a.im, b.re, b.im
// --> b.re, b.im, b.re, b.im
#[inline(always)]
pub unsafe fn duplicate_hi_f32(values: __m128) -> __m128 {
    _mm_shuffle_ps(values, values, 0xEE)
}

// transpose a 2x2 complex matrix given as [x0, x1], [x2, x3]
// result is [x0, x2], [x1, x3]
#[inline(always)]
pub unsafe fn transpose_complex_2x2_f32(left: __m128, right: __m128) -> [__m128; 2] {
    let temp02 = extract_lo_lo_f32(left, right);
    let temp13 = extract_hi_hi_f32(left, right);
    [temp02, temp13]
}

//  __  __       _   _                __   _  _   _     _ _
// |  \/  | __ _| |_| |__            / /_ | || | | |__ (_) |_
// | |\/| |/ _` | __| '_ \   _____  | '_ \| || |_| '_ \| | __|
// | |  | | (_| | |_| | | | |_____| | (_) |__   _| |_) | | |_
// |_|  |_|\__,_|\__|_| |_|          \___/   |_| |_.__/|_|\__|
//

pub(crate) struct Rotate90F64 {
    sign: __m128d,
}

impl Rotate90F64 {
    pub fn new(positive: bool) -> Self {
        let sign = unsafe {
            if positive {
                _mm_set_pd(0.0, -0.0)
            } else {
                _mm_set_pd(-0.0, 0.0)
            }
        };
        Self { sign }
    }

    #[inline(always)]
    pub unsafe fn rotate(&self, values: __m128d) -> __m128d {
        let temp = _mm_shuffle_pd(values, values, 0x01);
        _mm_xor_pd(temp, self.sign)
    }

    #[inline(always)]
    pub unsafe fn rotate_45(&self, values: __m128d) -> __m128d {
        let rotated = self.rotate(values);
        let sum = _mm_add_pd(rotated, values);
        _mm_mul_pd(sum, _mm_set1_pd(0.5f64.sqrt()))
    }

    #[inline(always)]
    pub unsafe fn rotate_135(&self, values: __m128d) -> __m128d {
        let rotated = self.rotate(values);
        let diff = _mm_sub_pd(rotated, values);
        _mm_mul_pd(diff, _mm_set1_pd(0.5f64.sqrt()))
    }

    #[inline(always)]
    pub unsafe fn rotate_225(&self, values: __m128d) -> __m128d {
        let rotated = self.rotate(values);
        let diff = _mm_add_pd(rotated, values);
        _mm_mul_pd(diff, _mm_set1_pd(-(0.5f64.sqrt())))
    }
}

#[cfg(test)]
mod unit_tests {
    use crate::sse::sse_vector::SseVector;

    use super::*;
    use num_complex::Complex;

    #[test]
    fn test_mul_complex_f64() {
        unsafe {
            let right = _mm_set_pd(1.0, 2.0);
            let left = _mm_set_pd(5.0, 7.0);
            let res = SseVector::mul_complex(left, right);
            let expected = _mm_set_pd(2.0 * 5.0 + 1.0 * 7.0, 2.0 * 7.0 - 1.0 * 5.0);
            assert_eq!(
                std::mem::transmute::<__m128d, Complex<f64>>(res),
                std::mem::transmute::<__m128d, Complex<f64>>(expected)
            );
        }
    }

    #[test]
    fn test_mul_complex_f32() {
        unsafe {
            let val1 = Complex::<f32>::new(1.0, 2.5);
            let val2 = Complex::<f32>::new(3.2, 4.2);
            let val3 = Complex::<f32>::new(5.6, 6.2);
            let val4 = Complex::<f32>::new(7.4, 8.3);

            let nbr2 = _mm_set_ps(val4.im, val4.re, val3.im, val3.re);
            let nbr1 = _mm_set_ps(val2.im, val2.re, val1.im, val1.re);
            let res = SseVector::mul_complex(nbr1, nbr2);
            let res = std::mem::transmute::<__m128, [Complex<f32>; 2]>(res);
            let expected = [val1 * val3, val2 * val4];
            assert_eq!(res, expected);
        }
    }

    #[test]
    fn test_pack() {
        unsafe {
            let nbr2 = _mm_set_ps(8.0, 7.0, 6.0, 5.0);
            let nbr1 = _mm_set_ps(4.0, 3.0, 2.0, 1.0);
            let first = extract_lo_lo_f32(nbr1, nbr2);
            let second = extract_hi_hi_f32(nbr1, nbr2);
            let first = std::mem::transmute::<__m128, [Complex<f32>; 2]>(first);
            let second = std::mem::transmute::<__m128, [Complex<f32>; 2]>(second);
            let first_expected = [Complex::new(1.0, 2.0), Complex::new(5.0, 6.0)];
            let second_expected = [Complex::new(3.0, 4.0), Complex::new(7.0, 8.0)];
            assert_eq!(first, first_expected);
            assert_eq!(second, second_expected);
        }
    }
}
