use std::any::TypeId;

use crate::fft_helper::{
    fft_helper_immut, fft_helper_immut_unroll2x, fft_helper_inplace, fft_helper_inplace_unroll2x,
    fft_helper_outofplace, fft_helper_outofplace_unroll2x,
};

/// Helper function to assert we have the right float type
pub fn assert_f32<T: 'static>() {
    let id_f32 = TypeId::of::<f32>();
    let id_t = TypeId::of::<T>();
    assert!(id_t == id_f32, "Wrong float type, must be f32");
}

/// Helper function to assert we have the right float type
pub fn assert_f64<T: 'static>() {
    let id_f64 = TypeId::of::<f64>();
    let id_t = TypeId::of::<T>();
    assert!(id_t == id_f64, "Wrong float type, must be f64");
}

/// Shuffle elements to interleave two contiguous sets of f32, from an array of simd vectors to a new array of simd vectors
macro_rules! interleave_complex_f32 {
    ($input:ident, $offset:literal, { $($idx:literal),* }) => {
        [
        $(
            extract_lo_lo_f32_v128($input[$idx], $input[$idx+$offset]),
            extract_hi_hi_f32_v128($input[$idx], $input[$idx+$offset]),
        )*
        ]
    }
}

/// Shuffle elements to interleave two contiguous sets of f32, from an array of simd vectors to a new array of simd vectors
/// This statement:
///
/// let values = separate_interleaved_complex_f32!(input, {0, 2, 4});
///
/// is equivalent to:
///
/// let values = [
///    extract_lo_lo_f32(input[0], input[1]),
///    extract_lo_lo_f32(input[2], input[3]),
///    extract_lo_lo_f32(input[4], input[5]),
///    extract_hi_hi_f32(input[0], input[1]),
///    extract_hi_hi_f32(input[2], input[3]),
///    extract_hi_hi_f32(input[4], input[5]),
/// ];
///
macro_rules! separate_interleaved_complex_f32 {
    ($input:ident, { $($idx:literal),* }) => {
        [
        $(
            extract_lo_lo_f32_v128($input[$idx], $input[$idx+1]),
        )*
        $(
            extract_hi_hi_f32_v128($input[$idx], $input[$idx+1]),
        )*
        ]
    }
}

macro_rules! boilerplate_fft_wasm_simd_oop {
    ($struct_name:ident, $len_fn:expr) => {
        impl<S: WasmNum, T: FftNum> Fft<T> for $struct_name<S, T> {
            fn process_immutable_with_scratch(
                &self,
                input: &[Complex<T>],
                output: &mut [Complex<T>],
                _scratch: &mut [Complex<T>],
            ) {
                unsafe {
                    let simd_input = crate::array_utils::workaround_transmute(input);
                    let simd_output = crate::array_utils::workaround_transmute_mut(output);
                    super::wasm_simd_common::wasm_simd_fft_helper_immut(
                        simd_input,
                        simd_output,
                        &mut [],
                        self.len(),
                        0,
                        |input, output, _| self.perform_fft_immut(input, output, &mut []),
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
                    super::wasm_simd_common::wasm_simd_fft_helper_outofplace(
                        simd_input,
                        simd_output,
                        &mut [],
                        self.len(),
                        0,
                        |input, output, _| self.perform_fft_out_of_place(input, output, &mut []),
                    );
                }
            }
            fn process_with_scratch(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
                unsafe {
                    let simd_buffer = crate::array_utils::workaround_transmute_mut(buffer);
                    let simd_scratch = crate::array_utils::workaround_transmute_mut(scratch);
                    super::wasm_simd_common::wasm_simd_fft_helper_inplace(
                        simd_buffer,
                        simd_scratch,
                        self.len(),
                        self.get_inplace_scratch_len(),
                        |chunk, scratch| {
                            self.perform_fft_out_of_place(chunk, scratch, &mut []);
                            chunk.copy_from_slice(scratch);
                        },
                    )
                }
            }
            #[inline(always)]
            fn get_inplace_scratch_len(&self) -> usize {
                self.len()
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
        impl<S: WasmNum, T> Length for $struct_name<S, T> {
            #[inline(always)]
            fn len(&self) -> usize {
                $len_fn(self)
            }
        }
        impl<S: WasmNum, T> Direction for $struct_name<S, T> {
            #[inline(always)]
            fn fft_direction(&self) -> FftDirection {
                self.direction
            }
        }
    };
}

// A wrapper for the FFT helper functions that make sure the entire thing happens with the benefit of the Wasm SIMD target feature,
// so that things like loading twiddle factor registers etc can be lifted out of the loop
#[target_feature(enable = "simd128")]
pub unsafe fn wasm_simd_fft_helper_immut<T>(
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

// A wrapper for the FFT helper functions that make sure the entire thing happens with the benefit of the Wasm SIMD target feature,
// so that things like loading twiddle factor registers etc can be lifted out of the loop
#[target_feature(enable = "simd128")]
pub unsafe fn wasm_simd_fft_helper_outofplace<T>(
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

// A wrapper for the FFT helper functions that make sure the entire thing happens with the benefit of the Wasm SIMD target feature,
// so that things like loading twiddle factor registers etc can be lifted out of the loop
#[target_feature(enable = "simd128")]
pub unsafe fn wasm_simd_fft_helper_inplace<T>(
    buffer: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    chunk_fn: impl FnMut(&mut [T], &mut [T]),
) {
    fft_helper_inplace(buffer, scratch, chunk_size, required_scratch, chunk_fn)
}

// A wrapper for the FFT helper functions that make sure the entire thing happens with the benefit of the Wasm SIMD target feature,
// so that things like loading twiddle factor registers etc can be lifted out of the loop
#[target_feature(enable = "simd128")]
pub unsafe fn wasm_simd_fft_helper_immut_unroll2x<T>(
    input: &[T],
    output: &mut [T],
    chunk_size: usize,
    chunk2x_fn: impl FnMut(&[T], &mut [T]),
    chunk_fn: impl FnMut(&[T], &mut [T]),
) {
    fft_helper_immut_unroll2x(input, output, chunk_size, chunk2x_fn, chunk_fn)
}

// A wrapper for the FFT helper functions that make sure the entire thing happens with the benefit of the Wasm SIMD target feature,
// so that things like loading twiddle factor registers etc can be lifted out of the loop
#[target_feature(enable = "simd128")]
pub unsafe fn wasm_simd_fft_helper_outofplace_unroll2x<T>(
    input: &mut [T],
    output: &mut [T],
    chunk_size: usize,
    chunk2x_fn: impl FnMut(&mut [T], &mut [T]),
    chunk_fn: impl FnMut(&mut [T], &mut [T]),
) {
    fft_helper_outofplace_unroll2x(input, output, chunk_size, chunk2x_fn, chunk_fn)
}

// A wrapper for the FFT helper functions that make sure the entire thing happens with the benefit of the Wasm SIMD target feature,
// so that things like loading twiddle factor registers etc can be lifted out of the loop
#[target_feature(enable = "simd128")]
pub unsafe fn wasm_simd_fft_helper_inplace_unroll2x<T>(
    buffer: &mut [T],
    chunk_size: usize,
    chunk2x_fn: impl FnMut(&mut [T]),
    chunk_fn: impl FnMut(&mut [T]),
) {
    fft_helper_inplace_unroll2x(buffer, chunk_size, chunk2x_fn, chunk_fn)
}
