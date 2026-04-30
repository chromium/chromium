use core::arch::aarch64::*;
use num_complex::Complex;
use num_traits::Zero;
use std::fmt::Debug;
use std::ops::{Deref, DerefMut};

use crate::{array_utils::DoubleBuf, twiddles, FftDirection};

use super::NeonNum;

// Read these indexes from an NeonArray and build an array of simd vectors.
// Takes a name of a vector to read from, and a list of indexes to read.
// This statement:
// ```
// let values = read_complex_to_array!(input, {0, 1, 2, 3});
// ```
// is equivalent to:
// ```
// let values = [
//     input.load_complex(0),
//     input.load_complex(1),
//     input.load_complex(2),
//     input.load_complex(3),
// ];
// ```
macro_rules! read_complex_to_array {
    ($input:ident, { $($idx:literal),* }) => {
        [
        $(
            $input.load_complex($idx),
        )*
        ]
    }
}

// Read these indexes from an NeonArray and build an array or partially filled simd vectors.
// Takes a name of a vector to read from, and a list of indexes to read.
// This statement:
// ```
// let values = read_partial1_complex_to_array!(input, {0, 1, 2, 3});
// ```
// is equivalent to:
// ```
// let values = [
//     input.load1_complex(0),
//     input.load1_complex(1),
//     input.load1_complex(2),
//     input.load1_complex(3),
// ];
// ```
macro_rules! read_partial1_complex_to_array {
    ($input:ident, { $($idx:literal),* }) => {
        [
        $(
            $input.load1_complex($idx),
        )*
        ]
    }
}

// Write these indexes of an array of simd vectors to the same indexes of an NeonArray.
// Takes a name of a vector to read from, one to write to, and a list of indexes.
// This statement:
// ```
// let values = write_complex_to_array!(input, output, {0, 1, 2, 3});
// ```
// is equivalent to:
// ```
// let values = [
//     output.store_complex(input[0], 0),
//     output.store_complex(input[1], 1),
//     output.store_complex(input[2], 2),
//     output.store_complex(input[3], 3),
// ];
// ```
macro_rules! write_complex_to_array {
    ($input:ident, $output:ident, { $($idx:literal),* }) => {
        $(
            $output.store_complex($input[$idx], $idx);
        )*
    }
}

// Write the low half of these indexes of an array of simd vectors to the same indexes of an NeonArray.
// Takes a name of a vector to read from, one to write to, and a list of indexes.
// This statement:
// ```
// let values = write_partial_lo_complex_to_array!(input, output, {0, 1, 2, 3});
// ```
// is equivalent to:
// ```
// let values = [
//     output.store_partial_lo_complex(input[0], 0),
//     output.store_partial_lo_complex(input[1], 1),
//     output.store_partial_lo_complex(input[2], 2),
//     output.store_partial_lo_complex(input[3], 3),
// ];
// ```
macro_rules! write_partial_lo_complex_to_array {
    ($input:ident, $output:ident, { $($idx:literal),* }) => {
        $(
            $output.store_partial_lo_complex($input[$idx], $idx);
        )*
    }
}

// Write these indexes of an array of simd vectors to the same indexes, multiplied by a stride, of an NeonArray.
// Takes a name of a vector to read from, one to write to, an integer stride, and a list of indexes.
// This statement:
// ```
// let values = write_complex_to_array_separate!(input, output, {0, 1, 2, 3});
// ```
// is equivalent to:
// ```
// let values = [
//     output.store_complex(input[0], 0),
//     output.store_complex(input[1], 2),
//     output.store_complex(input[2], 4),
//     output.store_complex(input[3], 6),
// ];
// ```
macro_rules! write_complex_to_array_strided {
    ($input:ident, $output:ident, $stride:literal, { $($idx:literal),* }) => {
        $(
            $output.store_complex($input[$idx], $idx*$stride);
        )*
    }
}

#[derive(Copy, Clone)]
pub struct Rotation90<V: NeonVector>(V);

// A trait to hold the BVectorType and COMPLEX_PER_VECTOR associated data
pub trait NeonVector: Copy + Debug + Send + Sync {
    const COMPLEX_PER_VECTOR: usize;

    type ScalarType: NeonNum<VectorType = Self>;

    // loads of complex numbers
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self;
    unsafe fn load_partial_lo_complex(ptr: *const Complex<Self::ScalarType>) -> Self;
    unsafe fn load1_complex(ptr: *const Complex<Self::ScalarType>) -> Self;

    // stores of complex numbers
    unsafe fn store_complex(ptr: *mut Complex<Self::ScalarType>, data: Self);
    unsafe fn store_partial_lo_complex(ptr: *mut Complex<Self::ScalarType>, data: Self);

    // Keep this around even though it's unused - research went into how to do it, keeping it ensures that research doesn't need to be repeated
    #[allow(unused)]
    unsafe fn store_partial_hi_complex(ptr: *mut Complex<Self::ScalarType>, data: Self);

    // math ops
    unsafe fn neg(a: Self) -> Self;
    unsafe fn add(a: Self, b: Self) -> Self;
    unsafe fn mul(a: Self, b: Self) -> Self;
    unsafe fn fmadd(acc: Self, a: Self, b: Self) -> Self;
    unsafe fn nmadd(acc: Self, a: Self, b: Self) -> Self;

    unsafe fn broadcast_scalar(value: Self::ScalarType) -> Self;

    /// Generates a chunk of twiddle factors starting at (X,Y) and incrementing X `COMPLEX_PER_VECTOR` times.
    /// The result will be [twiddle(x*y, len), twiddle((x+1)*y, len), twiddle((x+2)*y, len), ...] for as many complex numbers fit in a vector
    unsafe fn make_mixedradix_twiddle_chunk(
        x: usize,
        y: usize,
        len: usize,
        direction: FftDirection,
    ) -> Self;

    /// Pairwise multiply the complex numbers in `left` with the complex numbers in `right`.
    unsafe fn mul_complex(left: Self, right: Self) -> Self;

    /// Constructs a Rotate90 object that will apply eithr a 90 or 270 degree rotationto the complex elements
    unsafe fn make_rotate90(direction: FftDirection) -> Rotation90<Self>;

    /// Uses a pre-constructed rotate90 object to apply the given rotation
    unsafe fn apply_rotate90(direction: Rotation90<Self>, values: Self) -> Self;

    /// Each of these Interprets the input as rows of a Self::COMPLEX_PER_VECTOR-by-N 2D array, and computes parallel butterflies down the columns of the 2D array
    unsafe fn column_butterfly2(rows: [Self; 2]) -> [Self; 2];
    unsafe fn column_butterfly4(rows: [Self; 4], rotation: Rotation90<Self>) -> [Self; 4];
}

impl NeonVector for float32x4_t {
    const COMPLEX_PER_VECTOR: usize = 2;

    type ScalarType = f32;

    #[inline(always)]
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        vld1q_f32(ptr as *const f32)
    }

    #[inline(always)]
    unsafe fn load_partial_lo_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        let temp = vmovq_n_f32(0.0);
        vreinterpretq_f32_u64(vld1q_lane_u64::<0>(
            ptr as *const u64,
            vreinterpretq_u64_f32(temp),
        ))
    }

    #[inline(always)]
    unsafe fn load1_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        vreinterpretq_f32_u64(vld1q_dup_u64(ptr as *const u64))
    }

    #[inline(always)]
    unsafe fn store_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        vst1q_f32(ptr as *mut f32, data);
    }

    #[inline(always)]
    unsafe fn store_partial_lo_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        let low = vget_low_f32(data);
        vst1_f32(ptr as *mut f32, low);
    }

    #[inline(always)]
    unsafe fn store_partial_hi_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        let high = vget_high_f32(data);
        vst1_f32(ptr as *mut f32, high);
    }

    #[inline(always)]
    unsafe fn neg(a: Self) -> Self {
        vnegq_f32(a)
    }
    #[inline(always)]
    unsafe fn add(a: Self, b: Self) -> Self {
        vaddq_f32(a, b)
    }
    #[inline(always)]
    unsafe fn mul(a: Self, b: Self) -> Self {
        vmulq_f32(a, b)
    }
    #[inline(always)]
    unsafe fn fmadd(acc: Self, a: Self, b: Self) -> Self {
        vfmaq_f32(acc, a, b)
    }
    #[inline(always)]
    unsafe fn nmadd(acc: Self, a: Self, b: Self) -> Self {
        vfmaq_f32(acc, a, vnegq_f32(b))
    }

    #[inline(always)]
    unsafe fn broadcast_scalar(value: Self::ScalarType) -> Self {
        vmovq_n_f32(value)
    }

    #[inline(always)]
    unsafe fn make_mixedradix_twiddle_chunk(
        x: usize,
        y: usize,
        len: usize,
        direction: FftDirection,
    ) -> Self {
        let mut twiddle_chunk = [Complex::<f32>::zero(); Self::COMPLEX_PER_VECTOR];
        for i in 0..Self::COMPLEX_PER_VECTOR {
            twiddle_chunk[i] = twiddles::compute_twiddle(y * (x + i), len, direction);
        }

        twiddle_chunk.as_slice().load_complex(0)
    }

    #[inline(always)]
    unsafe fn mul_complex(left: Self, right: Self) -> Self {
        // ARMv8.2-A introduced vcmulq_f32 and vcmlaq_f32 for complex multiplication, these intrinsics are not yet available.
        let temp1 = vtrn1q_f32(right, right);
        let temp2 = vtrn2q_f32(right, vnegq_f32(right));
        let temp3 = vmulq_f32(temp2, left);
        let temp4 = vrev64q_f32(temp3);
        vfmaq_f32(temp4, temp1, left)
    }

    #[inline(always)]
    unsafe fn make_rotate90(direction: FftDirection) -> Rotation90<Self> {
        Rotation90(match direction {
            FftDirection::Forward => vld1q_f32([0.0, -0.0, 0.0, -0.0].as_ptr()),
            FftDirection::Inverse => vld1q_f32([-0.0, 0.0, -0.0, 0.0].as_ptr()),
        })
    }

    #[inline(always)]
    unsafe fn apply_rotate90(direction: Rotation90<Self>, values: Self) -> Self {
        let temp = vrev64q_f32(values);
        vreinterpretq_f32_u32(veorq_u32(
            vreinterpretq_u32_f32(temp),
            vreinterpretq_u32_f32(direction.0),
        ))
    }

    #[inline(always)]
    unsafe fn column_butterfly2(rows: [Self; 2]) -> [Self; 2] {
        [vaddq_f32(rows[0], rows[1]), vsubq_f32(rows[0], rows[1])]
    }

    #[inline(always)]
    unsafe fn column_butterfly4(rows: [Self; 4], rotation: Rotation90<Self>) -> [Self; 4] {
        // Algorithm: 2x2 mixed radix

        // Perform the first set of size-2 FFTs.
        let [mid0, mid2] = Self::column_butterfly2([rows[0], rows[2]]);
        let [mid1, mid3] = Self::column_butterfly2([rows[1], rows[3]]);

        // Apply twiddle factors (in this case just a rotation)
        let mid3_rotated = Self::apply_rotate90(rotation, mid3);

        // Transpose the data and do size-2 FFTs down the columns
        let [output0, output1] = Self::column_butterfly2([mid0, mid1]);
        let [output2, output3] = Self::column_butterfly2([mid2, mid3_rotated]);

        // Swap outputs 1 and 2 in the output to do a square transpose
        [output0, output2, output1, output3]
    }
}

impl NeonVector for float64x2_t {
    const COMPLEX_PER_VECTOR: usize = 1;

    type ScalarType = f64;

    #[inline(always)]
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        vld1q_f64(ptr as *const f64)
    }

    #[inline(always)]
    unsafe fn load_partial_lo_complex(_ptr: *const Complex<Self::ScalarType>) -> Self {
        unimplemented!("Impossible to do a load store of complex f64's");
    }

    #[inline(always)]
    unsafe fn load1_complex(_ptr: *const Complex<Self::ScalarType>) -> Self {
        unimplemented!("Impossible to do a load store of complex f64's");
    }

    #[inline(always)]
    unsafe fn store_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        vst1q_f64(ptr as *mut f64, data);
    }

    #[inline(always)]
    unsafe fn store_partial_lo_complex(_ptr: *mut Complex<Self::ScalarType>, _data: Self) {
        unimplemented!("Impossible to do a partial store of complex f64's");
    }

    #[inline(always)]
    unsafe fn store_partial_hi_complex(_ptr: *mut Complex<Self::ScalarType>, _data: Self) {
        unimplemented!("Impossible to do a partial store of complex f64's");
    }

    #[inline(always)]
    unsafe fn neg(a: Self) -> Self {
        vnegq_f64(a)
    }
    #[inline(always)]
    unsafe fn add(a: Self, b: Self) -> Self {
        vaddq_f64(a, b)
    }
    #[inline(always)]
    unsafe fn mul(a: Self, b: Self) -> Self {
        vmulq_f64(a, b)
    }
    #[inline(always)]
    unsafe fn fmadd(acc: Self, a: Self, b: Self) -> Self {
        vfmaq_f64(acc, a, b)
    }
    #[inline(always)]
    unsafe fn nmadd(acc: Self, a: Self, b: Self) -> Self {
        vfmaq_f64(acc, a, vnegq_f64(b))
    }

    #[inline(always)]
    unsafe fn broadcast_scalar(value: Self::ScalarType) -> Self {
        vmovq_n_f64(value)
    }

    #[inline(always)]
    unsafe fn make_mixedradix_twiddle_chunk(
        x: usize,
        y: usize,
        len: usize,
        direction: FftDirection,
    ) -> Self {
        let mut twiddle_chunk = [Complex::<f64>::zero(); Self::COMPLEX_PER_VECTOR];
        for i in 0..Self::COMPLEX_PER_VECTOR {
            twiddle_chunk[i] = twiddles::compute_twiddle(y * (x + i), len, direction);
        }

        twiddle_chunk.as_slice().load_complex(0)
    }

    #[inline(always)]
    unsafe fn mul_complex(left: Self, right: Self) -> Self {
        // ARMv8.2-A introduced vcmulq_f64 and vcmlaq_f64 for complex multiplication, these intrinsics are not yet available.
        let temp = vcombine_f64(vneg_f64(vget_high_f64(left)), vget_low_f64(left));
        let sum = vmulq_laneq_f64::<0>(left, right);
        vfmaq_laneq_f64::<1>(sum, temp, right)
    }

    #[inline(always)]
    unsafe fn make_rotate90(direction: FftDirection) -> Rotation90<Self> {
        Rotation90(match direction {
            FftDirection::Forward => vld1q_f64([0.0, -0.0].as_ptr()),
            FftDirection::Inverse => vld1q_f64([-0.0, 0.0].as_ptr()),
        })
    }

    #[inline(always)]
    unsafe fn apply_rotate90(direction: Rotation90<Self>, values: Self) -> Self {
        let temp = vcombine_f64(vget_high_f64(values), vget_low_f64(values));
        vreinterpretq_f64_u64(veorq_u64(
            vreinterpretq_u64_f64(temp),
            vreinterpretq_u64_f64(direction.0),
        ))
    }

    #[inline(always)]
    unsafe fn column_butterfly2(rows: [Self; 2]) -> [Self; 2] {
        [vaddq_f64(rows[0], rows[1]), vsubq_f64(rows[0], rows[1])]
    }

    #[inline(always)]
    unsafe fn column_butterfly4(rows: [Self; 4], rotation: Rotation90<Self>) -> [Self; 4] {
        // Algorithm: 2x2 mixed radix

        // Perform the first set of size-2 FFTs.
        let [mid0, mid2] = Self::column_butterfly2([rows[0], rows[2]]);
        let [mid1, mid3] = Self::column_butterfly2([rows[1], rows[3]]);

        // Apply twiddle factors (in this case just a rotation)
        let mid3_rotated = Self::apply_rotate90(rotation, mid3);

        // Transpose the data and do size-2 FFTs down the columns
        let [output0, output1] = Self::column_butterfly2([mid0, mid1]);
        let [output2, output3] = Self::column_butterfly2([mid2, mid3_rotated]);

        // Swap outputs 1 and 2 in the output to do a square transpose
        [output0, output2, output1, output3]
    }
}

// A trait to handle reading from an array of complex floats into NEON vectors.
// NEON works with 128-bit vectors, meaning a vector can hold two complex f32,
// or a single complex f64.
pub trait NeonArray<S: NeonNum>: Deref {
    // Load complex numbers from the array to fill a NEON vector.
    unsafe fn load_complex(&self, index: usize) -> S::VectorType;
    // Load a single complex number from the array into a NEON vector, setting the unused elements to zero.
    unsafe fn load_partial_lo_complex(&self, index: usize) -> S::VectorType;
    // Load a single complex number from the array, and copy it to all elements of a NEON vector.
    unsafe fn load1_complex(&self, index: usize) -> S::VectorType;
}

impl<S: NeonNum> NeonArray<S> for &[Complex<S>] {
    #[inline(always)]
    unsafe fn load_complex(&self, index: usize) -> S::VectorType {
        debug_assert!(self.len() >= index + S::VectorType::COMPLEX_PER_VECTOR);
        S::VectorType::load_complex(self.as_ptr().add(index))
    }

    #[inline(always)]
    unsafe fn load_partial_lo_complex(&self, index: usize) -> S::VectorType {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::load_partial_lo_complex(self.as_ptr().add(index))
    }

    #[inline(always)]
    unsafe fn load1_complex(&self, index: usize) -> S::VectorType {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::load1_complex(self.as_ptr().add(index))
    }
}
impl<S: NeonNum> NeonArray<S> for &mut [Complex<S>] {
    #[inline(always)]
    unsafe fn load_complex(&self, index: usize) -> S::VectorType {
        debug_assert!(self.len() >= index + S::VectorType::COMPLEX_PER_VECTOR);
        S::VectorType::load_complex(self.as_ptr().add(index))
    }

    #[inline(always)]
    unsafe fn load_partial_lo_complex(&self, index: usize) -> S::VectorType {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::load_partial_lo_complex(self.as_ptr().add(index))
    }

    #[inline(always)]
    unsafe fn load1_complex(&self, index: usize) -> S::VectorType {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::load1_complex(self.as_ptr().add(index))
    }
}

impl<'a, S: NeonNum> NeonArray<S> for DoubleBuf<'a, S>
where
    &'a [Complex<S>]: NeonArray<S>,
{
    #[inline(always)]
    unsafe fn load_complex(&self, index: usize) -> S::VectorType {
        self.input.load_complex(index)
    }
    #[inline(always)]
    unsafe fn load_partial_lo_complex(&self, index: usize) -> S::VectorType {
        self.input.load_partial_lo_complex(index)
    }
    #[inline(always)]
    unsafe fn load1_complex(&self, index: usize) -> S::VectorType {
        self.input.load1_complex(index)
    }
}

// A trait to handle writing to an array of complex floats from NEON vectors.
// NEON works with 128-bit vectors, meaning a vector can hold two complex f32,
// or a single complex f64.
pub trait NeonArrayMut<S: NeonNum>: NeonArray<S> + DerefMut {
    // Store all complex numbers from a NEON vector to the array.
    unsafe fn store_complex(&mut self, vector: S::VectorType, index: usize);
    // Store the low complex number from a NEON vector to the array.
    unsafe fn store_partial_lo_complex(&mut self, vector: S::VectorType, index: usize);
}

impl<S: NeonNum> NeonArrayMut<S> for &mut [Complex<S>] {
    #[inline(always)]
    unsafe fn store_complex(&mut self, vector: S::VectorType, index: usize) {
        debug_assert!(self.len() >= index + S::VectorType::COMPLEX_PER_VECTOR);
        S::VectorType::store_complex(self.as_mut_ptr().add(index), vector)
    }
    #[inline(always)]
    unsafe fn store_partial_lo_complex(&mut self, vector: S::VectorType, index: usize) {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::store_partial_lo_complex(self.as_mut_ptr().add(index), vector)
    }
}

impl<'a, T: NeonNum> NeonArrayMut<T> for DoubleBuf<'a, T>
where
    Self: NeonArray<T>,
    &'a mut [Complex<T>]: NeonArrayMut<T>,
{
    #[inline(always)]
    unsafe fn store_complex(&mut self, vector: T::VectorType, index: usize) {
        self.output.store_complex(vector, index);
    }
    #[inline(always)]
    unsafe fn store_partial_lo_complex(&mut self, vector: T::VectorType, index: usize) {
        self.output.store_partial_lo_complex(vector, index);
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;

    use num_complex::Complex;

    #[test]
    fn test_load_f64() {
        unsafe {
            let val1: Complex<f64> = Complex::new(1.0, 2.0);
            let val2: Complex<f64> = Complex::new(3.0, 4.0);
            let val3: Complex<f64> = Complex::new(5.0, 6.0);
            let val4: Complex<f64> = Complex::new(7.0, 8.0);
            let values = vec![val1, val2, val3, val4];
            let slice = values.as_slice();
            let load1 = slice.load_complex(0);
            let load2 = slice.load_complex(1);
            let load3 = slice.load_complex(2);
            let load4 = slice.load_complex(3);
            assert_eq!(
                val1,
                std::mem::transmute::<float64x2_t, Complex<f64>>(load1)
            );
            assert_eq!(
                val2,
                std::mem::transmute::<float64x2_t, Complex<f64>>(load2)
            );
            assert_eq!(
                val3,
                std::mem::transmute::<float64x2_t, Complex<f64>>(load3)
            );
            assert_eq!(
                val4,
                std::mem::transmute::<float64x2_t, Complex<f64>>(load4)
            );
        }
    }

    #[test]
    fn test_store_f64() {
        unsafe {
            let val1: Complex<f64> = Complex::new(1.0, 2.0);
            let val2: Complex<f64> = Complex::new(3.0, 4.0);
            let val3: Complex<f64> = Complex::new(5.0, 6.0);
            let val4: Complex<f64> = Complex::new(7.0, 8.0);

            let nbr1 = vld1q_f64(&val1 as *const _ as *const f64);
            let nbr2 = vld1q_f64(&val2 as *const _ as *const f64);
            let nbr3 = vld1q_f64(&val3 as *const _ as *const f64);
            let nbr4 = vld1q_f64(&val4 as *const _ as *const f64);

            let mut values: Vec<Complex<f64>> = vec![Complex::new(0.0, 0.0); 4];
            let mut slice = values.as_mut_slice();
            slice.store_complex(nbr1, 0);
            slice.store_complex(nbr2, 1);
            slice.store_complex(nbr3, 2);
            slice.store_complex(nbr4, 3);
            assert_eq!(val1, values[0]);
            assert_eq!(val2, values[1]);
            assert_eq!(val3, values[2]);
            assert_eq!(val4, values[3]);
        }
    }
}
