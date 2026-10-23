// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use num_traits::abs;

use crate::api::JxlParallelRunner;
use crate::error::Result;
use crate::headers::frame_header::FrameHeader;
use crate::image::Image;
use crate::render::buffer_splitter::OutputChannelSplitter;

#[allow(clippy::excessive_precision)]
const W_SIDE: f32 = 0.20345139757231578;
#[allow(clippy::excessive_precision)]
const W_CORNER: f32 = 0.0334829185968739;
const W_CENTER: f32 = 1.0 - 4.0 * (W_SIDE + W_CORNER);

#[allow(clippy::too_many_arguments)]
fn compute_pixel_channel(
    dc_factor: f32,
    gap: f32,
    x_left: usize,
    x_center: usize,
    x_right: usize,
    row_top: &[f32],
    row: &[f32],
    row_bottom: &[f32],
) -> (f32, f32, f32) {
    let tl = row_top[x_left];
    let tc = row_top[x_center];
    let tr = row_top[x_right];
    let ml = row[x_left];
    let mc = row[x_center];
    let mr = row[x_right];
    let bl = row_bottom[x_left];
    let bc = row_bottom[x_center];
    let br = row_bottom[x_right];
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

    parallel_runner.run_ordered(num_lf_groups, None, &|g| {
        let r = frame_header.lf_group_rect(g);
        let mut out_ref_0 = splitter0.borrow_typed_rect::<f32>(r);
        let mut out_ref_1 = splitter1.borrow_typed_rect::<f32>(r);
        let mut out_ref_2 = splitter2.borrow_typed_rect::<f32>(r);

        if frame_header.is444() {
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
                        gx - 1,
                        gx,
                        gx + 1,
                        lf_image[0].row(gy - 1),
                        lf_image[0].row(gy),
                        lf_image[0].row(gy + 1),
                    );
                    let (mc_y, sm_y, gap) = compute_pixel_channel(
                        lf_factors[1],
                        gap,
                        gx - 1,
                        gx,
                        gx + 1,
                        lf_image[1].row(gy - 1),
                        lf_image[1].row(gy),
                        lf_image[1].row(gy + 1),
                    );
                    let (mc_b, sm_b, gap) = compute_pixel_channel(
                        lf_factors[2],
                        gap,
                        gx - 1,
                        gx,
                        gx + 1,
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
        } else {
            let group_dim = frame_header.group_dim();
            let lf_dims = frame_header.size_lf_groups();
            let gx_group = g % lf_dims.0;
            let gy_group = g / lf_dims.0;
            let mut out_refs = [out_ref_0, out_ref_1, out_ref_2];

            for (c, out_ref) in out_refs.iter_mut().enumerate() {
                let hs = frame_header.hshift(c);
                let vs = frame_header.vshift(c);
                let xsize_c = xsize >> hs;
                let ysize_c = ysize >> vs;

                let rect_w = r.size.0 >> hs;
                let rect_h = r.size.1 >> vs;

                let lfg_dim_x = group_dim >> hs;
                let lfg_dim_y = group_dim >> vs;
                let log_x0 = gx_group * lfg_dim_x;
                let log_y0 = gy_group * lfg_dim_y;

                let to_storage_x = |x: usize| (x / lfg_dim_x) * group_dim + (x % lfg_dim_x);
                let to_storage_y = |y: usize| (y / lfg_dim_y) * group_dim + (y % lfg_dim_y);

                for ly in 0..rect_h {
                    let log_y = log_y0 + ly;
                    let phys_y = r.origin.1 + ly;
                    let out_row = out_ref.typed_row_mut::<f32>(ly);

                    if ysize_c <= 2 || log_y == 0 || log_y == ysize_c - 1 {
                        let in_row = lf_image[c].row(phys_y);
                        out_row[..rect_w].copy_from_slice(&in_row[r.origin.0..r.origin.0 + rect_w]);
                        continue;
                    }

                    let row_top = lf_image[c].row(to_storage_y(log_y - 1));
                    let row = lf_image[c].row(phys_y);
                    let row_bottom = lf_image[c].row(to_storage_y(log_y + 1));

                    for (lx, out_val) in out_row[..rect_w].iter_mut().enumerate() {
                        let log_x = log_x0 + lx;
                        let phys_x = r.origin.0 + lx;

                        if xsize_c <= 2 || log_x == 0 || log_x == xsize_c - 1 {
                            *out_val = row[phys_x];
                            continue;
                        }

                        let x_left = to_storage_x(log_x - 1);
                        let x_right = to_storage_x(log_x + 1);

                        let (mc, sm, gap) = compute_pixel_channel(
                            lf_factors[c],
                            0.5,
                            x_left,
                            phys_x,
                            x_right,
                            row_top,
                            row,
                            row_bottom,
                        );
                        let factor = (3.0 - 4.0 * gap).max(0.0);
                        *out_val = (sm - mc) * factor + mc;
                    }
                }
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
