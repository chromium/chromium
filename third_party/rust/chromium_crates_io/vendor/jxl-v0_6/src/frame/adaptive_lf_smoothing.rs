// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::api::JxlParallelRunner;
use crate::error::Result;
use crate::headers::frame_header::FrameHeader;
use crate::image::Image;
use crate::render::buffer_splitter::OutputChannelSplitter;
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

// TODO(veluca): consider SIMDfying this.
pub fn adaptive_lf_smoothing(
    lf_factors: [f32; 3],
    frame_header: &FrameHeader,
    lf_image: &mut [Image<f32>; 3],
    parallel_runner: &mut dyn JxlParallelRunner,
) -> Result<()> {
    let (xsize, ysize) = lf_image[0].size();
    if ysize <= 2 || xsize <= 2 {
        return Ok(());
    }
    let mut smoothed0 = Image::<f32>::new((xsize, ysize))?;
    let mut smoothed1 = Image::<f32>::new((xsize, ysize))?;
    let mut smoothed2 = Image::<f32>::new((xsize, ysize))?;

    let splitter0 = OutputChannelSplitter::from_image(&mut smoothed0);
    let splitter1 = OutputChannelSplitter::from_image(&mut smoothed1);
    let splitter2 = OutputChannelSplitter::from_image(&mut smoothed2);

    let num_lf_groups = frame_header.num_lf_groups();

    parallel_runner.run(num_lf_groups, &|g| {
        let r = frame_header.lf_group_rect(g);
        let mut out_ref_0 = splitter0.borrow_typed_rect::<f32>(r);
        let mut out_ref_1 = splitter1.borrow_typed_rect::<f32>(r);
        let mut out_ref_2 = splitter2.borrow_typed_rect::<f32>(r);

        for ly in 0..r.size.1 {
            let gy = r.origin.1 + ly;
            let row_0 = out_ref_0.typed_row_mut::<f32>(ly);
            let row_1 = out_ref_1.typed_row_mut::<f32>(ly);
            let row_2 = out_ref_2.typed_row_mut::<f32>(ly);

            for lx in 0..r.size.0 {
                let gx = r.origin.0 + lx;

                if gy == 0 || gy == ysize - 1 || gx == 0 || gx == xsize - 1 {
                    row_0[lx] = lf_image[0].row(gy)[gx];
                    row_1[lx] = lf_image[1].row(gy)[gx];
                    row_2[lx] = lf_image[2].row(gy)[gx];
                    continue;
                }

                let gap = 0.5;
                let (mc_x, sm_x, gap) = compute_pixel_channel(
                    lf_factors[0],
                    gap,
                    gx,
                    lf_image[0].row(gy - 1),
                    lf_image[0].row(gy),
                    lf_image[0].row(gy + 1),
                );
                let (mc_y, sm_y, gap) = compute_pixel_channel(
                    lf_factors[1],
                    gap,
                    gx,
                    lf_image[1].row(gy - 1),
                    lf_image[1].row(gy),
                    lf_image[1].row(gy + 1),
                );
                let (mc_b, sm_b, gap) = compute_pixel_channel(
                    lf_factors[2],
                    gap,
                    gx,
                    lf_image[2].row(gy - 1),
                    lf_image[2].row(gy),
                    lf_image[2].row(gy + 1),
                );
                let factor = (3.0 - 4.0 * gap).max(0.0);
                row_0[lx] = (sm_x - mc_x) * factor + mc_x;
                row_1[lx] = (sm_y - mc_y) * factor + mc_y;
                row_2[lx] = (sm_b - mc_b) * factor + mc_b;
            }
        }
        Ok(())
    })?;

    drop(splitter0);
    drop(splitter1);
    drop(splitter2);
    *lf_image = [smoothed0, smoothed1, smoothed2];
    Ok(())
}
