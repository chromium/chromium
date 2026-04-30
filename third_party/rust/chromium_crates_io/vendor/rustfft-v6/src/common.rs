use num_traits::{FromPrimitive, Signed};
use std::fmt::Debug;

/// Generic floating point number, implemented for f32 and f64
pub trait FftNum: Copy + FromPrimitive + Signed + Sync + Send + Debug + 'static {}

impl<T> FftNum for T where T: Copy + FromPrimitive + Signed + Sync + Send + Debug + 'static {}

// Prints an error raised by an in-place FFT algorithm's `process_inplace` method
// Marked cold and inline never to keep all formatting code out of the many monomorphized process_inplace methods
#[cold]
#[inline(never)]
pub fn fft_error_inplace(
    expected_len: usize,
    actual_len: usize,
    expected_scratch: usize,
    actual_scratch: usize,
) {
    assert!(
        actual_len >= expected_len,
        "Provided FFT buffer was too small. Expected len = {}, got len = {}",
        expected_len,
        actual_len
    );
    assert_eq!(
        actual_len % expected_len,
        0,
        "Input FFT buffer must be a multiple of FFT length. Expected multiple of {}, got len = {}",
        expected_len,
        actual_len
    );
    assert!(
        actual_scratch >= expected_scratch,
        "Not enough scratch space was provided. Expected scratch len >= {}, got scratch len = {}",
        expected_scratch,
        actual_scratch
    );
}

// Prints an error raised by an in-place FFT algorithm's `process_inplace` method
// Marked cold and inline never to keep all formatting code out of the many monomorphized process_inplace methods
#[cold]
#[inline(never)]
pub fn fft_error_outofplace(
    expected_len: usize,
    actual_input: usize,
    actual_output: usize,
    expected_scratch: usize,
    actual_scratch: usize,
) {
    assert_eq!(actual_input, actual_output, "Provided FFT input buffer and output buffer must have the same length. Got input.len() = {}, output.len() = {}", actual_input, actual_output);
    assert!(
        actual_input >= expected_len,
        "Provided FFT buffer was too small. Expected len = {}, got len = {}",
        expected_len,
        actual_input
    );
    assert_eq!(
        actual_input % expected_len,
        0,
        "Input FFT buffer must be a multiple of FFT length. Expected multiple of {}, got len = {}",
        expected_len,
        actual_input
    );
    assert!(
        actual_scratch >= expected_scratch,
        "Not enough scratch space was provided. Expected scratch len >= {}, got scratch len = {}",
        expected_scratch,
        actual_scratch
    );
}

// Prints an error raised by an in-place FFT algorithm's `process_inplace` method
// Marked cold and inline never to keep all formatting code out of the many monomorphized process_inplace methods
#[cold]
#[inline(never)]
pub fn fft_error_immut(
    expected_len: usize,
    actual_input: usize,
    actual_output: usize,
    expected_scratch: usize,
    actual_scratch: usize,
) {
    assert_eq!(actual_input, actual_output, "Provided FFT input buffer and output buffer must have the same length. Got input.len() = {}, output.len() = {}", actual_input, actual_output);
    assert!(
        actual_input >= expected_len,
        "Provided FFT buffer was too small. Expected len = {}, got len = {}",
        expected_len,
        actual_input
    );
    assert_eq!(
        actual_input % expected_len,
        0,
        "Input FFT buffer must be a multiple of FFT length. Expected multiple of {}, got len = {}",
        expected_len,
        actual_input
    );
    assert!(
        actual_scratch >= expected_scratch,
        "Not enough scratch space was provided. Expected scratch len >= {}, got scratch len = {}",
        expected_scratch,
        actual_scratch
    );
}

macro_rules! boilerplate_fft_oop {
    ($struct_name:ident, $len_fn:expr) => {
        impl<T: FftNum> Fft<T> for $struct_name<T> {
            fn process_immutable_with_scratch(
                &self,
                input: &[Complex<T>],
                output: &mut [Complex<T>],
                scratch: &mut [Complex<T>],
            ) {
                crate::fft_helper::fft_helper_immut(
                    input,
                    output,
                    scratch,
                    self.len(),
                    self.get_immutable_scratch_len(),
                    |in_chunk, out_chunk, scratch| {
                        self.perform_fft_immut(in_chunk, out_chunk, scratch)
                    },
                );
            }
            fn process_outofplace_with_scratch(
                &self,
                input: &mut [Complex<T>],
                output: &mut [Complex<T>],
                scratch: &mut [Complex<T>],
            ) {
                crate::fft_helper::fft_helper_outofplace(
                    input,
                    output,
                    scratch,
                    self.len(),
                    self.get_outofplace_scratch_len(),
                    |in_chunk, out_chunk, scratch| {
                        self.perform_fft_out_of_place(in_chunk, out_chunk, scratch)
                    },
                );
            }
            fn process_with_scratch(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
                crate::fft_helper::fft_helper_inplace(
                    buffer,
                    scratch,
                    self.len(),
                    self.get_inplace_scratch_len(),
                    |chunk, scratch| {
                        let (self_scratch, inner_scratch) = scratch.split_at_mut(self.len());
                        self.perform_fft_out_of_place(chunk, self_scratch, inner_scratch);
                        chunk.copy_from_slice(self_scratch);
                    },
                );
            }
            #[inline(always)]
            fn get_inplace_scratch_len(&self) -> usize {
                self.inplace_scratch_len()
            }
            #[inline(always)]
            fn get_outofplace_scratch_len(&self) -> usize {
                self.outofplace_scratch_len()
            }
            #[inline(always)]
            fn get_immutable_scratch_len(&self) -> usize {
                self.immut_scratch_len()
            }
        }
        impl<T> Length for $struct_name<T> {
            #[inline(always)]
            fn len(&self) -> usize {
                $len_fn(self)
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

macro_rules! boilerplate_fft {
    ($struct_name:ident, $len_fn:expr, $inplace_scratch_len_fn:expr, $out_of_place_scratch_len_fn:expr, $immut_scratch_len:expr) => {
        impl<T: FftNum> Fft<T> for $struct_name<T> {
            fn process_immutable_with_scratch(
                &self,
                input: &[Complex<T>],
                output: &mut [Complex<T>],
                scratch: &mut [Complex<T>],
            ) {
                crate::fft_helper::fft_helper_immut(
                    input,
                    output,
                    scratch,
                    self.len(),
                    self.get_immutable_scratch_len(),
                    |in_chunk, out_chunk, scratch| {
                        self.perform_fft_immut(in_chunk, out_chunk, scratch)
                    },
                );
            }

            fn process_outofplace_with_scratch(
                &self,
                input: &mut [Complex<T>],
                output: &mut [Complex<T>],
                scratch: &mut [Complex<T>],
            ) {
                crate::fft_helper::fft_helper_outofplace(
                    input,
                    output,
                    scratch,
                    self.len(),
                    self.get_outofplace_scratch_len(),
                    |in_chunk, out_chunk, scratch| {
                        self.perform_fft_out_of_place(in_chunk, out_chunk, scratch)
                    },
                );
            }
            fn process_with_scratch(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]) {
                crate::fft_helper::fft_helper_inplace(
                    buffer,
                    scratch,
                    self.len(),
                    self.get_inplace_scratch_len(),
                    |chunk, scratch| {
                        self.perform_fft_inplace(chunk, scratch);
                    },
                );
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
                $immut_scratch_len(self)
            }
        }
        impl<T: FftNum> Length for $struct_name<T> {
            #[inline(always)]
            fn len(&self) -> usize {
                $len_fn(self)
            }
        }
        impl<T: FftNum> Direction for $struct_name<T> {
            #[inline(always)]
            fn fft_direction(&self) -> FftDirection {
                self.direction
            }
        }
    };
}

#[non_exhaustive]
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq)]
pub(crate) enum RadixFactor {
    Factor2,
    Factor3,
    Factor4,
    Factor5,
    Factor6,
    Factor7,
}
impl RadixFactor {
    pub const fn radix(&self) -> usize {
        // note: if we had rustc 1.66, we could just turn these values explicit discriminators on the enum
        match self {
            RadixFactor::Factor2 => 2,
            RadixFactor::Factor3 => 3,
            RadixFactor::Factor4 => 4,
            RadixFactor::Factor5 => 5,
            RadixFactor::Factor6 => 6,
            RadixFactor::Factor7 => 7,
        }
    }
}
