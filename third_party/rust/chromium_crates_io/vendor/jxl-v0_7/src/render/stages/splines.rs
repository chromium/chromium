// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::features::spline::Splines;
use crate::render::{ErasedLocalState, RenderPipelineInPlaceStage};
use crate::util::sync::{Arc, RwLock};

pub struct SplinesStage {
    splines: Arc<RwLock<Splines>>,
}

impl SplinesStage {
    pub fn new(splines: Arc<RwLock<Splines>>) -> Self {
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
        _state: Option<&mut ErasedLocalState>,
        _previous_call_was_previous_row: bool,
    ) {
        let splines = self.splines.try_read().unwrap();
        if splines.splines.is_empty() {
            return;
        }
        assert!(splines.is_initialized());
        splines.draw_segments(row, position, xsize);
    }
}

#[cfg(test)]
mod test {
    use test_log::test;

    use crate::error::Result;
    use crate::features::spline::{Point, QuantizedSpline, Splines};
    use crate::frame::color_correlation_map::ColorCorrelationParams;
    use crate::render::stages::splines::SplinesStage;
    use crate::util::sync::{Arc, RwLock};

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
            .initialize_draw_cache(500, 500, &ColorCorrelationParams::default(), false, true)
            .unwrap();

        crate::render::test::test_stage_consistency(
            || SplinesStage::new(Arc::new(RwLock::new(splines.clone()))),
            (500, 500),
            6,
        )
    }
}
