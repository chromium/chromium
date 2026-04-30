use crate::{common::FftNum, FftDirection};
use num_complex::Complex;

use strength_reduce::{StrengthReducedU128, StrengthReducedU64};

pub fn compute_twiddle<T: FftNum>(
    index: usize,
    fft_len: usize,
    direction: FftDirection,
) -> Complex<T> {
    let constant = -2f64 * std::f64::consts::PI / fft_len as f64;
    let angle = constant * index as f64;

    let result = Complex {
        re: T::from_f64(angle.cos()).unwrap(),
        im: T::from_f64(angle.sin()).unwrap(),
    };

    match direction {
        FftDirection::Forward => result,
        FftDirection::Inverse => result.conj(),
    }
}

pub fn fill_bluesteins_twiddles<T: FftNum>(
    destination: &mut [Complex<T>],
    direction: FftDirection,
) {
    let twice_len = destination.len() * 2;

    // Standard bluestein's twiddle computation requires us to square the index before usingit to compute a twiddle factor
    // And since twiddle factors are cyclic, we can improve precision once the squared index gets converted to floating point by taking a modulo
    // Modulo is expensive, so we're going to use strength-reduction to keep it manageable

    // Strength-reduced u128s are very heavy, so we only want to use them if we need them - and we only need them if
    // len * len doesn't fit in a u64, AKA if len doesn't fit in a u32
    if destination.len() < std::u32::MAX as usize {
        let twice_len_reduced = StrengthReducedU64::new(twice_len as u64);

        for (i, e) in destination.iter_mut().enumerate() {
            let i_squared = i as u64 * i as u64;
            let i_mod = i_squared % twice_len_reduced;
            *e = compute_twiddle(i_mod as usize, twice_len, direction);
        }
    } else {
        // Sadly, the len doesn't fit in a u64, so we have to crank it up to u128 arithmetic
        let twice_len_reduced = StrengthReducedU128::new(twice_len as u128);

        for (i, e) in destination.iter_mut().enumerate() {
            // Standard bluestein's twiddle computation requires us to square the index before usingit to compute a twiddle factor
            // And since twiddle factors are cyclic, we can improve precision once the squared index gets converted to floating point by taking a modulo
            let i_squared = i as u128 * i as u128;
            let i_mod = i_squared % twice_len_reduced;
            *e = compute_twiddle(i_mod as usize, twice_len, direction);
        }
    }
}

pub fn rotate_90<T: FftNum>(value: Complex<T>, direction: FftDirection) -> Complex<T> {
    match direction {
        FftDirection::Forward => Complex {
            re: value.im,
            im: -value.re,
        },
        FftDirection::Inverse => Complex {
            re: -value.im,
            im: value.re,
        },
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;

    #[test]
    fn test_rotate() {
        // Verify that the rotate90 function does the same thing as multiplying by twiddle(1,4), in the forward direction
        let value = Complex { re: 9.1, im: 2.2 };
        let rotated_forward = rotate_90(value, FftDirection::Forward);
        let twiddled_forward = value * compute_twiddle(1, 4, FftDirection::Forward);

        assert_eq!(value.re, -rotated_forward.im);
        assert_eq!(value.im, rotated_forward.re);

        assert!(value.re + twiddled_forward.im < 0.0001);
        assert!(value.im - rotated_forward.re < 0.0001);

        // Verify that the rotate90 function does the same thing as multiplying by twiddle(1,4), in the inverse direction
        let rotated_forward = rotate_90(value, FftDirection::Inverse);
        let twiddled_forward = value * compute_twiddle(1, 4, FftDirection::Inverse);

        assert_eq!(value.re, rotated_forward.im);
        assert_eq!(value.im, -rotated_forward.re);

        assert!(value.re - twiddled_forward.im < 0.0001);
        assert!(value.im + rotated_forward.re < 0.0001);
    }
}
