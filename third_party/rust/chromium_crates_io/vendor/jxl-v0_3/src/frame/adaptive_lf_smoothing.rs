// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::error::Result;
use crate::image::Image;
use num_traits::abs;

#[allow(clippy::excessive_precision)]
const W_SIDE: f32 = 0.20345139757231578;
#[allow(clippy::excessive_precision)]
const W_CORNER: f32 = 0.0334829185968739;
const W_CENTER: f32 = 1.0 - 4.0 * (W_SIDE + W_CORNER);

fn compute_pixel_channel(
    dc_factor: f32,
    gap: f32,
    x: usize,
    row_top: &[f32],
    row: &[f32],
    row_bottom: &[f32],
) -> (f32, f32, f32) {
    let tl = row_top[x - 1];
    let tc = row_top[x];
    let tr = row_top[x + 1];
    let ml = row[x - 1];
    let mc = row[x];
    let mr = row[x + 1];
    let bl = row_bottom[x - 1];
    let bc = row_bottom[x];
    let br = row_bottom[x + 1];
    let corner = tl + tr + bl + br;
    let side = ml + mr + tc + bc;
    let sm = corner * W_CORNER + side * W_SIDE + mc * W_CENTER;
    (mc, sm, gap.max(abs((mc - sm) / dc_factor)))
}

pub fn adaptive_lf_smoothing(lf_factors: [f32; 3], lf_image: &mut [Image<f32>; 3]) -> Result<()> {
    let xsize = lf_image[0].size().0;
    let ysize = lf_image[0].size().1;
    if ysize <= 2 || xsize <= 2 {
        return Ok(());
    }
    let mut smoothed: [Image<f32>; 3] = [
        Image::<f32>::new((xsize, ysize))?,
        Image::<f32>::new((xsize, ysize))?,
        Image::<f32>::new((xsize, ysize))?,
    ];
    for c in 0..3 {
        for y in [0, ysize - 1] {
            smoothed[c].row_mut(y).copy_from_slice(lf_image[c].row(y));
        }
    }
    for y in 1..ysize - 1 {
        for x in [0, xsize - 1] {
            for c in 0..3 {
                smoothed[c].row_mut(y)[x] = lf_image[c].row(y)[x];
            }
        }
        for x in 1..xsize - 1 {
            let gap = 0.5;
            let (mc_x, sm_x, gap) = compute_pixel_channel(
                lf_factors[0],
                gap,
                x,
                lf_image[0].row(y - 1),
                lf_image[0].row(y),
                lf_image[0].row(y + 1),
            );
            let (mc_y, sm_y, gap) = compute_pixel_channel(
                lf_factors[1],
                gap,
                x,
                lf_image[1].row(y - 1),
                lf_image[1].row(y),
                lf_image[1].row(y + 1),
            );
            let (mc_b, sm_b, gap) = compute_pixel_channel(
                lf_factors[2],
                gap,
                x,
                lf_image[2].row(y - 1),
                lf_image[2].row(y),
                lf_image[2].row(y + 1),
            );
            let factor = (3.0 - 4.0 * gap).max(0.0);
            smoothed[0].row_mut(y)[x] = (sm_x - mc_x) * factor + mc_x;
            smoothed[1].row_mut(y)[x] = (sm_y - mc_y) * factor + mc_y;
            smoothed[2].row_mut(y)[x] = (sm_b - mc_b) * factor + mc_b;
        }
    }
    *lf_image = smoothed;
    Ok(())
}
