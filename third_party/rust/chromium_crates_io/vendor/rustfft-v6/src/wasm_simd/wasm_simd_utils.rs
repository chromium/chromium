use super::wasm_simd_vector::WasmVector32;
use core::arch::wasm32::*;

//  __  __       _   _               _________  _     _ _
// |  \/  | __ _| |_| |__           |___ /___ \| |__ (_) |_
// | |\/| |/ _` | __| '_ \   _____    |_ \ __) | '_ \| | __|
// | |  | | (_| | |_| | | | |_____|  ___) / __/| |_) | | |_
// |_|  |_|\__,_|\__|_| |_|         |____/_____|_.__/|_|\__|
//

/// Utility functions to rotate complex numbers by 90 degrees
pub struct Rotate90F32 {
    sign_hi: v128,
    sign_both: v128,
}

impl Rotate90F32 {
    pub fn new(positive: bool) -> Self {
        let sign_hi = if positive {
            f32x4(0.0, 0.0, -0.0, 0.0)
        } else {
            f32x4(0.0, 0.0, 0.0, -0.0)
        };
        let sign_both = if positive {
            f32x4(-0.0, 0.0, -0.0, 0.0)
        } else {
            f32x4(0.0, -0.0, 0.0, -0.0)
        };
        Self { sign_hi, sign_both }
    }

    #[inline(always)]
    pub fn rotate_hi(&self, values: v128) -> v128 {
        v128_xor(u32x4_shuffle::<0, 1, 3, 2>(values, values), self.sign_hi)
    }

    #[inline(always)]
    pub unsafe fn rotate_both(&self, values: v128) -> v128 {
        v128_xor(u32x4_shuffle::<1, 0, 3, 2>(values, values), self.sign_both)
    }

    #[inline(always)]
    pub unsafe fn rotate_both_45(&self, values: v128) -> v128 {
        let rotated = self.rotate_both(values);
        let sum = f32x4_add(rotated, values);
        f32x4_mul(sum, f32x4_splat(0.5f32.sqrt()))
    }

    #[inline(always)]
    pub unsafe fn rotate_both_135(&self, values: v128) -> v128 {
        let rotated = self.rotate_both(values);
        let diff = f32x4_sub(rotated, values);
        f32x4_mul(diff, f32x4_splat(0.5f32.sqrt()))
    }

    #[inline(always)]
    pub unsafe fn rotate_both_225(&self, values: v128) -> v128 {
        let rotated = self.rotate_both(values);
        let diff = f32x4_add(rotated, values);
        f32x4_mul(diff, f32x4_splat(-(0.5f32.sqrt())))
    }
}

/// Pack low (1st) complex
/// left: l1.re, l1.im, l2.re, l2.im
/// right: r1.re, r1.im, r2.re, r2.im
/// --> l1.re, l1.im, r1.re, r1.im
#[inline(always)]
pub fn extract_lo_lo_f32(left: WasmVector32, right: WasmVector32) -> WasmVector32 {
    WasmVector32(u32x4_shuffle::<0, 1, 4, 5>(left.0, right.0))
}

/// Pack high (2nd) complex
/// left: l1.re, l1.im, l2.re, l2.im
/// right: r1.re, r1.im, r2.re, r2.im
/// --> l2.re, l2.im, r2.re, r2.im
#[inline(always)]
pub fn extract_hi_hi_f32(left: WasmVector32, right: WasmVector32) -> WasmVector32 {
    WasmVector32(u32x4_shuffle::<2, 3, 6, 7>(left.0, right.0))
}

/// Pack low (1st) and high (2nd) complex
/// left: l1.re, l1.im, l2.re, l2.im
/// right: r1.re, r1.im, r2.re, r2.im
/// --> l1.re, l1.im, r2.re, r2.im
#[inline(always)]
pub fn extract_lo_hi_f32(left: WasmVector32, right: WasmVector32) -> WasmVector32 {
    WasmVector32(u32x4_shuffle::<0, 1, 6, 7>(left.0, right.0))
}

/// Pack high (2nd) and low (1st) complex
/// left: r1.re, r1.im, r2.re, r2.im
/// right: l1.re, l1.im, l2.re, l2.im
/// --> r2.re, r2.im, l1.re, l1.im
#[inline(always)]
pub fn extract_hi_lo_f32(left: WasmVector32, right: WasmVector32) -> WasmVector32 {
    WasmVector32(u32x4_shuffle::<2, 3, 4, 5>(left.0, right.0))
}

/// Pack low (1st) complex
/// left: l1.re, l1.im, l2.re, l2.im
/// right: r1.re, r1.im, r2.re, r2.im
/// --> l1.re, l1.im, r1.re, r1.im
#[inline(always)]
pub fn extract_lo_lo_f32_v128(left: v128, right: v128) -> v128 {
    u32x4_shuffle::<0, 1, 4, 5>(left, right)
}

/// Pack high (2nd) complex
/// left: l1.re, l1.im, l2.re, l2.im
/// right: r1.re, r1.im, r2.re, r2.im
/// --> l2.re, l2.im, r2.re, r2.im
#[inline(always)]
pub fn extract_hi_hi_f32_v128(left: v128, right: v128) -> v128 {
    u32x4_shuffle::<2, 3, 6, 7>(left, right)
}

/// Pack low (1st) and high (2nd) complex
/// left: l1.re, l1.im, l2.re, l2.im
/// right: r1.re, r1.im, r2.re, r2.im
/// --> l1.re, l1.im, r2.re, r2.im
#[inline(always)]
pub fn extract_lo_hi_f32_v128(left: v128, right: v128) -> v128 {
    u32x4_shuffle::<0, 1, 6, 7>(left, right)
}

/// Pack high (2nd) and low (1st) complex
/// left: r1.re, r1.im, r2.re, r2.im
/// right: l1.re, l1.im, l2.re, l2.im
/// --> r2.re, r2.im, l1.re, l1.im
#[inline(always)]
pub fn extract_hi_lo_f32_v128(left: v128, right: v128) -> v128 {
    u32x4_shuffle::<2, 3, 4, 5>(left, right)
}

/// Reverse complex
/// values: a.re, a.im, b.re, b.im
/// --> b.re, b.im, a.re, a.im
#[inline(always)]
pub fn reverse_complex_elements_f32(values: v128) -> v128 {
    u64x2_shuffle::<1, 0>(values, values)
}

/// Reverse complex and then negate hi complex
/// values: a.re, a.im, b.re, b.im
/// --> b.re, b.im, -a.re, -a.im
#[inline(always)]
pub unsafe fn reverse_complex_and_negate_hi_f32(values: v128) -> v128 {
    v128_xor(
        u32x4_shuffle::<2, 3, 0, 1>(values, values),
        f32x4(0.0, 0.0, -0.0, -0.0),
    )
}

// Invert sign of high (2nd) complex
// values: a.re, a.im, b.re, b.im
// -->  a.re, a.im, -b.re, -b.im
//#[inline(always)]
//pub unsafe fn negate_hi_f32(values: float32x4_t) -> float32x4_t {
//    vcombine_f32(vget_low_f32(values), vneg_f32(vget_high_f32(values)))
//}

/// Duplicate low (1st) complex
/// values: a.re, a.im, b.re, b.im
/// --> a.re, a.im, a.re, a.im
#[inline(always)]
pub unsafe fn duplicate_lo_f32(values: v128) -> v128 {
    u64x2_shuffle::<0, 0>(values, values)
}

/// Duplicate high (2nd) complex
/// values: a.re, a.im, b.re, b.im
/// --> b.re, b.im, b.re, b.im
#[inline(always)]
pub unsafe fn duplicate_hi_f32(values: v128) -> v128 {
    u64x2_shuffle::<1, 1>(values, values)
}

/// transpose a 2x2 complex matrix given as [x0, x1], [x2, x3]
/// result is [x0, x2], [x1, x3]
#[inline(always)]
pub unsafe fn transpose_complex_2x2_f32(left: v128, right: v128) -> [v128; 2] {
    let temp02 = extract_lo_lo_f32_v128(left, right);
    let temp13 = extract_hi_hi_f32_v128(left, right);
    [temp02, temp13]
}

/// Complex multiplication.
/// Each input contains two complex values, which are multiplied in parallel.
#[inline(always)]
pub unsafe fn mul_complex_f32(left: v128, right: v128) -> v128 {
    let temp1 = u32x4_shuffle::<0, 4, 2, 6>(right, right);
    let temp2 = u32x4_shuffle::<1, 5, 3, 7>(right, f32x4_neg(right));
    let temp3 = f32x4_mul(temp2, left);
    let temp4 = u32x4_shuffle::<1, 0, 3, 2>(temp3, temp3);
    let temp5 = f32x4_mul(temp1, left);
    f32x4_add(temp4, temp5)
}

//  __  __       _   _                __   _  _   _     _ _
// |  \/  | __ _| |_| |__            / /_ | || | | |__ (_) |_
// | |\/| |/ _` | __| '_ \   _____  | '_ \| || |_| '_ \| | __|
// | |  | | (_| | |_| | | | |_____| | (_) |__   _| |_) | | |_
// |_|  |_|\__,_|\__|_| |_|          \___/   |_| |_.__/|_|\__|
//

/// Utility functions to rotate complex pointers by 90 degrees
pub(crate) struct Rotate90F64 {
    sign: v128,
}

impl Rotate90F64 {
    pub fn new(positive: bool) -> Self {
        let sign = if positive {
            f64x2(-0.0, 0.0)
        } else {
            f64x2(0.0, -0.0)
        };
        Self { sign }
    }

    #[inline(always)]
    pub unsafe fn rotate(&self, values: v128) -> v128 {
        v128_xor(u64x2_shuffle::<1, 0>(values, values), self.sign)
    }

    #[inline(always)]
    pub unsafe fn rotate_45(&self, values: v128) -> v128 {
        let rotated = self.rotate(values);
        let sum = f64x2_add(rotated, values);
        f64x2_mul(sum, f64x2_splat(0.5f64.sqrt()))
    }

    #[inline(always)]
    pub unsafe fn rotate_135(&self, values: v128) -> v128 {
        let rotated = self.rotate(values);
        let diff = f64x2_sub(rotated, values);
        f64x2_mul(diff, f64x2_splat(0.5f64.sqrt()))
    }

    #[inline(always)]
    pub unsafe fn rotate_225(&self, values: v128) -> v128 {
        let rotated = self.rotate(values);
        let diff = f64x2_add(rotated, values);
        f64x2_mul(diff, f64x2_splat(-(0.5f64.sqrt())))
    }
}

#[inline(always)]
pub unsafe fn mul_complex_f64(left: v128, right: v128) -> v128 {
    const NEGATE_LEFT: v128 = f64x2(-0.0, 0.0);
    let temp = v128_xor(u64x2_shuffle::<1, 0>(left, left), NEGATE_LEFT);
    let sum = f64x2_mul(left, u64x2_shuffle::<0, 0>(right, right));
    f64x2_add(sum, f64x2_mul(temp, u64x2_shuffle::<1, 1>(right, right)))
}

#[cfg(test)]
mod unit_tests {
    use super::*;
    use num_complex::Complex;
    use wasm_bindgen_test::wasm_bindgen_test;

    #[wasm_bindgen_test]
    fn test_positive_rotation_f32() {
        unsafe {
            let rotate = Rotate90F32::new(true);
            let input = f32x4(1.0, 2.0, 69.0, 420.0);
            let actual_hi = rotate.rotate_hi(input);
            let expected_hi = f32x4(1.0, 2.0, -420.0, 69.0);
            assert_eq!(
                std::mem::transmute::<v128, [Complex<f32>; 2]>(actual_hi),
                std::mem::transmute::<v128, [Complex<f32>; 2]>(expected_hi)
            );

            let actual = rotate.rotate_both(input);
            let expected = f32x4(-2.0, 1.0, -420.0, 69.0);
            assert_eq!(
                std::mem::transmute::<v128, [Complex<f32>; 2]>(actual),
                std::mem::transmute::<v128, [Complex<f32>; 2]>(expected)
            );
        }
    }

    #[wasm_bindgen_test]
    fn test_negative_rotation_f32() {
        unsafe {
            let rotate = Rotate90F32::new(false);
            let input = f32x4(1.0, 2.0, 69.0, 420.0);
            let actual_hi = rotate.rotate_hi(input);
            let expected_hi = f32x4(1.0, 2.0, 420.0, -69.0);
            assert_eq!(
                std::mem::transmute::<v128, [Complex<f32>; 2]>(actual_hi),
                std::mem::transmute::<v128, [Complex<f32>; 2]>(expected_hi)
            );

            let actual = rotate.rotate_both(input);
            let expected = f32x4(2.0, -1.0, 420.0, -69.0);
            assert_eq!(
                std::mem::transmute::<v128, [Complex<f32>; 2]>(actual),
                std::mem::transmute::<v128, [Complex<f32>; 2]>(expected)
            );
        }
    }

    #[wasm_bindgen_test]
    fn test_negative_rotation_f64() {
        unsafe {
            let rotate = Rotate90F64::new(false);
            let input = f64x2(69.0, 420.0);
            let actual = rotate.rotate(input);
            let expected = f64x2(420.0, -69.0);
            assert_eq!(
                std::mem::transmute::<v128, Complex<f64>>(actual),
                std::mem::transmute::<v128, Complex<f64>>(expected)
            );
        }
    }

    #[wasm_bindgen_test]
    fn test_positive_rotation_f64() {
        unsafe {
            let rotate = Rotate90F64::new(true);
            let input = f64x2(69.0, 420.0);
            let actual = rotate.rotate(input);
            let expected = f64x2(-420.0, 69.0);
            assert_eq!(
                std::mem::transmute::<v128, Complex<f64>>(actual),
                std::mem::transmute::<v128, Complex<f64>>(expected)
            );
        }
    }

    #[wasm_bindgen_test]
    fn test_reverse_complex_number_f32() {
        let input = f32x4(1.0, 5.0, 9.0, 13.0);
        let actual = reverse_complex_elements_f32(input);
        let expected = f32x4(9.0, 13.0, 1.0, 5.0);
        unsafe {
            assert_eq!(
                std::mem::transmute::<v128, [Complex<f32>; 2]>(actual),
                std::mem::transmute::<v128, [Complex<f32>; 2]>(expected)
            );
        }
    }

    #[wasm_bindgen_test]
    fn test_mul_complex_f64() {
        unsafe {
            // let right = vld1q_f64([1.0, 2.0].as_ptr());
            let right = f64x2(1.0, 2.0);
            // let left = vld1q_f64([5.0, 7.0].as_ptr());
            let left = f64x2(5.0, 7.0);
            let res = mul_complex_f64(left, right);
            // let expected = vld1q_f64([1.0 * 5.0 - 2.0 * 7.0, 1.0 * 7.0 + 2.0 * 5.0].as_ptr());
            let expected = f64x2(1.0 * 5.0 - 2.0 * 7.0, 1.0 * 7.0 + 2.0 * 5.0);
            assert_eq!(
                std::mem::transmute::<v128, Complex<f64>>(res),
                std::mem::transmute::<v128, Complex<f64>>(expected)
            );
        }
    }

    #[wasm_bindgen_test]
    fn test_mul_complex_f32() {
        unsafe {
            let val1 = Complex::<f32>::new(1.0, 2.5);
            let val2 = Complex::<f32>::new(3.2, 4.75);
            let val3 = Complex::<f32>::new(5.75, 6.25);
            let val4 = Complex::<f32>::new(7.4, 8.5);

            let nbr2 = v128_load([val3, val4].as_ptr() as *const v128);
            let nbr1 = v128_load([val1, val2].as_ptr() as *const v128);
            let res = mul_complex_f32(nbr1, nbr2);
            let res = std::mem::transmute::<v128, [Complex<f32>; 2]>(res);
            let expected = [val1 * val3, val2 * val4];
            assert_eq!(res, expected);
        }
    }

    #[wasm_bindgen_test]
    fn test_pack() {
        unsafe {
            let nbr2 = f32x4(5.0, 6.0, 7.0, 8.0);
            let nbr1 = f32x4(1.0, 2.0, 3.0, 4.0);
            let first = extract_lo_lo_f32_v128(nbr1, nbr2);
            let second = extract_hi_hi_f32_v128(nbr1, nbr2);
            let first = std::mem::transmute::<v128, [Complex<f32>; 2]>(first);
            let second = std::mem::transmute::<v128, [Complex<f32>; 2]>(second);
            let first_expected = [Complex::new(1.0, 2.0), Complex::new(5.0, 6.0)];
            let second_expected = [Complex::new(3.0, 4.0), Complex::new(7.0, 8.0)];
            assert_eq!(first, first_expected);
            assert_eq!(second, second_expected);
        }
    }
}
