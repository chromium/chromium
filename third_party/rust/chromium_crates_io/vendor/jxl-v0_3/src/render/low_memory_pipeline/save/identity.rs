// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unsafe_code)]

use std::mem::MaybeUninit;
use std::ops::Range;

use jxl_simd::{F32SimdVec, SimdDescriptor, simd_function};

use crate::{
    api::{Endianness, JxlDataFormat, JxlOutputBuffer},
    render::low_memory_pipeline::row_buffers::RowBuffer,
};

#[inline(always)]
fn run_interleaved_2<D: SimdDescriptor>(
    d: D,
    a: &[f32],
    b: &[f32],
    out: &mut [MaybeUninit<f32>],
) -> usize {
    let len = D::F32Vec::LEN;
    let mut n = 0;

    for ((chunk_a, chunk_b), chunk_out) in a
        .chunks_exact(len)
        .zip(b.chunks_exact(len))
        .zip(out.chunks_exact_mut(len * 2))
    {
        let va = D::F32Vec::load(d, chunk_a);
        let vb = D::F32Vec::load(d, chunk_b);
        D::F32Vec::store_interleaved_2_uninit(va, vb, chunk_out);
        n += len;
    }

    n
}

#[inline(always)]
fn run_interleaved_3<D: SimdDescriptor>(
    d: D,
    a: &[f32],
    b: &[f32],
    c: &[f32],
    out: &mut [MaybeUninit<f32>],
) -> usize {
    let len = D::F32Vec::LEN;
    let mut n = 0;

    for (((chunk_a, chunk_b), chunk_c), chunk_out) in a
        .chunks_exact(len)
        .zip(b.chunks_exact(len))
        .zip(c.chunks_exact(len))
        .zip(out.chunks_exact_mut(len * 3))
    {
        let va = D::F32Vec::load(d, chunk_a);
        let vb = D::F32Vec::load(d, chunk_b);
        let vc = D::F32Vec::load(d, chunk_c);
        D::F32Vec::store_interleaved_3_uninit(va, vb, vc, chunk_out);
        n += len;
    }

    n
}

#[inline(always)]
fn run_interleaved_4<D: SimdDescriptor>(
    d: D,
    a: &[f32],
    b: &[f32],
    c: &[f32],
    e: &[f32],
    out: &mut [MaybeUninit<f32>],
) -> usize {
    let len = D::F32Vec::LEN;
    let mut n = 0;

    for ((((chunk_a, chunk_b), chunk_c), chunk_e), chunk_out) in a
        .chunks_exact(len)
        .zip(b.chunks_exact(len))
        .zip(c.chunks_exact(len))
        .zip(e.chunks_exact(len))
        .zip(out.chunks_exact_mut(len * 4))
    {
        let va = D::F32Vec::load(d, chunk_a);
        let vb = D::F32Vec::load(d, chunk_b);
        let vc = D::F32Vec::load(d, chunk_c);
        let ve = D::F32Vec::load(d, chunk_e);
        D::F32Vec::store_interleaved_4_uninit(va, vb, vc, ve, chunk_out);
        n += len;
    }

    n
}

simd_function!(
    store_interleaved,
    d: D,
    fn store_interleaved_impl(
        inputs: &[&[f32]],
        output: &mut [MaybeUninit<f32>]
    ) -> usize {
        match inputs.len() {
            2 => run_interleaved_2(d, inputs[0], inputs[1], output),
            3 => run_interleaved_3(d, inputs[0], inputs[1], inputs[2], output),
            4 => run_interleaved_4(d, inputs[0], inputs[1], inputs[2], inputs[3], output),
            _ => 0,
        }
    }
);

pub(super) fn store(
    input_buf: &[&RowBuffer],
    input_y: usize,
    xrange: Range<usize>,
    output_buf: &mut JxlOutputBuffer,
    output_y: usize,
    data_format: JxlDataFormat,
) -> usize {
    let byte_start = xrange.start * data_format.bytes_per_sample() + RowBuffer::x0_byte_offset();
    let byte_end = xrange.end * data_format.bytes_per_sample() + RowBuffer::x0_byte_offset();
    let is_native_endian = match data_format {
        JxlDataFormat::U8 { .. } => true,
        JxlDataFormat::F16 { endianness, .. }
        | JxlDataFormat::U16 { endianness, .. }
        | JxlDataFormat::F32 { endianness, .. } => endianness == Endianness::native(),
    };
    // SAFETY: we never write uninit memory to the `output_row`.
    let output_buf = unsafe { output_buf.row_mut(output_y) };
    let output_buf = &mut output_buf[0..(byte_end - byte_start) * input_buf.len()];
    match (
        input_buf.len(),
        data_format.bytes_per_sample(),
        is_native_endian,
    ) {
        (1, _, true) => {
            // We can just do a memcpy.
            let input_buf = &input_buf[0].get_row::<u8>(input_y)[byte_start..byte_end];
            assert_eq!(input_buf.len(), output_buf.len());
            // SAFETY: we are copying `u8`s, which have an alignment of 1, from a slice of [u8] to
            // a slice of [MaybeUninit<u8>] of the same length (as we checked just above). u8 and
            // MaybeUninit<u8> have the same layout, and aliasing rules guarantee that the two
            // slices are non-overlapping.
            unsafe {
                std::ptr::copy_nonoverlapping(
                    input_buf.as_ptr(),
                    output_buf.as_mut_ptr() as *mut u8,
                    output_buf.len(),
                );
            }
            input_buf.len() / data_format.bytes_per_sample()
        }
        (channels, 4, true) if (2..=4).contains(&channels) => {
            let ptr = output_buf.as_mut_ptr();
            if ptr.align_offset(std::mem::align_of::<f32>()) == 0 {
                let len_f32 = output_buf.len() / std::mem::size_of::<f32>();
                // SAFETY: we checked alignment above, and the size is correct by definition
                // (note that it is guaranteed that MaybeUninit<T> has the same size and align
                // of T for any T).
                let output_f32 = unsafe {
                    std::slice::from_raw_parts_mut(
                        output_buf.as_mut_ptr().cast::<MaybeUninit<f32>>(),
                        len_f32,
                    )
                };

                let start_f32 = byte_start / 4;
                let end_f32 = byte_end / 4;

                let mut slices = [&[] as &[f32]; 4];
                for (i, buf) in input_buf.iter().enumerate() {
                    slices[i] = &buf.get_row::<f32>(input_y)[start_f32..end_f32];
                }

                // Note that, by the conditions on the *_uninit methods on F32Vec, this function
                // never writes uninitialized memory.
                store_interleaved(&slices[..channels], output_f32)
            } else {
                0
            }
        }
        _ => 0,
    }
}
