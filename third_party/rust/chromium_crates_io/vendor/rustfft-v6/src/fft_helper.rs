use crate::{
    array_utils,
    common::{fft_error_immut, fft_error_inplace, fft_error_outofplace},
};

// A utility that validates the provided FFT parameters, executes the FFT if validation succeeds, or panics with a hopefully helpful error message if validation fails
// Since this implementation is basically always the same across all algorithms, this helper keeps us from having to duplicate it
#[inline(always)]
pub fn fft_helper_inplace<T>(
    buffer: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    chunk_fn: impl FnMut(&mut [T], &mut [T]),
) {
    if chunk_size == 0 {
        return;
    }

    let result =
        array_utils::validate_and_iter(buffer, scratch, chunk_size, required_scratch, chunk_fn);

    if result.is_err() {
        // We want to trigger a panic, because the passed parameters failed validation in some way.
        // But we want to avoid doing it in this function to reduce code size, so call a function marked cold and inline(never) that will do it for us
        fft_error_inplace(chunk_size, buffer.len(), required_scratch, scratch.len());
    }
}

// A utility that validates the provided FFT parameters, executes the FFT if validation succeeds, or panics with a hopefully helpful error message if validation fails
// Since this implementation is basically always the same across all algorithms, this helper keeps us from having to duplicate it
#[allow(dead_code)]
#[inline(always)]
pub fn fft_helper_inplace_unroll2x<T>(
    buffer: &mut [T],
    chunk_size: usize,
    chunk2x_fn: impl FnMut(&mut [T]),
    chunk_fn: impl FnMut(&mut [T]),
) {
    if chunk_size == 0 {
        return;
    }

    let result = array_utils::validate_and_iter_unroll2x(buffer, chunk_size, chunk2x_fn, chunk_fn);

    if result.is_err() {
        // We want to trigger a panic, because the passed parameters failed validation in some way.
        // But we want to avoid doing it in this function to reduce code size, so call a function marked cold and inline(never) that will do it for us
        fft_error_inplace(chunk_size, buffer.len(), 0, 0);
    }
}

// A utility that validates the provided FFT parameters, executes the FFT if validation succeeds, or panics with a hopefully helpful error message if validation fails
// Since this implementation is basically always the same across all algorithms, this helper keeps us from having to duplicate it
#[inline(always)]
pub fn fft_helper_immut<T>(
    input: &[T],
    output: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    chunk_fn: impl FnMut(&[T], &mut [T], &mut [T]),
) {
    if chunk_size == 0 {
        return;
    }

    let result = array_utils::validate_and_zip(
        input,
        output,
        scratch,
        chunk_size,
        required_scratch,
        chunk_fn,
    );

    if result.is_err() {
        // We want to trigger a panic, because the passed parameters failed validation in some way.
        // But we want to avoid doing it in this function to reduce code size, so call a function marked cold and inline(never) that will do it for us
        fft_error_immut(
            chunk_size,
            input.len(),
            output.len(),
            required_scratch,
            scratch.len(),
        );
    }
}

// A utility that validates the provided FFT parameters, executes the FFT if validation succeeds, or panics with a hopefully helpful error message if validation fails
// Since this implementation is basically always the same across all algorithms, this helper keeps us from having to duplicate it
#[allow(dead_code)]
#[inline(always)]
pub fn fft_helper_immut_unroll2x<T>(
    input: &[T],
    output: &mut [T],
    chunk_size: usize,
    chunk2x_fn: impl FnMut(&[T], &mut [T]),
    chunk_fn: impl FnMut(&[T], &mut [T]),
) {
    if chunk_size == 0 {
        return;
    }

    let result =
        array_utils::validate_and_zip_unroll2x(input, output, chunk_size, chunk2x_fn, chunk_fn);

    if result.is_err() {
        // We want to trigger a panic, because the passed parameters failed validation in some way.
        // But we want to avoid doing it in this function to reduce code size, so call a function marked cold and inline(never) that will do it for us
        fft_error_immut(chunk_size, input.len(), output.len(), 0, 0);
    }
}

// A utility that validates the provided FFT parameters, executes the FFT if validation succeeds, or panics with a hopefully helpful error message if validation fails
// Since this implementation is basically always the same across all algorithms, this helper keeps us from having to duplicate it
#[inline(always)]
pub fn fft_helper_outofplace<T>(
    input: &mut [T],
    output: &mut [T],
    scratch: &mut [T],
    chunk_size: usize,
    required_scratch: usize,
    chunk_fn: impl FnMut(&mut [T], &mut [T], &mut [T]),
) {
    if chunk_size == 0 {
        return;
    }

    let result = array_utils::validate_and_zip_mut(
        input,
        output,
        scratch,
        chunk_size,
        required_scratch,
        chunk_fn,
    );

    if result.is_err() {
        // We want to trigger a panic, because the passed parameters failed validation in some way.
        // But we want to avoid doing it in this function to reduce code size, so call a function marked cold and inline(never) that will do it for us
        fft_error_outofplace(
            chunk_size,
            input.len(),
            output.len(),
            required_scratch,
            scratch.len(),
        );
    }
}

// A utility that validates the provided FFT parameters, executes the FFT if validation succeeds, or panics with a hopefully helpful error message if validation fails
// Since this implementation is basically always the same across all algorithms, this helper keeps us from having to duplicate it
#[allow(dead_code)]
#[inline(always)]
pub fn fft_helper_outofplace_unroll2x<T>(
    input: &mut [T],
    output: &mut [T],
    chunk_size: usize,
    chunk2x_fn: impl FnMut(&mut [T], &mut [T]),
    chunk_fn: impl FnMut(&mut [T], &mut [T]),
) {
    if chunk_size == 0 {
        return;
    }

    let result =
        array_utils::validate_and_zip_mut_unroll2x(input, output, chunk_size, chunk2x_fn, chunk_fn);

    if result.is_err() {
        // We want to trigger a panic, because the passed parameters failed validation in some way.
        // But we want to avoid doing it in this function to reduce code size, so call a function marked cold and inline(never) that will do it for us
        fft_error_outofplace(chunk_size, input.len(), output.len(), 0, 0);
    }
}
