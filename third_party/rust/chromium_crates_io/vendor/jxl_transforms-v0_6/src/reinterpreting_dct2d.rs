// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::*;
use jxl_simd::{F32SimdVec, SimdDescriptor};

#[inline(always)]
fn reinterpreting_dct2d_1_2_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 2, "Data length mismatch");
    assert!(output.len() > 1);
    const { assert!(1usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(2usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        do_reinterpreting_dct_2(d, data, 1);
    }
    for y in 0..1 {
        for x in 0..2 {
            output[y * 16 + x] = data[y * 2 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_2_1_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 2, "Data length mismatch");
    assert!(output.len() > 1);
    const { assert!(2usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(1usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        do_reinterpreting_dct_2(d, data, 1);
    }
    for y in 0..1 {
        for x in 0..2 {
            output[y * 16 + x] = data[y * 2 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_1_4_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 4, "Data length mismatch");
    assert!(output.len() > 3);
    const { assert!(1usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        do_reinterpreting_dct_4(d, data, 1);
    }
    for y in 0..1 {
        for x in 0..4 {
            output[y * 32 + x] = data[y * 4 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_4_1_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 4, "Data length mismatch");
    assert!(output.len() > 3);
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(1usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        do_reinterpreting_dct_4(d, data, 1);
    }
    for y in 0..1 {
        for x in 0..4 {
            output[y * 32 + x] = data[y * 4 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_2_2_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 4, "Data length mismatch");
    assert!(output.len() > 17);
    const { assert!(2usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(2usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let chunks = 2 / D::F32Vec::LEN;
        for i in 0..chunks {
            do_reinterpreting_dct_2(d, &mut data[i..], chunks);
        }
        for i in 0..chunks {
            D::F32Vec::transpose_square(d, &mut data[i * 2 + i..], chunks);
            for j in i + 1..chunks {
                D::F32Vec::transpose_square(d, &mut data[j * 2 + i..], chunks);
                D::F32Vec::transpose_square(d, &mut data[i * 2 + j..], chunks);
                for k in 0..D::F32Vec::LEN {
                    data.swap(i * 2 + j + k * chunks, j * 2 + i + k * chunks);
                }
            }
            do_reinterpreting_dct_2(d, &mut data[i..], chunks);
        }
    }
    for y in 0..2 {
        for x in 0..2 {
            output[y * 16 + x] = data[y * 2 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_2_4_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 8, "Data length mismatch");
    assert!(output.len() > 35);
    const { assert!(2usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let column_chunks = 4 / D::F32Vec::LEN;
        let row_chunks = 2 / D::F32Vec::LEN;
        for i in 0..row_chunks {
            for j in 0..column_chunks {
                D::F32Vec::transpose_square(d, &mut data[i * 4 + j..], column_chunks);
            }
            do_reinterpreting_dct_4_rowblock(d, &mut data[i * 4..]);
        }
        for i in 0..column_chunks {
            for j in 0..row_chunks {
                D::F32Vec::transpose_square(d, &mut data[j * 4 + i..], column_chunks);
            }
            do_reinterpreting_dct_2(d, &mut data[i..], column_chunks)
        }
    }
    for y in 0..2 {
        for x in 0..4 {
            output[y * 32 + x] = data[y * 4 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_4_2_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 8, "Data length mismatch");
    assert!(output.len() > 35);
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(2usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let column_chunks = 2 / D::F32Vec::LEN;
        let row_chunks = 4 / D::F32Vec::LEN;
        for i in 0..column_chunks {
            do_reinterpreting_dct_4_trh(d, &mut data[i..]);
        }
        for l in 0..2 {
            for i in 0..column_chunks {
                let tr_block =
                    |data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray], i, j, l| {
                        D::F32Vec::transpose_square(
                            d,
                            &mut data[i * 4 + j + l * column_chunks..],
                            row_chunks,
                        )
                    };
                tr_block(data, i, i, l);
                for j in i + 1..column_chunks {
                    tr_block(data, i, j, l);
                    tr_block(data, j, i, l);
                    for k in 0..D::F32Vec::LEN {
                        data.swap(
                            i * 4 + j + k * row_chunks + l * column_chunks,
                            j * 4 + i + k * row_chunks + l * column_chunks,
                        );
                    }
                }
                do_reinterpreting_dct_2(d, &mut data[i + l * column_chunks..], row_chunks);
            }
        }
    }
    for y in 0..2 {
        for x in 0..4 {
            output[y * 32 + x] = data[y * 4 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_4_4_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 16, "Data length mismatch");
    assert!(output.len() > 99);
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let chunks = 4 / D::F32Vec::LEN;
        for i in 0..chunks {
            do_reinterpreting_dct_4(d, &mut data[i..], chunks);
        }
        for i in 0..chunks {
            D::F32Vec::transpose_square(d, &mut data[i * 4 + i..], chunks);
            for j in i + 1..chunks {
                D::F32Vec::transpose_square(d, &mut data[j * 4 + i..], chunks);
                D::F32Vec::transpose_square(d, &mut data[i * 4 + j..], chunks);
                for k in 0..D::F32Vec::LEN {
                    data.swap(i * 4 + j + k * chunks, j * 4 + i + k * chunks);
                }
            }
            do_reinterpreting_dct_4(d, &mut data[i..], chunks);
        }
    }
    for y in 0..4 {
        for x in 0..4 {
            output[y * 32 + x] = data[y * 4 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_4_8_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 32, "Data length mismatch");
    assert!(output.len() > 199);
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let column_chunks = 8 / D::F32Vec::LEN;
        let row_chunks = 4 / D::F32Vec::LEN;
        for i in 0..row_chunks {
            for j in 0..column_chunks {
                D::F32Vec::transpose_square(d, &mut data[i * 8 + j..], column_chunks);
            }
            do_reinterpreting_dct_8_rowblock(d, &mut data[i * 8..]);
        }
        for i in 0..column_chunks {
            for j in 0..row_chunks {
                D::F32Vec::transpose_square(d, &mut data[j * 8 + i..], column_chunks);
            }
            do_reinterpreting_dct_4(d, &mut data[i..], column_chunks)
        }
    }
    for y in 0..4 {
        for x in 0..8 {
            output[y * 64 + x] = data[y * 8 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_8_4_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 32, "Data length mismatch");
    assert!(output.len() > 199);
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let column_chunks = 4 / D::F32Vec::LEN;
        let row_chunks = 8 / D::F32Vec::LEN;
        for i in 0..column_chunks {
            do_reinterpreting_dct_8_trh(d, &mut data[i..]);
        }
        for l in 0..2 {
            for i in 0..column_chunks {
                let tr_block =
                    |data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray], i, j, l| {
                        D::F32Vec::transpose_square(
                            d,
                            &mut data[i * 8 + j + l * column_chunks..],
                            row_chunks,
                        )
                    };
                tr_block(data, i, i, l);
                for j in i + 1..column_chunks {
                    tr_block(data, i, j, l);
                    tr_block(data, j, i, l);
                    for k in 0..D::F32Vec::LEN {
                        data.swap(
                            i * 8 + j + k * row_chunks + l * column_chunks,
                            j * 8 + i + k * row_chunks + l * column_chunks,
                        );
                    }
                }
                do_reinterpreting_dct_4(d, &mut data[i + l * column_chunks..], row_chunks);
            }
        }
    }
    for y in 0..4 {
        for x in 0..8 {
            output[y * 64 + x] = data[y * 8 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_8_8_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 64, "Data length mismatch");
    assert!(output.len() > 455);
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let chunks = 8 / D::F32Vec::LEN;
        for i in 0..chunks {
            do_reinterpreting_dct_8(d, &mut data[i..], chunks);
        }
        for i in 0..chunks {
            D::F32Vec::transpose_square(d, &mut data[i * 8 + i..], chunks);
            for j in i + 1..chunks {
                D::F32Vec::transpose_square(d, &mut data[j * 8 + i..], chunks);
                D::F32Vec::transpose_square(d, &mut data[i * 8 + j..], chunks);
                for k in 0..D::F32Vec::LEN {
                    data.swap(i * 8 + j + k * chunks, j * 8 + i + k * chunks);
                }
            }
            do_reinterpreting_dct_8(d, &mut data[i..], chunks);
        }
    }
    for y in 0..8 {
        for x in 0..8 {
            output[y * 64 + x] = data[y * 8 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_8_16_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 128, "Data length mismatch");
    assert!(output.len() > 911);
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(16usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let column_chunks = 16 / D::F32Vec::LEN;
        let row_chunks = 8 / D::F32Vec::LEN;
        for i in 0..row_chunks {
            for j in 0..column_chunks {
                D::F32Vec::transpose_square(d, &mut data[i * 16 + j..], column_chunks);
            }
            do_reinterpreting_dct_16_rowblock(d, &mut data[i * 16..]);
        }
        for i in 0..column_chunks {
            for j in 0..row_chunks {
                D::F32Vec::transpose_square(d, &mut data[j * 16 + i..], column_chunks);
            }
            do_reinterpreting_dct_8(d, &mut data[i..], column_chunks)
        }
    }
    for y in 0..8 {
        for x in 0..16 {
            output[y * 128 + x] = data[y * 16 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_16_8_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 128, "Data length mismatch");
    assert!(output.len() > 911);
    const { assert!(16usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let column_chunks = 8 / D::F32Vec::LEN;
        let row_chunks = 16 / D::F32Vec::LEN;
        for i in 0..column_chunks {
            do_reinterpreting_dct_16_trh(d, &mut data[i..]);
        }
        for l in 0..2 {
            for i in 0..column_chunks {
                let tr_block =
                    |data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray], i, j, l| {
                        D::F32Vec::transpose_square(
                            d,
                            &mut data[i * 16 + j + l * column_chunks..],
                            row_chunks,
                        )
                    };
                tr_block(data, i, i, l);
                for j in i + 1..column_chunks {
                    tr_block(data, i, j, l);
                    tr_block(data, j, i, l);
                    for k in 0..D::F32Vec::LEN {
                        data.swap(
                            i * 16 + j + k * row_chunks + l * column_chunks,
                            j * 16 + i + k * row_chunks + l * column_chunks,
                        );
                    }
                }
                do_reinterpreting_dct_8(d, &mut data[i + l * column_chunks..], row_chunks);
            }
        }
    }
    for y in 0..8 {
        for x in 0..16 {
            output[y * 128 + x] = data[y * 16 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_16_16_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 256, "Data length mismatch");
    assert!(output.len() > 1935);
    const { assert!(16usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(16usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let chunks = 16 / D::F32Vec::LEN;
        for i in 0..chunks {
            do_reinterpreting_dct_16(d, &mut data[i..], chunks);
        }
        for i in 0..chunks {
            D::F32Vec::transpose_square(d, &mut data[i * 16 + i..], chunks);
            for j in i + 1..chunks {
                D::F32Vec::transpose_square(d, &mut data[j * 16 + i..], chunks);
                D::F32Vec::transpose_square(d, &mut data[i * 16 + j..], chunks);
                for k in 0..D::F32Vec::LEN {
                    data.swap(i * 16 + j + k * chunks, j * 16 + i + k * chunks);
                }
            }
            do_reinterpreting_dct_16(d, &mut data[i..], chunks);
        }
    }
    for y in 0..16 {
        for x in 0..16 {
            output[y * 128 + x] = data[y * 16 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_16_32_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 512, "Data length mismatch");
    assert!(output.len() > 3871);
    const { assert!(16usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(32usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let column_chunks = 32 / D::F32Vec::LEN;
        let row_chunks = 16 / D::F32Vec::LEN;
        for i in 0..row_chunks {
            for j in 0..column_chunks {
                D::F32Vec::transpose_square(d, &mut data[i * 32 + j..], column_chunks);
            }
            do_reinterpreting_dct_32_rowblock(d, &mut data[i * 32..]);
        }
        for i in 0..column_chunks {
            for j in 0..row_chunks {
                D::F32Vec::transpose_square(d, &mut data[j * 32 + i..], column_chunks);
            }
            do_reinterpreting_dct_16(d, &mut data[i..], column_chunks)
        }
    }
    for y in 0..16 {
        for x in 0..32 {
            output[y * 256 + x] = data[y * 32 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_32_16_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 512, "Data length mismatch");
    assert!(output.len() > 3871);
    const { assert!(32usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(16usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let column_chunks = 16 / D::F32Vec::LEN;
        let row_chunks = 32 / D::F32Vec::LEN;
        for i in 0..column_chunks {
            do_reinterpreting_dct_32_trh(d, &mut data[i..]);
        }
        for l in 0..2 {
            for i in 0..column_chunks {
                let tr_block =
                    |data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray], i, j, l| {
                        D::F32Vec::transpose_square(
                            d,
                            &mut data[i * 32 + j + l * column_chunks..],
                            row_chunks,
                        )
                    };
                tr_block(data, i, i, l);
                for j in i + 1..column_chunks {
                    tr_block(data, i, j, l);
                    tr_block(data, j, i, l);
                    for k in 0..D::F32Vec::LEN {
                        data.swap(
                            i * 32 + j + k * row_chunks + l * column_chunks,
                            j * 32 + i + k * row_chunks + l * column_chunks,
                        );
                    }
                }
                do_reinterpreting_dct_16(d, &mut data[i + l * column_chunks..], row_chunks);
            }
        }
    }
    for y in 0..16 {
        for x in 0..32 {
            output[y * 256 + x] = data[y * 32 + x];
        }
    }
}

#[inline(always)]
fn reinterpreting_dct2d_32_32_impl<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    assert_eq!(data.len(), 1024, "Data length mismatch");
    assert!(output.len() > 7967);
    const { assert!(32usize.is_multiple_of(D::F32Vec::LEN)) };
    const { assert!(32usize.is_multiple_of(D::F32Vec::LEN)) };
    {
        let data = D::F32Vec::make_array_slice_mut(data);
        let chunks = 32 / D::F32Vec::LEN;
        for i in 0..chunks {
            do_reinterpreting_dct_32(d, &mut data[i..], chunks);
        }
        for i in 0..chunks {
            D::F32Vec::transpose_square(d, &mut data[i * 32 + i..], chunks);
            for j in i + 1..chunks {
                D::F32Vec::transpose_square(d, &mut data[j * 32 + i..], chunks);
                D::F32Vec::transpose_square(d, &mut data[i * 32 + j..], chunks);
                for k in 0..D::F32Vec::LEN {
                    data.swap(i * 32 + j + k * chunks, j * 32 + i + k * chunks);
                }
            }
            do_reinterpreting_dct_32(d, &mut data[i..], chunks);
        }
    }
    for y in 0..32 {
        for x in 0..32 {
            output[y * 256 + x] = data[y * 32 + x];
        }
    }
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_1_2<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = jxl_simd::ScalarDescriptor::new().unwrap();
    reinterpreting_dct2d_1_2_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_2_1<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = jxl_simd::ScalarDescriptor::new().unwrap();
    reinterpreting_dct2d_2_1_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_1_4<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = jxl_simd::ScalarDescriptor::new().unwrap();
    reinterpreting_dct2d_1_4_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_4_1<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = jxl_simd::ScalarDescriptor::new().unwrap();
    reinterpreting_dct2d_4_1_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_2_2<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = jxl_simd::ScalarDescriptor::new().unwrap();
    reinterpreting_dct2d_2_2_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_2_4<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = jxl_simd::ScalarDescriptor::new().unwrap();
    reinterpreting_dct2d_2_4_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_4_2<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = jxl_simd::ScalarDescriptor::new().unwrap();
    reinterpreting_dct2d_4_2_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_4_4<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = d.maybe_downgrade_128bit();
    reinterpreting_dct2d_4_4_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_4_8<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = d.maybe_downgrade_128bit();
    reinterpreting_dct2d_4_8_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_8_4<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = d.maybe_downgrade_128bit();
    reinterpreting_dct2d_8_4_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_8_8<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = d.maybe_downgrade_256bit();
    reinterpreting_dct2d_8_8_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_8_16<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = d.maybe_downgrade_256bit();
    reinterpreting_dct2d_8_16_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_16_8<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    let d = d.maybe_downgrade_256bit();
    reinterpreting_dct2d_16_8_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_16_16<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    reinterpreting_dct2d_16_16_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_16_32<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    reinterpreting_dct2d_16_32_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_32_16<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    reinterpreting_dct2d_32_16_impl(d, data, output)
}

#[inline(always)]
#[allow(unused_variables)]
pub fn reinterpreting_dct2d_32_32<D: SimdDescriptor>(d: D, data: &mut [f32], output: &mut [f32]) {
    reinterpreting_dct2d_32_32_impl(d, data, output)
}
