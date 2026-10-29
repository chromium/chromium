/*
 * Copyright (c) 2023.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

use core::convert::TryInto;

pub(super) const CR_FRAC: i16 = 26_345;
pub(super) const CB_FRAC: i16 = -14_942;
pub(super) const C_G_CR_FRAC: i16 = 18_734;
pub(super) const C_G_CB_FRAC: i16 = -22_554;
pub(super) const YUV_RND: i32 = 1 << 15;

/// Limit values to 0 and 255
#[inline]
#[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss, dead_code)]
fn clamp(a: i32) -> u8 {
    a.clamp(0, 255) as u8
}

// Keep exact 16-bit rounding while splitting out whole chroma contributions,
// leaving fractional coefficients that fit signed 16-bit multiply-adds.
fn native_rgb(y: i16, cb: i16, cr: i16) -> (u8, u8, u8) {
    let y = i32::from(y);
    let cb = i32::from(cb - 128);
    let cr = i32::from(cr - 128);
    (
        clamp(y + cr + ((i32::from(CR_FRAC) * cr + YUV_RND) >> 16)),
        clamp(
            y - cr + ((i32::from(C_G_CR_FRAC) * cr + i32::from(C_G_CB_FRAC) * cb + YUV_RND) >> 16)
        ),
        clamp(y + 2 * cb + ((i32::from(CB_FRAC) * cb + YUV_RND) >> 16))
    )
}

/// Convert YCbCr to RGB/BGR
///
/// Converts to RGB if const BGRA is false
///
/// Converts to BGR if const BGRA is true
pub fn ycbcr_to_rgba_inner_16_scalar<const BGRA: bool>(
    y: &[i16; 16], cb: &[i16; 16], cr: &[i16; 16], output: &mut [u8], pos: &mut usize
) {
    let (_, output_position) = output.split_at_mut(*pos);

    // Convert into a slice with 64 elements for Rust to see we won't go out of bounds.
    let opt: &mut [u8; 64] = output_position
        .get_mut(0..64)
        .expect("Slice to small cannot write")
        .try_into()
        .unwrap();
    for ((&y, (cb, cr)), out) in y
        .iter()
        .zip(cb.iter().zip(cr.iter()))
        .zip(opt.chunks_exact_mut(4))
    {
        let (r, g, b) = native_rgb(y, *cb, *cr);

        if BGRA {
            out[0] = b;
            out[1] = g;
            out[2] = r;
            out[3] = 255;
        } else {
            out[0] = r;
            out[1] = g;
            out[2] = b;
            out[3] = 255;
        }
    }
    *pos += 64;
}

/// Convert YCbCr to RGB/BGR
///
/// Converts to RGB if const BGRA is false
///
/// Converts to BGR if const BGRA is true
pub fn ycbcr_to_rgb_inner_16_scalar<const BGRA: bool>(
    y: &[i16; 16], cb: &[i16; 16], cr: &[i16; 16], output: &mut [u8], pos: &mut usize
) {
    let (_, output_position) = output.split_at_mut(*pos);

    // Convert into a slice with 48 elements
    let opt: &mut [u8; 48] = output_position
        .get_mut(0..48)
        .expect("Slice to small cannot write")
        .try_into()
        .unwrap();

    for ((&y, (cb, cr)), out) in y
        .iter()
        .zip(cb.iter().zip(cr.iter()))
        .zip(opt.chunks_exact_mut(3))
    {
        let (r, g, b) = native_rgb(y, *cb, *cr);

        if BGRA {
            out[0] = b;
            out[1] = g;
            out[2] = r;
        } else {
            out[0] = r;
            out[1] = g;
            out[2] = b;
        }
    }

    // Increment pos
    *pos += 48;
}

pub fn ycbcr_to_grayscale(y: &[i16], width: usize, padded_width: usize, output: &mut [u8]) {
    for (y_in, out) in y
        .chunks_exact(padded_width)
        .zip(output.chunks_exact_mut(width))
    {
        for (y, out) in y_in.iter().zip(out.iter_mut()) {
            *out = *y as u8;
        }
    }
}
