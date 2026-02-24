///! Optional module containing `portable_simd` versions of the most
///! important unfiltering algorithms. Enable using the `unstable` feature.
use core::convert::TryInto;
use core::simd::prelude::*;
use core::simd::Select;
// Import the fastest arch-specific scalar implementations from the outer crate.
#[cfg(not(target_arch = "x86_64"))]
use super::paeth::filter_paeth as filter_paeth_chosen;
#[cfg(target_arch = "x86_64")]
use super::paeth::filter_paeth_stbi as filter_paeth_chosen;

/// Paeth predictor specialized for AArch64 systems. Ported from the libpng
/// implementation at
/// https://github.com/pnggroup/libpng/blob/master/arm/filter_neon_intrinsics.c
#[cfg(target_arch = "aarch64")]
#[inline(always)]
fn paeth_predictor_simd<const SIZE: usize>(
    a_i16: Simd<i16, SIZE>,
    b_i16: Simd<i16, SIZE>,
    c_i16: Simd<i16, SIZE>,
) -> Simd<u8, SIZE> {
    let pa = (b_i16 - c_i16).abs();
    let pb = (a_i16 - c_i16).abs();
    let pc = (a_i16 + b_i16 - c_i16 - c_i16).abs();

    let mut nearest = a_i16;
    let mut min_dist = pa;

    // Tie-breaking: left, then above, then upper-left.
    let pb_lt_min = pb.simd_lt(min_dist);
    nearest = pb_lt_min.select(b_i16, nearest);
    min_dist = pb_lt_min.select(pb, min_dist);

    let pc_lt_min = pc.simd_lt(min_dist);
    nearest = pc_lt_min.select(c_i16, nearest);

    nearest.cast::<u8>()
}

/// Paeth predictor based on the `filter_paeth_stbi` formulation, which
/// performs better on x86_64 systems.
#[cfg(not(target_arch = "aarch64"))]
#[inline(always)]
fn paeth_predictor_simd<const SIZE: usize>(
    a_i16: Simd<i16, SIZE>,
    b_i16: Simd<i16, SIZE>,
    c_i16: Simd<i16, SIZE>,
) -> Simd<u8, SIZE> {
    let thresh = c_i16 * Simd::splat(3) - (a_i16 + b_i16);
    let lo = a_i16.simd_min(b_i16);
    let hi = a_i16.simd_max(b_i16);
    let t0 = hi.simd_le(thresh).select(lo, c_i16);
    thresh.simd_le(lo).select(hi, t0).cast::<u8>()
}

/// Core kernel for 3bpp Paeth unfiltering. Takes a `b_vec` consisting of
/// 16 RGB above the starting offset, a `c_vec_initial` which is the
/// previous pixel from before the starting offset, and a `current_a`
/// which is the previous pixel to the left of `x_out`, which is the 16
/// pixels we're unfiltering. Returns the next `a` pixel and also updates
/// `x_out`.
#[inline(always)]
fn process_paeth_chunk_bpp3_s48(
    // Previous `a` pixel from the last pump, or zero if this is the first.
    mut current_a: Simd<u8, 3>,
    // 48 bytes from the row above.
    b_vec: &Simd<u8, 48>,
    // The last three bytes of the previous `b_vec`, or zero if this if the first.
    c_vec_initial: Simd<u8, 3>,
    // 48 bytes from the current row (filtered) also re-used as the output.
    x_out: &mut Simd<u8, 48>,
) -> Simd<u8, 3> {
    let x_in = *x_out;

    // Store the outputs of the caluation as we go along, then we can do a
    // a wide vectorized add at the end of the loop.
    let mut preds = [0u8; 48];

    // Shift b and sift in the lowest 3 elements from the previous pump
    // to form c.
    let mut c_vec = b_vec.shift_elements_right::<3>(0u8);
    c_vec.as_mut_array()[0..3].copy_from_slice(c_vec_initial.as_array());

    // For each RGB pixel in the 48-byte window, we a) extract the relevant
    // parts of a, b, c and input x, then b) apply the paeth predictor to
    // what's extracted, then c) merge them to form the 'a' pixel for the
    // next part of the calculation, and update the predictpr buffer.
    macro_rules! process_pixel {
        ($shift:expr) => {
            let a_i16 = current_a.cast::<i16>();
            let b_i16 = b_vec.extract::<$shift, 3>().cast::<i16>();
            let c_i16 = c_vec.extract::<$shift, 3>().cast::<i16>();
            let pred = paeth_predictor_simd(a_i16, b_i16, c_i16);
            current_a = x_in.extract::<$shift, 3>() + pred;
            // This is necessary to break the data dependency between the
            // output and the next pixel as much as possible.
            preds[$shift..$shift + 3].copy_from_slice(pred.as_array());
        };
    }

    process_pixel!(0);
    process_pixel!(3);
    process_pixel!(6);
    process_pixel!(9);
    process_pixel!(12);
    process_pixel!(15);
    process_pixel!(18);
    process_pixel!(21);
    process_pixel!(24);
    process_pixel!(27);
    process_pixel!(30);
    process_pixel!(33);
    process_pixel!(36);
    process_pixel!(39);
    process_pixel!(42);
    process_pixel!(45);

    // Commit the unfiltered result and return the next 'a' pixel.
    *x_out += Simd::from_array(preds);
    current_a
}

/// Applies Paeth unfiltering on the `current` pixel row using `prev` row,
/// interpreting the input data as RGB.
pub fn paeth_unfilter_3bpp(current: &mut [u8], prev: &[u8]) {
    const BPP: usize = 3;
    const STRIDE_BYTES: usize = 48; // 16 pixels * 3 bytes/pixel.

    // Use the standard convention of [c] [b]
    //                                [a] [x]
    // We load 48 bytes of each and use a sliding window approach to minimize loads/stores.
    // Whilst we cannot break the strict data dependency on [a], we can agressively unroll
    // the calculation and allow independent computation of the `pa` prediction variables.
    // Initially set these to zero.
    let mut a: Simd<u8, BPP> = Default::default(); // Left pixel (unfiltered)
    let mut c: Simd<u8, BPP> = Default::default(); // Upper-left pixel (unfiltered)

    // Decide the number of chunks and setup iterators for the SIMD body and scalar fallback.
    let mut current_iter = current.chunks_exact_mut(STRIDE_BYTES);
    let mut previous_iter = prev.chunks_exact(STRIDE_BYTES);
    let combined_iter = (&mut current_iter).zip(&mut previous_iter);

    for (chunk, prev_chunk) in combined_iter {
        let mut x: Simd<u8, STRIDE_BYTES> = Simd::<u8, STRIDE_BYTES>::from_slice(chunk);
        let b: Simd<u8, STRIDE_BYTES> = Simd::<u8, STRIDE_BYTES>::from_slice(prev_chunk);

        // Process the chunk using the SIMD helper, passing the initial `c`.
        a = process_paeth_chunk_bpp3_s48(a, &b, c, &mut x);

        // Update `c` for the next chunk: it's the upper-left of the next   chunk,
        // which corresponds to the upper-right of the current chunk's `b` vector.
        c = b.extract::<{ STRIDE_BYTES - BPP }, BPP>();

        // `TryInto` and `copy_to_slice` have similar performance here.
        x.copy_to_slice(chunk);
    }

    // Scalar remainder.
    let mut a_bpp = a.to_array();
    let mut c_bpp = c.to_array();
    for (chunk, b_bpp) in current_iter
        .into_remainder()
        .chunks_exact_mut(BPP)
        .zip(previous_iter.remainder().chunks_exact(BPP))
    {
        let new_chunk = [
            chunk[0].wrapping_add(filter_paeth_chosen(
                a_bpp[0].into(),
                b_bpp[0].into(),
                c_bpp[0].into(),
            )),
            chunk[1].wrapping_add(filter_paeth_chosen(
                a_bpp[1].into(),
                b_bpp[1].into(),
                c_bpp[1].into(),
            )),
            chunk[2].wrapping_add(filter_paeth_chosen(
                a_bpp[2].into(),
                b_bpp[2].into(),
                c_bpp[2].into(),
            )),
        ];
        *TryInto::<&mut [u8; BPP]>::try_into(chunk).unwrap() = new_chunk;
        a_bpp = new_chunk;
        c_bpp = b_bpp.try_into().unwrap();
    }
}

/// Core kernel for 4bpp Paeth unfiltering. Takes a `b_vec` consisting of
/// 16 RGB above the starting offset, a `c_vec_initial` which is the
/// previous pixel from before the starting offset, and a `current_a`
/// which is the previous pixel to the left of `x_out`, which is the 16
/// pixels we're unfiltering. Returns the next `a` pixel and also updates
/// `x_out`.
#[inline(always)]
fn process_paeth_chunk_bpp4_s64(
    mut current_a: Simd<u8, 4>,
    b_vec: &Simd<u8, 64>,
    c_vec_initial: Simd<u8, 4>,
    x_out: &mut Simd<u8, 64>,
) -> Simd<u8, 4> {
    let x_in = *x_out;
    let mut preds = [0u8; 64];

    // Mix and shift the previous pixel into b to form c
    let mut c_vec = b_vec.shift_elements_right::<4>(0u8);
    c_vec.as_mut_array()[0..4].copy_from_slice(c_vec_initial.as_array());

    // For each RGB pixel in the 48-byte window, we a) extract the relevant
    // parts of a, b, c and input x, then b) apply the paeth predictor to
    // what's extracted, then c) merge them to form the 'a' pixel for the
    // next part of the calculation, and update the predictor buffer.
    macro_rules! process_pixel {
        ($shift:expr) => {
            let a_i16 = current_a.cast::<i16>();
            let b_i16 = b_vec.extract::<$shift, 4>().cast::<i16>();
            let c_i16 = c_vec.extract::<$shift, 4>().cast::<i16>();
            let pred = paeth_predictor_simd(a_i16, b_i16, c_i16);
            current_a = x_in.extract::<$shift, 4>() + pred;
            preds[$shift..$shift + 4].copy_from_slice(pred.as_array());
        };
    }

    process_pixel!(0);
    process_pixel!(4);
    process_pixel!(8);
    process_pixel!(12);
    process_pixel!(16);
    process_pixel!(20);
    process_pixel!(24);
    process_pixel!(28);
    process_pixel!(32);
    process_pixel!(36);
    process_pixel!(40);
    process_pixel!(44);
    process_pixel!(48);
    process_pixel!(52);
    process_pixel!(56);
    process_pixel!(60);

    // Do a wide vectorized add to commit the predictions and return the next `a` pixel.
    *x_out += Simd::from_array(preds);
    current_a
}

/// Applies Paeth unfiltering on the `current` pixel row using `prev` row,
/// interpreting the input data as RGBA.
pub fn paeth_unfilter_4bpp(row: &mut [u8], prev_row: &[u8]) {
    const BPP: usize = 4;
    const STRIDE_BYTES: usize = 64; // 16 pixels * 4 bytes/pixel

    let mut a: Simd<u8, BPP> = Default::default(); // Left pixel (unfiltered)
    let mut c: Simd<u8, BPP> = Default::default(); // Upper-left pixel (unfiltered)

    // Set up iterators for SIMD body and scalar remainder.
    let chunks = row.len() / STRIDE_BYTES;
    let (simd_row, remainder_row) = row.split_at_mut(chunks * STRIDE_BYTES);
    let (simd_prev_row, remainder_prev_row) = prev_row.split_at(chunks * STRIDE_BYTES);
    let row_iter = simd_row.chunks_exact_mut(STRIDE_BYTES);
    let prev_iter = simd_prev_row.chunks_exact(STRIDE_BYTES);
    let combined_iter = row_iter.zip(prev_iter);

    for (chunk, prev_chunk) in combined_iter {
        let mut x: Simd<u8, STRIDE_BYTES> = Simd::<u8, STRIDE_BYTES>::from_slice(chunk);
        let b: Simd<u8, STRIDE_BYTES> = Simd::<u8, STRIDE_BYTES>::from_slice(prev_chunk);

        a = process_paeth_chunk_bpp4_s64(a, &b, c, &mut x);

        // Update `vlast` for the next chunk: last BPP bytes of `b`
        c = b.extract::<{ STRIDE_BYTES - BPP }, BPP>();

        x.copy_to_slice(chunk);
    }

    // Scalar remainder.
    let mut a_bpp = a.to_array();
    let mut c_bpp = c.to_array();
    for (chunk, b_bpp) in remainder_row
        .chunks_exact_mut(BPP)
        .zip(remainder_prev_row.chunks_exact(BPP))
    {
        let new_chunk = [
            chunk[0].wrapping_add(filter_paeth_chosen(
                a_bpp[0].into(),
                b_bpp[0].into(),
                c_bpp[0].into(),
            )),
            chunk[1].wrapping_add(filter_paeth_chosen(
                a_bpp[1].into(),
                b_bpp[1].into(),
                c_bpp[1].into(),
            )),
            chunk[2].wrapping_add(filter_paeth_chosen(
                a_bpp[2].into(),
                b_bpp[2].into(),
                c_bpp[2].into(),
            )),
            chunk[3].wrapping_add(filter_paeth_chosen(
                a_bpp[3].into(),
                b_bpp[3].into(),
                c_bpp[3].into(),
            )),
        ];
        *TryInto::<&mut [u8; BPP]>::try_into(chunk).unwrap() = new_chunk;
        a_bpp = new_chunk;
        c_bpp = b_bpp.try_into().unwrap();
    }
}
