// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unsafe_code)]

use std::mem::MaybeUninit;
use std::ops::Range;

use jxl_simd::{F32SimdVec, SimdDescriptor, U8SimdVec, U16SimdVec, simd_function};

use crate::{
    api::{Endianness, JxlDataFormat, JxlOutputBuffer},
    render::low_memory_pipeline::row_buffers::RowBuffer,
};

macro_rules! define_run_interleaved {
    ($fn_name:ident, $ty:ty, $vec_trait:ident, $store_fn:ident, $cnt:expr, $($arg:ident),+) => {
        #[inline(always)]
        fn $fn_name<D: SimdDescriptor>(
            d: D,
            $($arg: &[$ty]),+,
            out: &mut [MaybeUninit<$ty>],
        ) -> usize {
            let len = D::$vec_trait::LEN;
            let mut n = 0;
            let limit = [$($arg.len()),+][0];

            {
                let out_chunks = out[..limit * $cnt].chunks_exact_mut(len * $cnt);
                $(let mut $arg = $arg.chunks_exact(len);)+
                for out_chunk in out_chunks {
                    $(let $arg = D::$vec_trait::load(d, $arg.next().unwrap());)+
                    D::$vec_trait::$store_fn($($arg),+, out_chunk);
                    n += len;
                }
            }

            let d256 = d.maybe_downgrade_256bit();
            let len256 = <D::Descriptor256 as SimdDescriptor>::$vec_trait::LEN;
            if len256 < len {
                let out_chunks = out[n * $cnt..limit * $cnt].chunks_exact_mut(len256 * $cnt);
                $(let mut $arg = $arg[n..limit].chunks_exact(len256);)+
                for out_chunk in out_chunks {
                    $(let $arg = <D::Descriptor256 as SimdDescriptor>::$vec_trait::load(d256, $arg.next().unwrap());)+
                    <D::Descriptor256 as SimdDescriptor>::$vec_trait::$store_fn($($arg),+, out_chunk);
                    n += len256;
                }
            }

            let d128 = d.maybe_downgrade_128bit();
            let len128 = <D::Descriptor128 as SimdDescriptor>::$vec_trait::LEN;
            if len128 < len {
                let out_chunks = out[n * $cnt..limit * $cnt].chunks_exact_mut(len128 * $cnt);
                $(let mut $arg = $arg[n..limit].chunks_exact(len128);)+
                for out_chunk in out_chunks {
                    $(let $arg = <D::Descriptor128 as SimdDescriptor>::$vec_trait::load(d128, $arg.next().unwrap());)+
                    <D::Descriptor128 as SimdDescriptor>::$vec_trait::$store_fn($($arg),+, out_chunk);
                    n += len128;
                }
            }

            n
        }
    };
}

define_run_interleaved!(
    run_interleaved_2_f32,
    f32,
    F32Vec,
    store_interleaved_2_uninit,
    2,
    a,
    b
);
define_run_interleaved!(
    run_interleaved_3_f32,
    f32,
    F32Vec,
    store_interleaved_3_uninit,
    3,
    a,
    b,
    c
);
define_run_interleaved!(
    run_interleaved_4_f32,
    f32,
    F32Vec,
    store_interleaved_4_uninit,
    4,
    a,
    b,
    c,
    e
);

simd_function!(
    store_interleaved_f32,
    d: D,
    fn store_interleaved_impl_f32(
        inputs: &[&[f32]],
        output: &mut [MaybeUninit<f32>]
    ) -> usize {
        match inputs.len() {
            2 => run_interleaved_2_f32(d, inputs[0], inputs[1], output),
            3 => run_interleaved_3_f32(d, inputs[0], inputs[1], inputs[2], output),
            4 => run_interleaved_4_f32(d, inputs[0], inputs[1], inputs[2], inputs[3], output),
            _ => 0,
        }
    }
);

define_run_interleaved!(
    run_interleaved_2_u8,
    u8,
    U8Vec,
    store_interleaved_2_uninit,
    2,
    a,
    b
);
define_run_interleaved!(
    run_interleaved_3_u8,
    u8,
    U8Vec,
    store_interleaved_3_uninit,
    3,
    a,
    b,
    c
);
define_run_interleaved!(
    run_interleaved_4_u8,
    u8,
    U8Vec,
    store_interleaved_4_uninit,
    4,
    a,
    b,
    c,
    e
);

simd_function!(
    store_interleaved_u8,
    d: D,
    fn store_interleaved_impl_u8(
        inputs: &[&[u8]],
        output: &mut [MaybeUninit<u8>]
    ) -> usize {
        match inputs.len() {
            2 => run_interleaved_2_u8(d, inputs[0], inputs[1], output),
            3 => run_interleaved_3_u8(d, inputs[0], inputs[1], inputs[2], output),
            4 => run_interleaved_4_u8(d, inputs[0], inputs[1], inputs[2], inputs[3], output),
            _ => 0,
        }
    }
);

define_run_interleaved!(
    run_interleaved_2_u16,
    u16,
    U16Vec,
    store_interleaved_2_uninit,
    2,
    a,
    b
);
define_run_interleaved!(
    run_interleaved_3_u16,
    u16,
    U16Vec,
    store_interleaved_3_uninit,
    3,
    a,
    b,
    c
);
define_run_interleaved!(
    run_interleaved_4_u16,
    u16,
    U16Vec,
    store_interleaved_4_uninit,
    4,
    a,
    b,
    c,
    e
);

simd_function!(
    store_interleaved_u16,
    d: D,
    fn store_interleaved_impl_u16(
        inputs: &[&[u16]],
        output: &mut [MaybeUninit<u16>]
    ) -> usize {
        match inputs.len() {
            2 => run_interleaved_2_u16(d, inputs[0], inputs[1], output),
            3 => run_interleaved_3_u16(d, inputs[0], inputs[1], inputs[2], output),
            4 => run_interleaved_4_u16(d, inputs[0], inputs[1], inputs[2], inputs[3], output),
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
        (channels, 1, true) if (2..=4).contains(&channels) => {
            let start_u8 = byte_start;
            let end_u8 = byte_end;
            let mut slices = [&[] as &[u8]; 4];
            for (i, buf) in input_buf.iter().enumerate() {
                slices[i] = &buf.get_row::<u8>(input_y)[start_u8..end_u8];
            }
            // Note that, by the conditions on the *_uninit methods on U8Vec, this function
            // never writes uninitialized memory.
            store_interleaved_u8(&slices[..channels], output_buf)
        }
        (channels, 2, true) if (2..=4).contains(&channels) => {
            let ptr = output_buf.as_mut_ptr();
            if ptr.align_offset(std::mem::align_of::<u16>()) == 0 {
                let len_u16 = output_buf.len() / 2;
                // SAFETY: we checked alignment above, and the size is correct by definition
                // (note that it is guaranteed that MaybeUninit<T> has the same size and align
                // of T for any T).
                let output_u16 = unsafe {
                    std::slice::from_raw_parts_mut(
                        output_buf.as_mut_ptr().cast::<MaybeUninit<u16>>(),
                        len_u16,
                    )
                };
                let start_u16 = byte_start / 2;
                let end_u16 = byte_end / 2;
                let mut slices = [&[] as &[u16]; 4];
                for (i, buf) in input_buf.iter().enumerate() {
                    slices[i] = &buf.get_row::<u16>(input_y)[start_u16..end_u16];
                }
                // Note that, by the conditions on the *_uninit methods on U16Vec, this function
                // never writes uninitialized memory.
                store_interleaved_u16(&slices[..channels], output_u16)
            } else {
                0
            }
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
                store_interleaved_f32(&slices[..channels], output_f32)
            } else {
                0
            }
        }
        _ => 0,
    }
}
