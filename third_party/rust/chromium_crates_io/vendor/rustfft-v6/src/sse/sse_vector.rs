use num_complex::Complex;
use num_traits::Zero;
use std::arch::x86_64::*;
use std::fmt::Debug;
use std::ops::{Deref, DerefMut};

use crate::array_utils::DoubleBuf;
use crate::{twiddles, FftDirection};

use super::SseNum;

// Read these indexes from an SseArray and build an array of simd vectors.
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

// Read these indexes from an SseArray and build an array or partially filled simd vectors.
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

// Write these indexes of an array of simd vectors to the same indexes of an SseArray.
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

// Write the low half of these indexes of an array of simd vectors to the same indexes of an SseArray.
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

// Write these indexes of an array of simd vectors to the same indexes, multiplied by a stride, of an SseArray.
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
pub struct Rotation90<V: SseVector>(V);

// A trait to hold the BVectorType and COMPLEX_PER_VECTOR associated data
pub trait SseVector: Copy + Debug + Send + Sync {
    const COMPLEX_PER_VECTOR: usize;

    type ScalarType: SseNum<VectorType = Self>;

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

impl SseVector for __m128 {
    const COMPLEX_PER_VECTOR: usize = 2;

    type ScalarType = f32;

    #[inline(always)]
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        _mm_loadu_ps(ptr as *const f32)
    }

    #[inline(always)]
    unsafe fn load_partial_lo_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        _mm_castpd_ps(_mm_load_sd(ptr as *const f64))
    }

    #[inline(always)]
    unsafe fn load1_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        _mm_castpd_ps(_mm_load1_pd(ptr as *const f64))
    }

    #[inline(always)]
    unsafe fn store_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        _mm_storeu_ps(ptr as *mut f32, data);
    }

    #[inline(always)]
    unsafe fn store_partial_lo_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        _mm_storel_pd(ptr as *mut f64, _mm_castps_pd(data));
    }

    #[inline(always)]
    unsafe fn store_partial_hi_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        _mm_storeh_pd(ptr as *mut f64, _mm_castps_pd(data));
    }

    #[inline(always)]
    unsafe fn neg(a: Self) -> Self {
        _mm_xor_ps(a, _mm_set1_ps(-0.0))
    }
    #[inline(always)]
    unsafe fn add(a: Self, b: Self) -> Self {
        _mm_add_ps(a, b)
    }
    #[inline(always)]
    unsafe fn mul(a: Self, b: Self) -> Self {
        _mm_mul_ps(a, b)
    }
    #[inline(always)]
    unsafe fn fmadd(acc: Self, a: Self, b: Self) -> Self {
        _mm_add_ps(acc, _mm_mul_ps(a, b))
    }
    #[inline(always)]
    unsafe fn nmadd(acc: Self, a: Self, b: Self) -> Self {
        _mm_sub_ps(acc, _mm_mul_ps(a, b))
    }

    #[inline(always)]
    unsafe fn broadcast_scalar(value: Self::ScalarType) -> Self {
        _mm_set1_ps(value)
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
        //SSE3, taken from Intel performance manual
        let mut temp1 = _mm_shuffle_ps(right, right, 0xA0);
        let mut temp2 = _mm_shuffle_ps(right, right, 0xF5);
        temp1 = _mm_mul_ps(temp1, left);
        temp2 = _mm_mul_ps(temp2, left);
        temp2 = _mm_shuffle_ps(temp2, temp2, 0xB1);
        _mm_addsub_ps(temp1, temp2)
    }

    #[inline(always)]
    unsafe fn make_rotate90(direction: FftDirection) -> Rotation90<Self> {
        Rotation90(match direction {
            FftDirection::Forward => _mm_set_ps(-0.0, 0.0, -0.0, 0.0),
            FftDirection::Inverse => _mm_set_ps(0.0, -0.0, 0.0, -0.0),
        })
    }

    #[inline(always)]
    unsafe fn apply_rotate90(direction: Rotation90<Self>, values: Self) -> Self {
        let temp = _mm_shuffle_ps(values, values, 0xB1);
        _mm_xor_ps(temp, direction.0)
    }

    #[inline(always)]
    unsafe fn column_butterfly2(rows: [Self; 2]) -> [Self; 2] {
        [_mm_add_ps(rows[0], rows[1]), _mm_sub_ps(rows[0], rows[1])]
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

impl SseVector for __m128d {
    const COMPLEX_PER_VECTOR: usize = 1;

    type ScalarType = f64;

    #[inline(always)]
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        _mm_loadu_pd(ptr as *const f64)
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
        _mm_storeu_pd(ptr as *mut f64, data);
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
        _mm_xor_pd(a, _mm_set1_pd(-0.0))
    }
    #[inline(always)]
    unsafe fn add(a: Self, b: Self) -> Self {
        _mm_add_pd(a, b)
    }
    #[inline(always)]
    unsafe fn mul(a: Self, b: Self) -> Self {
        _mm_mul_pd(a, b)
    }
    #[inline(always)]
    unsafe fn fmadd(acc: Self, a: Self, b: Self) -> Self {
        _mm_add_pd(acc, _mm_mul_pd(a, b))
    }
    #[inline(always)]
    unsafe fn nmadd(acc: Self, a: Self, b: Self) -> Self {
        _mm_sub_pd(acc, _mm_mul_pd(a, b))
    }

    #[inline(always)]
    unsafe fn broadcast_scalar(value: Self::ScalarType) -> Self {
        _mm_set1_pd(value)
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
        // SSE3, taken from Intel performance manual
        let mut temp1 = _mm_unpacklo_pd(right, right);
        let mut temp2 = _mm_unpackhi_pd(right, right);
        temp1 = _mm_mul_pd(temp1, left);
        temp2 = _mm_mul_pd(temp2, left);
        temp2 = _mm_shuffle_pd(temp2, temp2, 0x01);
        _mm_addsub_pd(temp1, temp2)
    }

    #[inline(always)]
    unsafe fn make_rotate90(direction: FftDirection) -> Rotation90<Self> {
        Rotation90(match direction {
            FftDirection::Forward => _mm_set_pd(-0.0, 0.0),
            FftDirection::Inverse => _mm_set_pd(0.0, -0.0),
        })
    }

    #[inline(always)]
    unsafe fn apply_rotate90(direction: Rotation90<Self>, values: Self) -> Self {
        let temp = _mm_shuffle_pd(values, values, 0x01);
        _mm_xor_pd(temp, direction.0)
    }

    #[inline(always)]
    unsafe fn column_butterfly2(rows: [Self; 2]) -> [Self; 2] {
        [_mm_add_pd(rows[0], rows[1]), _mm_sub_pd(rows[0], rows[1])]
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

// A trait to handle reading from an array of complex floats into SSE vectors.
// SSE works with 128-bit vectors, meaning a vector can hold two complex f32,
// or a single complex f64.
pub trait SseArray<S: SseNum>: Deref {
    // Load complex numbers from the array to fill a SSE vector.
    unsafe fn load_complex(&self, index: usize) -> S::VectorType;
    // Load a single complex number from the array into a SSE vector, setting the unused elements to zero.
    unsafe fn load_partial_lo_complex(&self, index: usize) -> S::VectorType;
    // Load a single complex number from the array, and copy it to all elements of a SSE vector.
    unsafe fn load1_complex(&self, index: usize) -> S::VectorType;
}

impl<S: SseNum> SseArray<S> for &[Complex<S>] {
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
impl<S: SseNum> SseArray<S> for &mut [Complex<S>] {
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

impl<'a, S: SseNum> SseArray<S> for DoubleBuf<'a, S>
where
    &'a [Complex<S>]: SseArray<S>,
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

// A trait to handle writing to an array of complex floats from SSE vectors.
// SSE works with 128-bit vectors, meaning a vector can hold two complex f32,
// or a single complex f64.
pub trait SseArrayMut<S: SseNum>: SseArray<S> + DerefMut {
    // Store all complex numbers from a SSE vector to the array.
    unsafe fn store_complex(&mut self, vector: S::VectorType, index: usize);
    // Store the low complex number from a SSE vector to the array.
    unsafe fn store_partial_lo_complex(&mut self, vector: S::VectorType, index: usize);
}

impl<S: SseNum> SseArrayMut<S> for &mut [Complex<S>] {
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

impl<'a, T: SseNum> SseArrayMut<T> for DoubleBuf<'a, T>
where
    Self: SseArray<T>,
    &'a mut [Complex<T>]: SseArrayMut<T>,
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
