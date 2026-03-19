// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{any::Any, sync::Arc};

use crate::{
    features::spline::Splines, frame::color_correlation_map::ColorCorrelationParams,
    render::RenderPipelineInPlaceStage, util::AtomicRefCell,
};

pub struct SplinesStage {
    splines: Arc<AtomicRefCell<Splines>>,
    image_size: (usize, usize),
    color_correlation_params: Arc<AtomicRefCell<ColorCorrelationParams>>,
    high_precision: bool,
}

impl SplinesStage {
    pub fn new(
        splines: Arc<AtomicRefCell<Splines>>,
        image_size: (usize, usize),
        color_correlation_params: Arc<AtomicRefCell<ColorCorrelationParams>>,
        high_precision: bool,
    ) -> Self {
        SplinesStage {
            splines,
            image_size,
            color_correlation_params,
            high_precision,
        }
    }
}

impl std::fmt::Display for SplinesStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "splines")
    }
}

impl RenderPipelineInPlaceStage for SplinesStage {
    type Type = f32;

    fn uses_channel(&self, c: usize) -> bool {
        c < 3
    }

    fn process_row_chunk(
        &self,
        position: (usize, usize),
        xsize: usize,
        row: &mut [&mut [f32]],
        _state: Option<&mut dyn Any>,
    ) {
        // TODO(veluca): this is wrong!! Race condition in MT.
        let mut splines = self.splines.borrow_mut();
        if splines.splines.is_empty() {
            return;
        }
        if !splines.is_initialized() {
            let color_correlation_params = self.color_correlation_params.borrow();
            splines
                .initialize_draw_cache(
                    self.image_size.0 as u64,
                    self.image_size.1 as u64,
                    &color_correlation_params,
                    self.high_precision,
                )
                .unwrap();
        }
        splines.draw_segments(row, position, xsize);
    }
}

#[cfg(test)]
mod test {
    use std::sync::Arc;

    use crate::features::spline::{Point, QuantizedSpline, Splines};
    use crate::frame::color_correlation_map::ColorCorrelationParams;
    use crate::render::test::make_and_run_simple_pipeline;
    use crate::util::AtomicRefCell;
    use crate::util::test::{self, assert_all_almost_abs_eq, read_pfm};
    use crate::{error::Result, image::Image, render::stages::splines::SplinesStage};
    use test_log::test;

    #[test]
    fn splines_process_row_chunk() -> Result<(), test::Error> {
        let want_image = read_pfm(include_bytes!("../../../resources/test/splines.pfm"))?;
        let target_images = [
            Image::<f32>::new((320, 320))?,
            Image::<f32>::new((320, 320))?,
            Image::<f32>::new((320, 320))?,
        ];
        let size = target_images[0].size();
        let splines = Splines::create(
            0,
            vec![QuantizedSpline {
                control_points: vec![
                    (109, 105),
                    (-130, -261),
                    (-66, 193),
                    (227, -52),
                    (-170, 290),
                ],
                color_dct: [
                    [
                        168, 119, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0,
                    ],
                    [
                        9, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0,
                    ],
                    [
                        -10, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0,
                    ],
                ],
                sigma_dct: [
                    4, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0,
                ],
            }],
            vec![Point { x: 9.0, y: 54.0 }],
        );
        let output: Vec<Image<f32>> = make_and_run_simple_pipeline(
            SplinesStage::new(
                Arc::new(AtomicRefCell::new(splines.clone())),
                size,
                Arc::new(AtomicRefCell::new(ColorCorrelationParams::default())),
                true,
            ),
            &target_images,
            size,
            0,
            256,
        )?;
        for c in 0..3 {
            for row in 0..size.1 {
                assert_all_almost_abs_eq(output[c].row(row), want_image[c].row(row), 1e-3);
            }
        }
        Ok(())
    }

    #[ignore = "spline rendering is not fully consistent due to sqrt precision differences"]
    #[test]
    fn splines_consistency() -> Result<()> {
        let splines = Splines::create(
            0,
            vec![QuantizedSpline {
                control_points: vec![
                    (109, 105),
                    (-130, -261),
                    (-66, 193),
                    (227, -52),
                    (-170, 290),
                ],
                color_dct: [
                    [
                        168, 119, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0,
                    ],
                    [
                        9, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0,
                    ],
                    [
                        -10, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0,
                    ],
                ],
                sigma_dct: [
                    4, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0,
                ],
            }],
            vec![Point { x: 9.0, y: 54.0 }],
        );

        crate::render::test::test_stage_consistency(
            || {
                SplinesStage::new(
                    Arc::new(AtomicRefCell::new(splines.clone())),
                    (500, 500),
                    Arc::new(AtomicRefCell::new(ColorCorrelationParams::default())),
                    false,
                )
            },
            (500, 500),
            6,
        )
    }
}
