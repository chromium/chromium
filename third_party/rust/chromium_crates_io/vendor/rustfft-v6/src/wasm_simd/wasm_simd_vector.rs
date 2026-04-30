use core::arch::wasm32::*;
use num_complex::Complex;
use num_traits::Zero;
use std::fmt::Debug;
use std::ops::{Deref, DerefMut};

use crate::{array_utils::DoubleBuf, twiddles, FftDirection};

use super::WasmNum;

/// Read these indexes from an WasmSimdArray and build an array of simd vectors.
/// Takes a name of a vector to read from, and a list of indexes to read.
/// This statement:
///
/// let values = read_complex_to_array!(input, {0, 1, 2, 3});
///
/// is equivalent to:
///
/// let values = [
///     input.load_complex(0),
///     input.load_complex(1),
///     input.load_complex(2),
///     input.load_complex(3),
/// ];
macro_rules! read_complex_to_array {
    ($input:ident, { $($idx:literal),* }) => {
        [
        $(
            $input.load_complex($idx),
        )*
        ]
    }
}

/// Read these indexes from an WasmSimdArray and build an array or partially filled simd vectors.
/// Takes a name of a vector to read from, and a list of indexes to read.
/// This statement:
///
/// let values = read_partial1_complex_to_array!(input, {0, 1, 2, 3});
///
/// is equivalent to:
///
/// let values = [
///     input.load1_complex(0),
///     input.load1_complex(1),
///     input.load1_complex(2),
///     input.load1_complex(3),
/// ];
///
macro_rules! read_partial1_complex_to_array {
    ($input:ident, { $($idx:literal),* }) => {
        [
        $(
            $input.load1_complex($idx),
        )*
        ]
    }
}

/// Write these indexes of an array of simd vectors to the same indexes of an WasmSimdArray.
/// Takes a name of a vector to read from, one to write to, and a list of indexes.
/// This statement:
///
/// let values = write_complex_to_array!(input, output, {0, 1, 2, 3});
///
/// is equivalent to:
///
/// let values = [
///     output.store_complex(input[0], 0),
///     output.store_complex(input[1], 1),
///     output.store_complex(input[2], 2),
///     output.store_complex(input[3], 3),
/// ];
///
macro_rules! write_complex_to_array {
    ($input:ident, $output:ident, { $($idx:literal),* }) => {
        $(
            $output.store_complex($input[$idx], $idx);
        )*
    }
}

/// Write the low half of these indexes of an array of simd vectors to the same indexes of an WasmSimdArray.
/// Takes a name of a vector to read from, one to write to, and a list of indexes.
/// This statement:
///
/// let values = write_partial_lo_complex_to_array!(input, output, {0, 1, 2, 3});
///
/// is equivalent to:
///
/// let values = [
///     output.store_partial_lo_complex(input[0], 0),
///     output.store_partial_lo_complex(input[1], 1),
///     output.store_partial_lo_complex(input[2], 2),
///     output.store_partial_lo_complex(input[3], 3),
/// ];
///
macro_rules! write_partial_lo_complex_to_array {
    ($input:ident, $output:ident, { $($idx:literal),* }) => {
        $(
            $output.store_partial_lo_complex($input[$idx], $idx);
        )*
    }
}

/// Write these indexes of an array of simd vectors to the same indexes, multiplied by a stride, of an WasmSimdArray.
/// Takes a name of a vector to read from, one to write to, an integer stride, and a list of indexes.
/// This statement:
///
/// let values = write_complex_to_array_strided!(input, output, {0, 1, 2, 3});
///
/// is equivalent to:
///
/// let values = [
///     output.store_complex(input[0], 0),
///     output.store_complex(input[1], 2),
///     output.store_complex(input[2], 4),
///     output.store_complex(input[3], 6),
/// ];
///
macro_rules! write_complex_to_array_strided {
    ($input:ident, $output:ident, $stride:literal, { $($idx:literal),* }) => {
        $(
            $output.store_complex($input[$idx], $idx*$stride);
        )*
    }
}

/// Read these indexes from an WasmSimdArray and build an array of simd vectors.
/// Takes a name of a vector to read from, and a list of indexes to read.
/// This statement:
///
/// let values = read_complex_to_array_v128!(input, {0, 1, 2, 3});
///
/// is equivalent to:
///
/// let values = [
///     input.load_complex_v128(0),
///     input.load_complex_v128(1),
///     input.load_complex_v128(2),
///     input.load_complex_v128(3),
/// ];
///
macro_rules! read_complex_to_array_v128 {
    ($input:ident, { $($idx:literal),* }) => {
        [
        $(
            $input.load_complex_v128($idx),
        )*
        ]
    }
}

/// Read these indexes from an WasmSimdArray and build an array or partially filled simd vectors.
/// Takes a name of a vector to read from, and a list of indexes to read.
/// This statement:
///
/// let values = read_partial1_complex_to_array_v128!(input, {0, 1, 2, 3});
///
/// is equivalent to:
///
/// let values = [
///     input.load1_complex_v128(0),
///     input.load1_complex_v128(1),
///     input.load1_complex_v128(2),
///     input.load1_complex_v128(3),
/// ];
///
macro_rules! read_partial1_complex_to_array_v128 {
    ($input:ident, { $($idx:literal),* }) => {
        [
        $(
            $input.load1_complex_v128($idx),
        )*
        ]
    }
}

/// Write these indexes of an array of simd vectors to the same indexes of an WasmSimdArray.
/// Takes a name of a vector to read from, one to write to, and a list of indexes.
/// This statement:
///
/// let values = write_complex_to_array_v128!(input, output, {0, 1, 2, 3});
///
/// is equivalent to:
///
/// let values = [
///     output.store_complex_v128(input[0], 0),
///     output.store_complex_v128(input[1], 1),
///     output.store_complex_v128(input[2], 2),
///     output.store_complex_v128(input[3], 3),
/// ];
///
macro_rules! write_complex_to_array_v128 {
    ($input:ident, $output:ident, { $($idx:literal),* }) => {
        $(
            $output.store_complex_v128($input[$idx], $idx);
        )*
    }
}

/// Write these indexes of an array of simd vectors to the same indexes, multiplied by a stride, of an WasmSimdArray.
/// Takes a name of a vector to read from, one to write to, an integer stride, and a list of indexes.
/// This statement:
///
/// let values = write_complex_to_array_strided_v128!(input, output, {0, 1, 2, 3});
///
/// is equivalent to:
///
/// let values = [
///     output.store_complex_v128(input[0], 0),
///     output.store_complex_v128(input[1], 2),
///     output.store_complex_v128(input[2], 4),
///     output.store_complex_v128(input[3], 6),
/// ];
///
macro_rules! write_complex_to_array_strided_v128 {
    ($input:ident, $output:ident, $stride:literal, { $($idx:literal),* }) => {
        $(
            $output.store_complex_v128($input[$idx], $idx*$stride);
        )*
    }
}

// We need newtypes for wasm vectors, since they don't have different vector types for f32 vs f64
#[derive(Copy, Clone, Debug)]
#[repr(transparent)]
pub struct WasmVector32(pub v128);

#[derive(Copy, Clone, Debug)]
#[repr(transparent)]
pub struct WasmVector64(pub v128);

#[derive(Copy, Clone)]
pub struct Rotation90<V: WasmVector>(V);

// A trait to hold the BVectorType and COMPLEX_PER_VECTOR associated data
pub trait WasmVector: Copy + Debug + Send + Sync {
    const COMPLEX_PER_VECTOR: usize;

    type ScalarType: WasmNum<VectorType = Self>;

    fn unwrap(self) -> v128;

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

impl WasmVector for WasmVector32 {
    const COMPLEX_PER_VECTOR: usize = 2;

    type ScalarType = f32;

    #[inline(always)]
    fn unwrap(self) -> v128 {
        self.0
    }

    #[inline(always)]
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        Self(v128_load(ptr as *const v128))
    }

    #[inline(always)]
    unsafe fn load_partial_lo_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        Self(v128_load64_lane::<0>(f32x4_splat(0.0), ptr as *const u64))
    }

    #[inline(always)]
    unsafe fn load1_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        Self(v128_load64_splat(ptr as *const u64))
    }

    #[inline(always)]
    unsafe fn store_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        v128_store(ptr as *mut v128, data.0);
    }

    #[inline(always)]
    unsafe fn store_partial_lo_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        v128_store64_lane::<0>(data.0, ptr as *mut u64);
    }

    #[inline(always)]
    unsafe fn store_partial_hi_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        v128_store64_lane::<1>(data.0, ptr as *mut u64);
    }

    #[inline(always)]
    unsafe fn neg(a: Self) -> Self {
        Self(f32x4_neg(a.0))
    }
    #[inline(always)]
    unsafe fn add(a: Self, b: Self) -> Self {
        Self(f32x4_add(a.0, b.0))
    }
    #[inline(always)]
    unsafe fn mul(a: Self, b: Self) -> Self {
        Self(f32x4_mul(a.0, b.0))
    }
    #[inline(always)]
    unsafe fn fmadd(acc: Self, a: Self, b: Self) -> Self {
        Self(f32x4_add(acc.0, f32x4_mul(a.0, b.0)))
    }
    #[inline(always)]
    unsafe fn nmadd(acc: Self, a: Self, b: Self) -> Self {
        Self(f32x4_sub(acc.0, f32x4_mul(a.0, b.0)))
    }

    #[inline(always)]
    unsafe fn broadcast_scalar(value: Self::ScalarType) -> Self {
        Self(f32x4_splat(value))
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
        let temp1 = u32x4_shuffle::<0, 4, 2, 6>(right.0, right.0);
        let temp2 = u32x4_shuffle::<1, 5, 3, 7>(right.0, f32x4_neg(right.0));
        let temp3 = f32x4_mul(temp2, left.0);
        let temp4 = u32x4_shuffle::<1, 0, 3, 2>(temp3, temp3);
        let temp5 = f32x4_mul(temp1, left.0);
        Self(f32x4_add(temp4, temp5))
    }

    #[inline(always)]
    unsafe fn make_rotate90(direction: FftDirection) -> Rotation90<Self> {
        Rotation90(Self(match direction {
            FftDirection::Forward => f32x4(0.0, -0.0, 0.0, -0.0),
            FftDirection::Inverse => f32x4(-0.0, 0.0, -0.0, 0.0),
        }))
    }

    #[inline(always)]
    unsafe fn apply_rotate90(direction: Rotation90<Self>, values: Self) -> Self {
        Self(v128_xor(
            u32x4_shuffle::<1, 0, 3, 2>(values.0, values.0),
            direction.0 .0,
        ))
    }

    #[inline(always)]
    unsafe fn column_butterfly2(rows: [Self; 2]) -> [Self; 2] {
        [
            Self(f32x4_add(rows[0].0, rows[1].0)),
            Self(f32x4_sub(rows[0].0, rows[1].0)),
        ]
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

impl WasmVector for WasmVector64 {
    const COMPLEX_PER_VECTOR: usize = 1;

    type ScalarType = f64;

    #[inline(always)]
    fn unwrap(self) -> v128 {
        self.0
    }

    #[inline(always)]
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        Self(v128_load(ptr as *const v128))
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
        v128_store(ptr as *mut v128, data.0);
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
        Self(f64x2_neg(a.0))
    }
    #[inline(always)]
    unsafe fn add(a: Self, b: Self) -> Self {
        Self(f64x2_add(a.0, b.0))
    }
    #[inline(always)]
    unsafe fn mul(a: Self, b: Self) -> Self {
        Self(f64x2_mul(a.0, b.0))
    }
    #[inline(always)]
    unsafe fn fmadd(acc: Self, a: Self, b: Self) -> Self {
        Self(f64x2_add(acc.0, f64x2_mul(a.0, b.0)))
    }
    #[inline(always)]
    unsafe fn nmadd(acc: Self, a: Self, b: Self) -> Self {
        Self(f64x2_sub(acc.0, f64x2_mul(a.0, b.0)))
    }

    #[inline(always)]
    unsafe fn broadcast_scalar(value: Self::ScalarType) -> Self {
        Self(f64x2_splat(value))
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
        const NEGATE_LEFT: v128 = f64x2(-0.0, 0.0);
        let temp = v128_xor(u64x2_shuffle::<1, 0>(left.0, left.0), NEGATE_LEFT);
        let sum = f64x2_mul(left.0, u64x2_shuffle::<0, 0>(right.0, right.0));
        Self(f64x2_add(
            sum,
            f64x2_mul(temp, u64x2_shuffle::<1, 1>(right.0, right.0)),
        ))
    }

    #[inline(always)]
    unsafe fn make_rotate90(direction: FftDirection) -> Rotation90<Self> {
        Rotation90(Self(match direction {
            FftDirection::Forward => f64x2(0.0, -0.0),
            FftDirection::Inverse => f64x2(-0.0, 0.0),
        }))
    }

    #[inline(always)]
    unsafe fn apply_rotate90(direction: Rotation90<Self>, values: Self) -> Self {
        Self(v128_xor(
            u64x2_shuffle::<1, 0>(values.0, values.0),
            direction.0 .0,
        ))
    }

    #[inline(always)]
    unsafe fn column_butterfly2(rows: [Self; 2]) -> [Self; 2] {
        [
            Self(f64x2_add(rows[0].0, rows[1].0)),
            Self(f64x2_sub(rows[0].0, rows[1].0)),
        ]
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

// A trait to handle reading from an array of complex floats into Wasm SIMD vectors.
// Wasm SIMD works with 128-bit vectors, meaning a vector can hold two complex f32,
// or a single complex f64.
pub trait WasmSimdArray<S: WasmNum>: Deref {
    // Load complex numbers from the array to fill a Wasm SIMD vector.
    unsafe fn load_complex_v128(&self, index: usize) -> v128;
    // Load a single complex number from the array into a Wasm SIMD vector, setting the unused elements to zero.
    unsafe fn load_partial_lo_complex_v128(&self, index: usize) -> v128;
    // Load a single complex number from the array, and copy it to all elements of a Wasm SIMD vector.
    unsafe fn load1_complex_v128(&self, index: usize) -> v128;

    // Load complex numbers from the array to fill a Wasm SIMD vector.
    unsafe fn load_complex(&self, index: usize) -> S::VectorType;
    // Load a single complex number from the array into a Wasm SIMD vector, setting the unused elements to zero.
    unsafe fn load_partial_lo_complex(&self, index: usize) -> S::VectorType;
    // Load a single complex number from the array, and copy it to all elements of a Wasm SIMD vector.
    unsafe fn load1_complex(&self, index: usize) -> S::VectorType;
}

impl<S: WasmNum> WasmSimdArray<S> for &[Complex<S>] {
    #[inline(always)]
    unsafe fn load_complex_v128(&self, index: usize) -> v128 {
        debug_assert!(self.len() >= index + S::VectorType::COMPLEX_PER_VECTOR);
        S::VectorType::load_complex(self.as_ptr().add(index)).unwrap()
    }

    #[inline(always)]
    unsafe fn load_partial_lo_complex_v128(&self, index: usize) -> v128 {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::load_partial_lo_complex(self.as_ptr().add(index)).unwrap()
    }

    #[inline(always)]
    unsafe fn load1_complex_v128(&self, index: usize) -> v128 {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::load1_complex(self.as_ptr().add(index)).unwrap()
    }

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
impl<S: WasmNum> WasmSimdArray<S> for &mut [Complex<S>] {
    #[inline(always)]
    unsafe fn load_complex_v128(&self, index: usize) -> v128 {
        debug_assert!(self.len() >= index + S::VectorType::COMPLEX_PER_VECTOR);
        S::VectorType::load_complex(self.as_ptr().add(index)).unwrap()
    }

    #[inline(always)]
    unsafe fn load_partial_lo_complex_v128(&self, index: usize) -> v128 {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::load_partial_lo_complex(self.as_ptr().add(index)).unwrap()
    }

    #[inline(always)]
    unsafe fn load1_complex_v128(&self, index: usize) -> v128 {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::load1_complex(self.as_ptr().add(index)).unwrap()
    }

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

impl<'a, S: WasmNum> WasmSimdArray<S> for DoubleBuf<'a, S>
where
    &'a [Complex<S>]: WasmSimdArray<S>,
{
    #[inline(always)]
    unsafe fn load_complex_v128(&self, index: usize) -> v128 {
        self.input.load_complex(index).unwrap()
    }
    #[inline(always)]
    unsafe fn load_partial_lo_complex_v128(&self, index: usize) -> v128 {
        self.input.load_partial_lo_complex(index).unwrap()
    }
    #[inline(always)]
    unsafe fn load1_complex_v128(&self, index: usize) -> v128 {
        self.input.load1_complex(index).unwrap()
    }

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

// A trait to handle writing to an array of complex floats from Wasm SIMD vectors.
// Wasm SIMD works with 128-bit vectors, meaning a vector can hold two complex f32,
// or a single complex f64.
pub trait WasmSimdArrayMut<S: WasmNum>: WasmSimdArray<S> + DerefMut {
    // Store all complex numbers from a SSE vector to the array.
    unsafe fn store_complex_v128(&mut self, vector: v128, index: usize);
    // Store the low complex number from a SSE vector to the array.
    unsafe fn store_partial_lo_complex_v128(&mut self, vector: v128, index: usize);

    // Store all complex numbers from a SSE vector to the array.
    unsafe fn store_complex(&mut self, vector: S::VectorType, index: usize);
    // Store the low complex number from a SSE vector to the array.
    unsafe fn store_partial_lo_complex(&mut self, vector: S::VectorType, index: usize);
}

impl<S: WasmNum> WasmSimdArrayMut<S> for &mut [Complex<S>] {
    #[inline(always)]
    unsafe fn store_complex_v128(&mut self, vector: v128, index: usize) {
        debug_assert!(self.len() >= index + S::VectorType::COMPLEX_PER_VECTOR);
        S::VectorType::store_complex(self.as_mut_ptr().add(index), S::wrap(vector))
    }
    #[inline(always)]
    unsafe fn store_partial_lo_complex_v128(&mut self, vector: v128, index: usize) {
        debug_assert!(self.len() >= index + 1);
        S::VectorType::store_partial_lo_complex(self.as_mut_ptr().add(index), S::wrap(vector))
    }

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

impl<'a, T: WasmNum> WasmSimdArrayMut<T> for DoubleBuf<'a, T>
where
    Self: WasmSimdArray<T>,
    &'a mut [Complex<T>]: WasmSimdArrayMut<T>,
{
    #[inline(always)]
    unsafe fn store_complex_v128(&mut self, vector: v128, index: usize) {
        self.output.store_complex_v128(vector, index);
    }
    #[inline(always)]
    unsafe fn store_partial_lo_complex_v128(&mut self, vector: v128, index: usize) {
        self.output.store_partial_lo_complex_v128(vector, index);
    }

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
    use wasm_bindgen_test::wasm_bindgen_test;

    #[wasm_bindgen_test]
    fn test_load_f64() {
        unsafe {
            let val1: Complex<f64> = Complex::new(1.0, 2.0);
            let val2: Complex<f64> = Complex::new(3.0, 4.0);
            let val3: Complex<f64> = Complex::new(5.0, 6.0);
            let val4: Complex<f64> = Complex::new(7.0, 8.0);
            let values = vec![val1, val2, val3, val4];
            let slice = values.as_slice();
            let load1 = slice.load_complex_v128(0);
            let load2 = slice.load_complex_v128(1);
            let load3 = slice.load_complex_v128(2);
            let load4 = slice.load_complex_v128(3);
            assert_eq!(val1, std::mem::transmute::<v128, Complex<f64>>(load1));
            assert_eq!(val2, std::mem::transmute::<v128, Complex<f64>>(load2));
            assert_eq!(val3, std::mem::transmute::<v128, Complex<f64>>(load3));
            assert_eq!(val4, std::mem::transmute::<v128, Complex<f64>>(load4));
        }
    }

    #[wasm_bindgen_test]
    fn test_store_f64() {
        unsafe {
            let val1: Complex<f64> = Complex::new(1.0, 2.0);
            let val2: Complex<f64> = Complex::new(3.0, 4.0);
            let val3: Complex<f64> = Complex::new(5.0, 6.0);
            let val4: Complex<f64> = Complex::new(7.0, 8.0);

            let nbr1 = v128_load(&val1 as *const _ as *const v128);
            let nbr2 = v128_load(&val2 as *const _ as *const v128);
            let nbr3 = v128_load(&val3 as *const _ as *const v128);
            let nbr4 = v128_load(&val4 as *const _ as *const v128);

            let mut values: Vec<Complex<f64>> = vec![Complex::new(0.0, 0.0); 4];
            let mut slice = values.as_mut_slice();
            slice.store_complex_v128(nbr1, 0);
            slice.store_complex_v128(nbr2, 1);
            slice.store_complex_v128(nbr3, 2);
            slice.store_complex_v128(nbr4, 3);
            assert_eq!(val1, values[0]);
            assert_eq!(val2, values[1]);
            assert_eq!(val3, values[2]);
            assert_eq!(val4, values[3]);
        }
    }
}
