// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unsafe_code)]

use std::ops::Range;

use crate::{
    api::{Endianness, JxlDataFormat, JxlOutputBuffer},
    render::low_memory_pipeline::row_buffers::RowBuffer,
};

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
        (3, 4, true) => {
            #[cfg(target_arch = "x86_64")]
            {
                let [a, b, c] = input_buf else { unreachable!() };
                super::x86_64::interleave3_32b(
                    &[
                        &a.get_row(input_y)[byte_start..byte_end],
                        &b.get_row(input_y)[byte_start..byte_end],
                        &c.get_row(input_y)[byte_start..byte_end],
                    ],
                    output_buf,
                )
            }
            #[cfg(not(target_arch = "x86_64"))]
            {
                0
            }
        }
        _ => 0,
    }
}
