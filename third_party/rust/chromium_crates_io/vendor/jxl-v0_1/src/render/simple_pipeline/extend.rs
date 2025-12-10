// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    image::{Image, ImageDataType},
    render::stages::ExtendToImageDimensionsStage,
    util::{round_up_size_to_cache_line, tracing_wrappers::*},
};

impl ExtendToImageDimensionsStage {
    #[instrument(skip_all)]
    pub(super) fn extend_simple(
        &self,
        chunk_size: usize,
        input_buffers: &[Image<f64>],
        output_buffers: &mut [Image<f64>],
    ) {
        debug!("running extend stage '{self}' in simple pipeline");
        let numc = input_buffers.len();
        if numc == 0 {
            return;
        }
        assert_eq!(output_buffers.len(), numc);
        let input_size = input_buffers[0].size();
        let output_size = output_buffers[0].size();
        for c in 1..numc {
            assert_eq!(input_size, input_buffers[c].size());
            assert_eq!(output_size, output_buffers[c].size());
        }
        assert_eq!(output_size, self.image_size);
        let origin = self.frame_origin;
        assert!(origin.0 <= output_size.0 as isize);
        assert!(origin.1 <= output_size.1 as isize);
        assert!(origin.0 + input_size.0 as isize >= 0);
        assert!(origin.1 + input_size.1 as isize >= 0);
        debug!("input_size = {input_size:?} output_size = {output_size:?} origin = {origin:?}");
        // Compute the input rectangle
        let x0 = origin.0.max(0) as usize;
        let y0 = origin.1.max(0) as usize;
        let x1 = (origin.0 + input_size.0 as isize).min(output_size.0 as isize) as usize;
        let y1 = (origin.1 + input_size.1 as isize).min(output_size.1 as isize) as usize;
        debug!("x0 = {x0} x1 = {x1} y0 = {y0} y1 = {y1}");
        let in_x0 = (x0 as isize - origin.0) as usize;
        let in_x1 = (x1 as isize - origin.0) as usize;
        let in_y0 = (y0 as isize - origin.1) as usize;
        let in_y1 = (y1 as isize - origin.1) as usize;
        debug!("in_x0 = {in_x0} in_x1 = {in_x1} in_y0 = {in_y0} in_y1 = {in_y1}");
        // First, copy the data in the middle.
        for c in 0..numc {
            for in_y in in_y0..in_y1 {
                debug!("copy row: {in_y}");
                let in_row = input_buffers[c].row(in_y);
                let y = (in_y as isize + origin.1) as usize;
                output_buffers[c].row_mut(y)[x0..x1].copy_from_slice(&in_row[in_x0..in_x1]);
            }
        }
        // Fill in rows above and below the original data.
        let mut buffer = vec![f32::default(); round_up_size_to_cache_line::<f32>(chunk_size)];
        for y in (0..y0).chain(y1..output_size.1) {
            for x in (0..output_size.0).step_by(chunk_size) {
                let xsize = output_size.0.min(x + chunk_size) - x;
                debug!("position above/below: ({x},{y}) xsize: {xsize}");
                for (c, buf) in output_buffers.iter_mut().enumerate() {
                    self.process_row_chunk((x, y), xsize, c, &mut buffer);
                    for (ix, px) in buffer.iter().enumerate().take(xsize) {
                        buf.row_mut(y)[x + ix] = px.to_f64();
                    }
                }
            }
        }
        // Fill in left and right of the original data.
        for y in y0..y1 {
            for (x, xsize) in (0..x0)
                .step_by(chunk_size)
                .map(|x| (x, x0.min(x + chunk_size) - x))
                .chain(
                    (x1..output_size.0)
                        .step_by(chunk_size)
                        .map(|x| (x, output_size.0.min(x + chunk_size) - x)),
                )
            {
                debug!("position on the side: ({x},{y}) xsize: {xsize}");
                for (c, buf) in output_buffers.iter_mut().enumerate() {
                    self.process_row_chunk((x, y), xsize, c, &mut buffer);
                    for (ix, px) in buffer.iter().enumerate().take(xsize) {
                        buf.row_mut(y)[x + ix] = px.to_f64();
                    }
                }
            }
        }
    }
}
