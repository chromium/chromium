use std::arch::x86_64::*;
use std::fmt::Debug;
use std::ops::{Deref, DerefMut};

use num_complex::Complex;
use num_traits::Zero;

use crate::{array_utils::DoubleBuf, twiddles, FftDirection};

use super::AvxNum;

/// A SIMD vector of complex numbers, stored with the real values and imaginary values interleaved.
/// Implemented for __m128, __m128d, __m256, __m256d, but these all require the AVX instruction set.
///
/// The goal of this trait is to reduce code duplication by letting code be generic over the vector type
pub trait AvxVector: Copy + Debug + Send + Sync {
    const COMPLEX_PER_VECTOR: usize;

    // useful constants
    unsafe fn zero() -> Self;
    unsafe fn half_root2() -> Self; // an entire vector filled with 0.5.sqrt()

    // Basic operations that map directly to 1-2 AVX intrinsics
    unsafe fn add(left: Self, right: Self) -> Self;
    unsafe fn sub(left: Self, right: Self) -> Self;
    unsafe fn xor(left: Self, right: Self) -> Self;
    unsafe fn neg(self) -> Self;
    unsafe fn mul(left: Self, right: Self) -> Self;
    unsafe fn fmadd(left: Self, right: Self, add: Self) -> Self;
    unsafe fn fnmadd(left: Self, right: Self, add: Self) -> Self;
    unsafe fn fmaddsub(left: Self, right: Self, add: Self) -> Self;
    unsafe fn fmsubadd(left: Self, right: Self, add: Self) -> Self;

    // More basic operations that end up being implemented in 1-2 intrinsics, but unlike the ones above, these have higher-level meaning than just arithmetic
    /// Swap each real number with its corresponding imaginary number
    unsafe fn swap_complex_components(self) -> Self;

    /// first return is the reals duplicated into the imaginaries, second return is the imaginaries duplicated into the reals
    unsafe fn duplicate_complex_components(self) -> (Self, Self);

    /// Reverse the order of complex numbers in the vector, so that the last is the first and the first is the last
    unsafe fn reverse_complex_elements(self) -> Self;

    /// Copies the even elements of rows[1] into the corresponding odd elements of rows[0] and returns the result.
    unsafe fn unpacklo_complex(rows: [Self; 2]) -> Self;
    /// Copies the odd elements of rows[0] into the corresponding even elements of rows[1] and returns the result.
    unsafe fn unpackhi_complex(rows: [Self; 2]) -> Self;

    #[inline(always)]
    unsafe fn unpack_complex(rows: [Self; 2]) -> [Self; 2] {
        [Self::unpacklo_complex(rows), Self::unpackhi_complex(rows)]
    }

    /// Fill a vector by computing a twiddle factor and repeating it across the whole vector
    unsafe fn broadcast_twiddle(index: usize, len: usize, direction: FftDirection) -> Self;

    /// create a Rotator90 instance to rotate complex numbers either 90 or 270 degrees, based on the value of `inverse`
    unsafe fn make_rotation90(direction: FftDirection) -> Rotation90<Self>;

    /// Generates a chunk of twiddle factors starting at (X,Y) and incrementing X `COMPLEX_PER_VECTOR` times.
    /// The result will be [twiddle(x*y, len), twiddle((x+1)*y, len), twiddle((x+2)*y, len), ...] for as many complex numbers fit in a vector
    unsafe fn make_mixedradix_twiddle_chunk(
        x: usize,
        y: usize,
        len: usize,
        direction: FftDirection,
    ) -> Self;

    /// Packed transposes. Used by mixed radix. These all take a NxC array, where C is COMPLEX_PER_VECTOR, and transpose it to a CxN array.
    /// But they also pack the result into as few vectors as possible, with the goal of writing the transposed data out contiguously.
    unsafe fn transpose2_packed(rows: [Self; 2]) -> [Self; 2];
    unsafe fn transpose3_packed(rows: [Self; 3]) -> [Self; 3];
    unsafe fn transpose4_packed(rows: [Self; 4]) -> [Self; 4];
    unsafe fn transpose5_packed(rows: [Self; 5]) -> [Self; 5];
    unsafe fn transpose6_packed(rows: [Self; 6]) -> [Self; 6];
    unsafe fn transpose7_packed(rows: [Self; 7]) -> [Self; 7];
    unsafe fn transpose8_packed(rows: [Self; 8]) -> [Self; 8];
    unsafe fn transpose9_packed(rows: [Self; 9]) -> [Self; 9];
    unsafe fn transpose11_packed(rows: [Self; 11]) -> [Self; 11];
    unsafe fn transpose12_packed(rows: [Self; 12]) -> [Self; 12];
    unsafe fn transpose16_packed(rows: [Self; 16]) -> [Self; 16];

    /// Pairwise multiply the complex numbers in `left` with the complex numbers in `right`.
    #[inline(always)]
    unsafe fn mul_complex(left: Self, right: Self) -> Self {
        // Extract the real and imaginary components from left into 2 separate registers
        let (left_real, left_imag) = Self::duplicate_complex_components(left);

        // create a shuffled version of right where the imaginary values are swapped with the reals
        let right_shuffled = Self::swap_complex_components(right);

        // multiply our duplicated imaginary left vector by our shuffled right vector. that will give us the right side of the traditional complex multiplication formula
        let output_right = Self::mul(left_imag, right_shuffled);

        // use a FMA instruction to multiply together left side of the complex multiplication formula, then alternatingly add and subtract the left side from the right
        Self::fmaddsub(left_real, right, output_right)
    }

    #[inline(always)]
    unsafe fn rotate90(self, rotation: Rotation90<Self>) -> Self {
        // Use the pre-computed vector stored in the Rotation90 instance to negate either the reals or imaginaries
        let negated = Self::xor(self, rotation.0);

        // Our goal is to swap the reals with the imaginaries, then negate either the reals or the imaginaries, based on whether we're an inverse or not
        Self::swap_complex_components(negated)
    }

    #[inline(always)]
    unsafe fn column_butterfly2(rows: [Self; 2]) -> [Self; 2] {
        [Self::add(rows[0], rows[1]), Self::sub(rows[0], rows[1])]
    }

    #[inline(always)]
    unsafe fn column_butterfly3(rows: [Self; 3], twiddles: Self) -> [Self; 3] {
        // This algorithm is derived directly from the definition of the Dft of size 3
        // We'd theoretically have to do 4 complex multiplications, but all of the twiddles we'd be multiplying by are conjugates of each other
        // By doing some algebra to expand the complex multiplications and factor out the multiplications, we get this

        let [mut mid1, mid2] = Self::column_butterfly2([rows[1], rows[2]]);
        let output0 = Self::add(rows[0], mid1);

        let (twiddle_real, twiddle_imag) = Self::duplicate_complex_components(twiddles);

        mid1 = Self::fmadd(mid1, twiddle_real, rows[0]);

        let rotation = Self::make_rotation90(FftDirection::Inverse);
        let mid2_rotated = Self::rotate90(mid2, rotation);

        let output1 = Self::fmadd(mid2_rotated, twiddle_imag, mid1);
        let output2 = Self::fnmadd(mid2_rotated, twiddle_imag, mid1);

        [output0, output1, output2]
    }

    #[inline(always)]
    unsafe fn column_butterfly4(rows: [Self; 4], rotation: Rotation90<Self>) -> [Self; 4] {
        // Algorithm: 2x2 mixed radix

        // Perform the first set of size-2 FFTs.
        let [mid0, mid2] = Self::column_butterfly2([rows[0], rows[2]]);
        let [mid1, mid3] = Self::column_butterfly2([rows[1], rows[3]]);

        // Apply twiddle factors (in this case just a rotation)
        let mid3_rotated = mid3.rotate90(rotation);

        // Transpose the data and do size-2 FFTs down the columns
        let [output0, output1] = Self::column_butterfly2([mid0, mid1]);
        let [output2, output3] = Self::column_butterfly2([mid2, mid3_rotated]);

        // Swap outputs 1 and 2 in the output to do a square transpose
        [output0, output2, output1, output3]
    }

    #[inline(always)]
    unsafe fn column_butterfly5(rows: [Self; 5], twiddles: [Self; 2]) -> [Self; 5] {
        // This algorithm is derived directly from the definition of the Dft of size 5
        // We'd theoretically have to do 16 complex multiplications for the Dft, but many of the twiddles we'd be multiplying by are conjugates of each other
        // By doing some algebra to expand the complex multiplications and factor out the real multiplications, we get this faster formula where we only do the equivalent of 4 multiplications

        // do some prep work before we can start applying twiddle factors
        let [sum1, diff4] = Self::column_butterfly2([rows[1], rows[4]]);
        let [sum2, diff3] = Self::column_butterfly2([rows[2], rows[3]]);

        let rotation = Self::make_rotation90(FftDirection::Inverse);
        let rotated4 = Self::rotate90(diff4, rotation);
        let rotated3 = Self::rotate90(diff3, rotation);

        // to compute the first output, compute the sum of all elements. sum1 and sum2 already have the sum of 1+4 and 2+3 respectively, so if we add them, we'll get the sum of all 4
        let sum1234 = Self::add(sum1, sum2);
        let output0 = Self::add(rows[0], sum1234);

        // apply twiddle factors
        let (twiddles0_re, twiddles0_im) = Self::duplicate_complex_components(twiddles[0]);
        let (twiddles1_re, twiddles1_im) = Self::duplicate_complex_components(twiddles[1]);

        let twiddled1_mid = Self::fmadd(twiddles0_re, sum1, rows[0]);
        let twiddled2_mid = Self::fmadd(twiddles1_re, sum1, rows[0]);
        let twiddled3_mid = Self::mul(twiddles1_im, rotated4);
        let twiddled4_mid = Self::mul(twiddles0_im, rotated4);

        let twiddled1 = Self::fmadd(twiddles1_re, sum2, twiddled1_mid);
        let twiddled2 = Self::fmadd(twiddles0_re, sum2, twiddled2_mid);
        let twiddled3 = Self::fnmadd(twiddles0_im, rotated3, twiddled3_mid); // fnmadd instead of fmadd because we're actually re-using twiddle0 here. remember that this algorithm is all about factoring out conjugated multiplications -- this negation of the twiddle0 imaginaries is a reflection of one of those conugations
        let twiddled4 = Self::fmadd(twiddles1_im, rotated3, twiddled4_mid);

        // Post-processing to mix the twiddle factors between the rest of the output
        let [output1, output4] = Self::column_butterfly2([twiddled1, twiddled4]);
        let [output2, output3] = Self::column_butterfly2([twiddled2, twiddled3]);

        [output0, output1, output2, output3, output4]
    }

    #[inline(always)]
    unsafe fn column_butterfly7(rows: [Self; 7], twiddles: [Self; 3]) -> [Self; 7] {
        // This algorithm is derived directly from the definition of the Dft of size 7
        // We'd theoretically have to do 36 complex multiplications for the Dft, but many of the twiddles we'd be multiplying by are conjugates of each other
        // By doing some algebra to expand the complex multiplications and factor out the real multiplications, we get this faster formula where we only do the equivalent of 9 multiplications

        // do some prep work before we can start applying twiddle factors
        let [sum1, diff6] = Self::column_butterfly2([rows[1], rows[6]]);
        let [sum2, diff5] = Self::column_butterfly2([rows[2], rows[5]]);
        let [sum3, diff4] = Self::column_butterfly2([rows[3], rows[4]]);

        let rotation = Self::make_rotation90(FftDirection::Inverse);
        let rotated4 = Self::rotate90(diff4, rotation);
        let rotated5 = Self::rotate90(diff5, rotation);
        let rotated6 = Self::rotate90(diff6, rotation);

        // to compute the first output, compute the sum of all elements. sum1, sum2, and sum3 already have the sum of 1+6 and 2+5 and 3+4 respectively, so if we add them, we'll get the sum of all 6
        let output0_left = Self::add(sum1, sum2);
        let output0_right = Self::add(sum3, rows[0]);
        let output0 = Self::add(output0_left, output0_right);

        // apply twiddle factors. This is probably pushing the limit of how much we should do with this technique.
        // We probably shouldn't do a size-11 FFT with this technique, for example, because this block of multiplies would grow quadratically
        let (twiddles0_re, twiddles0_im) = Self::duplicate_complex_components(twiddles[0]);
        let (twiddles1_re, twiddles1_im) = Self::duplicate_complex_components(twiddles[1]);
        let (twiddles2_re, twiddles2_im) = Self::duplicate_complex_components(twiddles[2]);

        // Let's do a plain 7-point Dft
        // | X0 |   | W0  W0  W0  W0  W0  W0  W0  |   | x0 |
        // | X1 |   | W0  W1  W2  W3  W4  W5  W6  |   | x1 |
        // | X2 |   | W0  W2  W4  W6  W8  W10 W12 |   | x2 |
        // | X3 |   | W0  W3  W6  W9  W12 W15 W18 |   | x3 |
        // | X4 |   | W0  W4  W8  W12 W16 W20 W24 |   | x4 |
        // | X5 |   | W0  W5  W10 W15 W20 W25 W30 |   | x5 |
        // | X6 |   | W0  W6  W12 W18 W24 W30 W36 |   | x6 |
        // where Wn = exp(-2*pi*n/7) for a forward transform, and exp(+2*pi*n/7) for an inverse.

        // Next, take advantage of the fact that twiddle factor indexes for a size-7 Dft are cyclical mod 7
        // | X0 |   | W0  W0  W0  W0  W0  W0  W0  |   | x0 |
        // | X1 |   | W0  W1  W2  W3  W4  W5  W6  |   | x1 |
        // | X2 |   | W0  W2  W4  W6  W1  W3  W5  |   | x2 |
        // | X3 |   | W0  W3  W6  W2  W5  W1  W4  |   | x3 |
        // | X4 |   | W0  W4  W1  W5  W2  W6  W3  |   | x4 |
        // | X5 |   | W0  W5  W3  W1  W6  W4  W2  |   | x5 |
        // | X6 |   | W0  W6  W5  W4  W3  W2  W1  |   | x6 |

        // Finally, take advantage of the fact that for a size-7 Dft,
        // twiddles 4 through 6 are conjugates of twiddes 3 through 0 (Asterisk marks conjugates)
        // | X0 |   | W0  W0  W0  W0  W0  W0  W0  |   | x0 |
        // | X1 |   | W0  W1  W2  W3  W3* W2* W1* |   | x1 |
        // | X2 |   | W0  W2  W3* W1* W1  W3  W2* |   | x2 |
        // | X3 |   | W0  W3  W1* W2  W2* W1  W3* |   | x3 |
        // | X4 |   | W0  W3* W1  W2* W2  W1* W3  |   | x4 |
        // | X5 |   | W0  W2* W3  W1  W1* W3* W2  |   | x5 |
        // | X6 |   | W0  W1* W2* W3* W3  W2  W1  |   | x6 |

        let twiddled1_mid = Self::fmadd(twiddles0_re, sum1, rows[0]);
        let twiddled2_mid = Self::fmadd(twiddles1_re, sum1, rows[0]);
        let twiddled3_mid = Self::fmadd(twiddles2_re, sum1, rows[0]);
        let twiddled4_mid = Self::mul(twiddles2_im, rotated6);
        let twiddled5_mid = Self::mul(twiddles1_im, rotated6);
        let twiddled6_mid = Self::mul(twiddles0_im, rotated6);

        let twiddled1_mid2 = Self::fmadd(twiddles1_re, sum2, twiddled1_mid);
        let twiddled2_mid2 = Self::fmadd(twiddles2_re, sum2, twiddled2_mid);
        let twiddled3_mid2 = Self::fmadd(twiddles0_re, sum2, twiddled3_mid);
        let twiddled4_mid2 = Self::fnmadd(twiddles0_im, rotated5, twiddled4_mid); // fnmadd instead of fmadd because we're actually re-using twiddle0 here. remember that this algorithm is all about factoring out conjugated multiplications -- this negation of the twiddle0 imaginaries is a reflection of one of those conugations
        let twiddled5_mid2 = Self::fnmadd(twiddles2_im, rotated5, twiddled5_mid);
        let twiddled6_mid2 = Self::fmadd(twiddles1_im, rotated5, twiddled6_mid);

        let twiddled1 = Self::fmadd(twiddles2_re, sum3, twiddled1_mid2);
        let twiddled2 = Self::fmadd(twiddles0_re, sum3, twiddled2_mid2);
        let twiddled3 = Self::fmadd(twiddles1_re, sum3, twiddled3_mid2);
        let twiddled4 = Self::fmadd(twiddles1_im, rotated4, twiddled4_mid2);
        let twiddled5 = Self::fnmadd(twiddles0_im, rotated4, twiddled5_mid2);
        let twiddled6 = Self::fmadd(twiddles2_im, rotated4, twiddled6_mid2);

        // Post-processing to mix the twiddle factors between the rest of the output
        let [output1, output6] = Self::column_butterfly2([twiddled1, twiddled6]);
        let [output2, output5] = Self::column_butterfly2([twiddled2, twiddled5]);
        let [output3, output4] = Self::column_butterfly2([twiddled3, twiddled4]);

        [
            output0, output1, output2, output3, output4, output5, output6,
        ]
    }

    #[inline(always)]
    unsafe fn column_butterfly8(rows: [Self; 8], rotation: Rotation90<Self>) -> [Self; 8] {
        // Algorithm: 4x2 mixed radix

        // Size-4 FFTs down the columns
        let mid0 = Self::column_butterfly4([rows[0], rows[2], rows[4], rows[6]], rotation);
        let mut mid1 = Self::column_butterfly4([rows[1], rows[3], rows[5], rows[7]], rotation);

        // Apply twiddle factors
        mid1[1] = apply_butterfly8_twiddle1(mid1[1], rotation);
        mid1[2] = mid1[2].rotate90(rotation);
        mid1[3] = apply_butterfly8_twiddle3(mid1[3], rotation);

        // Transpose the data and do size-2 FFTs down the columns
        let [output0, output1] = Self::column_butterfly2([mid0[0], mid1[0]]);
        let [output2, output3] = Self::column_butterfly2([mid0[1], mid1[1]]);
        let [output4, output5] = Self::column_butterfly2([mid0[2], mid1[2]]);
        let [output6, output7] = Self::column_butterfly2([mid0[3], mid1[3]]);

        [
            output0, output2, output4, output6, output1, output3, output5, output7,
        ]
    }

    #[inline(always)]
    unsafe fn column_butterfly11(rows: [Self; 11], twiddles: [Self; 5]) -> [Self; 11] {
        // This algorithm is derived directly from the definition of the Dft of size 11
        // We'd theoretically have to do 100 complex multiplications for the Dft, but many of the twiddles we'd be multiplying by are conjugates of each other
        // By doing some algebra to expand the complex multiplications and factor out the real multiplications, we get this faster formula where we only do the equivalent of 9 multiplications

        // do some prep work before we can start applying twiddle factors
        let [sum1, diff10] = Self::column_butterfly2([rows[1], rows[10]]);
        let [sum2, diff9] = Self::column_butterfly2([rows[2], rows[9]]);
        let [sum3, diff8] = Self::column_butterfly2([rows[3], rows[8]]);
        let [sum4, diff7] = Self::column_butterfly2([rows[4], rows[7]]);
        let [sum5, diff6] = Self::column_butterfly2([rows[5], rows[6]]);

        let rotation = Self::make_rotation90(FftDirection::Inverse);
        let rotated10 = Self::rotate90(diff10, rotation);
        let rotated9 = Self::rotate90(diff9, rotation);
        let rotated8 = Self::rotate90(diff8, rotation);
        let rotated7 = Self::rotate90(diff7, rotation);
        let rotated6 = Self::rotate90(diff6, rotation);

        // to compute the first output, compute the sum of all elements. sum1, sum2, and sum3 already have the sum of 1+6 and 2+5 and 3+4 respectively, so if we add them, we'll get the sum of all 6
        let sum01 = Self::add(rows[0], sum1);
        let sum23 = Self::add(sum2, sum3);
        let sum45 = Self::add(sum4, sum5);
        let sum0123 = Self::add(sum01, sum23);
        let output0 = Self::add(sum0123, sum45);

        // apply twiddle factors. This is probably pushing the limit of how much we should do with this technique.
        // We probably shouldn't do a size-11 FFT with this technique, for example, because this block of multiplies would grow quadratically
        let (twiddles0_re, twiddles0_im) = Self::duplicate_complex_components(twiddles[0]);
        let (twiddles1_re, twiddles1_im) = Self::duplicate_complex_components(twiddles[1]);
        let (twiddles2_re, twiddles2_im) = Self::duplicate_complex_components(twiddles[2]);
        let (twiddles3_re, twiddles3_im) = Self::duplicate_complex_components(twiddles[3]);
        let (twiddles4_re, twiddles4_im) = Self::duplicate_complex_components(twiddles[4]);

        let twiddled1 = Self::fmadd(twiddles0_re, sum1, rows[0]);
        let twiddled2 = Self::fmadd(twiddles1_re, sum1, rows[0]);
        let twiddled3 = Self::fmadd(twiddles2_re, sum1, rows[0]);
        let twiddled4 = Self::fmadd(twiddles3_re, sum1, rows[0]);
        let twiddled5 = Self::fmadd(twiddles4_re, sum1, rows[0]);
        let twiddled6 = Self::mul(twiddles4_im, rotated10);
        let twiddled7 = Self::mul(twiddles3_im, rotated10);
        let twiddled8 = Self::mul(twiddles2_im, rotated10);
        let twiddled9 = Self::mul(twiddles1_im, rotated10);
        let twiddled10 = Self::mul(twiddles0_im, rotated10);

        let twiddled1 = Self::fmadd(twiddles1_re, sum2, twiddled1);
        let twiddled2 = Self::fmadd(twiddles3_re, sum2, twiddled2);
        let twiddled3 = Self::fmadd(twiddles4_re, sum2, twiddled3);
        let twiddled4 = Self::fmadd(twiddles2_re, sum2, twiddled4);
        let twiddled5 = Self::fmadd(twiddles0_re, sum2, twiddled5);
        let twiddled6 = Self::fnmadd(twiddles0_im, rotated9, twiddled6);
        let twiddled7 = Self::fnmadd(twiddles2_im, rotated9, twiddled7);
        let twiddled8 = Self::fnmadd(twiddles4_im, rotated9, twiddled8);
        let twiddled9 = Self::fmadd(twiddles3_im, rotated9, twiddled9);
        let twiddled10 = Self::fmadd(twiddles1_im, rotated9, twiddled10);

        let twiddled1 = Self::fmadd(twiddles2_re, sum3, twiddled1);
        let twiddled2 = Self::fmadd(twiddles4_re, sum3, twiddled2);
        let twiddled3 = Self::fmadd(twiddles1_re, sum3, twiddled3);
        let twiddled4 = Self::fmadd(twiddles0_re, sum3, twiddled4);
        let twiddled5 = Self::fmadd(twiddles3_re, sum3, twiddled5);
        let twiddled6 = Self::fmadd(twiddles3_im, rotated8, twiddled6);
        let twiddled7 = Self::fmadd(twiddles0_im, rotated8, twiddled7);
        let twiddled8 = Self::fnmadd(twiddles1_im, rotated8, twiddled8);
        let twiddled9 = Self::fnmadd(twiddles4_im, rotated8, twiddled9);
        let twiddled10 = Self::fmadd(twiddles2_im, rotated8, twiddled10);

        let twiddled1 = Self::fmadd(twiddles3_re, sum4, twiddled1);
        let twiddled2 = Self::fmadd(twiddles2_re, sum4, twiddled2);
        let twiddled3 = Self::fmadd(twiddles0_re, sum4, twiddled3);
        let twiddled4 = Self::fmadd(twiddles4_re, sum4, twiddled4);
        let twiddled5 = Self::fmadd(twiddles1_re, sum4, twiddled5);
        let twiddled6 = Self::fnmadd(twiddles1_im, rotated7, twiddled6);
        let twiddled7 = Self::fmadd(twiddles4_im, rotated7, twiddled7);
        let twiddled8 = Self::fmadd(twiddles0_im, rotated7, twiddled8);
        let twiddled9 = Self::fnmadd(twiddles2_im, rotated7, twiddled9);
        let twiddled10 = Self::fmadd(twiddles3_im, rotated7, twiddled10);

        let twiddled1 = Self::fmadd(twiddles4_re, sum5, twiddled1);
        let twiddled2 = Self::fmadd(twiddles0_re, sum5, twiddled2);
        let twiddled3 = Self::fmadd(twiddles3_re, sum5, twiddled3);
        let twiddled4 = Self::fmadd(twiddles1_re, sum5, twiddled4);
        let twiddled5 = Self::fmadd(twiddles2_re, sum5, twiddled5);
        let twiddled6 = Self::fmadd(twiddles2_im, rotated6, twiddled6);
        let twiddled7 = Self::fnmadd(twiddles1_im, rotated6, twiddled7);
        let twiddled8 = Self::fmadd(twiddles3_im, rotated6, twiddled8);
        let twiddled9 = Self::fnmadd(twiddles0_im, rotated6, twiddled9);
        let twiddled10 = Self::fmadd(twiddles4_im, rotated6, twiddled10);

        // Post-processing to mix the twiddle factors between the rest of the output
        let [output1, output10] = Self::column_butterfly2([twiddled1, twiddled10]);
        let [output2, output9] = Self::column_butterfly2([twiddled2, twiddled9]);
        let [output3, output8] = Self::column_butterfly2([twiddled3, twiddled8]);
        let [output4, output7] = Self::column_butterfly2([twiddled4, twiddled7]);
        let [output5, output6] = Self::column_butterfly2([twiddled5, twiddled6]);

        [
            output0, output1, output2, output3, output4, output5, output6, output7, output8,
            output9, output10,
        ]
    }
}

/// A 256-bit SIMD vector of complex numbers, stored with the real values and imaginary values interleaved.
/// Implemented for __m256, __m256d
///
/// This trait implements things specific to 256-types, like splitting a 256 vector into 128 vectors
/// For compiler-placation reasons, all interactions/awareness the scalar type go here
pub trait AvxVector256: AvxVector {
    type HalfVector: AvxVector128<FullVector = Self>;
    type ScalarType: AvxNum<VectorType = Self>;

    unsafe fn lo(self) -> Self::HalfVector;
    unsafe fn hi(self) -> Self::HalfVector;
    unsafe fn merge(lo: Self::HalfVector, hi: Self::HalfVector) -> Self;

    /// Fill a vector by repeating the provided complex number as many times as possible
    unsafe fn broadcast_complex_elements(value: Complex<Self::ScalarType>) -> Self;

    // loads/stores of complex numbers
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self;
    unsafe fn store_complex(ptr: *mut Complex<Self::ScalarType>, data: Self);

    // Gather 4 complex numbers (for f32) or 2 complex numbers (for f64) using 4 i32 indexes (for index32) or 4 i64 indexes (for index64).
    // For f32, there should be 1 index per complex. For f64, there should be 2 indexes, each duplicated
    // (So to load the complex<f64> at index 5 and 7, the index vector should contain 5,5,7,7. this api sucks but it's internal so whatever.)
    unsafe fn gather_complex_avx2_index32(
        ptr: *const Complex<Self::ScalarType>,
        indexes: __m128i,
    ) -> Self;
    unsafe fn gather_complex_avx2_index64(
        ptr: *const Complex<Self::ScalarType>,
        indexes: __m256i,
    ) -> Self;

    // loads/stores of partial vectors of complex numbers. When loading, empty elements are zeroed
    // unimplemented!() if Self::COMPLEX_PER_VECTOR is not greater than the partial count
    unsafe fn load_partial1_complex(ptr: *const Complex<Self::ScalarType>) -> Self::HalfVector;
    unsafe fn load_partial2_complex(ptr: *const Complex<Self::ScalarType>) -> Self::HalfVector;
    unsafe fn load_partial3_complex(ptr: *const Complex<Self::ScalarType>) -> Self;
    unsafe fn store_partial1_complex(ptr: *mut Complex<Self::ScalarType>, data: Self::HalfVector);
    unsafe fn store_partial2_complex(ptr: *mut Complex<Self::ScalarType>, data: Self::HalfVector);
    unsafe fn store_partial3_complex(ptr: *mut Complex<Self::ScalarType>, data: Self);

    #[inline(always)]
    unsafe fn column_butterfly6(rows: [Self; 6], twiddles: Self) -> [Self; 6] {
        // Algorithm: 3x2 good-thomas

        // Size-3 FFTs down the columns of our reordered array
        let mid0 = Self::column_butterfly3([rows[0], rows[2], rows[4]], twiddles);
        let mid1 = Self::column_butterfly3([rows[3], rows[5], rows[1]], twiddles);

        // We normally would put twiddle factors right here, but since this is good-thomas algorithm, we don't need twiddle factors

        // Transpose the data and do size-2 FFTs down the columns
        let [output0, output1] = Self::column_butterfly2([mid0[0], mid1[0]]);
        let [output2, output3] = Self::column_butterfly2([mid0[1], mid1[1]]);
        let [output4, output5] = Self::column_butterfly2([mid0[2], mid1[2]]);

        // Reorder into output
        [output0, output3, output4, output1, output2, output5]
    }

    #[inline(always)]
    unsafe fn column_butterfly9(
        rows: [Self; 9],
        twiddles: [Self; 3],
        butterfly3_twiddles: Self,
    ) -> [Self; 9] {
        // Algorithm: 3x3 mixed radix

        // Size-3 FFTs down the columns
        let mid0 = Self::column_butterfly3([rows[0], rows[3], rows[6]], butterfly3_twiddles);
        let mut mid1 = Self::column_butterfly3([rows[1], rows[4], rows[7]], butterfly3_twiddles);
        let mut mid2 = Self::column_butterfly3([rows[2], rows[5], rows[8]], butterfly3_twiddles);

        // Apply twiddle factors. Note that we're re-using twiddles[1]
        mid1[1] = Self::mul_complex(twiddles[0], mid1[1]);
        mid1[2] = Self::mul_complex(twiddles[1], mid1[2]);
        mid2[1] = Self::mul_complex(twiddles[1], mid2[1]);
        mid2[2] = Self::mul_complex(twiddles[2], mid2[2]);

        let [output0, output1, output2] =
            Self::column_butterfly3([mid0[0], mid1[0], mid2[0]], butterfly3_twiddles);
        let [output3, output4, output5] =
            Self::column_butterfly3([mid0[1], mid1[1], mid2[1]], butterfly3_twiddles);
        let [output6, output7, output8] =
            Self::column_butterfly3([mid0[2], mid1[2], mid2[2]], butterfly3_twiddles);

        [
            output0, output3, output6, output1, output4, output7, output2, output5, output8,
        ]
    }

    #[inline(always)]
    unsafe fn column_butterfly12(
        rows: [Self; 12],
        butterfly3_twiddles: Self,
        rotation: Rotation90<Self>,
    ) -> [Self; 12] {
        // Algorithm: 4x3 good-thomas

        // Size-4 FFTs down the columns of our reordered array
        let mid0 = Self::column_butterfly4([rows[0], rows[3], rows[6], rows[9]], rotation);
        let mid1 = Self::column_butterfly4([rows[4], rows[7], rows[10], rows[1]], rotation);
        let mid2 = Self::column_butterfly4([rows[8], rows[11], rows[2], rows[5]], rotation);

        // Since this is good-thomas algorithm, we don't need twiddle factors

        // Transpose the data and do size-2 FFTs down the columns
        let [output0, output1, output2] =
            Self::column_butterfly3([mid0[0], mid1[0], mid2[0]], butterfly3_twiddles);
        let [output3, output4, output5] =
            Self::column_butterfly3([mid0[1], mid1[1], mid2[1]], butterfly3_twiddles);
        let [output6, output7, output8] =
            Self::column_butterfly3([mid0[2], mid1[2], mid2[2]], butterfly3_twiddles);
        let [output9, output10, output11] =
            Self::column_butterfly3([mid0[3], mid1[3], mid2[3]], butterfly3_twiddles);

        [
            output0, output4, output8, output9, output1, output5, output6, output10, output2,
            output3, output7, output11,
        ]
    }
}

/// A 128-bit SIMD vector of complex numbers, stored with the real values and imaginary values interleaved.
/// Implemented for __m128, __m128d, but these are all oriented around AVX, so don't call methods on these from a SSE-only context
///
/// This trait implements things specific to 128-types, like merging 2 128 vectors into a 256 vector
pub trait AvxVector128: AvxVector {
    type FullVector: AvxVector256<HalfVector = Self>;

    unsafe fn merge(lo: Self, hi: Self) -> Self::FullVector;
    unsafe fn zero_extend(self) -> Self::FullVector;

    unsafe fn lo(input: Self::FullVector) -> Self;
    unsafe fn hi(input: Self::FullVector) -> Self;
    unsafe fn split(input: Self::FullVector) -> (Self, Self) {
        (Self::lo(input), Self::hi(input))
    }
    unsafe fn lo_rotation(input: Rotation90<Self::FullVector>) -> Rotation90<Self>;

    /// Fill a vector by repeating the provided complex number as many times as possible
    unsafe fn broadcast_complex_elements(
        value: Complex<<<Self as AvxVector128>::FullVector as AvxVector256>::ScalarType>,
    ) -> Self;

    // Gather 2 complex numbers (for f32) or 1 complex number (for f64) using 2 i32 indexes (for gather32) or 2 i64 indexes (for gather64).
    // For f32, there should be 1 index per complex. For f64, there should be 2 indexes, each duplicated
    // (So to load the complex<f64> at index 5, the index vector should contain 5,5. this api sucks but it's internal so whatever.)
    unsafe fn gather32_complex_avx2(
        ptr: *const Complex<<Self::FullVector as AvxVector256>::ScalarType>,
        indexes: __m128i,
    ) -> Self;
    unsafe fn gather64_complex_avx2(
        ptr: *const Complex<<Self::FullVector as AvxVector256>::ScalarType>,
        indexes: __m128i,
    ) -> Self;

    #[inline(always)]
    unsafe fn column_butterfly6(rows: [Self; 6], twiddles: Self::FullVector) -> [Self; 6] {
        // Algorithm: 3x2 good-thomas

        // if we merge some of our 128 registers into 256 registers, we can do 1 inner butterfly3 instead of 2
        let rows03 = Self::merge(rows[0], rows[3]);
        let rows25 = Self::merge(rows[2], rows[5]);
        let rows41 = Self::merge(rows[4], rows[1]);

        // Size-3 FFTs down the columns of our reordered array
        let mid = Self::FullVector::column_butterfly3([rows03, rows25, rows41], twiddles);

        // We normally would put twiddle factors right here, but since this is good-thomas algorithm, we don't need twiddle factors

        // we can't use our merged columns anymore. so split them back into half vectors
        let (mid0_0, mid1_0) = Self::split(mid[0]);
        let (mid0_1, mid1_1) = Self::split(mid[1]);
        let (mid0_2, mid1_2) = Self::split(mid[2]);

        // Transpose the data and do size-2 FFTs down the columns
        let [output0, output1] = Self::column_butterfly2([mid0_0, mid1_0]);
        let [output2, output3] = Self::column_butterfly2([mid0_1, mid1_1]);
        let [output4, output5] = Self::column_butterfly2([mid0_2, mid1_2]);

        // Reorder into output
        [output0, output3, output4, output1, output2, output5]
    }

    #[inline(always)]
    unsafe fn column_butterfly9(
        rows: [Self; 9],
        twiddles_merged: [Self::FullVector; 2],
        butterfly3_twiddles: Self::FullVector,
    ) -> [Self; 9] {
        // Algorithm: 3x3 mixed radix

        // if we merge some of our 128 registers into 256 registers, we can do 2 inner butterfly3's instead of 3
        let rows12 = Self::merge(rows[1], rows[2]);
        let rows45 = Self::merge(rows[4], rows[5]);
        let rows78 = Self::merge(rows[7], rows[8]);

        let mid0 =
            Self::column_butterfly3([rows[0], rows[3], rows[6]], Self::lo(butterfly3_twiddles));
        let mut mid12 =
            Self::FullVector::column_butterfly3([rows12, rows45, rows78], butterfly3_twiddles);

        // Apply twiddle factors. we're applying them on the merged set of vectors, so we need slightly different twiddle factors
        mid12[1] = Self::FullVector::mul_complex(twiddles_merged[0], mid12[1]);
        mid12[2] = Self::FullVector::mul_complex(twiddles_merged[1], mid12[2]);

        // we can't use our merged columns anymore. so split them back into half vectors
        let (mid1_0, mid2_0) = Self::split(mid12[0]);
        let (mid1_1, mid2_1) = Self::split(mid12[1]);
        let (mid1_2, mid2_2) = Self::split(mid12[2]);

        // Re-merge our half vectors into different, transposed full vectors. Thankfully the compiler is smart enough to combine these inserts and extracts into permutes
        let transposed12 = Self::merge(mid0[1], mid0[2]);
        let transposed45 = Self::merge(mid1_1, mid1_2);
        let transposed78 = Self::merge(mid2_1, mid2_2);

        let [output0, output1, output2] =
            Self::column_butterfly3([mid0[0], mid1_0, mid2_0], Self::lo(butterfly3_twiddles));
        let [output36, output47, output58] = Self::FullVector::column_butterfly3(
            [transposed12, transposed45, transposed78],
            butterfly3_twiddles,
        );

        // Finally, extract our second set of merged columns
        let (output3, output6) = Self::split(output36);
        let (output4, output7) = Self::split(output47);
        let (output5, output8) = Self::split(output58);

        [
            output0, output3, output6, output1, output4, output7, output2, output5, output8,
        ]
    }

    #[inline(always)]
    unsafe fn column_butterfly12(
        rows: [Self; 12],
        butterfly3_twiddles: Self::FullVector,
        rotation: Rotation90<Self::FullVector>,
    ) -> [Self; 12] {
        // Algorithm: 4x3 good-thomas

        // if we merge some of our 128 registers into 256 registers, we can do 2 inner butterfly4's instead of 3
        let rows48 = Self::merge(rows[4], rows[8]);
        let rows711 = Self::merge(rows[7], rows[11]);
        let rows102 = Self::merge(rows[10], rows[2]);
        let rows15 = Self::merge(rows[1], rows[5]);

        // Size-4 FFTs down the columns of our reordered array
        let mid0 = Self::column_butterfly4(
            [rows[0], rows[3], rows[6], rows[9]],
            Self::lo_rotation(rotation),
        );
        let mid12 =
            Self::FullVector::column_butterfly4([rows48, rows711, rows102, rows15], rotation);

        // We normally would put twiddle factors right here, but since this is good-thomas algorithm, we don't need twiddle factors

        // we can't use our merged columns anymore. so split them back into half vectors
        let (mid1_0, mid2_0) = Self::split(mid12[0]);
        let (mid1_1, mid2_1) = Self::split(mid12[1]);
        let (mid1_2, mid2_2) = Self::split(mid12[2]);
        let (mid1_3, mid2_3) = Self::split(mid12[3]);

        // Re-merge our half vectors into different, transposed full vectors. This will let us do 2 inner butterfly 3's instead of 4!
        // Thankfully the compiler is smart enough to combine these inserts and extracts into permutes
        let transposed03 = Self::merge(mid0[0], mid0[1]);
        let transposed14 = Self::merge(mid1_0, mid1_1);
        let transposed25 = Self::merge(mid2_0, mid2_1);

        let transposed69 = Self::merge(mid0[2], mid0[3]);
        let transposed710 = Self::merge(mid1_2, mid1_3);
        let transposed811 = Self::merge(mid2_2, mid2_3);

        // Transpose the data and do size-2 FFTs down the columns
        let [output03, output14, output25] = Self::FullVector::column_butterfly3(
            [transposed03, transposed14, transposed25],
            butterfly3_twiddles,
        );
        let [output69, output710, output811] = Self::FullVector::column_butterfly3(
            [transposed69, transposed710, transposed811],
            butterfly3_twiddles,
        );

        // Finally, extract our second set of merged columns
        let (output0, output3) = Self::split(output03);
        let (output1, output4) = Self::split(output14);
        let (output2, output5) = Self::split(output25);
        let (output6, output9) = Self::split(output69);
        let (output7, output10) = Self::split(output710);
        let (output8, output11) = Self::split(output811);

        [
            output0, output4, output8, output9, output1, output5, output6, output10, output2,
            output3, output7, output11,
        ]
    }
}

#[inline(always)]
pub unsafe fn apply_butterfly8_twiddle1<V: AvxVector>(input: V, rotation: Rotation90<V>) -> V {
    let rotated = input.rotate90(rotation);
    let combined = V::add(rotated, input);
    V::mul(V::half_root2(), combined)
}
#[inline(always)]
pub unsafe fn apply_butterfly8_twiddle3<V: AvxVector>(input: V, rotation: Rotation90<V>) -> V {
    let rotated = input.rotate90(rotation);
    let combined = V::sub(rotated, input);
    V::mul(V::half_root2(), combined)
}

#[repr(transparent)]
#[derive(Clone, Copy, Debug)]
pub struct Rotation90<V>(V);
impl<V: AvxVector256> Rotation90<V> {
    #[inline(always)]
    pub unsafe fn lo(self) -> Rotation90<V::HalfVector> {
        Rotation90(self.0.lo())
    }
}

impl AvxVector for __m256 {
    const COMPLEX_PER_VECTOR: usize = 4;

    #[inline(always)]
    unsafe fn zero() -> Self {
        _mm256_setzero_ps()
    }
    #[inline(always)]
    unsafe fn half_root2() -> Self {
        // note: we're computing a square root here, but checking the assembly says the compiler is smart enough to turn this into a constant
        _mm256_broadcast_ss(&0.5f32.sqrt())
    }

    #[inline(always)]
    unsafe fn xor(left: Self, right: Self) -> Self {
        _mm256_xor_ps(left, right)
    }
    #[inline(always)]
    unsafe fn neg(self) -> Self {
        _mm256_xor_ps(self, _mm256_broadcast_ss(&-0.0))
    }
    #[inline(always)]
    unsafe fn add(left: Self, right: Self) -> Self {
        _mm256_add_ps(left, right)
    }
    #[inline(always)]
    unsafe fn sub(left: Self, right: Self) -> Self {
        _mm256_sub_ps(left, right)
    }
    #[inline(always)]
    unsafe fn mul(left: Self, right: Self) -> Self {
        _mm256_mul_ps(left, right)
    }
    #[inline(always)]
    unsafe fn fmadd(left: Self, right: Self, add: Self) -> Self {
        _mm256_fmadd_ps(left, right, add)
    }
    #[inline(always)]
    unsafe fn fnmadd(left: Self, right: Self, add: Self) -> Self {
        _mm256_fnmadd_ps(left, right, add)
    }
    #[inline(always)]
    unsafe fn fmaddsub(left: Self, right: Self, add: Self) -> Self {
        _mm256_fmaddsub_ps(left, right, add)
    }
    #[inline(always)]
    unsafe fn fmsubadd(left: Self, right: Self, add: Self) -> Self {
        _mm256_fmsubadd_ps(left, right, add)
    }
    #[inline(always)]
    unsafe fn reverse_complex_elements(self) -> Self {
        // swap the elements in-lane
        let permuted = _mm256_permute_ps(self, 0x4E);
        // swap the lanes
        _mm256_permute2f128_ps(permuted, permuted, 0x01)
    }
    #[inline(always)]
    unsafe fn unpacklo_complex(rows: [Self; 2]) -> Self {
        let row0_double = _mm256_castps_pd(rows[0]);
        let row1_double = _mm256_castps_pd(rows[1]);
        let unpacked = _mm256_unpacklo_pd(row0_double, row1_double);
        _mm256_castpd_ps(unpacked)
    }
    #[inline(always)]
    unsafe fn unpackhi_complex(rows: [Self; 2]) -> Self {
        let row0_double = _mm256_castps_pd(rows[0]);
        let row1_double = _mm256_castps_pd(rows[1]);
        let unpacked = _mm256_unpackhi_pd(row0_double, row1_double);
        _mm256_castpd_ps(unpacked)
    }

    #[inline(always)]
    unsafe fn swap_complex_components(self) -> Self {
        _mm256_permute_ps(self, 0xB1)
    }

    #[inline(always)]
    unsafe fn duplicate_complex_components(self) -> (Self, Self) {
        (_mm256_moveldup_ps(self), _mm256_movehdup_ps(self))
    }

    #[inline(always)]
    unsafe fn make_rotation90(direction: FftDirection) -> Rotation90<Self> {
        let broadcast = match direction {
            FftDirection::Forward => Complex::new(-0.0, 0.0),
            FftDirection::Inverse => Complex::new(0.0, -0.0),
        };
        Rotation90(Self::broadcast_complex_elements(broadcast))
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
    unsafe fn broadcast_twiddle(index: usize, len: usize, direction: FftDirection) -> Self {
        Self::broadcast_complex_elements(twiddles::compute_twiddle(index, len, direction))
    }

    #[inline(always)]
    unsafe fn transpose2_packed(rows: [Self; 2]) -> [Self; 2] {
        let unpacked = Self::unpack_complex(rows);
        let output0 = _mm256_permute2f128_ps(unpacked[0], unpacked[1], 0x20);
        let output1 = _mm256_permute2f128_ps(unpacked[0], unpacked[1], 0x31);

        [output0, output1]
    }
    #[inline(always)]
    unsafe fn transpose3_packed(rows: [Self; 3]) -> [Self; 3] {
        let unpacked0 = Self::unpacklo_complex([rows[0], rows[1]]);
        let unpacked2 = Self::unpackhi_complex([rows[1], rows[2]]);

        // output0 and output2 each need to swap some elements. thankfully we can blend those elements into the same intermediate value, and then do a permute 128 from there
        let blended = _mm256_blend_ps(rows[0], rows[2], 0x33);

        let output1 = _mm256_permute2f128_ps(unpacked0, unpacked2, 0x12);

        let output0 = _mm256_permute2f128_ps(unpacked0, blended, 0x20);
        let output2 = _mm256_permute2f128_ps(unpacked2, blended, 0x13);

        [output0, output1, output2]
    }
    #[inline(always)]
    unsafe fn transpose4_packed(rows: [Self; 4]) -> [Self; 4] {
        let permute0 = _mm256_permute2f128_ps(rows[0], rows[2], 0x20);
        let permute1 = _mm256_permute2f128_ps(rows[1], rows[3], 0x20);
        let permute2 = _mm256_permute2f128_ps(rows[0], rows[2], 0x31);
        let permute3 = _mm256_permute2f128_ps(rows[1], rows[3], 0x31);

        let [unpacked0, unpacked1] = Self::unpack_complex([permute0, permute1]);
        let [unpacked2, unpacked3] = Self::unpack_complex([permute2, permute3]);

        [unpacked0, unpacked1, unpacked2, unpacked3]
    }
    #[inline(always)]
    unsafe fn transpose5_packed(rows: [Self; 5]) -> [Self; 5] {
        let unpacked0 = Self::unpacklo_complex([rows[0], rows[1]]);
        let unpacked1 = Self::unpackhi_complex([rows[1], rows[2]]);
        let unpacked2 = Self::unpacklo_complex([rows[2], rows[3]]);
        let unpacked3 = Self::unpackhi_complex([rows[3], rows[4]]);
        let blended04 = _mm256_blend_ps(rows[0], rows[4], 0x33);

        [
            _mm256_permute2f128_ps(unpacked0, unpacked2, 0x20),
            _mm256_permute2f128_ps(blended04, unpacked1, 0x20),
            _mm256_blend_ps(unpacked0, unpacked3, 0x0f),
            _mm256_permute2f128_ps(unpacked2, blended04, 0x31),
            _mm256_permute2f128_ps(unpacked1, unpacked3, 0x31),
        ]
    }
    #[inline(always)]
    unsafe fn transpose6_packed(rows: [Self; 6]) -> [Self; 6] {
        let [unpacked0, unpacked1] = Self::unpack_complex([rows[0], rows[1]]);
        let [unpacked2, unpacked3] = Self::unpack_complex([rows[2], rows[3]]);
        let [unpacked4, unpacked5] = Self::unpack_complex([rows[4], rows[5]]);

        [
            _mm256_permute2f128_ps(unpacked0, unpacked2, 0x20),
            _mm256_permute2f128_ps(unpacked1, unpacked4, 0x02),
            _mm256_permute2f128_ps(unpacked3, unpacked5, 0x20),
            _mm256_permute2f128_ps(unpacked0, unpacked2, 0x31),
            _mm256_permute2f128_ps(unpacked1, unpacked4, 0x13),
            _mm256_permute2f128_ps(unpacked3, unpacked5, 0x31),
        ]
    }
    #[inline(always)]
    unsafe fn transpose7_packed(rows: [Self; 7]) -> [Self; 7] {
        let unpacked0 = Self::unpacklo_complex([rows[0], rows[1]]);
        let unpacked1 = Self::unpackhi_complex([rows[1], rows[2]]);
        let unpacked2 = Self::unpacklo_complex([rows[2], rows[3]]);
        let unpacked3 = Self::unpackhi_complex([rows[3], rows[4]]);
        let unpacked4 = Self::unpacklo_complex([rows[4], rows[5]]);
        let unpacked5 = Self::unpackhi_complex([rows[5], rows[6]]);
        let blended06 = _mm256_blend_ps(rows[0], rows[6], 0x33);

        [
            _mm256_permute2f128_ps(unpacked0, unpacked2, 0x20),
            _mm256_permute2f128_ps(unpacked4, blended06, 0x20),
            _mm256_permute2f128_ps(unpacked1, unpacked3, 0x20),
            _mm256_blend_ps(unpacked0, unpacked5, 0x0f),
            _mm256_permute2f128_ps(unpacked2, unpacked4, 0x31),
            _mm256_permute2f128_ps(blended06, unpacked1, 0x31),
            _mm256_permute2f128_ps(unpacked3, unpacked5, 0x31),
        ]
    }
    #[inline(always)]
    unsafe fn transpose8_packed(rows: [Self; 8]) -> [Self; 8] {
        let chunk0 = [rows[0], rows[1], rows[2], rows[3]];
        let chunk1 = [rows[4], rows[5], rows[6], rows[7]];

        let output0 = Self::transpose4_packed(chunk0);
        let output1 = Self::transpose4_packed(chunk1);

        [
            output0[0], output1[0], output0[1], output1[1], output0[2], output1[2], output0[3],
            output1[3],
        ]
    }
    #[inline(always)]
    unsafe fn transpose9_packed(rows: [Self; 9]) -> [Self; 9] {
        let unpacked0 = Self::unpacklo_complex([rows[0], rows[1]]);
        let unpacked1 = Self::unpackhi_complex([rows[1], rows[2]]);
        let unpacked2 = Self::unpacklo_complex([rows[2], rows[3]]);
        let unpacked3 = Self::unpackhi_complex([rows[3], rows[4]]);
        let unpacked5 = Self::unpacklo_complex([rows[4], rows[5]]);
        let unpacked6 = Self::unpackhi_complex([rows[5], rows[6]]);
        let unpacked7 = Self::unpacklo_complex([rows[6], rows[7]]);
        let unpacked8 = Self::unpackhi_complex([rows[7], rows[8]]);
        let blended9 = _mm256_blend_ps(rows[0], rows[8], 0x33);

        [
            _mm256_permute2f128_ps(unpacked0, unpacked2, 0x20),
            _mm256_permute2f128_ps(unpacked5, unpacked7, 0x20),
            _mm256_permute2f128_ps(blended9, unpacked1, 0x20),
            _mm256_permute2f128_ps(unpacked3, unpacked6, 0x20),
            _mm256_blend_ps(unpacked0, unpacked8, 0x0f),
            _mm256_permute2f128_ps(unpacked2, unpacked5, 0x31),
            _mm256_permute2f128_ps(unpacked7, blended9, 0x31),
            _mm256_permute2f128_ps(unpacked1, unpacked3, 0x31),
            _mm256_permute2f128_ps(unpacked6, unpacked8, 0x31),
        ]
    }
    #[inline(always)]
    unsafe fn transpose11_packed(rows: [Self; 11]) -> [Self; 11] {
        let unpacked0 = Self::unpacklo_complex([rows[0], rows[1]]);
        let unpacked1 = Self::unpackhi_complex([rows[1], rows[2]]);
        let unpacked2 = Self::unpacklo_complex([rows[2], rows[3]]);
        let unpacked3 = Self::unpackhi_complex([rows[3], rows[4]]);
        let unpacked4 = Self::unpacklo_complex([rows[4], rows[5]]);
        let unpacked5 = Self::unpackhi_complex([rows[5], rows[6]]);
        let unpacked6 = Self::unpacklo_complex([rows[6], rows[7]]);
        let unpacked7 = Self::unpackhi_complex([rows[7], rows[8]]);
        let unpacked8 = Self::unpacklo_complex([rows[8], rows[9]]);
        let unpacked9 = Self::unpackhi_complex([rows[9], rows[10]]);
        let blended10 = _mm256_blend_ps(rows[0], rows[10], 0x33);

        [
            _mm256_permute2f128_ps(unpacked0, unpacked2, 0x20),
            _mm256_permute2f128_ps(unpacked4, unpacked6, 0x20),
            _mm256_permute2f128_ps(unpacked8, blended10, 0x20),
            _mm256_permute2f128_ps(unpacked1, unpacked3, 0x20),
            _mm256_permute2f128_ps(unpacked5, unpacked7, 0x20),
            _mm256_blend_ps(unpacked0, unpacked9, 0x0f),
            _mm256_permute2f128_ps(unpacked2, unpacked4, 0x31),
            _mm256_permute2f128_ps(unpacked6, unpacked8, 0x31),
            _mm256_permute2f128_ps(blended10, unpacked1, 0x31),
            _mm256_permute2f128_ps(unpacked3, unpacked5, 0x31),
            _mm256_permute2f128_ps(unpacked7, unpacked9, 0x31),
        ]
    }
    #[inline(always)]
    unsafe fn transpose12_packed(rows: [Self; 12]) -> [Self; 12] {
        let chunk0 = [rows[0], rows[1], rows[2], rows[3]];
        let chunk1 = [rows[4], rows[5], rows[6], rows[7]];
        let chunk2 = [rows[8], rows[9], rows[10], rows[11]];

        let output0 = Self::transpose4_packed(chunk0);
        let output1 = Self::transpose4_packed(chunk1);
        let output2 = Self::transpose4_packed(chunk2);

        [
            output0[0], output1[0], output2[0], output0[1], output1[1], output2[1], output0[2],
            output1[2], output2[2], output0[3], output1[3], output2[3],
        ]
    }
    #[inline(always)]
    unsafe fn transpose16_packed(rows: [Self; 16]) -> [Self; 16] {
        let chunk0 = [
            rows[0], rows[1], rows[2], rows[3], rows[4], rows[5], rows[6], rows[7],
        ];
        let chunk1 = [
            rows[8], rows[9], rows[10], rows[11], rows[12], rows[13], rows[14], rows[15],
        ];

        let output0 = Self::transpose8_packed(chunk0);
        let output1 = Self::transpose8_packed(chunk1);

        [
            output0[0], output0[1], output1[0], output1[1], output0[2], output0[3], output1[2],
            output1[3], output0[4], output0[5], output1[4], output1[5], output0[6], output0[7],
            output1[6], output1[7],
        ]
    }
}
impl AvxVector256 for __m256 {
    type ScalarType = f32;
    type HalfVector = __m128;

    #[inline(always)]
    unsafe fn lo(self) -> Self::HalfVector {
        _mm256_castps256_ps128(self)
    }
    #[inline(always)]
    unsafe fn hi(self) -> Self::HalfVector {
        _mm256_extractf128_ps(self, 1)
    }
    #[inline(always)]
    unsafe fn merge(lo: Self::HalfVector, hi: Self::HalfVector) -> Self {
        _mm256_insertf128_ps(_mm256_castps128_ps256(lo), hi, 1)
    }

    #[inline(always)]
    unsafe fn broadcast_complex_elements(value: Complex<Self::ScalarType>) -> Self {
        _mm256_set_ps(
            value.im, value.re, value.im, value.re, value.im, value.re, value.im, value.re,
        )
    }

    #[inline(always)]
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        _mm256_loadu_ps(ptr as *const Self::ScalarType)
    }
    #[inline(always)]
    unsafe fn store_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        _mm256_storeu_ps(ptr as *mut Self::ScalarType, data)
    }
    #[inline(always)]
    unsafe fn gather_complex_avx2_index32(
        ptr: *const Complex<Self::ScalarType>,
        indexes: __m128i,
    ) -> Self {
        _mm256_castpd_ps(_mm256_i32gather_pd(ptr as *const f64, indexes, 8))
    }
    #[inline(always)]
    unsafe fn gather_complex_avx2_index64(
        ptr: *const Complex<Self::ScalarType>,
        indexes: __m256i,
    ) -> Self {
        _mm256_castpd_ps(_mm256_i64gather_pd(ptr as *const f64, indexes, 8))
    }

    #[inline(always)]
    unsafe fn load_partial1_complex(ptr: *const Complex<Self::ScalarType>) -> Self::HalfVector {
        let data = _mm_load_sd(ptr as *const f64);
        _mm_castpd_ps(data)
    }
    #[inline(always)]
    unsafe fn load_partial2_complex(ptr: *const Complex<Self::ScalarType>) -> Self::HalfVector {
        _mm_loadu_ps(ptr as *const f32)
    }
    #[inline(always)]
    unsafe fn load_partial3_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        let lo = Self::load_partial2_complex(ptr);
        let hi = Self::load_partial1_complex(ptr.add(2));
        Self::merge(lo, hi)
    }
    #[inline(always)]
    unsafe fn store_partial1_complex(ptr: *mut Complex<Self::ScalarType>, data: Self::HalfVector) {
        _mm_store_sd(ptr as *mut f64, _mm_castps_pd(data));
    }
    #[inline(always)]
    unsafe fn store_partial2_complex(ptr: *mut Complex<Self::ScalarType>, data: Self::HalfVector) {
        _mm_storeu_ps(ptr as *mut f32, data);
    }
    #[inline(always)]
    unsafe fn store_partial3_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        Self::store_partial2_complex(ptr, data.lo());
        Self::store_partial1_complex(ptr.add(2), data.hi());
    }
}

impl AvxVector for __m128 {
    const COMPLEX_PER_VECTOR: usize = 2;

    #[inline(always)]
    unsafe fn zero() -> Self {
        _mm_setzero_ps()
    }
    #[inline(always)]
    unsafe fn half_root2() -> Self {
        // note: we're computing a square root here, but checking the assembly says the compiler is smart enough to turn this into a constant
        _mm_broadcast_ss(&0.5f32.sqrt())
    }

    #[inline(always)]
    unsafe fn xor(left: Self, right: Self) -> Self {
        _mm_xor_ps(left, right)
    }
    #[inline(always)]
    unsafe fn neg(self) -> Self {
        _mm_xor_ps(self, _mm_broadcast_ss(&-0.0))
    }
    #[inline(always)]
    unsafe fn add(left: Self, right: Self) -> Self {
        _mm_add_ps(left, right)
    }
    #[inline(always)]
    unsafe fn sub(left: Self, right: Self) -> Self {
        _mm_sub_ps(left, right)
    }
    #[inline(always)]
    unsafe fn mul(left: Self, right: Self) -> Self {
        _mm_mul_ps(left, right)
    }
    #[inline(always)]
    unsafe fn fmadd(left: Self, right: Self, add: Self) -> Self {
        _mm_fmadd_ps(left, right, add)
    }
    #[inline(always)]
    unsafe fn fnmadd(left: Self, right: Self, add: Self) -> Self {
        _mm_fnmadd_ps(left, right, add)
    }
    #[inline(always)]
    unsafe fn fmaddsub(left: Self, right: Self, add: Self) -> Self {
        _mm_fmaddsub_ps(left, right, add)
    }
    #[inline(always)]
    unsafe fn fmsubadd(left: Self, right: Self, add: Self) -> Self {
        _mm_fmsubadd_ps(left, right, add)
    }

    #[inline(always)]
    unsafe fn reverse_complex_elements(self) -> Self {
        // swap the elements in-lane
        _mm_permute_ps(self, 0x4E)
    }

    #[inline(always)]
    unsafe fn unpacklo_complex(rows: [Self; 2]) -> Self {
        let row0_double = _mm_castps_pd(rows[0]);
        let row1_double = _mm_castps_pd(rows[1]);
        let unpacked = _mm_unpacklo_pd(row0_double, row1_double);
        _mm_castpd_ps(unpacked)
    }
    #[inline(always)]
    unsafe fn unpackhi_complex(rows: [Self; 2]) -> Self {
        let row0_double = _mm_castps_pd(rows[0]);
        let row1_double = _mm_castps_pd(rows[1]);
        let unpacked = _mm_unpackhi_pd(row0_double, row1_double);
        _mm_castpd_ps(unpacked)
    }

    #[inline(always)]
    unsafe fn swap_complex_components(self) -> Self {
        _mm_permute_ps(self, 0xB1)
    }
    #[inline(always)]
    unsafe fn duplicate_complex_components(self) -> (Self, Self) {
        (_mm_moveldup_ps(self), _mm_movehdup_ps(self))
    }

    #[inline(always)]
    unsafe fn make_rotation90(direction: FftDirection) -> Rotation90<Self> {
        let broadcast = match direction {
            FftDirection::Forward => Complex::new(-0.0, 0.0),
            FftDirection::Inverse => Complex::new(0.0, -0.0),
        };
        Rotation90(Self::broadcast_complex_elements(broadcast))
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

        _mm_loadu_ps(twiddle_chunk.as_ptr() as *const f32)
    }
    #[inline(always)]
    unsafe fn broadcast_twiddle(index: usize, len: usize, direction: FftDirection) -> Self {
        Self::broadcast_complex_elements(twiddles::compute_twiddle(index, len, direction))
    }

    #[inline(always)]
    unsafe fn transpose2_packed(rows: [Self; 2]) -> [Self; 2] {
        Self::unpack_complex(rows)
    }
    #[inline(always)]
    unsafe fn transpose3_packed(rows: [Self; 3]) -> [Self; 3] {
        let unpacked0 = Self::unpacklo_complex([rows[0], rows[1]]);
        let blended = _mm_blend_ps(rows[0], rows[2], 0x03);
        let unpacked2 = Self::unpackhi_complex([rows[1], rows[2]]);

        [unpacked0, blended, unpacked2]
    }
    #[inline(always)]
    unsafe fn transpose4_packed(rows: [Self; 4]) -> [Self; 4] {
        let [unpacked0, unpacked1] = Self::unpack_complex([rows[0], rows[1]]);
        let [unpacked2, unpacked3] = Self::unpack_complex([rows[2], rows[3]]);

        [unpacked0, unpacked2, unpacked1, unpacked3]
    }
    #[inline(always)]
    unsafe fn transpose5_packed(rows: [Self; 5]) -> [Self; 5] {
        [
            Self::unpacklo_complex([rows[0], rows[1]]),
            Self::unpacklo_complex([rows[2], rows[3]]),
            _mm_blend_ps(rows[0], rows[4], 0x03),
            Self::unpackhi_complex([rows[1], rows[2]]),
            Self::unpackhi_complex([rows[3], rows[4]]),
        ]
    }
    #[inline(always)]
    unsafe fn transpose6_packed(rows: [Self; 6]) -> [Self; 6] {
        let [unpacked0, unpacked1] = Self::unpack_complex([rows[0], rows[1]]);
        let [unpacked2, unpacked3] = Self::unpack_complex([rows[2], rows[3]]);
        let [unpacked4, unpacked5] = Self::unpack_complex([rows[4], rows[5]]);

        [
            unpacked0, unpacked2, unpacked4, unpacked1, unpacked3, unpacked5,
        ]
    }
    #[inline(always)]
    unsafe fn transpose7_packed(rows: [Self; 7]) -> [Self; 7] {
        [
            Self::unpacklo_complex([rows[0], rows[1]]),
            Self::unpacklo_complex([rows[2], rows[3]]),
            Self::unpacklo_complex([rows[4], rows[5]]),
            _mm_shuffle_ps(rows[6], rows[0], 0xE4),
            Self::unpackhi_complex([rows[1], rows[2]]),
            Self::unpackhi_complex([rows[3], rows[4]]),
            Self::unpackhi_complex([rows[5], rows[6]]),
        ]
    }
    #[inline(always)]
    unsafe fn transpose8_packed(rows: [Self; 8]) -> [Self; 8] {
        let chunk0 = [rows[0], rows[1], rows[2], rows[3]];
        let chunk1 = [rows[4], rows[5], rows[6], rows[7]];

        let output0 = Self::transpose4_packed(chunk0);
        let output1 = Self::transpose4_packed(chunk1);

        [
            output0[0], output0[1], output1[0], output1[1], output0[2], output0[3], output1[2],
            output1[3],
        ]
    }
    #[inline(always)]
    unsafe fn transpose9_packed(rows: [Self; 9]) -> [Self; 9] {
        [
            Self::unpacklo_complex([rows[0], rows[1]]),
            Self::unpacklo_complex([rows[2], rows[3]]),
            Self::unpacklo_complex([rows[4], rows[5]]),
            Self::unpacklo_complex([rows[6], rows[7]]),
            _mm_shuffle_ps(rows[8], rows[0], 0xE4),
            Self::unpackhi_complex([rows[1], rows[2]]),
            Self::unpackhi_complex([rows[3], rows[4]]),
            Self::unpackhi_complex([rows[5], rows[6]]),
            Self::unpackhi_complex([rows[7], rows[8]]),
        ]
    }
    #[inline(always)]
    unsafe fn transpose11_packed(rows: [Self; 11]) -> [Self; 11] {
        [
            Self::unpacklo_complex([rows[0], rows[1]]),
            Self::unpacklo_complex([rows[2], rows[3]]),
            Self::unpacklo_complex([rows[4], rows[5]]),
            Self::unpacklo_complex([rows[6], rows[7]]),
            Self::unpacklo_complex([rows[8], rows[9]]),
            _mm_shuffle_ps(rows[10], rows[0], 0xE4),
            Self::unpackhi_complex([rows[1], rows[2]]),
            Self::unpackhi_complex([rows[3], rows[4]]),
            Self::unpackhi_complex([rows[5], rows[6]]),
            Self::unpackhi_complex([rows[7], rows[8]]),
            Self::unpackhi_complex([rows[9], rows[10]]),
        ]
    }
    #[inline(always)]
    unsafe fn transpose12_packed(rows: [Self; 12]) -> [Self; 12] {
        let chunk0 = [rows[0], rows[1], rows[2], rows[3]];
        let chunk1 = [rows[4], rows[5], rows[6], rows[7]];
        let chunk2 = [rows[8], rows[9], rows[10], rows[11]];

        let output0 = Self::transpose4_packed(chunk0);
        let output1 = Self::transpose4_packed(chunk1);
        let output2 = Self::transpose4_packed(chunk2);

        [
            output0[0], output0[1], output1[0], output1[1], output2[0], output2[1], output0[2],
            output0[3], output1[2], output1[3], output2[2], output2[3],
        ]
    }
    #[inline(always)]
    unsafe fn transpose16_packed(rows: [Self; 16]) -> [Self; 16] {
        let chunk0 = [
            rows[0], rows[1], rows[2], rows[3], rows[4], rows[5], rows[6], rows[7],
        ];
        let chunk1 = [
            rows[8], rows[9], rows[10], rows[11], rows[12], rows[13], rows[14], rows[15],
        ];

        let output0 = Self::transpose8_packed(chunk0);
        let output1 = Self::transpose8_packed(chunk1);

        [
            output0[0], output0[1], output0[2], output0[3], output1[0], output1[1], output1[2],
            output1[3], output0[4], output0[5], output0[6], output0[7], output1[4], output1[5],
            output1[6], output1[7],
        ]
    }
}
impl AvxVector128 for __m128 {
    type FullVector = __m256;

    #[inline(always)]
    unsafe fn lo(input: Self::FullVector) -> Self {
        _mm256_castps256_ps128(input)
    }
    #[inline(always)]
    unsafe fn hi(input: Self::FullVector) -> Self {
        _mm256_extractf128_ps(input, 1)
    }
    #[inline(always)]
    unsafe fn merge(lo: Self, hi: Self) -> Self::FullVector {
        _mm256_insertf128_ps(_mm256_castps128_ps256(lo), hi, 1)
    }
    #[inline(always)]
    unsafe fn zero_extend(self) -> Self::FullVector {
        _mm256_zextps128_ps256(self)
    }
    #[inline(always)]
    unsafe fn lo_rotation(input: Rotation90<Self::FullVector>) -> Rotation90<Self> {
        input.lo()
    }
    #[inline(always)]
    unsafe fn broadcast_complex_elements(value: Complex<f32>) -> Self {
        _mm_set_ps(value.im, value.re, value.im, value.re)
    }
    #[inline(always)]
    unsafe fn gather32_complex_avx2(ptr: *const Complex<f32>, indexes: __m128i) -> Self {
        _mm_castpd_ps(_mm_i32gather_pd(ptr as *const f64, indexes, 8))
    }
    #[inline(always)]
    unsafe fn gather64_complex_avx2(ptr: *const Complex<f32>, indexes: __m128i) -> Self {
        _mm_castpd_ps(_mm_i64gather_pd(ptr as *const f64, indexes, 8))
    }
}

impl AvxVector for __m256d {
    const COMPLEX_PER_VECTOR: usize = 2;

    #[inline(always)]
    unsafe fn zero() -> Self {
        _mm256_setzero_pd()
    }
    #[inline(always)]
    unsafe fn half_root2() -> Self {
        // note: we're computing a square root here, but checking the assembly says the compiler is smart enough to turn this into a constant
        _mm256_broadcast_sd(&0.5f64.sqrt())
    }

    #[inline(always)]
    unsafe fn xor(left: Self, right: Self) -> Self {
        _mm256_xor_pd(left, right)
    }
    #[inline(always)]
    unsafe fn neg(self) -> Self {
        _mm256_xor_pd(self, _mm256_broadcast_sd(&-0.0))
    }
    #[inline(always)]
    unsafe fn add(left: Self, right: Self) -> Self {
        _mm256_add_pd(left, right)
    }
    #[inline(always)]
    unsafe fn sub(left: Self, right: Self) -> Self {
        _mm256_sub_pd(left, right)
    }
    #[inline(always)]
    unsafe fn mul(left: Self, right: Self) -> Self {
        _mm256_mul_pd(left, right)
    }
    #[inline(always)]
    unsafe fn fmadd(left: Self, right: Self, add: Self) -> Self {
        _mm256_fmadd_pd(left, right, add)
    }
    #[inline(always)]
    unsafe fn fnmadd(left: Self, right: Self, add: Self) -> Self {
        _mm256_fnmadd_pd(left, right, add)
    }
    #[inline(always)]
    unsafe fn fmaddsub(left: Self, right: Self, add: Self) -> Self {
        _mm256_fmaddsub_pd(left, right, add)
    }
    #[inline(always)]
    unsafe fn fmsubadd(left: Self, right: Self, add: Self) -> Self {
        _mm256_fmsubadd_pd(left, right, add)
    }

    #[inline(always)]
    unsafe fn reverse_complex_elements(self) -> Self {
        _mm256_permute2f128_pd(self, self, 0x01)
    }
    #[inline(always)]
    unsafe fn unpacklo_complex(rows: [Self; 2]) -> Self {
        _mm256_permute2f128_pd(rows[0], rows[1], 0x20)
    }
    #[inline(always)]
    unsafe fn unpackhi_complex(rows: [Self; 2]) -> Self {
        _mm256_permute2f128_pd(rows[0], rows[1], 0x31)
    }

    #[inline(always)]
    unsafe fn swap_complex_components(self) -> Self {
        _mm256_permute_pd(self, 0x05)
    }
    #[inline(always)]
    unsafe fn duplicate_complex_components(self) -> (Self, Self) {
        (_mm256_movedup_pd(self), _mm256_permute_pd(self, 0x0F))
    }

    #[inline(always)]
    unsafe fn make_rotation90(direction: FftDirection) -> Rotation90<Self> {
        let broadcast = match direction {
            FftDirection::Forward => Complex::new(-0.0, 0.0),
            FftDirection::Inverse => Complex::new(0.0, -0.0),
        };
        Rotation90(Self::broadcast_complex_elements(broadcast))
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
    unsafe fn broadcast_twiddle(index: usize, len: usize, direction: FftDirection) -> Self {
        Self::broadcast_complex_elements(twiddles::compute_twiddle(index, len, direction))
    }

    #[inline(always)]
    unsafe fn transpose2_packed(rows: [Self; 2]) -> [Self; 2] {
        Self::unpack_complex(rows)
    }
    #[inline(always)]
    unsafe fn transpose3_packed(rows: [Self; 3]) -> [Self; 3] {
        let unpacked0 = Self::unpacklo_complex([rows[0], rows[1]]);
        let blended = _mm256_blend_pd(rows[0], rows[2], 0x03);
        let unpacked2 = Self::unpackhi_complex([rows[1], rows[2]]);

        [unpacked0, blended, unpacked2]
    }
    #[inline(always)]
    unsafe fn transpose4_packed(rows: [Self; 4]) -> [Self; 4] {
        let [unpacked0, unpacked1] = Self::unpack_complex([rows[0], rows[1]]);
        let [unpacked2, unpacked3] = Self::unpack_complex([rows[2], rows[3]]);

        [unpacked0, unpacked2, unpacked1, unpacked3]
    }
    #[inline(always)]
    unsafe fn transpose5_packed(rows: [Self; 5]) -> [Self; 5] {
        [
            Self::unpacklo_complex([rows[0], rows[1]]),
            Self::unpacklo_complex([rows[2], rows[3]]),
            _mm256_blend_pd(rows[0], rows[4], 0x03),
            Self::unpackhi_complex([rows[1], rows[2]]),
            Self::unpackhi_complex([rows[3], rows[4]]),
        ]
    }
    #[inline(always)]
    unsafe fn transpose6_packed(rows: [Self; 6]) -> [Self; 6] {
        let [unpacked0, unpacked1] = Self::unpack_complex([rows[0], rows[1]]);
        let [unpacked2, unpacked3] = Self::unpack_complex([rows[2], rows[3]]);
        let [unpacked4, unpacked5] = Self::unpack_complex([rows[4], rows[5]]);

        [
            unpacked0, unpacked2, unpacked4, unpacked1, unpacked3, unpacked5,
        ]
    }
    #[inline(always)]
    unsafe fn transpose7_packed(rows: [Self; 7]) -> [Self; 7] {
        [
            Self::unpacklo_complex([rows[0], rows[1]]),
            Self::unpacklo_complex([rows[2], rows[3]]),
            Self::unpacklo_complex([rows[4], rows[5]]),
            _mm256_blend_pd(rows[0], rows[6], 0x03),
            Self::unpackhi_complex([rows[1], rows[2]]),
            Self::unpackhi_complex([rows[3], rows[4]]),
            Self::unpackhi_complex([rows[5], rows[6]]),
        ]
    }
    #[inline(always)]
    unsafe fn transpose8_packed(rows: [Self; 8]) -> [Self; 8] {
        let chunk0 = [rows[0], rows[1], rows[2], rows[3]];
        let chunk1 = [rows[4], rows[5], rows[6], rows[7]];

        let output0 = Self::transpose4_packed(chunk0);
        let output1 = Self::transpose4_packed(chunk1);

        [
            output0[0], output0[1], output1[0], output1[1], output0[2], output0[3], output1[2],
            output1[3],
        ]
    }
    #[inline(always)]
    unsafe fn transpose9_packed(rows: [Self; 9]) -> [Self; 9] {
        [
            _mm256_permute2f128_pd(rows[0], rows[1], 0x20),
            _mm256_permute2f128_pd(rows[2], rows[3], 0x20),
            _mm256_permute2f128_pd(rows[4], rows[5], 0x20),
            _mm256_permute2f128_pd(rows[6], rows[7], 0x20),
            _mm256_permute2f128_pd(rows[8], rows[0], 0x30),
            _mm256_permute2f128_pd(rows[1], rows[2], 0x31),
            _mm256_permute2f128_pd(rows[3], rows[4], 0x31),
            _mm256_permute2f128_pd(rows[5], rows[6], 0x31),
            _mm256_permute2f128_pd(rows[7], rows[8], 0x31),
        ]
    }
    #[inline(always)]
    unsafe fn transpose11_packed(rows: [Self; 11]) -> [Self; 11] {
        [
            _mm256_permute2f128_pd(rows[0], rows[1], 0x20),
            _mm256_permute2f128_pd(rows[2], rows[3], 0x20),
            _mm256_permute2f128_pd(rows[4], rows[5], 0x20),
            _mm256_permute2f128_pd(rows[6], rows[7], 0x20),
            _mm256_permute2f128_pd(rows[8], rows[9], 0x20),
            _mm256_permute2f128_pd(rows[10], rows[0], 0x30),
            _mm256_permute2f128_pd(rows[1], rows[2], 0x31),
            _mm256_permute2f128_pd(rows[3], rows[4], 0x31),
            _mm256_permute2f128_pd(rows[5], rows[6], 0x31),
            _mm256_permute2f128_pd(rows[7], rows[8], 0x31),
            _mm256_permute2f128_pd(rows[9], rows[10], 0x31),
        ]
    }
    #[inline(always)]
    unsafe fn transpose12_packed(rows: [Self; 12]) -> [Self; 12] {
        let chunk0 = [rows[0], rows[1], rows[2], rows[3]];
        let chunk1 = [rows[4], rows[5], rows[6], rows[7]];
        let chunk2 = [rows[8], rows[9], rows[10], rows[11]];

        let output0 = Self::transpose4_packed(chunk0);
        let output1 = Self::transpose4_packed(chunk1);
        let output2 = Self::transpose4_packed(chunk2);

        [
            output0[0], output0[1], output1[0], output1[1], output2[0], output2[1], output0[2],
            output0[3], output1[2], output1[3], output2[2], output2[3],
        ]
    }
    #[inline(always)]
    unsafe fn transpose16_packed(rows: [Self; 16]) -> [Self; 16] {
        let chunk0 = [
            rows[0], rows[1], rows[2], rows[3], rows[4], rows[5], rows[6], rows[7],
        ];
        let chunk1 = [
            rows[8], rows[9], rows[10], rows[11], rows[12], rows[13], rows[14], rows[15],
        ];

        let output0 = Self::transpose8_packed(chunk0);
        let output1 = Self::transpose8_packed(chunk1);

        [
            output0[0], output0[1], output0[2], output0[3], output1[0], output1[1], output1[2],
            output1[3], output0[4], output0[5], output0[6], output0[7], output1[4], output1[5],
            output1[6], output1[7],
        ]
    }
}
impl AvxVector256 for __m256d {
    type ScalarType = f64;
    type HalfVector = __m128d;

    #[inline(always)]
    unsafe fn lo(self) -> Self::HalfVector {
        _mm256_castpd256_pd128(self)
    }
    #[inline(always)]
    unsafe fn hi(self) -> Self::HalfVector {
        _mm256_extractf128_pd(self, 1)
    }
    #[inline(always)]
    unsafe fn merge(lo: Self::HalfVector, hi: Self::HalfVector) -> Self {
        _mm256_insertf128_pd(_mm256_castpd128_pd256(lo), hi, 1)
    }

    #[inline(always)]
    unsafe fn broadcast_complex_elements(value: Complex<Self::ScalarType>) -> Self {
        _mm256_set_pd(value.im, value.re, value.im, value.re)
    }

    #[inline(always)]
    unsafe fn load_complex(ptr: *const Complex<Self::ScalarType>) -> Self {
        _mm256_loadu_pd(ptr as *const Self::ScalarType)
    }
    #[inline(always)]
    unsafe fn store_complex(ptr: *mut Complex<Self::ScalarType>, data: Self) {
        _mm256_storeu_pd(ptr as *mut Self::ScalarType, data)
    }
    #[inline(always)]
    unsafe fn gather_complex_avx2_index32(
        ptr: *const Complex<Self::ScalarType>,
        indexes: __m128i,
    ) -> Self {
        let offsets = _mm_set_epi32(1, 0, 1, 0);
        let shifted = _mm_slli_epi32(indexes, 1);
        let modified_indexes = _mm_add_epi32(offsets, shifted);

        _mm256_i32gather_pd(ptr as *const f64, modified_indexes, 8)
    }
    #[inline(always)]
    unsafe fn gather_complex_avx2_index64(
        ptr: *const Complex<Self::ScalarType>,
        indexes: __m256i,
    ) -> Self {
        let offsets = _mm256_set_epi64x(1, 0, 1, 0);
        let shifted = _mm256_slli_epi64(indexes, 1);
        let modified_indexes = _mm256_add_epi64(offsets, shifted);

        _mm256_i64gather_pd(ptr as *const f64, modified_indexes, 8)
    }
    #[inline(always)]
    unsafe fn load_partial1_complex(ptr: *const Complex<Self::ScalarType>) -> Self::HalfVector {
        _mm_loadu_pd(ptr as *const f64)
    }
    #[inline(always)]
    unsafe fn load_partial2_complex(_ptr: *const Complex<Self::ScalarType>) -> Self::HalfVector {
        unimplemented!("Impossible to do a partial load of 2 complex f64's")
    }
    #[inline(always)]
    unsafe fn load_partial3_complex(_ptr: *const Complex<Self::ScalarType>) -> Self {
        unimplemented!("Impossible to do a partial load of 3 complex f64's")
    }
    #[inline(always)]
    unsafe fn store_partial1_complex(ptr: *mut Complex<Self::ScalarType>, data: Self::HalfVector) {
        _mm_storeu_pd(ptr as *mut f64, data);
    }
    #[inline(always)]
    unsafe fn store_partial2_complex(
        _ptr: *mut Complex<Self::ScalarType>,
        _data: Self::HalfVector,
    ) {
        unimplemented!("Impossible to do a partial store of 2 complex f64's")
    }
    #[inline(always)]
    unsafe fn store_partial3_complex(_ptr: *mut Complex<Self::ScalarType>, _data: Self) {
        unimplemented!("Impossible to do a partial store of 3 complex f64's")
    }
}

impl AvxVector for __m128d {
    const COMPLEX_PER_VECTOR: usize = 1;

    #[inline(always)]
    unsafe fn zero() -> Self {
        _mm_setzero_pd()
    }
    #[inline(always)]
    unsafe fn half_root2() -> Self {
        // note: we're computing a square root here, but checking the assembly says the compiler is smart enough to turn this into a constant
        _mm_load1_pd(&0.5f64.sqrt())
    }

    #[inline(always)]
    unsafe fn xor(left: Self, right: Self) -> Self {
        _mm_xor_pd(left, right)
    }
    #[inline(always)]
    unsafe fn neg(self) -> Self {
        _mm_xor_pd(self, _mm_load1_pd(&-0.0))
    }
    #[inline(always)]
    unsafe fn add(left: Self, right: Self) -> Self {
        _mm_add_pd(left, right)
    }
    #[inline(always)]
    unsafe fn sub(left: Self, right: Self) -> Self {
        _mm_sub_pd(left, right)
    }
    #[inline(always)]
    unsafe fn mul(left: Self, right: Self) -> Self {
        _mm_mul_pd(left, right)
    }
    #[inline(always)]
    unsafe fn fmadd(left: Self, right: Self, add: Self) -> Self {
        _mm_fmadd_pd(left, right, add)
    }
    #[inline(always)]
    unsafe fn fnmadd(left: Self, right: Self, add: Self) -> Self {
        _mm_fnmadd_pd(left, right, add)
    }
    #[inline(always)]
    unsafe fn fmaddsub(left: Self, right: Self, add: Self) -> Self {
        _mm_fmaddsub_pd(left, right, add)
    }
    #[inline(always)]
    unsafe fn fmsubadd(left: Self, right: Self, add: Self) -> Self {
        _mm_fmsubadd_pd(left, right, add)
    }

    #[inline(always)]
    unsafe fn reverse_complex_elements(self) -> Self {
        // nothing to reverse
        self
    }
    #[inline(always)]
    unsafe fn unpacklo_complex(_rows: [Self; 2]) -> Self {
        unimplemented!(); // this operation doesn't make sense with one element. TODO: I don't know if it would be more useful to error here or to just return the inputs unchanged. If returning the inputs is useful, do that.
    }
    #[inline(always)]
    unsafe fn unpackhi_complex(_rows: [Self; 2]) -> Self {
        unimplemented!(); // this operation doesn't make sense with one element. TODO: I don't know if it would be more useful to error here or to just return the inputs unchanged. If returning the inputs is useful, do that.
    }

    #[inline(always)]
    unsafe fn swap_complex_components(self) -> Self {
        _mm_permute_pd(self, 0x01)
    }
    #[inline(always)]
    unsafe fn duplicate_complex_components(self) -> (Self, Self) {
        (_mm_movedup_pd(self), _mm_permute_pd(self, 0x03))
    }

    #[inline(always)]
    unsafe fn make_rotation90(direction: FftDirection) -> Rotation90<Self> {
        let broadcast = match direction {
            FftDirection::Forward => Complex::new(-0.0, 0.0),
            FftDirection::Inverse => Complex::new(0.0, -0.0),
        };
        Rotation90(Self::broadcast_complex_elements(broadcast))
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

        _mm_loadu_pd(twiddle_chunk.as_ptr() as *const f64)
    }
    #[inline(always)]
    unsafe fn broadcast_twiddle(index: usize, len: usize, direction: FftDirection) -> Self {
        Self::broadcast_complex_elements(twiddles::compute_twiddle(index, len, direction))
    }

    #[inline(always)]
    unsafe fn transpose2_packed(rows: [Self; 2]) -> [Self; 2] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose3_packed(rows: [Self; 3]) -> [Self; 3] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose4_packed(rows: [Self; 4]) -> [Self; 4] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose5_packed(rows: [Self; 5]) -> [Self; 5] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose6_packed(rows: [Self; 6]) -> [Self; 6] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose7_packed(rows: [Self; 7]) -> [Self; 7] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose8_packed(rows: [Self; 8]) -> [Self; 8] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose9_packed(rows: [Self; 9]) -> [Self; 9] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose11_packed(rows: [Self; 11]) -> [Self; 11] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose12_packed(rows: [Self; 12]) -> [Self; 12] {
        rows
    }
    #[inline(always)]
    unsafe fn transpose16_packed(rows: [Self; 16]) -> [Self; 16] {
        rows
    }
}
impl AvxVector128 for __m128d {
    type FullVector = __m256d;

    #[inline(always)]
    unsafe fn lo(input: Self::FullVector) -> Self {
        _mm256_castpd256_pd128(input)
    }
    #[inline(always)]
    unsafe fn hi(input: Self::FullVector) -> Self {
        _mm256_extractf128_pd(input, 1)
    }
    #[inline(always)]
    unsafe fn merge(lo: Self, hi: Self) -> Self::FullVector {
        _mm256_insertf128_pd(_mm256_castpd128_pd256(lo), hi, 1)
    }
    #[inline(always)]
    unsafe fn zero_extend(self) -> Self::FullVector {
        _mm256_zextpd128_pd256(self)
    }
    #[inline(always)]
    unsafe fn lo_rotation(input: Rotation90<Self::FullVector>) -> Rotation90<Self> {
        input.lo()
    }
    #[inline(always)]
    unsafe fn broadcast_complex_elements(value: Complex<f64>) -> Self {
        _mm_set_pd(value.im, value.re)
    }
    #[inline(always)]
    unsafe fn gather32_complex_avx2(ptr: *const Complex<f64>, indexes: __m128i) -> Self {
        let mut index_storage: [i32; 4] = [0; 4];
        _mm_storeu_si128(index_storage.as_mut_ptr() as *mut __m128i, indexes);

        _mm_loadu_pd(ptr.offset(index_storage[0] as isize) as *const f64)
    }
    #[inline(always)]
    unsafe fn gather64_complex_avx2(ptr: *const Complex<f64>, indexes: __m128i) -> Self {
        let mut index_storage: [i64; 4] = [0; 4];
        _mm_storeu_si128(index_storage.as_mut_ptr() as *mut __m128i, indexes);

        _mm_loadu_pd(ptr.offset(index_storage[0] as isize) as *const f64)
    }
}

pub trait AvxArray<T: AvxNum>: Deref {
    unsafe fn load_complex(&self, index: usize) -> T::VectorType;
    unsafe fn load_partial1_complex(
        &self,
        index: usize,
    ) -> <T::VectorType as AvxVector256>::HalfVector;
    unsafe fn load_partial2_complex(
        &self,
        index: usize,
    ) -> <T::VectorType as AvxVector256>::HalfVector;
    unsafe fn load_partial3_complex(&self, index: usize) -> T::VectorType;

    // some avx operations need bespoke one-off things that don't fit into the methods above, so we should provide an escape hatch for them
    fn input_ptr(&self) -> *const Complex<T>;
}
pub trait AvxArrayMut<T: AvxNum>: AvxArray<T> + DerefMut {
    unsafe fn store_complex(&mut self, data: T::VectorType, index: usize);
    unsafe fn store_partial1_complex(
        &mut self,
        data: <T::VectorType as AvxVector256>::HalfVector,
        index: usize,
    );
    unsafe fn store_partial2_complex(
        &mut self,
        data: <T::VectorType as AvxVector256>::HalfVector,
        index: usize,
    );
    unsafe fn store_partial3_complex(&mut self, data: T::VectorType, index: usize);
}

impl<T: AvxNum> AvxArray<T> for &[Complex<T>] {
    #[inline(always)]
    unsafe fn load_complex(&self, index: usize) -> T::VectorType {
        debug_assert!(self.len() >= index + T::VectorType::COMPLEX_PER_VECTOR);
        T::VectorType::load_complex(self.as_ptr().add(index))
    }
    #[inline(always)]
    unsafe fn load_partial1_complex(
        &self,
        index: usize,
    ) -> <T::VectorType as AvxVector256>::HalfVector {
        debug_assert!(self.len() >= index + 1);
        T::VectorType::load_partial1_complex(self.as_ptr().add(index))
    }
    #[inline(always)]
    unsafe fn load_partial2_complex(
        &self,
        index: usize,
    ) -> <T::VectorType as AvxVector256>::HalfVector {
        debug_assert!(self.len() >= index + 2);
        T::VectorType::load_partial2_complex(self.as_ptr().add(index))
    }
    #[inline(always)]
    unsafe fn load_partial3_complex(&self, index: usize) -> T::VectorType {
        debug_assert!(self.len() >= index + 3);
        T::VectorType::load_partial3_complex(self.as_ptr().add(index))
    }
    #[inline(always)]
    fn input_ptr(&self) -> *const Complex<T> {
        self.as_ptr()
    }
}
impl<T: AvxNum> AvxArray<T> for &mut [Complex<T>] {
    #[inline(always)]
    unsafe fn load_complex(&self, index: usize) -> T::VectorType {
        debug_assert!(self.len() >= index + T::VectorType::COMPLEX_PER_VECTOR);
        T::VectorType::load_complex(self.as_ptr().add(index))
    }
    #[inline(always)]
    unsafe fn load_partial1_complex(
        &self,
        index: usize,
    ) -> <T::VectorType as AvxVector256>::HalfVector {
        debug_assert!(self.len() >= index + 1);
        T::VectorType::load_partial1_complex(self.as_ptr().add(index))
    }
    #[inline(always)]
    unsafe fn load_partial2_complex(
        &self,
        index: usize,
    ) -> <T::VectorType as AvxVector256>::HalfVector {
        debug_assert!(self.len() >= index + 2);
        T::VectorType::load_partial2_complex(self.as_ptr().add(index))
    }
    #[inline(always)]
    unsafe fn load_partial3_complex(&self, index: usize) -> T::VectorType {
        debug_assert!(self.len() >= index + 3);
        T::VectorType::load_partial3_complex(self.as_ptr().add(index))
    }
    #[inline(always)]
    fn input_ptr(&self) -> *const Complex<T> {
        self.as_ptr()
    }
}
impl<'a, T: AvxNum> AvxArray<T> for DoubleBuf<'a, T>
where
    &'a [Complex<T>]: AvxArray<T>,
{
    #[inline(always)]
    unsafe fn load_complex(&self, index: usize) -> T::VectorType {
        self.input.load_complex(index)
    }
    #[inline(always)]
    unsafe fn load_partial1_complex(
        &self,
        index: usize,
    ) -> <T::VectorType as AvxVector256>::HalfVector {
        self.input.load_partial1_complex(index)
    }
    #[inline(always)]
    unsafe fn load_partial2_complex(
        &self,
        index: usize,
    ) -> <T::VectorType as AvxVector256>::HalfVector {
        self.input.load_partial2_complex(index)
    }
    #[inline(always)]
    unsafe fn load_partial3_complex(&self, index: usize) -> T::VectorType {
        self.input.load_partial3_complex(index)
    }
    #[inline(always)]
    fn input_ptr(&self) -> *const Complex<T> {
        self.input.input_ptr()
    }
}

impl<T: AvxNum> AvxArrayMut<T> for &mut [Complex<T>] {
    #[inline(always)]
    unsafe fn store_complex(&mut self, data: T::VectorType, index: usize) {
        debug_assert!(self.len() >= index + T::VectorType::COMPLEX_PER_VECTOR);
        T::VectorType::store_complex(self.as_mut_ptr().add(index), data);
    }
    #[inline(always)]
    unsafe fn store_partial1_complex(
        &mut self,
        data: <T::VectorType as AvxVector256>::HalfVector,
        index: usize,
    ) {
        debug_assert!(self.len() >= index + 1);
        T::VectorType::store_partial1_complex(self.as_mut_ptr().add(index), data);
    }
    #[inline(always)]
    unsafe fn store_partial2_complex(
        &mut self,
        data: <T::VectorType as AvxVector256>::HalfVector,
        index: usize,
    ) {
        debug_assert!(self.len() >= index + 2);
        T::VectorType::store_partial2_complex(self.as_mut_ptr().add(index), data);
    }
    #[inline(always)]
    unsafe fn store_partial3_complex(&mut self, data: T::VectorType, index: usize) {
        debug_assert!(self.len() >= index + 3);
        T::VectorType::store_partial3_complex(self.as_mut_ptr().add(index), data);
    }
}
impl<'a, T: AvxNum> AvxArrayMut<T> for DoubleBuf<'a, T>
where
    Self: AvxArray<T>,
    &'a mut [Complex<T>]: AvxArrayMut<T>,
{
    #[inline(always)]
    unsafe fn store_complex(&mut self, data: T::VectorType, index: usize) {
        self.output.store_complex(data, index);
    }
    #[inline(always)]
    unsafe fn store_partial1_complex(
        &mut self,
        data: <T::VectorType as AvxVector256>::HalfVector,
        index: usize,
    ) {
        self.output.store_partial1_complex(data, index);
    }
    #[inline(always)]
    unsafe fn store_partial2_complex(
        &mut self,
        data: <T::VectorType as AvxVector256>::HalfVector,
        index: usize,
    ) {
        self.output.store_partial2_complex(data, index);
    }
    #[inline(always)]
    unsafe fn store_partial3_complex(&mut self, data: T::VectorType, index: usize) {
        self.output.store_partial3_complex(data, index);
    }
}

// A custom butterfly-16 function that calls a lambda to load/store data instead of taking an array
// This is particularly useful for butterfly 16, because the whole problem doesn't fit into registers, and the compiler isn't smart enough to only load data when it's needed
// So the version that takes an array ends up loading data and immediately re-storing it on the stack. By lazily loading and storing exactly when we need to, we can avoid some data reshuffling
macro_rules! column_butterfly16_loadfn{
    ($load_expr: expr, $store_expr: expr, $twiddles: expr, $rotation: expr) => (
        // Size-4 FFTs down the columns
        let input1 = [$load_expr(1), $load_expr(5), $load_expr(9), $load_expr(13)];
        let mut mid1 = AvxVector::column_butterfly4(input1, $rotation);

        mid1[1] = AvxVector::mul_complex(mid1[1], $twiddles[0]);
        mid1[2] = avx_vector::apply_butterfly8_twiddle1(mid1[2], $rotation);
        mid1[3] = AvxVector::mul_complex(mid1[3], $twiddles[1]);

        let input2 = [$load_expr(2), $load_expr(6), $load_expr(10), $load_expr(14)];
        let mut mid2 = AvxVector::column_butterfly4(input2, $rotation);

        mid2[1] = avx_vector::apply_butterfly8_twiddle1(mid2[1], $rotation);
        mid2[2] = mid2[2].rotate90($rotation);
        mid2[3] = avx_vector::apply_butterfly8_twiddle3(mid2[3], $rotation);

        let input3 = [$load_expr(3), $load_expr(7), $load_expr(11), $load_expr(15)];
        let mut mid3 = AvxVector::column_butterfly4(input3, $rotation);

        mid3[1] = AvxVector::mul_complex(mid3[1], $twiddles[1]);
        mid3[2] = avx_vector::apply_butterfly8_twiddle3(mid3[2], $rotation);
        mid3[3] = AvxVector::mul_complex(mid3[3], $twiddles[0].neg());

        // do the first row last, because it doesn't need twiddles and therefore requires fewer intermediates
        let input0 = [$load_expr(0), $load_expr(4), $load_expr(8), $load_expr(12)];
        let mid0     = AvxVector::column_butterfly4(input0, $rotation);

        // All of the data is now in the right format to just do a bunch of butterfly 8's.
        // Write the data out to the final output as we go so that the compiler can stop worrying about finding stack space for it
        for i in 0..4 {
            let output = AvxVector::column_butterfly4([mid0[i], mid1[i], mid2[i], mid3[i]], $rotation);
            $store_expr(output[0], i);
            $store_expr(output[1], i + 4);
            $store_expr(output[2], i + 8);
            $store_expr(output[3], i + 12);
        }
    )
}

// A custom butterfly-32 function that calls a lambda to load/store data instead of taking an array
// This is particularly useful for butterfly 32, because the whole problem doesn't fit into registers, and the compiler isn't smart enough to only load data when it's needed
// So the version that takes an array ends up loading data and immediately re-storing it on the stack. By lazily loading and storing exactly when we need to, we can avoid some data reshuffling
macro_rules! column_butterfly32_loadfn{
    ($load_expr: expr, $store_expr: expr, $twiddles: expr, $rotation: expr) => (
        // Size-4 FFTs down the columns
        let input1 = [$load_expr(1), $load_expr(9), $load_expr(17), $load_expr(25)];
        let mut mid1     = AvxVector::column_butterfly4(input1, $rotation);

        mid1[1] = AvxVector::mul_complex(mid1[1], $twiddles[0]);
        mid1[2] = AvxVector::mul_complex(mid1[2], $twiddles[1]);
        mid1[3] = AvxVector::mul_complex(mid1[3], $twiddles[2]);

        let input2 = [$load_expr(2), $load_expr(10), $load_expr(18), $load_expr(26)];
        let mut mid2     = AvxVector::column_butterfly4(input2, $rotation);

        mid2[1] = AvxVector::mul_complex(mid2[1], $twiddles[1]);
        mid2[2] = avx_vector::apply_butterfly8_twiddle1(mid2[2], $rotation);
        mid2[3] = AvxVector::mul_complex(mid2[3], $twiddles[4]);

        let input3 = [$load_expr(3), $load_expr(11), $load_expr(19), $load_expr(27)];
        let mut mid3     = AvxVector::column_butterfly4(input3, $rotation);

        mid3[1] = AvxVector::mul_complex(mid3[1], $twiddles[2]);
        mid3[2] = AvxVector::mul_complex(mid3[2], $twiddles[4]);
        mid3[3] = AvxVector::mul_complex(mid3[3], $twiddles[0].rotate90($rotation));

        let input4 = [$load_expr(4), $load_expr(12), $load_expr(20), $load_expr(28)];
        let mut mid4     = AvxVector::column_butterfly4(input4, $rotation);

        mid4[1] = avx_vector::apply_butterfly8_twiddle1(mid4[1], $rotation);
        mid4[2] = mid4[2].rotate90($rotation);
        mid4[3] = avx_vector::apply_butterfly8_twiddle3(mid4[3], $rotation);

        let input5 = [$load_expr(5), $load_expr(13), $load_expr(21), $load_expr(29)];
        let mut mid5     = AvxVector::column_butterfly4(input5, $rotation);

        mid5[1] = AvxVector::mul_complex(mid5[1], $twiddles[3]);
        mid5[2] = AvxVector::mul_complex(mid5[2], $twiddles[1].rotate90($rotation));
        mid5[3] = AvxVector::mul_complex(mid5[3], $twiddles[5].rotate90($rotation));

        let input6 = [$load_expr(6), $load_expr(14), $load_expr(22), $load_expr(30)];
        let mut mid6     = AvxVector::column_butterfly4(input6, $rotation);

        mid6[1] = AvxVector::mul_complex(mid6[1], $twiddles[4]);
        mid6[2] = avx_vector::apply_butterfly8_twiddle3(mid6[2], $rotation);
        mid6[3] = AvxVector::mul_complex(mid6[3], $twiddles[1].neg());

        let input7 = [$load_expr(7), $load_expr(15), $load_expr(23), $load_expr(31)];
        let mut mid7     = AvxVector::column_butterfly4(input7, $rotation);

        mid7[1] = AvxVector::mul_complex(mid7[1], $twiddles[5]);
        mid7[2] = AvxVector::mul_complex(mid7[2], $twiddles[4].rotate90($rotation));
        mid7[3] = AvxVector::mul_complex(mid7[3], $twiddles[3].neg());

        let input0 = [$load_expr(0), $load_expr(8), $load_expr(16), $load_expr(24)];
        let mid0     = AvxVector::column_butterfly4(input0, $rotation);

        // All of the data is now in the right format to just do a bunch of butterfly 8's in a loop.
        // Write the data out to the final output as we go so that the compiler can stop worrying about finding stack space for it
        for i in 0..4 {
            let output = AvxVector::column_butterfly8([mid0[i], mid1[i], mid2[i], mid3[i], mid4[i], mid5[i], mid6[i], mid7[i]], $rotation);
            $store_expr(output[0], i);
            $store_expr(output[1], i + 4);
            $store_expr(output[2], i + 8);
            $store_expr(output[3], i + 12);
            $store_expr(output[4], i + 16);
            $store_expr(output[5], i + 20);
            $store_expr(output[6], i + 24);
            $store_expr(output[7], i + 28);
        }
    )
}

/// Multiply the complex numbers in `left` by the complex numbers in `right`.
/// This is exactly the same as `mul_complex` in `AvxVector`, but this implementation also conjugates the `left` input before multiplying
#[inline(always)]
unsafe fn mul_complex_conjugated<V: AvxVector>(left: V, right: V) -> V {
    // Extract the real and imaginary components from left into 2 separate registers
    let (left_real, left_imag) = V::duplicate_complex_components(left);

    // create a shuffled version of right where the imaginary values are swapped with the reals
    let right_shuffled = V::swap_complex_components(right);

    // multiply our duplicated imaginary left vector by our shuffled right vector. that will give us the right side of the traditional complex multiplication formula
    let output_right = V::mul(left_imag, right_shuffled);

    // use a FMA instruction to multiply together left side of the complex multiplication formula, then alternatingly add and subtract the left side from the right
    // By using subadd instead of addsub, we can conjugate the left side for free.
    V::fmsubadd(left_real, right, output_right)
}

// compute buffer[i] = buffer[i].conj() * multiplier[i] pairwise complex multiplication for each element.
#[target_feature(enable = "avx", enable = "fma")]
pub unsafe fn pairwise_complex_mul_assign_conjugated<T: AvxNum>(
    mut buffer: &mut [Complex<T>],
    multiplier: &[T::VectorType],
) {
    assert!(multiplier.len() * T::VectorType::COMPLEX_PER_VECTOR >= buffer.len()); // Assert to convince the compiler to omit bounds checks inside the loop

    for (i, mut buffer_chunk) in buffer
        .chunks_exact_mut(T::VectorType::COMPLEX_PER_VECTOR)
        .enumerate()
    {
        let left = buffer_chunk.load_complex(0);

        // Do a complex multiplication between `left` and `right`
        let product = mul_complex_conjugated(left, multiplier[i]);

        // Store the result
        buffer_chunk.store_complex(product, 0);
    }

    // Process the remainder, if there is one
    let remainder_count = buffer.len() % T::VectorType::COMPLEX_PER_VECTOR;
    if remainder_count > 0 {
        let remainder_index = buffer.len() - remainder_count;
        let remainder_multiplier = multiplier.last().unwrap();
        match remainder_count {
            1 => {
                let left = buffer.load_partial1_complex(remainder_index);
                let product = mul_complex_conjugated(left, remainder_multiplier.lo());
                buffer.store_partial1_complex(product, remainder_index);
            }
            2 => {
                let left = buffer.load_partial2_complex(remainder_index);
                let product = mul_complex_conjugated(left, remainder_multiplier.lo());
                buffer.store_partial2_complex(product, remainder_index);
            }
            3 => {
                let left = buffer.load_partial3_complex(remainder_index);
                let product = mul_complex_conjugated(left, *remainder_multiplier);
                buffer.store_partial3_complex(product, remainder_index);
            }
            _ => unreachable!(),
        }
    }
}

// compute output[i] = input[i].conj() * multiplier[i] pairwise complex multiplication for each element.
#[target_feature(enable = "avx", enable = "fma")]
pub unsafe fn pairwise_complex_mul_conjugated<T: AvxNum>(
    input: &[Complex<T>],
    mut output: &mut [Complex<T>],
    multiplier: &[T::VectorType],
) {
    assert!(
        multiplier.len() * T::VectorType::COMPLEX_PER_VECTOR >= input.len(),
        "multiplier len = {}, input len = {}",
        multiplier.len(),
        input.len()
    ); // Assert to convince the compiler to omit bounds checks inside the loop
    assert_eq!(input.len(), output.len()); // Assert to convince the compiler to omit bounds checks inside the loop
    let main_loop_count = input.len() / T::VectorType::COMPLEX_PER_VECTOR;
    let remainder_count = input.len() % T::VectorType::COMPLEX_PER_VECTOR;

    for (i, m) in (&multiplier[..main_loop_count]).iter().enumerate() {
        let left = input.load_complex(i * T::VectorType::COMPLEX_PER_VECTOR);

        // Do a complex multiplication between `left` and `right`
        let product = mul_complex_conjugated(left, *m);

        // Store the result
        output.store_complex(product, i * T::VectorType::COMPLEX_PER_VECTOR);
    }

    // Process the remainder, if there is one
    if remainder_count > 0 {
        let remainder_index = input.len() - remainder_count;
        let remainder_multiplier = multiplier.last().unwrap();
        match remainder_count {
            1 => {
                let left = input.load_partial1_complex(remainder_index);
                let product = mul_complex_conjugated(left, remainder_multiplier.lo());
                output.store_partial1_complex(product, remainder_index);
            }
            2 => {
                let left = input.load_partial2_complex(remainder_index);
                let product = mul_complex_conjugated(left, remainder_multiplier.lo());
                output.store_partial2_complex(product, remainder_index);
            }
            3 => {
                let left = input.load_partial3_complex(remainder_index);
                let product = mul_complex_conjugated(left, *remainder_multiplier);
                output.store_partial3_complex(product, remainder_index);
            }
            _ => unreachable!(),
        }
    }
}
