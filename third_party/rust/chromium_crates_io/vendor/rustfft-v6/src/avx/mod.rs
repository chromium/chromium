use crate::fft_helper::{fft_helper_immut, fft_helper_inplace, fft_helper_outofplace};
use crate::{Fft, FftDirection, FftNum};
use std::arch::x86_64::{__m256, __m256d};
use std::sync::Arc;

pub trait AvxNum: FftNum {
    type VectorType: AvxVector256<ScalarType = Self>;
}

impl AvxNum for f32 {
    type VectorType = __m256;
}
impl AvxNum for f64 {
    type VectorType = __m256d;
}

// Data that most (non-butterfly) SIMD FFT algorithms share
// Algorithms aren't required to use this struct, but it allows for a lot of reduction in code duplication
struct CommonSimdData<T, V> {
    inner_fft: Arc<dyn Fft<T>>,
    twiddles: Box<[V]>,

    len: usize,

    inplace_scratch_len: usize,
    outofplace_scratch_len: usize,
    immut_scratch_len: usize,

    direction: FftDirection,
}

macro_rules! boilerplate_avx_fft {
    ($struct_name:ident, $len_fn:expr, $inplace_scratch_len_fn:expr, $out_of_place_scratch_len_fn:expr, $immut_scratch_len_fn:expr) => {
        impl<A: AvxNum, T: FftNum> Fft<T> for $struct_name<A, T> {
            fn process_immutable_with_scratch(
                &self,
                input: &[Complex<T>],
                output: &mut [Complex<T>],
                scratch: &mut [Complex<T>],
            ) {
                unsafe {
                    super::avx_fft_helper_immut(
                        input,
                        output,
                        scratch,
                        self.len(),
                        self.get_immutable_scratch_len(),
                        |in_chunk, out_chunk, scratch| {
                            self.perform_fft_immut(in_chunk, out_chunk, scratch)
                        },
                    )
                }
            }

            fn process_outofplace_with_scratch(
                &self,
                input: &mut [Complex<T>],
                output: &mut [Complex<T>],
                scratch: &mut [Complex<T>],
            ) {
                unsafe {
                    super::avx_fft_helper_outofplace(
                        input,
                        output,
                        scratch,
                        self.len(),
                        self.get_outofplace_scratch_len(),
                        |in_chunk, out_chunk, scratch| {
                            self.perform_fft_out_of_place(in_chunk, out_chunk, scratch)
                        },
                    )
                }
            }
            fn process_with_scratch(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
                unsafe {
                    super::avx_fft_helper_inplace(
                        buffer,
                        scratch,
                        self.len(),
                        self.get_inplace_scratch_len(),
                        |chunk, scratch| self.perform_fft_inplace(chunk, scratch),
                    )
                }
            }
            #[inline(always)]
            fn get_inplace_scratch_len(&self) -> usize {
                $inplace_scratch_len_fn(self)
            }
            #[inline(always)]
            fn get_outofplace_scratch_len(&self) -> usize {
                $out_of_place_scratch_len_fn(self)
            }
            #[inline(always)]
            fn get_immutable_scratch_len(&self) -> usize {
                $immut_scratch_len_fn(self)
            }
        }
        impl<A: AvxNum, T> Length for $struct_name<A, T> {
            #[inline(always)]
            fn len(&self) -> usize {
                $len_fn(self)
            }
        }
        impl<A: AvxNum, T> Direction for $struct_name<A, T> {
            #[inline(always)]
            fn fft_direction(&self) -> FftDirection {
                self.direction
            }
        }
    };
}

macro_rules! boilerplate_avx_fft_commondata {
    ($struct_name:ident) => {
        impl<A: AvxNum, T: FftNum> Fft<T> for $struct_name<A, T> {
            fn process_immutable_with_scratch(
                &self,
                input: &[Complex<T>],
                output: &mut [Complex<T>],
                scratch: &mut [Complex<T>],
            ) {
                unsafe {
                    super::avx_fft_helper_immut(
                        input,
                        output,
                        scratch,
                        self.len(),
                        self.get_immutable_scratch_len(),
                        |in_chunk, out_chunk, scratch| {
                            self.perform_fft_immut(in_chunk, out_chunk, scratch)
                        },
                    )
                }
            }
            fn process_outofplace_with_scratch(
                &self,
                input: &mut [Complex<T>],
                output: &mut [Complex<T>],
                scratch: &mut [Complex<T>],
            ) {
                unsafe {
                    super::avx_fft_helper_outofplace(
                        input,
                        output,
                        scratch,
                        self.len(),
                        self.get_outofplace_scratch_len(),
                        |in_chunk, out_chunk, scratch| {
                            self.perform_fft_out_of_place(in_chunk, out_chunk, scratch)
                        },
                    )
                }
            }
            fn process_with_scratch(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
                unsafe {
                    super::avx_fft_helper_inplace(
                        buffer,
                        scratch,
                        self.len(),
                        self.get_inplace_scratch_len(),
                        |chunk, scratch| self.perform_fft_inplace(chunk, scratch),
                    )
                }
            }
            #[inline(always)]
            fn get_inplace_scratch_len(&self) -> usize {
                self.common_data.inplace_scratch_len
            }
            #[inline(always)]
            fn get_outofplace_scratch_len(&self) -> usize {
                self.common_data.outofplace_scratch_len
            }
            #[inline(always)]
            fn get_immutable_scratch_len(&self) -> usize {
                self.common_data.immut_scratch_len
            }
        }
        impl<A: AvxNum, T> Length for $struct_name<A, T> {
            #[inline(always)]
            fn len(&self) -> usize {
                self.common_data.len
            }
        }
        impl<A: AvxNum, T> Direction for $struct_name<A, T> {
            #[inline(always)]
            fn fft_direction(&self) -> FftDirection {
                self.common_data.direction
            }
        }
    };
}

// A wrapper for the FFT helper functions that make sure the entire thing happens with the benefit of the AVX and FMA target features,
// so that things like loading twiddle factor registers etc can be lifted out of the loop
#[target_feature(enable = "avx", enable = "fma")]
pub unsafe fn avx_fft_helper_immut<T>(
    input: &[T],
    output: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    chunk_fn: impl FnMut(&[T], &mut [T], &mut [T]),
) {
    fft_helper_immut(
        input,
        output,
        scratch,
        chunk_size,
        required_scratch,
        chunk_fn,
    )
}

// A wrapper for the FFT helper functions that make sure the entire thing happens with the benefit of the AVX and FMA target features,
// so that things like loading twiddle factor registers etc can be lifted out of the loop
#[target_feature(enable = "avx", enable = "fma")]
pub unsafe fn avx_fft_helper_outofplace<T>(
    input: &mut [T],
    output: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    chunk_fn: impl FnMut(&mut [T], &mut [T], &mut [T]),
) {
    fft_helper_outofplace(
        input,
        output,
        scratch,
        chunk_size,
        required_scratch,
        chunk_fn,
    )
}

// A wrapper for the FFT helper functions that make sure the entire thing happens with the benefit of the AVX and FMA target features,
// so that things like loading twiddle factor registers etc can be lifted out of the loop
#[target_feature(enable = "avx", enable = "fma")]
pub unsafe fn avx_fft_helper_inplace<T>(
    buffer: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    chunk_fn: impl FnMut(&mut [T], &mut [T]),
) {
    fft_helper_inplace(buffer, scratch, chunk_size, required_scratch, chunk_fn)
}

#[macro_use]
mod avx_vector;

mod avx32_butterflies;
mod avx32_utils;

mod avx64_butterflies;
mod avx64_utils;

mod avx_bluesteins;
mod avx_mixed_radix;
mod avx_raders;

pub mod avx_planner;

pub use self::avx32_butterflies::{
    Butterfly11Avx, Butterfly128Avx, Butterfly12Avx, Butterfly16Avx, Butterfly24Avx,
    Butterfly256Avx, Butterfly27Avx, Butterfly32Avx, Butterfly36Avx, Butterfly48Avx,
    Butterfly512Avx, Butterfly54Avx, Butterfly5Avx, Butterfly64Avx, Butterfly72Avx, Butterfly7Avx,
    Butterfly8Avx, Butterfly9Avx,
};

pub use self::avx64_butterflies::{
    Butterfly11Avx64, Butterfly128Avx64, Butterfly12Avx64, Butterfly16Avx64, Butterfly18Avx64,
    Butterfly24Avx64, Butterfly256Avx64, Butterfly27Avx64, Butterfly32Avx64, Butterfly36Avx64,
    Butterfly512Avx64, Butterfly5Avx64, Butterfly64Avx64, Butterfly7Avx64, Butterfly8Avx64,
    Butterfly9Avx64,
};

pub use self::avx_bluesteins::BluesteinsAvx;
pub use self::avx_mixed_radix::{
    MixedRadix11xnAvx, MixedRadix12xnAvx, MixedRadix16xnAvx, MixedRadix2xnAvx, MixedRadix3xnAvx,
    MixedRadix4xnAvx, MixedRadix5xnAvx, MixedRadix6xnAvx, MixedRadix7xnAvx, MixedRadix8xnAvx,
    MixedRadix9xnAvx,
};
pub use self::avx_raders::RadersAvx2;
use self::avx_vector::AvxVector256;
