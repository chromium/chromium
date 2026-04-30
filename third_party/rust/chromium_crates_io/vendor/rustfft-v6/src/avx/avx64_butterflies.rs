use std::arch::x86_64::*;
use std::marker::PhantomData;
use std::mem::MaybeUninit;

use num_complex::Complex;

use crate::array_utils::DoubleBuf;
use crate::{common::FftNum, twiddles};
use crate::{Direction, Fft, FftDirection, Length};

use super::avx64_utils;
use super::avx_vector;
use super::avx_vector::{AvxArray, AvxArrayMut, AvxVector, AvxVector128, AvxVector256, Rotation90};

// Safety: This macro will call `self::perform_fft_f32()` which probably has a #[target_feature(enable = "...")] annotation on it.
// Calling functions with that annotation is unsafe, because it doesn't actually check if the CPU has the required features.
// Callers of this macro must guarantee that users can't even obtain an instance of $struct_name if their CPU doesn't have the required CPU features.
#[allow(unused)]
macro_rules! boilerplate_fft_simd_butterfly {
    ($struct_name:ident, $len:expr) => {
        impl $struct_name<f64> {
            #[inline]
            pub fn new(direction: FftDirection) -> Result<Self, ()> {
                let has_avx = is_x86_feature_detected!("avx");
                let has_fma = is_x86_feature_detected!("fma");
                if has_avx && has_fma {
                    // Safety: new_internal requires the "avx" feature set. Since we know it's present, we're safe
                    Ok(unsafe { Self::new_with_avx(direction) })
                } else {
                    Err(())
                }
            }
        }
        impl<T: FftNum> Fft<T> for $struct_name<f64> {
            fn process_immutable_with_scratch(
                &self,
                input: &[Complex<T>],
                output: &mut [Complex<T>],
                _scratch: &mut [Complex<T>],
            ) {
                unsafe {
                    let simd_input = crate::array_utils::workaround_transmute(input);
                    let simd_output = crate::array_utils::workaround_transmute_mut(output);
                    super::avx_fft_helper_immut(
                        simd_input,
                        simd_output,
                        &mut [],
                        self.len(),
                        0,
                        |input, output, _| self.perform_fft_f64(DoubleBuf { input, output }),
                    );
                }
            }
            fn process_outofplace_with_scratch(
                &self,
                input: &mut [Complex<T>],
                output: &mut [Complex<T>],
                _scratch: &mut [Complex<T>],
            ) {
                unsafe {
                    let simd_input = crate::array_utils::workaround_transmute_mut(input);
                    let simd_output = crate::array_utils::workaround_transmute_mut(output);
                    super::avx_fft_helper_outofplace(
                        simd_input,
                        simd_output,
                        &mut [],
                        self.len(),
                        0,
                        |input, output, _| self.perform_fft_f64(DoubleBuf { input, output }),
                    );
                }
            }
            fn process_with_scratch(&self, buffer: &mut [Complex<T>], _scratch: &mut [Complex<T>]) {
                unsafe {
                    let simd_buffer = crate::array_utils::workaround_transmute_mut(buffer);
                    super::avx_fft_helper_inplace(
                        simd_buffer,
                        &mut [],
                        self.len(),
                        0,
                        |chunk, _| self.perform_fft_f64(chunk),
                    )
                }
            }
            #[inline(always)]
            fn get_inplace_scratch_len(&self) -> usize {
                0
            }
            #[inline(always)]
            fn get_outofplace_scratch_len(&self) -> usize {
                0
            }
            #[inline(always)]
            fn get_immutable_scratch_len(&self) -> usize {
                0
            }
        }
        impl<T> Length for $struct_name<T> {
            #[inline(always)]
            fn len(&self) -> usize {
                $len
            }
        }
        impl<T> Direction for $struct_name<T> {
            #[inline(always)]
            fn fft_direction(&self) -> FftDirection {
                self.direction
            }
        }
    };
}

// Safety: This macro will call `self::column_butterflies_and_transpose()` and `self::row_butterflies()` which probably has a #[target_feature(enable = "...")] annotation on it.
// Calling functions with that annotation is unsafe, because it doesn't actually check if the CPU has the required features.
// Callers of this macro must guarantee that users can't even obtain an instance of $struct_name if their CPU doesn't have the required CPU features.
macro_rules! boilerplate_fft_simd_butterfly_with_scratch {
    ($struct_name:ident, $len:expr) => {
        impl $struct_name<f64> {
            #[inline]
            pub fn is_supported_by_cpu() -> bool {
                is_x86_feature_detected!("avx") && is_x86_feature_detected!("fma")
            }
            #[inline]
            pub fn new(direction: FftDirection) -> Result<Self, ()> {
                if Self::is_supported_by_cpu() {
                    // Safety: new_internal requires the "avx" feature set. Since we know it's present, we're safe
                    Ok(unsafe { Self::new_with_avx(direction) })
                } else {
                    Err(())
                }
            }
        }
        impl<T> $struct_name<T> {
            #[inline]
            fn perform_fft_inplace(
                &self,
                buffer: &mut [Complex<f64>],
                scratch: &mut [Complex<f64>],
            ) {
                // Perform the column FFTs
                // Safety: self.perform_column_butterflies() requres the "avx" and "fma" instruction sets, and we return Err() in our constructor if the instructions aren't available
                unsafe { self.column_butterflies_and_transpose(buffer, scratch) };

                // process the row FFTs, and copy from the scratch back to the buffer as we go
                // Safety: self.transpose() requres the "avx" instruction set, and we return Err() in our constructor if the instructions aren't available
                unsafe {
                    self.row_butterflies(DoubleBuf {
                        input: scratch,
                        output: buffer,
                    })
                };
            }

            #[inline]
            fn perform_fft_immut(&self, input: &[Complex<f64>], output: &mut [Complex<f64>]) {
                // Perform the column FFTs
                // Safety: self.perform_column_butterflies() requres the "avx" and "fma" instruction sets, and we return Err() in our constructor if the instructions aren't available
                unsafe { self.column_butterflies_and_transpose(input, output) };

                // process the row FFTs in-place in the output buffer
                // Safety: self.transpose() requres the "avx" instruction set, and we return Err() in our constructor if the instructions aren't available
                unsafe { self.row_butterflies(output) };
            }
        }
        impl<T: FftNum> Fft<T> for $struct_name<f64> {
            fn process_immutable_with_scratch(
                &self,
                input: &[Complex<T>],
                output: &mut [Complex<T>],
                _scratch: &mut [Complex<T>],
            ) {
                unsafe {
                    let simd_input = crate::array_utils::workaround_transmute(input);
                    let simd_output = crate::array_utils::workaround_transmute_mut(output);
                    super::avx_fft_helper_immut(
                        simd_input,
                        simd_output,
                        &mut [],
                        self.len(),
                        0,
                        |input, output, _| self.perform_fft_immut(input, output),
                    );
                }
            }
            fn process_outofplace_with_scratch(
                &self,
                input: &mut [Complex<T>],
                output: &mut [Complex<T>],
                _scratch: &mut [Complex<T>],
            ) {
                unsafe {
                    let simd_input = crate::array_utils::workaround_transmute_mut(input);
                    let simd_output = crate::array_utils::workaround_transmute_mut(output);
                    super::avx_fft_helper_outofplace(
                        simd_input,
                        simd_output,
                        &mut [],
                        self.len(),
                        0,
                        |input, output, _| self.perform_fft_immut(input, output),
                    );
                }
            }
            fn process_with_scratch(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
                unsafe {
                    let simd_buffer = crate::array_utils::workaround_transmute_mut(buffer);
                    let simd_scratch = crate::array_utils::workaround_transmute_mut(scratch);
                    super::avx_fft_helper_inplace(
                        simd_buffer,
                        simd_scratch,
                        self.len(),
                        self.len(),
                        |chunk, scratch| self.perform_fft_inplace(chunk, scratch),
                    )
                }
            }
            #[inline(always)]
            fn get_inplace_scratch_len(&self) -> usize {
                $len
            }
            #[inline(always)]
            fn get_outofplace_scratch_len(&self) -> usize {
                0
            }
            #[inline(always)]
            fn get_immutable_scratch_len(&self) -> usize {
                0
            }
        }
        impl<T> Length for $struct_name<T> {
            #[inline(always)]
            fn len(&self) -> usize {
                $len
            }
        }
        impl<T> Direction for $struct_name<T> {
            #[inline(always)]
            fn fft_direction(&self) -> FftDirection {
                self.direction
            }
        }
    };
}

macro_rules! gen_butterfly_twiddles_interleaved_columns {
    ($num_rows:expr, $num_cols:expr, $skip_cols:expr, $direction: expr) => {{
        const FFT_LEN: usize = $num_rows * $num_cols;
        const TWIDDLE_ROWS: usize = $num_rows - 1;
        const TWIDDLE_COLS: usize = $num_cols - $skip_cols;
        const TWIDDLE_VECTOR_COLS: usize = TWIDDLE_COLS / 2;
        const TWIDDLE_VECTOR_COUNT: usize = TWIDDLE_VECTOR_COLS * TWIDDLE_ROWS;
        let mut twiddles = [AvxVector::zero(); TWIDDLE_VECTOR_COUNT];
        for index in 0..TWIDDLE_VECTOR_COUNT {
            let y = (index / TWIDDLE_VECTOR_COLS) + 1;
            let x = (index % TWIDDLE_VECTOR_COLS) * 2 + $skip_cols;

            twiddles[index] = AvxVector::make_mixedradix_twiddle_chunk(x, y, FFT_LEN, $direction);
        }
        twiddles
    }};
}

macro_rules! gen_butterfly_twiddles_separated_columns {
    ($num_rows:expr, $num_cols:expr, $skip_cols:expr, $direction: expr) => {{
        const FFT_LEN: usize = $num_rows * $num_cols;
        const TWIDDLE_ROWS: usize = $num_rows - 1;
        const TWIDDLE_COLS: usize = $num_cols - $skip_cols;
        const TWIDDLE_VECTOR_COLS: usize = TWIDDLE_COLS / 2;
        const TWIDDLE_VECTOR_COUNT: usize = TWIDDLE_VECTOR_COLS * TWIDDLE_ROWS;
        let mut twiddles = [AvxVector::zero(); TWIDDLE_VECTOR_COUNT];
        for index in 0..TWIDDLE_VECTOR_COUNT {
            let y = (index % TWIDDLE_ROWS) + 1;
            let x = (index / TWIDDLE_ROWS) * 2 + $skip_cols;

            twiddles[index] = AvxVector::make_mixedradix_twiddle_chunk(x, y, FFT_LEN, $direction);
        }
        twiddles
    }};
}

pub struct Butterfly5Avx64<T> {
    twiddles: [__m256d; 3],
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly5Avx64, 5);
impl Butterfly5Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        let twiddle1 = twiddles::compute_twiddle(1, 5, direction);
        let twiddle2 = twiddles::compute_twiddle(2, 5, direction);
        Self {
            twiddles: [
                _mm256_set_pd(twiddle1.im, twiddle1.im, twiddle1.re, twiddle1.re),
                _mm256_set_pd(twiddle2.im, twiddle2.im, twiddle2.re, twiddle2.re),
                _mm256_set_pd(-twiddle1.im, -twiddle1.im, twiddle1.re, twiddle1.re),
            ],
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly5Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        let input0 = _mm256_loadu2_m128d(
            buffer.input_ptr() as *const f64,
            buffer.input_ptr() as *const f64,
        );
        let input12 = buffer.load_complex(1);
        let input34 = buffer.load_complex(3);

        // swap elements for inputs 3 and 4
        let input43 = AvxVector::reverse_complex_elements(input34);

        // do some prep work before we can start applying twiddle factors
        let [sum12, diff43] = AvxVector::column_butterfly2([input12, input43]);

        let rotation = AvxVector::make_rotation90(FftDirection::Inverse);
        let rotated43 = AvxVector::rotate90(diff43, rotation);

        let [mid14, mid23] = avx64_utils::transpose_2x2_f64([sum12, rotated43]);

        // to compute the first output, compute the sum of all elements. mid14[0] and mid23[0] already have the sum of 1+4 and 2+3 respectively, so if we add them, we'll get the sum of all 4
        let sum1234 = AvxVector::add(mid14.lo(), mid23.lo());
        let output0 = AvxVector::add(input0.lo(), sum1234);

        // apply twiddle factors
        let twiddled_outer14 = AvxVector::mul(mid14, self.twiddles[0]);
        let twiddled_inner14 = AvxVector::mul(mid14, self.twiddles[1]);
        let twiddled14 = AvxVector::fmadd(mid23, self.twiddles[1], twiddled_outer14);
        let twiddled23 = AvxVector::fmadd(mid23, self.twiddles[2], twiddled_inner14);

        // unpack the data for the last butterfly 2
        let [twiddled12, twiddled43] = avx64_utils::transpose_2x2_f64([twiddled14, twiddled23]);
        let [output12, output43] = AvxVector::column_butterfly2([twiddled12, twiddled43]);

        // swap the elements in output43 before writing them out, and add the first input to everything
        let final12 = AvxVector::add(input0, output12);
        let output34 = AvxVector::reverse_complex_elements(output43);
        let final34 = AvxVector::add(input0, output34);

        buffer.store_partial1_complex(output0, 0);
        buffer.store_complex(final12, 1);
        buffer.store_complex(final34, 3);
    }
}

pub struct Butterfly7Avx64<T> {
    twiddles: [__m256d; 5],
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly7Avx64, 7);
impl Butterfly7Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        let twiddle1 = twiddles::compute_twiddle(1, 7, direction);
        let twiddle2 = twiddles::compute_twiddle(2, 7, direction);
        let twiddle3 = twiddles::compute_twiddle(3, 7, direction);
        Self {
            twiddles: [
                _mm256_set_pd(twiddle1.im, twiddle1.im, twiddle1.re, twiddle1.re),
                _mm256_set_pd(twiddle2.im, twiddle2.im, twiddle2.re, twiddle2.re),
                _mm256_set_pd(twiddle3.im, twiddle3.im, twiddle3.re, twiddle3.re),
                _mm256_set_pd(-twiddle3.im, -twiddle3.im, twiddle3.re, twiddle3.re),
                _mm256_set_pd(-twiddle1.im, -twiddle1.im, twiddle1.re, twiddle1.re),
            ],
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly7Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        let input0 = _mm256_loadu2_m128d(
            buffer.input_ptr() as *const f64,
            buffer.input_ptr() as *const f64,
        );
        let input12 = buffer.load_complex(1);
        let input3 = buffer.load_partial1_complex(3);
        let input4 = buffer.load_partial1_complex(4);
        let input56 = buffer.load_complex(5);

        // reverse the order of input56
        let input65 = AvxVector::reverse_complex_elements(input56);

        // do some prep work before we can start applying twiddle factors
        let [sum12, diff65] = AvxVector::column_butterfly2([input12, input65]);
        let [sum3, diff4] = AvxVector::column_butterfly2([input3, input4]);

        let rotation = AvxVector::make_rotation90(FftDirection::Inverse);
        let rotated65 = AvxVector::rotate90(diff65, rotation);
        let rotated4 = AvxVector::rotate90(diff4, rotation.lo());

        let [mid16, mid25] = avx64_utils::transpose_2x2_f64([sum12, rotated65]);
        let mid34 = AvxVector128::merge(sum3, rotated4);

        // to compute the first output, compute the sum of all elements. mid16[0], mid25[0], and mid34[0] already have the sum of 1+6, 2+5 and 3+4 respectively, so if we add them, we'll get 1+2+3+4+5+6
        let output0_left = AvxVector::add(mid16.lo(), mid25.lo());
        let output0_right = AvxVector::add(input0.lo(), mid34.lo());
        let output0 = AvxVector::add(output0_left, output0_right);
        buffer.store_partial1_complex(output0, 0);

        // apply twiddle factors
        let twiddled16_intermediate1 = AvxVector::mul(mid16, self.twiddles[0]);
        let twiddled25_intermediate1 = AvxVector::mul(mid16, self.twiddles[1]);
        let twiddled34_intermediate1 = AvxVector::mul(mid16, self.twiddles[2]);

        let twiddled16_intermediate2 =
            AvxVector::fmadd(mid25, self.twiddles[1], twiddled16_intermediate1);
        let twiddled25_intermediate2 =
            AvxVector::fmadd(mid25, self.twiddles[3], twiddled25_intermediate1);
        let twiddled34_intermediate2 =
            AvxVector::fmadd(mid25, self.twiddles[4], twiddled34_intermediate1);

        let twiddled16 = AvxVector::fmadd(mid34, self.twiddles[2], twiddled16_intermediate2);
        let twiddled25 = AvxVector::fmadd(mid34, self.twiddles[4], twiddled25_intermediate2);
        let twiddled34 = AvxVector::fmadd(mid34, self.twiddles[1], twiddled34_intermediate2);

        // unpack the data for the last butterfly 2
        let [twiddled12, twiddled65] = avx64_utils::transpose_2x2_f64([twiddled16, twiddled25]);

        // we can save one add if we add input0 to twiddled3 now. normally we'd add input0 to the final output, but the arrangement of data makes that a little awkward
        let twiddled03 = AvxVector::add(twiddled34.lo(), input0.lo());

        let [output12, output65] = AvxVector::column_butterfly2([twiddled12, twiddled65]);
        let final12 = AvxVector::add(output12, input0);
        let output56 = AvxVector::reverse_complex_elements(output65);
        let final56 = AvxVector::add(output56, input0);

        let [final3, final4] = AvxVector::column_butterfly2([twiddled03, twiddled34.hi()]);

        buffer.store_complex(final12, 1);
        buffer.store_partial1_complex(final3, 3);
        buffer.store_partial1_complex(final4, 4);
        buffer.store_complex(final56, 5);
    }
}

pub struct Butterfly11Avx64<T> {
    twiddles: [__m256d; 10],
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly11Avx64, 11);
impl Butterfly11Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        let twiddle1 = twiddles::compute_twiddle(1, 11, direction);
        let twiddle2 = twiddles::compute_twiddle(2, 11, direction);
        let twiddle3 = twiddles::compute_twiddle(3, 11, direction);
        let twiddle4 = twiddles::compute_twiddle(4, 11, direction);
        let twiddle5 = twiddles::compute_twiddle(5, 11, direction);

        let twiddles = [
            _mm256_set_pd(twiddle1.im, twiddle1.im, twiddle1.re, twiddle1.re),
            _mm256_set_pd(twiddle2.im, twiddle2.im, twiddle2.re, twiddle2.re),
            _mm256_set_pd(twiddle3.im, twiddle3.im, twiddle3.re, twiddle3.re),
            _mm256_set_pd(twiddle4.im, twiddle4.im, twiddle4.re, twiddle4.re),
            _mm256_set_pd(twiddle5.im, twiddle5.im, twiddle5.re, twiddle5.re),
            _mm256_set_pd(-twiddle5.im, -twiddle5.im, twiddle5.re, twiddle5.re),
            _mm256_set_pd(-twiddle4.im, -twiddle4.im, twiddle4.re, twiddle4.re),
            _mm256_set_pd(-twiddle3.im, -twiddle3.im, twiddle3.re, twiddle3.re),
            _mm256_set_pd(-twiddle2.im, -twiddle2.im, twiddle2.re, twiddle2.re),
            _mm256_set_pd(-twiddle1.im, -twiddle1.im, twiddle1.re, twiddle1.re),
        ];

        Self {
            twiddles,
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly11Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        let input0 = buffer.load_partial1_complex(0);
        let input12 = buffer.load_complex(1);
        let input34 = buffer.load_complex(3);
        let input56 = buffer.load_complex(5);
        let input78 = buffer.load_complex(7);
        let input910 = buffer.load_complex(9);

        // reverse the order of input78910, and separate
        let [input55, input66] = AvxVector::unpack_complex([input56, input56]);
        let input87 = AvxVector::reverse_complex_elements(input78);
        let input109 = AvxVector::reverse_complex_elements(input910);

        // do some initial butterflies and rotations
        let [sum12, diff109] = AvxVector::column_butterfly2([input12, input109]);
        let [sum34, diff87] = AvxVector::column_butterfly2([input34, input87]);
        let [sum55, diff66] = AvxVector::column_butterfly2([input55, input66]);

        let rotation = AvxVector::make_rotation90(FftDirection::Inverse);
        let rotated109 = AvxVector::rotate90(diff109, rotation);
        let rotated87 = AvxVector::rotate90(diff87, rotation);
        let rotated66 = AvxVector::rotate90(diff66, rotation);

        // arrange the data into the format to apply twiddles
        let [mid110, mid29] = AvxVector::unpack_complex([sum12, rotated109]);
        let [mid38, mid47] = AvxVector::unpack_complex([sum34, rotated87]);
        let mid56 = AvxVector::unpacklo_complex([sum55, rotated66]);

        // to compute the first output, compute the sum of all elements. mid110[0], mid29[0], mid38[0], mid47 already have the sum of 1+10, 2+9 and so on, so if we add them, we'll get the sum of everything
        let mid12910 = AvxVector::add(mid110.lo(), mid29.lo());
        let mid3478 = AvxVector::add(mid38.lo(), mid47.lo());
        let output0_left = AvxVector::add(input0, mid56.lo());
        let output0_right = AvxVector::add(mid12910, mid3478);
        let output0 = AvxVector::add(output0_left, output0_right);
        buffer.store_partial1_complex(output0, 0);

        // we need to add the first input to each of our 5 twiddles values -- but only the first complex element of each vector. so just use zero for the other element
        let zero = _mm_setzero_pd();
        let input0 = AvxVector256::merge(input0, zero);

        // apply twiddle factors
        let twiddled110 = AvxVector::fmadd(mid110, self.twiddles[0], input0);
        let twiddled38 = AvxVector::fmadd(mid110, self.twiddles[2], input0);
        let twiddled29 = AvxVector::fmadd(mid110, self.twiddles[1], input0);
        let twiddled47 = AvxVector::fmadd(mid110, self.twiddles[3], input0);
        let twiddled56 = AvxVector::fmadd(mid110, self.twiddles[4], input0);

        let twiddled110 = AvxVector::fmadd(mid29, self.twiddles[1], twiddled110);
        let twiddled38 = AvxVector::fmadd(mid29, self.twiddles[5], twiddled38);
        let twiddled29 = AvxVector::fmadd(mid29, self.twiddles[3], twiddled29);
        let twiddled47 = AvxVector::fmadd(mid29, self.twiddles[7], twiddled47);
        let twiddled56 = AvxVector::fmadd(mid29, self.twiddles[9], twiddled56);

        let twiddled110 = AvxVector::fmadd(mid38, self.twiddles[2], twiddled110);
        let twiddled38 = AvxVector::fmadd(mid38, self.twiddles[8], twiddled38);
        let twiddled29 = AvxVector::fmadd(mid38, self.twiddles[5], twiddled29);
        let twiddled47 = AvxVector::fmadd(mid38, self.twiddles[0], twiddled47);
        let twiddled56 = AvxVector::fmadd(mid38, self.twiddles[3], twiddled56);

        let twiddled110 = AvxVector::fmadd(mid47, self.twiddles[3], twiddled110);
        let twiddled38 = AvxVector::fmadd(mid47, self.twiddles[0], twiddled38);
        let twiddled29 = AvxVector::fmadd(mid47, self.twiddles[7], twiddled29);
        let twiddled47 = AvxVector::fmadd(mid47, self.twiddles[4], twiddled47);
        let twiddled56 = AvxVector::fmadd(mid47, self.twiddles[8], twiddled56);

        let twiddled110 = AvxVector::fmadd(mid56, self.twiddles[4], twiddled110);
        let twiddled38 = AvxVector::fmadd(mid56, self.twiddles[3], twiddled38);
        let twiddled29 = AvxVector::fmadd(mid56, self.twiddles[9], twiddled29);
        let twiddled47 = AvxVector::fmadd(mid56, self.twiddles[8], twiddled47);
        let twiddled56 = AvxVector::fmadd(mid56, self.twiddles[2], twiddled56);

        // unpack the data for the last butterfly 2
        let [twiddled12, twiddled109] = AvxVector::unpack_complex([twiddled110, twiddled29]);
        let [twiddled34, twiddled87] = AvxVector::unpack_complex([twiddled38, twiddled47]);
        let [twiddled55, twiddled66] = AvxVector::unpack_complex([twiddled56, twiddled56]);

        let [output12, output109] = AvxVector::column_butterfly2([twiddled12, twiddled109]);
        let [output34, output87] = AvxVector::column_butterfly2([twiddled34, twiddled87]);
        let [output55, output66] = AvxVector::column_butterfly2([twiddled55, twiddled66]);
        let output78 = AvxVector::reverse_complex_elements(output87);
        let output910 = AvxVector::reverse_complex_elements(output109);

        buffer.store_complex(output12, 1);
        buffer.store_complex(output34, 3);
        buffer.store_partial1_complex(output55.lo(), 5);
        buffer.store_partial1_complex(output66.lo(), 6);
        buffer.store_complex(output78, 7);
        buffer.store_complex(output910, 9);
    }
}

pub struct Butterfly8Avx64<T> {
    twiddles: [__m256d; 2],
    twiddles_butterfly4: Rotation90<__m256d>,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly8Avx64, 8);
impl Butterfly8Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_interleaved_columns!(2, 4, 0, direction),
            twiddles_butterfly4: AvxVector::make_rotation90(direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly8Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        let row0 = buffer.load_complex(0);
        let row1 = buffer.load_complex(2);
        let row2 = buffer.load_complex(4);
        let row3 = buffer.load_complex(6);

        // Do our butterfly 2's down the columns of a 4x2 array
        let [mid0, mid2] = AvxVector::column_butterfly2([row0, row2]);
        let [mid1, mid3] = AvxVector::column_butterfly2([row1, row3]);

        let mid2_twiddled = AvxVector::mul_complex(mid2, self.twiddles[0]);
        let mid3_twiddled = AvxVector::mul_complex(mid3, self.twiddles[1]);

        // transpose to a 2x4 array
        let transposed =
            avx64_utils::transpose_4x2_to_2x4_f64([mid0, mid2_twiddled], [mid1, mid3_twiddled]);

        // butterfly 4's down the transposed array
        let output_rows = AvxVector::column_butterfly4(transposed, self.twiddles_butterfly4);

        buffer.store_complex(output_rows[0], 0);
        buffer.store_complex(output_rows[1], 2);
        buffer.store_complex(output_rows[2], 4);
        buffer.store_complex(output_rows[3], 6);
    }
}

pub struct Butterfly9Avx64<T> {
    twiddles: [__m256d; 2],
    twiddles_butterfly3: __m256d,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly9Avx64, 9);
impl Butterfly9Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_interleaved_columns!(3, 3, 1, direction),
            twiddles_butterfly3: AvxVector::broadcast_twiddle(1, 3, direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly9Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        // we're going to load our input as a 3x3 array. We have to load 3 columns, which is a little awkward
        // We can reduce the number of multiplies we do if we load the first column as half-width and the second column as full.
        let mut rows0 = [AvxVector::zero(); 3];
        let mut rows1 = [AvxVector::zero(); 3];

        for r in 0..3 {
            rows0[r] = buffer.load_partial1_complex(3 * r);
            rows1[r] = buffer.load_complex(3 * r + 1);
        }

        // do butterfly 3's down the columns
        let mid0 = AvxVector::column_butterfly3(rows0, self.twiddles_butterfly3.lo());
        let mut mid1 = AvxVector::column_butterfly3(rows1, self.twiddles_butterfly3);

        // apply twiddle factors
        for n in 1..3 {
            mid1[n] = AvxVector::mul_complex(mid1[n], self.twiddles[n - 1]);
        }

        // transpose our 3x3 array
        let (transposed0, transposed1) = avx64_utils::transpose_3x3_f64(mid0, mid1);

        // apply butterfly 3's down the columns
        let output0 = AvxVector::column_butterfly3(transposed0, self.twiddles_butterfly3.lo());
        let output1 = AvxVector::column_butterfly3(transposed1, self.twiddles_butterfly3);

        for r in 0..3 {
            buffer.store_partial1_complex(output0[r], 3 * r);
            buffer.store_complex(output1[r], 3 * r + 1);
        }
    }
}

pub struct Butterfly12Avx64<T> {
    twiddles: [__m256d; 3],
    twiddles_butterfly3: __m256d,
    twiddles_butterfly4: Rotation90<__m256d>,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly12Avx64, 12);
impl Butterfly12Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_interleaved_columns!(4, 3, 1, direction),
            twiddles_butterfly3: AvxVector::broadcast_twiddle(1, 3, direction),
            twiddles_butterfly4: AvxVector::make_rotation90(direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly12Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        // we're going to load our input as a 3x4 array. We have to load 3 columns, which is a little awkward
        // We can reduce the number of multiplies we do if we load the first column as half-width and the second column as full.
        let mut rows0 = [AvxVector::zero(); 4];
        let mut rows1 = [AvxVector::zero(); 4];

        for n in 0..4 {
            rows0[n] = buffer.load_partial1_complex(n * 3);
            rows1[n] = buffer.load_complex(n * 3 + 1);
        }

        // do butterfly 4's down the columns
        let mid0 = AvxVector::column_butterfly4(rows0, self.twiddles_butterfly4.lo());
        let mut mid1 = AvxVector::column_butterfly4(rows1, self.twiddles_butterfly4);

        // apply twiddle factors
        for n in 1..4 {
            mid1[n] = AvxVector::mul_complex(mid1[n], self.twiddles[n - 1]);
        }

        // transpose our 3x4 array to a 4x3 array
        let (transposed0, transposed1) = avx64_utils::transpose_3x4_to_4x3_f64(mid0, mid1);

        // apply butterfly 3's down the columns
        let output0 = AvxVector::column_butterfly3(transposed0, self.twiddles_butterfly3);
        let output1 = AvxVector::column_butterfly3(transposed1, self.twiddles_butterfly3);

        for r in 0..3 {
            buffer.store_complex(output0[r], 4 * r);
            buffer.store_complex(output1[r], 4 * r + 2);
        }
    }
}

pub struct Butterfly16Avx64<T> {
    twiddles: [__m256d; 6],
    twiddles_butterfly4: Rotation90<__m256d>,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly16Avx64, 16);
impl Butterfly16Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_interleaved_columns!(4, 4, 0, direction),
            twiddles_butterfly4: AvxVector::make_rotation90(direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly16Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        let mut rows0 = [AvxVector::zero(); 4];
        let mut rows1 = [AvxVector::zero(); 4];
        for r in 0..4 {
            rows0[r] = buffer.load_complex(4 * r);
            rows1[r] = buffer.load_complex(4 * r + 2);
        }

        // We're going to treat our input as a 4x4 2d array. First, do 4 butterfly 4's down the columns of that array.
        let mut mid0 = AvxVector::column_butterfly4(rows0, self.twiddles_butterfly4);
        let mut mid1 = AvxVector::column_butterfly4(rows1, self.twiddles_butterfly4);

        // apply twiddle factors
        for r in 1..4 {
            mid0[r] = AvxVector::mul_complex(mid0[r], self.twiddles[2 * r - 2]);
            mid1[r] = AvxVector::mul_complex(mid1[r], self.twiddles[2 * r - 1]);
        }

        // Transpose our 4x4 array
        let (transposed0, transposed1) = avx64_utils::transpose_4x4_f64(mid0, mid1);

        // Butterfly 4's down columns of the transposed array
        let output0 = AvxVector::column_butterfly4(transposed0, self.twiddles_butterfly4);
        let output1 = AvxVector::column_butterfly4(transposed1, self.twiddles_butterfly4);

        for r in 0..4 {
            buffer.store_complex(output0[r], 4 * r);
            buffer.store_complex(output1[r], 4 * r + 2);
        }
    }
}

pub struct Butterfly18Avx64<T> {
    twiddles: [__m256d; 5],
    twiddles_butterfly3: __m256d,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly18Avx64, 18);
impl Butterfly18Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_interleaved_columns!(6, 3, 1, direction),
            twiddles_butterfly3: AvxVector::broadcast_twiddle(1, 3, direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly18Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        // we're going to load our input as a 3x6 array. We have to load 3 columns, which is a little awkward
        // We can reduce the number of multiplies we do if we load the first column as half-width and the second column as full.
        let mut rows0 = [AvxVector::zero(); 6];
        let mut rows1 = [AvxVector::zero(); 6];
        for n in 0..6 {
            rows0[n] = buffer.load_partial1_complex(n * 3);
            rows1[n] = buffer.load_complex(n * 3 + 1);
        }

        // do butterfly 6's down the columns
        let mid0 = AvxVector128::column_butterfly6(rows0, self.twiddles_butterfly3);
        let mut mid1 = AvxVector256::column_butterfly6(rows1, self.twiddles_butterfly3);

        // apply twiddle factors
        for n in 1..6 {
            mid1[n] = AvxVector::mul_complex(mid1[n], self.twiddles[n - 1]);
        }

        // transpose our 3x6 array to a 6x3 array
        let (transposed0, transposed1, transposed2) =
            avx64_utils::transpose_3x6_to_6x3_f64(mid0, mid1);

        // apply butterfly 3's down the columns
        let output0 = AvxVector::column_butterfly3(transposed0, self.twiddles_butterfly3);
        let output1 = AvxVector::column_butterfly3(transposed1, self.twiddles_butterfly3);
        let output2 = AvxVector::column_butterfly3(transposed2, self.twiddles_butterfly3);

        for r in 0..3 {
            buffer.store_complex(output0[r], 6 * r);
            buffer.store_complex(output1[r], 6 * r + 2);
            buffer.store_complex(output2[r], 6 * r + 4);
        }
    }
}

pub struct Butterfly24Avx64<T> {
    twiddles: [__m256d; 9],
    twiddles_butterfly3: __m256d,
    twiddles_butterfly4: Rotation90<__m256d>,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly24Avx64, 24);
impl Butterfly24Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_interleaved_columns!(4, 6, 0, direction),
            twiddles_butterfly3: AvxVector::broadcast_twiddle(1, 3, direction),
            twiddles_butterfly4: AvxVector::make_rotation90(direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly24Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        let mut rows0 = [AvxVector::zero(); 4];
        let mut rows1 = [AvxVector::zero(); 4];
        let mut rows2 = [AvxVector::zero(); 4];
        for r in 0..4 {
            rows0[r] = buffer.load_complex(6 * r);
            rows1[r] = buffer.load_complex(6 * r + 2);
            rows2[r] = buffer.load_complex(6 * r + 4);
        }

        // We're going to treat our input as a 6x4 2d array. First, do 6 butterfly 4's down the columns of that array.
        let mut mid0 = AvxVector::column_butterfly4(rows0, self.twiddles_butterfly4);
        let mut mid1 = AvxVector::column_butterfly4(rows1, self.twiddles_butterfly4);
        let mut mid2 = AvxVector::column_butterfly4(rows2, self.twiddles_butterfly4);

        // apply twiddle factors
        for r in 1..4 {
            mid0[r] = AvxVector::mul_complex(mid0[r], self.twiddles[3 * r - 3]);
            mid1[r] = AvxVector::mul_complex(mid1[r], self.twiddles[3 * r - 2]);
            mid2[r] = AvxVector::mul_complex(mid2[r], self.twiddles[3 * r - 1]);
        }

        // Transpose our 6x4 array
        let (transposed0, transposed1) = avx64_utils::transpose_6x4_to_4x6_f64(mid0, mid1, mid2);

        // Butterfly 6's down columns of the transposed array
        let output0 = AvxVector256::column_butterfly6(transposed0, self.twiddles_butterfly3);
        let output1 = AvxVector256::column_butterfly6(transposed1, self.twiddles_butterfly3);

        for r in 0..6 {
            buffer.store_complex(output0[r], 4 * r);
            buffer.store_complex(output1[r], 4 * r + 2);
        }
    }
}

pub struct Butterfly27Avx64<T> {
    twiddles: [__m256d; 8],
    twiddles_butterfly9: [__m256d; 3],
    twiddles_butterfly9_lo: [__m256d; 2],
    twiddles_butterfly3: __m256d,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly27Avx64, 27);
impl Butterfly27Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        let twiddle1 = __m128d::broadcast_twiddle(1, 9, direction);
        let twiddle2 = __m128d::broadcast_twiddle(2, 9, direction);
        let twiddle4 = __m128d::broadcast_twiddle(4, 9, direction);

        Self {
            twiddles: gen_butterfly_twiddles_interleaved_columns!(3, 9, 1, direction),
            twiddles_butterfly9: [
                AvxVector::broadcast_twiddle(1, 9, direction),
                AvxVector::broadcast_twiddle(2, 9, direction),
                AvxVector::broadcast_twiddle(4, 9, direction),
            ],
            twiddles_butterfly9_lo: [
                AvxVector256::merge(twiddle1, twiddle2),
                AvxVector256::merge(twiddle2, twiddle4),
            ],
            twiddles_butterfly3: AvxVector::broadcast_twiddle(1, 3, direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly27Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        // we're going to load our input as a 9x3 array. We have to load 9 columns, which is a little awkward
        // We can reduce the number of multiplies we do if we load the first column as half-width and the remaining 4 sets of vectors as full.
        // We can't fit the whole problem into AVX registers at once, so we'll have to spill some things.
        // By computing chunks of the problem and then not referencing any of it for a while, we're making it easy for the compiler to decide what to spill
        let mut rows0 = [AvxVector::zero(); 3];
        for n in 0..3 {
            rows0[n] = buffer.load_partial1_complex(n * 9);
        }
        let mid0 = AvxVector::column_butterfly3(rows0, self.twiddles_butterfly3.lo());

        // First chunk is done and can be spilled, do 2 more chunks
        let mut rows1 = [AvxVector::zero(); 3];
        let mut rows2 = [AvxVector::zero(); 3];
        for n in 0..3 {
            rows1[n] = buffer.load_complex(n * 9 + 1);
            rows2[n] = buffer.load_complex(n * 9 + 3);
        }
        let mut mid1 = AvxVector::column_butterfly3(rows1, self.twiddles_butterfly3);
        let mut mid2 = AvxVector::column_butterfly3(rows2, self.twiddles_butterfly3);
        for r in 1..3 {
            mid1[r] = AvxVector::mul_complex(mid1[r], self.twiddles[4 * r - 4]);
            mid2[r] = AvxVector::mul_complex(mid2[r], self.twiddles[4 * r - 3]);
        }

        // First 3 chunks are done and can be spilled, do the final 2 chunks
        let mut rows3 = [AvxVector::zero(); 3];
        let mut rows4 = [AvxVector::zero(); 3];
        for n in 0..3 {
            rows3[n] = buffer.load_complex(n * 9 + 5);
            rows4[n] = buffer.load_complex(n * 9 + 7);
        }
        let mut mid3 = AvxVector::column_butterfly3(rows3, self.twiddles_butterfly3);
        let mut mid4 = AvxVector::column_butterfly3(rows4, self.twiddles_butterfly3);
        for r in 1..3 {
            mid3[r] = AvxVector::mul_complex(mid3[r], self.twiddles[4 * r - 2]);
            mid4[r] = AvxVector::mul_complex(mid4[r], self.twiddles[4 * r - 1]);
        }

        // transpose our 9x3 array to a 3x9 array
        let (transposed0, transposed1) =
            avx64_utils::transpose_9x3_to_3x9_f64(mid0, mid1, mid2, mid3, mid4);

        // apply butterfly 9's down the columns. Again, do the work in chunks to make it easier for the compiler to spill
        let output0 = AvxVector128::column_butterfly9(
            transposed0,
            self.twiddles_butterfly9_lo,
            self.twiddles_butterfly3,
        );
        for r in 0..3 {
            buffer.store_partial1_complex(output0[r * 3], 9 * r);
            buffer.store_partial1_complex(output0[r * 3 + 1], 9 * r + 3);
            buffer.store_partial1_complex(output0[r * 3 + 2], 9 * r + 6);
        }

        let output1 = AvxVector256::column_butterfly9(
            transposed1,
            self.twiddles_butterfly9,
            self.twiddles_butterfly3,
        );
        for r in 0..3 {
            buffer.store_complex(output1[r * 3], 9 * r + 1);
            buffer.store_complex(output1[r * 3 + 1], 9 * r + 4);
            buffer.store_complex(output1[r * 3 + 2], 9 * r + 7);
        }
    }
}

pub struct Butterfly32Avx64<T> {
    twiddles: [__m256d; 12],
    twiddles_butterfly4: Rotation90<__m256d>,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly32Avx64, 32);
impl Butterfly32Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_interleaved_columns!(4, 8, 0, direction),
            twiddles_butterfly4: AvxVector::make_rotation90(direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly32Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        // We're going to treat our input as a 8x4 2d array. First, do 8 butterfly 4's down the columns of that array.
        // We can't fit the whole problem into AVX registers at once, so we'll have to spill some things.
        // By computing half of the problem and then not referencing any of it for a while, we're making it easy for the compiler to decide what to spill
        let mut rows0 = [AvxVector::zero(); 4];
        let mut rows1 = [AvxVector::zero(); 4];
        for r in 0..4 {
            rows0[r] = buffer.load_complex(8 * r);
            rows1[r] = buffer.load_complex(8 * r + 2);
        }
        let mut mid0 = AvxVector::column_butterfly4(rows0, self.twiddles_butterfly4);
        let mut mid1 = AvxVector::column_butterfly4(rows1, self.twiddles_butterfly4);
        for r in 1..4 {
            mid0[r] = AvxVector::mul_complex(mid0[r], self.twiddles[4 * r - 4]);
            mid1[r] = AvxVector::mul_complex(mid1[r], self.twiddles[4 * r - 3]);
        }

        // One half is done, so the compiler can spill everything above this. Now do the other set of columns
        let mut rows2 = [AvxVector::zero(); 4];
        let mut rows3 = [AvxVector::zero(); 4];
        for r in 0..4 {
            rows2[r] = buffer.load_complex(8 * r + 4);
            rows3[r] = buffer.load_complex(8 * r + 6);
        }
        let mut mid2 = AvxVector::column_butterfly4(rows2, self.twiddles_butterfly4);
        let mut mid3 = AvxVector::column_butterfly4(rows3, self.twiddles_butterfly4);
        for r in 1..4 {
            mid2[r] = AvxVector::mul_complex(mid2[r], self.twiddles[4 * r - 2]);
            mid3[r] = AvxVector::mul_complex(mid3[r], self.twiddles[4 * r - 1]);
        }

        // Transpose our 8x4 array to a 4x8 array
        let (transposed0, transposed1) =
            avx64_utils::transpose_8x4_to_4x8_f64(mid0, mid1, mid2, mid3);

        // Do 4 butterfly 8's down columns of the transposed array
        // Same thing as above - Do the half of the butterfly 8's separately to give the compiler a better hint about what to spill
        let output0 = AvxVector::column_butterfly8(transposed0, self.twiddles_butterfly4);
        for r in 0..8 {
            buffer.store_complex(output0[r], 4 * r);
        }
        let output1 = AvxVector::column_butterfly8(transposed1, self.twiddles_butterfly4);
        for r in 0..8 {
            buffer.store_complex(output1[r], 4 * r + 2);
        }
    }
}

pub struct Butterfly36Avx64<T> {
    twiddles: [__m256d; 15],
    twiddles_butterfly3: __m256d,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly!(Butterfly36Avx64, 36);
impl Butterfly36Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_separated_columns!(6, 6, 0, direction),
            twiddles_butterfly3: AvxVector::broadcast_twiddle(1, 3, direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly36Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn perform_fft_f64(&self, mut buffer: impl AvxArrayMut<f64>) {
        // we're going to load our input as a 6x6 array
        // We can't fit the whole problem into AVX registers at once, so we'll have to spill some things.
        // By computing chunks of the problem and then not referencing any of it for a while, we're making it easy for the compiler to decide what to spill
        let mut rows0 = [AvxVector::zero(); 6];
        for n in 0..6 {
            rows0[n] = buffer.load_complex(n * 6);
        }
        let mut mid0 = AvxVector256::column_butterfly6(rows0, self.twiddles_butterfly3);
        for r in 1..6 {
            mid0[r] = AvxVector::mul_complex(mid0[r], self.twiddles[r - 1]);
        }

        // we're going to load our input as a 6x6 array
        let mut rows1 = [AvxVector::zero(); 6];
        for n in 0..6 {
            rows1[n] = buffer.load_complex(n * 6 + 2);
        }
        let mut mid1 = AvxVector256::column_butterfly6(rows1, self.twiddles_butterfly3);
        for r in 1..6 {
            mid1[r] = AvxVector::mul_complex(mid1[r], self.twiddles[r + 4]);
        }

        // we're going to load our input as a 6x6 array
        let mut rows2 = [AvxVector::zero(); 6];
        for n in 0..6 {
            rows2[n] = buffer.load_complex(n * 6 + 4);
        }
        let mut mid2 = AvxVector256::column_butterfly6(rows2, self.twiddles_butterfly3);
        for r in 1..6 {
            mid2[r] = AvxVector::mul_complex(mid2[r], self.twiddles[r + 9]);
        }

        // Transpose our 6x6 array
        let (transposed0, transposed1, transposed2) =
            avx64_utils::transpose_6x6_f64(mid0, mid1, mid2);

        // Apply butterfly 6's down the columns.  Again, do the work in chunks to make it easier for the compiler to spill
        let output0 = AvxVector256::column_butterfly6(transposed0, self.twiddles_butterfly3);
        for r in 0..3 {
            buffer.store_complex(output0[r * 2], 12 * r);
            buffer.store_complex(output0[r * 2 + 1], 12 * r + 6);
        }

        let output1 = AvxVector256::column_butterfly6(transposed1, self.twiddles_butterfly3);
        for r in 0..3 {
            buffer.store_complex(output1[r * 2], 12 * r + 2);
            buffer.store_complex(output1[r * 2 + 1], 12 * r + 8);
        }

        let output2 = AvxVector256::column_butterfly6(transposed2, self.twiddles_butterfly3);
        for r in 0..3 {
            buffer.store_complex(output2[r * 2], 12 * r + 4);
            buffer.store_complex(output2[r * 2 + 1], 12 * r + 10);
        }
    }
}

pub struct Butterfly64Avx64<T> {
    twiddles: [__m256d; 28],
    twiddles_butterfly4: Rotation90<__m256d>,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly_with_scratch!(Butterfly64Avx64, 64);
impl Butterfly64Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_separated_columns!(8, 8, 0, direction),
            twiddles_butterfly4: AvxVector::make_rotation90(direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly64Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn column_butterflies_and_transpose(
        &self,
        input: &[Complex<f64>],
        mut output: &mut [Complex<f64>],
    ) {
        // A size-64 FFT is way too big to fit in registers, so instead we're going to compute it in two phases, storing in scratch in between.

        // First phase is to treat this size-64 array like a 8x8 2D array, and do butterfly 8's down the columns
        // Then, apply twiddle factors, and finally transpose into the scratch space

        // But again, we don't have enough registers to load it all at once, so only load one column of AVX vectors at a time
        for columnset in 0..4 {
            let mut rows = [AvxVector::zero(); 8];
            for r in 0..8 {
                rows[r] = input.load_complex(columnset * 2 + 8 * r);
            }
            // apply butterfly 8
            let mut mid = AvxVector::column_butterfly8(rows, self.twiddles_butterfly4);

            // apply twiddle factors
            for r in 1..8 {
                mid[r] = AvxVector::mul_complex(mid[r], self.twiddles[r - 1 + 7 * columnset]);
            }

            // transpose
            let transposed = AvxVector::transpose8_packed(mid);

            // write out
            for i in 0..4 {
                output.store_complex(transposed[i * 2], columnset * 16 + i * 4);
                output.store_complex(transposed[i * 2 + 1], columnset * 16 + i * 4 + 2);
            }
        }
    }

    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn row_butterflies(&self, mut buffer: impl AvxArrayMut<f64>) {
        // Second phase: Butterfly 8's down the columns of our transposed array.
        // Thankfully, during the first phase, we set everything up so that all we have to do here is compute the size-8 FFT columns and write them back out where we got them
        for columnset in 0usize..4 {
            let mut rows = [AvxVector::zero(); 8];
            for r in 0..8 {
                rows[r] = buffer.load_complex(columnset * 2 + 8 * r);
            }
            let mid = AvxVector::column_butterfly8(rows, self.twiddles_butterfly4);
            for r in 0..8 {
                buffer.store_complex(mid[r], columnset * 2 + 8 * r);
            }
        }
    }
}

pub struct Butterfly128Avx64<T> {
    twiddles: [__m256d; 56],
    twiddles_butterfly16: [__m256d; 2],
    twiddles_butterfly4: Rotation90<__m256d>,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly_with_scratch!(Butterfly128Avx64, 128);
impl Butterfly128Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_separated_columns!(8, 16, 0, direction),
            twiddles_butterfly16: [
                AvxVector::broadcast_twiddle(1, 16, direction),
                AvxVector::broadcast_twiddle(3, 16, direction),
            ],
            twiddles_butterfly4: AvxVector::make_rotation90(direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly128Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn column_butterflies_and_transpose(
        &self,
        input: &[Complex<f64>],
        mut output: &mut [Complex<f64>],
    ) {
        // A size-128 FFT is way too big to fit in registers, so instead we're going to compute it in two phases, storing in scratch in between.

        // First phase is to treat this size-128 array like a 16x8 2D array, and do butterfly 8's down the columns
        // Then, apply twiddle factors, and finally transpose into the scratch space

        // But again, we don't have enough registers to load it all at once, so only load one column of AVX vectors at a time
        for columnset in 0..8 {
            let mut rows = [AvxVector::zero(); 8];
            for r in 0..8 {
                rows[r] = input.load_complex(columnset * 2 + 16 * r);
            }
            // apply butterfly 8
            let mut mid = AvxVector::column_butterfly8(rows, self.twiddles_butterfly4);

            // apply twiddle factors
            for r in 1..8 {
                mid[r] = AvxVector::mul_complex(mid[r], self.twiddles[r - 1 + 7 * columnset]);
            }

            // transpose
            let transposed = AvxVector::transpose8_packed(mid);

            // write out
            for i in 0..4 {
                output.store_complex(transposed[i * 2], columnset * 16 + i * 4);
                output.store_complex(transposed[i * 2 + 1], columnset * 16 + i * 4 + 2);
            }
        }
    }

    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn row_butterflies(&self, mut buffer: impl AvxArrayMut<f64>) {
        // Second phase: Butterfly 16's down the columns of our transposed array.
        // Thankfully, during the first phase, we set everything up so that all we have to do here is compute the size-16 FFT columns and write them back out where we got them
        // We're also using a customized butterfly16 function that is smarter about when it loads/stores data, to reduce register spilling
        for columnset in 0usize..4 {
            column_butterfly16_loadfn!(
                |index: usize| buffer.load_complex(columnset * 2 + index * 8),
                |data, index| buffer.store_complex(data, columnset * 2 + index * 8),
                self.twiddles_butterfly16,
                self.twiddles_butterfly4
            );
        }
    }
}

pub struct Butterfly256Avx64<T> {
    twiddles: [__m256d; 112],
    twiddles_butterfly32: [__m256d; 6],
    twiddles_butterfly4: Rotation90<__m256d>,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly_with_scratch!(Butterfly256Avx64, 256);
impl Butterfly256Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_separated_columns!(8, 32, 0, direction),
            twiddles_butterfly32: [
                AvxVector::broadcast_twiddle(1, 32, direction),
                AvxVector::broadcast_twiddle(2, 32, direction),
                AvxVector::broadcast_twiddle(3, 32, direction),
                AvxVector::broadcast_twiddle(5, 32, direction),
                AvxVector::broadcast_twiddle(6, 32, direction),
                AvxVector::broadcast_twiddle(7, 32, direction),
            ],
            twiddles_butterfly4: AvxVector::make_rotation90(direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly256Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn column_butterflies_and_transpose(
        &self,
        input: &[Complex<f64>],
        mut output: &mut [Complex<f64>],
    ) {
        // A size-256 FFT is way too big to fit in registers, so instead we're going to compute it in two phases, storing in scratch in between.

        // First phase is to treeat this size-256 array like a 32x8 2D array, and do butterfly 8's down the columns
        // Then, apply twiddle factors, and finally transpose into the scratch space

        // But again, we don't have enough registers to load it all at once, so only load one column of AVX vectors at a time
        for columnset in 0..16 {
            let mut rows = [AvxVector::zero(); 8];
            for r in 0..8 {
                rows[r] = input.load_complex(columnset * 2 + 32 * r);
            }
            // apply butterfly 8
            let mut mid = AvxVector::column_butterfly8(rows, self.twiddles_butterfly4);

            // apply twiddle factors
            for r in 1..8 {
                mid[r] = AvxVector::mul_complex(mid[r], self.twiddles[r - 1 + 7 * columnset]);
            }

            // transpose
            let transposed = AvxVector::transpose8_packed(mid);

            // write out
            for i in 0..4 {
                output.store_complex(transposed[i * 2], columnset * 16 + i * 4);
                output.store_complex(transposed[i * 2 + 1], columnset * 16 + i * 4 + 2);
            }
        }
    }

    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn row_butterflies(&self, mut buffer: impl AvxArrayMut<f64>) {
        // Second phase: Butterfly 32's down the columns of our transposed array.
        // Thankfully, during the first phase, we set everything up so that all we have to do here is compute the size-32 FFT columns and write them back out where we got them
        // We're also using a customized butterfly32 function that is smarter about when it loads/stores data, to reduce register spilling
        for columnset in 0usize..4 {
            column_butterfly32_loadfn!(
                |index: usize| buffer.load_complex(columnset * 2 + index * 8),
                |data, index| buffer.store_complex(data, columnset * 2 + index * 8),
                self.twiddles_butterfly32,
                self.twiddles_butterfly4
            );
        }
    }
}

pub struct Butterfly512Avx64<T> {
    twiddles: [__m256d; 240],
    twiddles_butterfly32: [__m256d; 6],
    twiddles_butterfly16: [__m256d; 2],
    twiddles_butterfly4: Rotation90<__m256d>,
    direction: FftDirection,
    _phantom_t: std::marker::PhantomData<T>,
}
boilerplate_fft_simd_butterfly_with_scratch!(Butterfly512Avx64, 512);
impl Butterfly512Avx64<f64> {
    #[target_feature(enable = "avx")]
    unsafe fn new_with_avx(direction: FftDirection) -> Self {
        Self {
            twiddles: gen_butterfly_twiddles_separated_columns!(16, 32, 0, direction),
            twiddles_butterfly32: [
                AvxVector::broadcast_twiddle(1, 32, direction),
                AvxVector::broadcast_twiddle(2, 32, direction),
                AvxVector::broadcast_twiddle(3, 32, direction),
                AvxVector::broadcast_twiddle(5, 32, direction),
                AvxVector::broadcast_twiddle(6, 32, direction),
                AvxVector::broadcast_twiddle(7, 32, direction),
            ],
            twiddles_butterfly16: [
                AvxVector::broadcast_twiddle(1, 16, direction),
                AvxVector::broadcast_twiddle(3, 16, direction),
            ],
            twiddles_butterfly4: AvxVector::make_rotation90(direction),
            direction,
            _phantom_t: PhantomData,
        }
    }
}
impl<T> Butterfly512Avx64<T> {
    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn column_butterflies_and_transpose(
        &self,
        input: &[Complex<f64>],
        mut output: &mut [Complex<f64>],
    ) {
        // A size-512 FFT is way too big to fit in registers, so instead we're going to compute it in two phases, storing in scratch in between.

        // First phase is to treat this size-512 array like a 32x16 2D array, and do butterfly 16's down the columns
        // Then, apply twiddle factors, and finally transpose into the scratch space

        // But again, we don't have enough registers to load it all at once, so only load one column of AVX vectors at a time
        // We're also using a customized butterfly16 function that is smarter about when it loads/stores data, to reduce register spilling
        const TWIDDLES_PER_COLUMN: usize = 15;
        for (columnset, twiddle_chunk) in
            self.twiddles.chunks_exact(TWIDDLES_PER_COLUMN).enumerate()
        {
            // Sadly we have to use MaybeUninit here. If we init an array like normal with AvxVector::Zero(), the compiler can't seem to figure out that it can
            // eliminate the dead stores of zeroes to the stack. By using uninit here, we avoid those unnecessary writes
            let mut mid_uninit: [MaybeUninit<__m256d>; 16] = [MaybeUninit::<__m256d>::uninit(); 16];

            column_butterfly16_loadfn!(
                |index: usize| input.load_complex(columnset * 2 + 32 * index),
                |data, index: usize| {
                    mid_uninit[index].as_mut_ptr().write(data);
                },
                self.twiddles_butterfly16,
                self.twiddles_butterfly4
            );

            // Apply twiddle factors, transpose, and store. Traditionally we apply all the twiddle factors at once and then do all the transposes at once,
            // But our data is pushing the limit of what we can store in registers, so the idea here is to get the data out the door with as few spills to the stack as possible
            for chunk in 0..4 {
                let twiddled = [
                    if chunk > 0 {
                        AvxVector::mul_complex(
                            mid_uninit[4 * chunk].assume_init(),
                            twiddle_chunk[4 * chunk - 1],
                        )
                    } else {
                        mid_uninit[4 * chunk].assume_init()
                    },
                    AvxVector::mul_complex(
                        mid_uninit[4 * chunk + 1].assume_init(),
                        twiddle_chunk[4 * chunk],
                    ),
                    AvxVector::mul_complex(
                        mid_uninit[4 * chunk + 2].assume_init(),
                        twiddle_chunk[4 * chunk + 1],
                    ),
                    AvxVector::mul_complex(
                        mid_uninit[4 * chunk + 3].assume_init(),
                        twiddle_chunk[4 * chunk + 2],
                    ),
                ];

                let transposed = AvxVector::transpose4_packed(twiddled);

                output.store_complex(transposed[0], columnset * 32 + 4 * chunk);
                output.store_complex(transposed[1], columnset * 32 + 4 * chunk + 2);
                output.store_complex(transposed[2], columnset * 32 + 4 * chunk + 16);
                output.store_complex(transposed[3], columnset * 32 + 4 * chunk + 18);
            }
        }
    }

    #[target_feature(enable = "avx", enable = "fma")]
    unsafe fn row_butterflies(&self, mut buffer: impl AvxArrayMut<f64>) {
        // Second phase: Butterfly 32's down the columns of our transposed array.
        // Thankfully, during the first phase, we set everything up so that all we have to do here is compute the size-32 FFT columns and write them back out where we got them
        // We're also using a customized butterfly32 function that is smarter about when it loads/stores data, to reduce register spilling
        for columnset in 0usize..8 {
            column_butterfly32_loadfn!(
                |index: usize| buffer.load_complex(columnset * 2 + index * 16),
                |data, index| buffer.store_complex(data, columnset * 2 + index * 16),
                self.twiddles_butterfly32,
                self.twiddles_butterfly4
            );
        }
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;
    use crate::test_utils::check_fft_algorithm;

    macro_rules! test_avx_butterfly {
        ($test_name:ident, $struct_name:ident, $size:expr) => (
            #[test]
            fn $test_name() {
                let butterfly = $struct_name::new(FftDirection::Forward).expect("Can't run test because this machine doesn't have the required instruction sets");
                check_fft_algorithm(&butterfly as &dyn Fft<f64>, $size, FftDirection::Forward);

                let butterfly_inverse = $struct_name::new(FftDirection::Inverse).expect("Can't run test because this machine doesn't have the required instruction sets");
                check_fft_algorithm(&butterfly_inverse as &dyn Fft<f64>, $size, FftDirection::Inverse);
            }
        )
    }

    test_avx_butterfly!(test_avx_butterfly5_f64, Butterfly5Avx64, 5);
    test_avx_butterfly!(test_avx_butterfly7_f64, Butterfly7Avx64, 7);
    test_avx_butterfly!(test_avx_mixedradix8_f64, Butterfly8Avx64, 8);
    test_avx_butterfly!(test_avx_mixedradix9_f64, Butterfly9Avx64, 9);
    test_avx_butterfly!(test_avx_mixedradix11_f64, Butterfly11Avx64, 11);
    test_avx_butterfly!(test_avx_mixedradix12_f64, Butterfly12Avx64, 12);
    test_avx_butterfly!(test_avx_mixedradix16_f64, Butterfly16Avx64, 16);
    test_avx_butterfly!(test_avx_mixedradix18_f64, Butterfly18Avx64, 18);
    test_avx_butterfly!(test_avx_mixedradix24_f64, Butterfly24Avx64, 24);
    test_avx_butterfly!(test_avx_mixedradix27_f64, Butterfly27Avx64, 27);
    test_avx_butterfly!(test_avx_mixedradix32_f64, Butterfly32Avx64, 32);
    test_avx_butterfly!(test_avx_mixedradix36_f64, Butterfly36Avx64, 36);
    test_avx_butterfly!(test_avx_mixedradix64_f64, Butterfly64Avx64, 64);
    test_avx_butterfly!(test_avx_mixedradix128_f64, Butterfly128Avx64, 128);
    test_avx_butterfly!(test_avx_mixedradix256_f64, Butterfly256Avx64, 256);
    test_avx_butterfly!(test_avx_mixedradix512_f64, Butterfly512Avx64, 512);
}
