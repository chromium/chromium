// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![feature(portable_simd)]

// Modules public for testing, don't expect stable API.
mod cxx;
pub mod decoder;
pub mod dither;
pub mod quant;
pub mod selectors;

use std::simd::prelude::*;
use std::simd::Simd;

use bytemuck::cast_slice;

use crate::dither::dither;
use crate::quant::{quantize_averages, QuantResult};
use crate::selectors::search_table_and_selectors;

// We primarily compute with 16-bit integers and a width of 8 fills a 128-bit
// wide lane (SSE, NEON). TODO(b/393494744): When we introduce multiversioning
// and support for AVX2 etc. this should be converted to a template parameter
// that varies based on the target architecture.
const SIMD_WIDTH: usize = 8;
const HALF_WIDTH: usize = SIMD_WIDTH / 2;
const QUARTER_WIDTH: usize = SIMD_WIDTH / 4;
type Reg = Simd<i16, SIMD_WIDTH>;
type Reg32 = Simd<i32, SIMD_WIDTH>;
type UReg = Simd<u16, SIMD_WIDTH>;

const ETC1_BLOCK_BYTES: usize = 8;

/// Define a helper to interleave elements from two vectors, reinterpret
/// it as a type twice as large, and return the resulting vector.
/// Each argument / return value is an array of vectors; conceptually, this
/// represents a vector that is <width> * <len> large; however, since std::simd
/// types have upper limits on their width we represent them using arrays to be
/// portable.
macro_rules! define_interleave {
    ($fn_name:ident, $src_ty:ty, $dst_ty:ty, $src_width:expr, $dst_width:expr, $src_len:literal) => {
        fn $fn_name(
            a: [Simd<$src_ty, $src_width>; $src_len],
            b: [Simd<$src_ty, $src_width>; $src_len],
        ) -> [Simd<$dst_ty, $dst_width>; $src_len * 2] {
            let mut iter = (0..$src_len).flat_map(|i| {
                let (a, b) = a[i].interleave(b[i]);
                [a, b].map(|x| bytemuck::cast(x))
            });
            let res = std::array::from_fn(|_| iter.next().unwrap());
            assert!(iter.next().is_none());
            res
        }
    };
}

/// Convert individual codewords laid out as [15..0, 31..16, 47..32, 63..48]
/// into interleaved u64 arrays, while flipping the endianness (our internal
/// representation is little endian while ETC1 requires big endian).
#[inline]
pub fn interleave_etc1(regs: [UReg; 4]) -> [Simd<u64, QUARTER_WIDTH>; 4] {
    // The interleaving assumes little endian.
    #[cfg(target_endian = "big")]
    compile_error!("Big endian is not supported");

    define_interleave!(conv_16_to_32, u16, u32, SIMD_WIDTH, HALF_WIDTH, 1);
    define_interleave!(conv_32_to_64, u32, u64, HALF_WIDTH, QUARTER_WIDTH, 2);
    // Step 1: make each u16 codeword big-endian
    let regs = regs.map(|r| r.swap_bytes());
    // Step 2: [aaaa, bbbb] to [baba, baba]
    let regs = [conv_16_to_32([regs[1]], [regs[0]]), conv_16_to_32([regs[3]], [regs[2]])];
    // Step 3: [baba, baba], [dcdc, dcdc] to [dcba, dcba], [dcba, dcba]
    let regs = conv_32_to_64(regs[1], regs[0]);
    regs
}

/// Load `SIMD_WIDTH` blocks from a region `4*SIMD_WIDTH` wide and `4` tall,
/// starting at `base_x` and `base_y`.
///
/// Out of bounds pixels are padded with mirroring. For example, `abcdxy`
/// becomes `abcdxyyx`.
///
/// Returns a 3D array of SIMD vectors. Each block is mapped to a SIMD lane
/// (from left to right), and each pixel in the block is accessed as
/// `[y][x][channel]`.
#[inline]
pub fn load_input_block(
    src: &[u32],
    width: u32,
    height: u32,
    row_width: u32,
    base_x: u32,
    base_y: u32,
) -> [[[Reg; 3]; 4]; 4] {
    let mut data = [[[Reg::default(); 3]; 4]; 4];
    // For now, input load and output store are not vectorized. The main reason is
    // that efficient loading requires shuffling and is poorly supported
    // by std::simd and the wide crate (which we plan to use for
    // supporting stable toolchain). Input load currently accounts for
    // ~20% of the runtime. If shuffle support improves this would be a
    // good candidate for optimization.
    for i in 0..4 {
        for j in 0..4 {
            let mut buf = [0u32; SIMD_WIDTH];
            for block in 0..SIMD_WIDTH as u32 {
                let x = base_x + block * 4 + j as u32;
                let y = base_y + i as u32;
                buf[block as usize] = if x < width && y < height {
                    // Fast path: load in-bound pixel
                    src[(y * row_width + x) as usize]
                } else {
                    // Slow path: mirror out-of-bound pixels
                    // If width or height is 1, mirroring can overflow, so make it saturate.
                    let xm = if x >= width { (width - 1).saturating_sub(x - width) } else { x };
                    let ym = if y >= height { (height - 1).saturating_sub(y - height) } else { y };
                    src[(ym * row_width + xm) as usize]
                };
            }
            let rgbx = Simd::from_array(buf);
            let extract_channel = |x: Simd<u32, SIMD_WIDTH>, shift: u32| {
                (x >> shift).cast::<i16>() & Simd::splat(0xFF)
            };
            data[i][j][0] = extract_channel(rgbx, 0);
            data[i][j][1] = extract_channel(rgbx, 8);
            data[i][j][2] = extract_channel(rgbx, 16);
        }
    }
    data
}

/// Compress RGB pixels to ETC1.
///
/// - `src` should be in RGBA format (the least significant byte is red).
/// - `dst` will be filled with compressed ETC1 blocks.
/// - `src_width` and `src_height` specifies the logical size of the image in
///   pixels. These does not need to be multiple of 4. The boundary pixels will
///   be padded with unspecified values.
/// - `src_row_width` and `dst_row_width` specifies the in-memory length of each
///   row, in pixels and blocks, respectively.
///
/// Note that `src` takes an aligned 32-bit buffer while `dst` takes a byte
/// buffer, even though each ETC1 codeword is 64-bit. This is due to two
/// reasons:
/// - 32-bit alignment is practical to get even on 32-bit platforms, whereas
///   64-bit values are not aligned to 8 bytes on 32-bit ARM.
/// - We require extensive shuffling when loading inputs, but store to the
///   output straight in the order of blocks. Dealing with unaligned buffers in
///   the latter case is significantly easier.
pub fn compress_etc1(
    src: &[u32],
    dst: &mut [u8],
    src_width: u32,
    src_height: u32,
    src_row_width: u32,
    dst_row_width: u32,
) {
    // Note: We deliberately do not declare the block size (4x4) of ETC1 as a
    //       constant. While magic constants in general are discouraged, the
    //       block size appears way too frequent that naming it would make the
    //       code verbose and less readable.
    let dst_height = src_height.div_ceil(4);
    let dst_width = src_width.div_ceil(4);
    // Aligned staging buffer. Data is copied into the potentially unaligned
    // destination buffer at the end of the each row.
    let mut staging_row = vec![[Simd::splat(0); 4]; (dst_width as usize).div_ceil(SIMD_WIDTH)];
    let copy_len = dst_width as usize * ETC1_BLOCK_BYTES;
    // Note on vectorization scheme:
    //
    // We process one 4x4 block per SIMD lane, instead of the more common practice
    // of processing pixels within the same block in parallel using multiple
    // lanes. The one-block-per-lane scheme, more akin to SPMD programming,
    // allows most of our code to be shuffle-free, and works much better with
    // portable SIMD than schemes that heavily shuffles.
    for dst_y in 0..dst_height {
        for dst_x0 in (0..dst_width).step_by(SIMD_WIDTH) {
            let data =
                load_input_block(src, src_width, src_height, src_row_width, dst_x0 * 4, dst_y * 4);
            let data = dither(&data);
            let QuantResult { lo: hdr0, hi: hdr1, scaled0: ep0, scaled1: ep1 } =
                quantize_averages(&data);
            let best_fit = search_table_and_selectors(hdr0, hdr1, &data, [ep0, ep1]);
            let codewords = interleave_etc1(best_fit);
            staging_row[dst_x0 as usize / SIMD_WIDTH] = codewords;
        }
        let dst_row = &mut dst[(dst_y * dst_row_width) as usize * ETC1_BLOCK_BYTES..];
        let staging_row_bytes = cast_slice(&*staging_row);
        dst_row[..copy_len].copy_from_slice(&staging_row_bytes[..copy_len]);
    }
}

/// Decompress ETC1 to RGBA
///
/// - `src` should be in ETC1
/// - `dst` will be filled with RGBA
/// - `width` and `height` should be the dimensions of `dst`. If width or height
///   are not multiples of 4, note that the edges become partial blocks and
///   pixels out of bounds will be discarded. The number is truncated.
/// - `src_row_width` should be the width of ETC1 image `dst_row_width` should
///   be the width of RGBA image
///
///
/// This is a stub.
/// TODO: b/393495436 - Implement ETC1 decoding logic.
pub fn decompress_etc1(
    _src: &[u8],
    dst: &mut [u32],
    dst_width: u32,
    dst_height: u32,
    _src_row_width: u32,
    dst_row_width: u32,
) {
    for y in 0..dst_height {
        for x in 0..dst_width {
            let r = (x % 256) as u32;
            let b = (y % 256) as u32;
            let pixel_value: u32 = 0xFF000000 // Alpha: 0xFF
                    | ((r & 0xFF) << 16) // Red
                    |  (b & 0xFF); // Blue
            dst[(y * dst_row_width + x) as usize] = pixel_value;
        }
    }
}
