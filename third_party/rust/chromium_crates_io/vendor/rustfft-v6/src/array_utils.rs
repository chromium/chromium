use crate::common::RadixFactor;
use crate::Complex;
use crate::FftNum;
use std::ops::{Deref, DerefMut};

/// Given an array of size width * height, representing a flattened 2D array,
/// transpose the rows and columns of that 2D array into the output
/// benchmarking shows that loop tiling isn't effective for small arrays (in the range of 50x50 or smaller)
pub unsafe fn transpose_small<T: Copy>(width: usize, height: usize, input: &[T], output: &mut [T]) {
    for x in 0..width {
        for y in 0..height {
            let input_index = x + y * width;
            let output_index = y + x * height;

            *output.get_unchecked_mut(output_index) = *input.get_unchecked(input_index);
        }
    }
}

#[allow(unused)]
pub unsafe fn workaround_transmute<T, U>(slice: &[T]) -> &[U] {
    let ptr = slice.as_ptr() as *const U;
    let len = slice.len();
    std::slice::from_raw_parts(ptr, len)
}
#[allow(unused)]
pub unsafe fn workaround_transmute_mut<T, U>(slice: &mut [T]) -> &mut [U] {
    let ptr = slice.as_mut_ptr() as *mut U;
    let len = slice.len();
    std::slice::from_raw_parts_mut(ptr, len)
}

pub(crate) trait LoadStore<T: FftNum>: DerefMut {
    unsafe fn load(&self, idx: usize) -> Complex<T>;
    unsafe fn store(&mut self, val: Complex<T>, idx: usize);
}

impl<T: FftNum> LoadStore<T> for &mut [Complex<T>] {
    #[inline(always)]
    unsafe fn load(&self, idx: usize) -> Complex<T> {
        debug_assert!(idx < self.len());
        *self.get_unchecked(idx)
    }
    #[inline(always)]
    unsafe fn store(&mut self, val: Complex<T>, idx: usize) {
        debug_assert!(idx < self.len());
        *self.get_unchecked_mut(idx) = val;
    }
}
impl<T: FftNum, const N: usize> LoadStore<T> for &mut [Complex<T>; N] {
    #[inline(always)]
    unsafe fn load(&self, idx: usize) -> Complex<T> {
        debug_assert!(idx < self.len());
        *self.get_unchecked(idx)
    }
    #[inline(always)]
    unsafe fn store(&mut self, val: Complex<T>, idx: usize) {
        debug_assert!(idx < self.len());
        *self.get_unchecked_mut(idx) = val;
    }
}

pub(crate) struct DoubleBuf<'a, T> {
    pub input: &'a [Complex<T>],
    pub output: &'a mut [Complex<T>],
}
impl<'a, T> Deref for DoubleBuf<'a, T> {
    type Target = [Complex<T>];
    fn deref(&self) -> &Self::Target {
        self.input
    }
}
impl<'a, T> DerefMut for DoubleBuf<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.output
    }
}
impl<'a, T: FftNum> LoadStore<T> for DoubleBuf<'a, T> {
    #[inline(always)]
    unsafe fn load(&self, idx: usize) -> Complex<T> {
        debug_assert!(idx < self.input.len());
        *self.input.get_unchecked(idx)
    }
    #[inline(always)]
    unsafe fn store(&mut self, val: Complex<T>, idx: usize) {
        debug_assert!(idx < self.output.len());
        *self.output.get_unchecked_mut(idx) = val;
    }
}

pub(crate) trait Load<T: FftNum>: Deref {
    unsafe fn load(&self, idx: usize) -> Complex<T>;
}

impl<T: FftNum> Load<T> for &[Complex<T>] {
    #[inline(always)]
    unsafe fn load(&self, idx: usize) -> Complex<T> {
        debug_assert!(idx < self.len());
        *self.get_unchecked(idx)
    }
}
impl<T: FftNum, const N: usize> Load<T> for &[Complex<T>; N] {
    #[inline(always)]
    unsafe fn load(&self, idx: usize) -> Complex<T> {
        debug_assert!(idx < self.len());
        *self.get_unchecked(idx)
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::random_signal;
    use num_complex::Complex;
    use num_traits::Zero;

    #[test]
    fn test_transpose() {
        let sizes: Vec<usize> = (1..16).collect();

        for &width in &sizes {
            for &height in &sizes {
                let len = width * height;

                let input: Vec<Complex<f32>> = random_signal(len);
                let mut output = vec![Zero::zero(); len];

                unsafe { transpose_small(width, height, &input, &mut output) };

                for x in 0..width {
                    for y in 0..height {
                        assert_eq!(
                            input[x + y * width],
                            output[y + x * height],
                            "x = {}, y = {}",
                            x,
                            y
                        );
                    }
                }
            }
        }
    }
}

// A utility that validates the following conditions, then calls chunk_fn() on each chunk of buffer. Passes the entire scratch buffer with each call.
// - buffer1.len() % chunk_size == 0
// - scratch.len() >= required_scratch
// Returns Ok(()) if the validation passed, Err(()) if there was a problem
// Since this is duplicated into every FFT algorithm we provide, this is tuned to reduce code size as much as possible, with a secondary focus being on ease of implementation
pub fn validate_and_iter<T>(
    mut buffer: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    mut chunk_fn: impl FnMut(&mut [T], &mut [T]),
) -> Result<(), ()> {
    if scratch.len() < required_scratch {
        return Err(());
    }
    let scratch = &mut scratch[..required_scratch];

    // Now that we know the two slices are the same length, loop over each one, splicing off chunk_size at a time, and calling chunk_fn on each
    while buffer.len() >= chunk_size {
        let (head, tail) = buffer.split_at_mut(chunk_size);
        buffer = tail;

        chunk_fn(head, scratch);
    }

    // We have a remainder if there's still data in the buffers -- in which case we want to indicate to the caller that there was an unwanted remainder
    if buffer.len() == 0 {
        Ok(())
    } else {
        Err(())
    }
}

// A utility that validates that buffer1.len() % chunk_size == 0, then calls chunk_fn() on each chunk of buffer.
// This version does 2x partial unrolling of the buffer, because most SIMD butterfly algorithms operate that way.
// Returns Ok(()) if the validation passed, Err(()) if there was a problem
pub fn validate_and_iter_unroll2x<T>(
    mut buffer: &mut [T],
    chunk_size: usize,
    mut chunk2x_fn: impl FnMut(&mut [T]),
    mut chunk_fn: impl FnMut(&mut [T]),
) -> Result<(), ()> {
    // Now that we know the two slices are the same length, loop over each one, splicing off chunk_size at a time, and calling chunk_fn on each
    while buffer.len() >= chunk_size * 2 {
        let (head, tail) = buffer.split_at_mut(chunk_size * 2);
        buffer = tail;

        chunk2x_fn(head);
    }

    if buffer.len() == chunk_size {
        chunk_fn(buffer);
        Ok(())
    } else if buffer.len() == 0 {
        Ok(())
    } else {
        Err(())
    }
}

// A utility that validates the following conditions, then calls chunk_fn() on each chunk of buffer1 and buffer 2 zipped together. Passes the entire scratch buffer with each call.
// - buffer1.len() == buffer2.len()
// - buffer1.len() % chunk_size == 0
// - scratch.len() >= required_scratch
// Returns Ok(()) if the validation passed, Err(()) if there was a problem
// Since this is duplicated into every FFT algorithm we provide, this is tuned to reduce code size as much as possible, with a secondary focus being on ease of implementation
pub fn validate_and_zip<T>(
    mut buffer1: &[T],
    mut buffer2: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    mut chunk_fn: impl FnMut(&[T], &mut [T], &mut [T]),
) -> Result<(), ()> {
    if scratch.len() < required_scratch {
        return Err(());
    }
    let scratch = &mut scratch[..required_scratch];

    if buffer1.len() != buffer2.len() {
        return Err(());
    }

    // Now that we know the two slices are the same length, loop over each one, splicing off chunk_size at a time, and calling chunk_fn on each
    while buffer1.len() >= chunk_size {
        let (head1, tail1) = buffer1.split_at(chunk_size);
        buffer1 = tail1;

        let (head2, tail2) = buffer2.split_at_mut(chunk_size);
        buffer2 = tail2;

        chunk_fn(head1, head2, scratch);
    }

    // We have a remainder if the 2 chunks were uneven to start with, or if there's still data in the buffers -- in which case we want to indicate to the caller that there was an unwanted remainder
    if buffer1.len() == 0 {
        Ok(())
    } else {
        Err(())
    }
}

// A utility that validates the following conditions, then calls chunk_fn() on each chunk of buffer1 and buffer 2 zipped together. Passes the entire scratch buffer with each call.
// - buffer1.len() == buffer2.len()
// - buffer1.len() % chunk_size == 0
// Returns Ok(()) if the validation passed, Err(()) if there was a problem
// This version does 2x partial unrolling of the buffer, because most SIMD butterfly algorithms operate that way.
// Since this is duplicated into every FFT algorithm we provide, this is tuned to reduce code size as much as possible, with a secondary focus being on ease of implementation
pub fn validate_and_zip_unroll2x<T>(
    mut buffer1: &[T],
    mut buffer2: &mut [T],
    chunk_size: usize,
    mut chunk2x_fn: impl FnMut(&[T], &mut [T]),
    mut chunk_fn: impl FnMut(&[T], &mut [T]),
) -> Result<(), ()> {
    if buffer1.len() != buffer2.len() {
        return Err(());
    }

    // Now that we know the two slices are the same length, loop over each one, splicing off chunk_size at a time, and calling chunk_fn on each
    while buffer1.len() >= chunk_size * 2 {
        let (head1, tail1) = buffer1.split_at(chunk_size * 2);
        buffer1 = tail1;

        let (head2, tail2) = buffer2.split_at_mut(chunk_size * 2);
        buffer2 = tail2;

        chunk2x_fn(head1, head2);
    }

    // We have a remainder if the 2 chunks were uneven to start with, or if there's still data in the buffers -- in which case we want to indicate to the caller that there was an unwanted remainder
    if buffer1.len() == chunk_size {
        chunk_fn(buffer1, buffer2);
        Ok(())
    } else if buffer1.len() == 0 {
        Ok(())
    } else {
        Err(())
    }
}

// A utility that validates the following conditions, then calls chunk_fn() on each chunk of buffer1 and buffer 2 zipped together. Passes the entire scratch buffer with each call.
// - buffer1.len() == buffer2.len()
// - buffer1.len() % chunk_size == 0
// - scratch.len() >= required_scratch
// Returns Ok(()) if the validation passed, Err(()) if there was a problem
// Since this is duplicated into every FFT algorithm we provide, this is tuned to reduce code size as much as possible, with a secondary focus being on ease of implementation
pub fn validate_and_zip_mut<T>(
    mut buffer1: &mut [T],
    mut buffer2: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    mut chunk_fn: impl FnMut(&mut [T], &mut [T], &mut [T]),
) -> Result<(), ()> {
    if scratch.len() < required_scratch {
        return Err(());
    }
    let scratch = &mut scratch[..required_scratch];

    if buffer1.len() != buffer2.len() {
        return Err(());
    }

    // Now that we know the two slices are the same length, loop over each one, splicing off chunk_size at a time, and calling chunk_fn on each
    while buffer1.len() >= chunk_size {
        let (head1, tail1) = buffer1.split_at_mut(chunk_size);
        buffer1 = tail1;

        let (head2, tail2) = buffer2.split_at_mut(chunk_size);
        buffer2 = tail2;

        chunk_fn(head1, head2, scratch);
    }

    // We have a remainder if the 2 chunks were uneven to start with, or if there's still data in the buffers -- in which case we want to indicate to the caller that there was an unwanted remainder
    if buffer1.len() == 0 {
        Ok(())
    } else {
        Err(())
    }
}

// A utility that validates the following conditions, then calls chunk_fn() on each chunk of buffer1 and buffer 2 zipped together. Passes the entire scratch buffer with each call.
// - buffer1.len() == buffer2.len()
// - buffer1.len() % chunk_size == 0
// Returns Ok(()) if the validation passed, Err(()) if there was a problem
// This version does 2x partial unrolling of the buffer, because most SIMD butterfly algorithms operate that way.
// Since this is duplicated into every FFT algorithm we provide, this is tuned to reduce code size as much as possible, with a secondary focus being on ease of implementation
pub fn validate_and_zip_mut_unroll2x<T>(
    mut buffer1: &mut [T],
    mut buffer2: &mut [T],
    chunk_size: usize,
    mut chunk2x_fn: impl FnMut(&mut [T], &mut [T]),
    mut chunk_fn: impl FnMut(&mut [T], &mut [T]),
) -> Result<(), ()> {
    if buffer1.len() != buffer2.len() {
        return Err(());
    }

    // Now that we know the two slices are the same length, loop over each one, splicing off chunk_size at a time, and calling chunk_fn on each
    while buffer1.len() >= chunk_size * 2 {
        let (head1, tail1) = buffer1.split_at_mut(chunk_size * 2);
        buffer1 = tail1;

        let (head2, tail2) = buffer2.split_at_mut(chunk_size * 2);
        buffer2 = tail2;

        chunk2x_fn(head1, head2);
    }

    // We have a remainder if the 2 chunks were uneven to start with, or if there's still data in the buffers -- in which case we want to indicate to the caller that there was an unwanted remainder
    if buffer1.len() == chunk_size {
        chunk_fn(buffer1, buffer2);
        Ok(())
    } else if buffer1.len() == 0 {
        Ok(())
    } else {
        Err(())
    }
}

// Utility to help reorder data as a part of computing RadixD FFTs. Conceputally, it works like a transpose, but with the column indexes bit-reversed.
// Use a lookup table to avoid repeating the slow bit reverse operations.
// Unrolling the outer loop by a factor D helps speed things up.
// const parameter D (for Divisor) determines the divisor to use for the "bit reverse", and how much to unroll. `input.len() / height` must be a power of D.
pub fn bitreversed_transpose<T: Copy, const D: usize>(
    height: usize,
    input: &[T],
    output: &mut [T],
) {
    let width = input.len() / height;

    // Let's make sure the arguments are ok
    assert!(D > 1 && input.len() % height == 0 && input.len() == output.len());

    let strided_width = width / D;
    let rev_digits = if D.is_power_of_two() {
        let width_bits = width.trailing_zeros();
        let d_bits = D.trailing_zeros();

        // verify that width is a power of d
        assert!(width_bits % d_bits == 0);
        width_bits / d_bits
    } else {
        compute_logarithm::<D>(width).unwrap()
    };

    for x in 0..strided_width {
        let mut i = 0;
        let x_fwd = [(); D].map(|_| {
            let value = D * x + i;
            i += 1;
            value
        }); // If we had access to rustc 1.63, we could use std::array::from_fn instead
        let x_rev = x_fwd.map(|x| reverse_bits::<D>(x, rev_digits));

        // Assert that the the bit reversed indices will not exceed the length of the output.
        // The highest index the loop reaches is: (x_rev[n] + 1)*height - 1
        // The last element of the data is at index: width*height - 1
        // Thus it is sufficient to assert that x_rev[n]<width.
        for r in x_rev {
            assert!(r < width);
        }
        for y in 0..height {
            for (fwd, rev) in x_fwd.iter().zip(x_rev.iter()) {
                let input_index = *fwd + y * width;
                let output_index = y + *rev * height;

                unsafe {
                    let temp = *input.get_unchecked(input_index);
                    *output.get_unchecked_mut(output_index) = temp;
                }
            }
        }
    }
}

// Repeatedly divide `value` by divisor `D`, `iters` times, and apply the remainders to a new value
// When D is a power of 2, this is exactly equal (implementation and assembly)-wise to a bit reversal
// When D is not a power of 2, think of this function as a logical equivalent to a bit reversal
pub fn reverse_bits<const D: usize>(value: usize, rev_digits: u32) -> usize {
    assert!(D > 1);

    let mut result: usize = 0;
    let mut value = value;
    for _ in 0..rev_digits {
        result = (result * D) + (value % D);
        value = value / D;
    }
    result
}

// computes `n` such that `D ^ n == value`. Returns `None` if `value` is not a perfect power of `D`, otherwise returns `Some(n)`
pub fn compute_logarithm<const D: usize>(value: usize) -> Option<u32> {
    if value == 0 || D < 2 {
        return None;
    }

    let mut current_exponent = 0;
    let mut current_value = value;

    while current_value % D == 0 {
        current_exponent += 1;
        current_value /= D;
    }

    if current_value == 1 {
        Some(current_exponent)
    } else {
        None
    }
}

pub(crate) struct TransposeFactor {
    pub factor: RadixFactor,
    pub count: u8,
}

// Utility to help reorder data as a part of computing RadixD FFTs. Conceputally, it works like a transpose, but with the column indexes bit-reversed.
// Use a lookup table to avoid repeating the slow bit reverse operations.
// Unrolling the outer loop by a factor D helps speed things up.
// const parameter D (for Divisor) determines how much to unroll. `input.len() / height` must divisible by D.
pub(crate) fn factor_transpose<T: Copy, const D: usize>(
    height: usize,
    input: &[T],
    output: &mut [T],
    factors: &[TransposeFactor],
) {
    let width = input.len() / height;

    // Let's make sure the arguments are ok
    assert!(width % D == 0 && D > 1 && input.len() % width == 0 && input.len() == output.len());

    let strided_width = width / D;
    for x in 0..strided_width {
        let mut i = 0;
        let x_fwd = [(); D].map(|_| {
            let value = D * x + i;
            i += 1;
            value
        }); // If we had access to rustc 1.63, we could use std::array::from_fn instead
        let x_rev = x_fwd.map(|x| reverse_remainders(x, factors));

        // Assert that the the bit reversed indices will not exceed the length of the output.
        // The highest index the loop reaches is: (x_rev[n] + 1)*height - 1
        // The last element of the data is at index: width*height - 1
        // Thus it is sufficient to assert that x_rev[n]<width.
        for r in x_rev {
            assert!(r < width);
        }
        for y in 0..height {
            for (fwd, rev) in x_fwd.iter().zip(x_rev.iter()) {
                let input_index = *fwd + y * width;
                let output_index = y + *rev * height;

                unsafe {
                    let temp = *input.get_unchecked(input_index);
                    *output.get_unchecked_mut(output_index) = temp;
                }
            }
        }
    }
}

// Divide `value` by the provided array of factors, and push the remainders into a new number
// When all of the provided factors are 2, this is exactly equal to a bit reversal
// When some of the factors are not 2, think of this as a "generalization" of a bit reversal, to something like a "Remainder reversal".
pub(crate) fn reverse_remainders(value: usize, factors: &[TransposeFactor]) -> usize {
    let mut result: usize = 0;
    let mut value = value;
    for f in factors.iter() {
        match f.factor {
            RadixFactor::Factor2 => {
                for _ in 0..f.count {
                    result = (result * 2) + (value % 2);
                    value = value / 2;
                }
            }
            RadixFactor::Factor3 => {
                for _ in 0..f.count {
                    result = (result * 3) + (value % 3);
                    value = value / 3;
                }
            }
            RadixFactor::Factor4 => {
                for _ in 0..f.count {
                    result = (result * 4) + (value % 4);
                    value = value / 4;
                }
            }
            RadixFactor::Factor5 => {
                for _ in 0..f.count {
                    result = (result * 5) + (value % 5);
                    value = value / 5;
                }
            }
            RadixFactor::Factor6 => {
                for _ in 0..f.count {
                    result = (result * 6) + (value % 6);
                    value = value / 6;
                }
            }
            RadixFactor::Factor7 => {
                for _ in 0..f.count {
                    result = (result * 7) + (value % 7);
                    value = value / 7;
                }
            }
        }
    }
    result
}
