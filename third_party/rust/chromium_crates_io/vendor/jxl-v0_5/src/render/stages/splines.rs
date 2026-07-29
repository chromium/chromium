// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{any::Any, sync::Arc};

use crate::{features::spline::Splines, render::RenderPipelineInPlaceStage, util::AtomicRefCell};

pub struct SplinesStage {
    splines: Arc<AtomicRefCell<Splines>>,
}

impl SplinesStage {
    pub fn new(splines: Arc<AtomicRefCell<Splines>>) -> Self {
        SplinesStage { splines }
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
        let splines = self.splines.borrow();
        if splines.splines.is_empty() {
            return;
        }
        assert!(splines.is_initialized());
        splines.draw_segments(row, position, xsize);
    }
}

#[cfg(test)]
mod test {
    use std::sync::Arc;

    use crate::features::spline::{Point, QuantizedSpline, Splines};
    use crate::frame::color_correlation_map::ColorCorrelationParams;
    use crate::util::AtomicRefCell;
    use crate::{error::Result, render::stages::splines::SplinesStage};
    use test_log::test;

    #[ignore = "spline rendering is not fully consistent due to sqrt precision differences"]
    #[test]
    fn splines_consistency() -> Result<()> {
        let mut splines = Splines::create(
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

        splines
            .initialize_draw_cache(500, 500, &ColorCorrelationParams::default(), false)
            .unwrap();

        crate::render::test::test_stage_consistency(
            || SplinesStage::new(Arc::new(AtomicRefCell::new(splines.clone()))),
            (500, 500),
            6,
        )
    }
}
