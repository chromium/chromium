// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    error::{Error, Result},
    headers::modular::WeightedHeader,
    image::Image,
    util::floor_log2_nonzero,
};
use num_derive::FromPrimitive;
use num_traits::FromPrimitive;

#[repr(u8)]
#[derive(Debug, FromPrimitive, Clone, Copy, PartialEq, Eq)]
pub enum Predictor {
    Zero = 0,
    West = 1,
    North = 2,
    AverageWestAndNorth = 3,
    Select = 4,
    Gradient = 5,
    Weighted = 6,
    NorthEast = 7,
    NorthWest = 8,
    WestWest = 9,
    AverageWestAndNorthWest = 10,
    AverageNorthAndNorthWest = 11,
    AverageNorthAndNorthEast = 12,
    AverageAll = 13,
}

impl Predictor {
    pub fn requires_full_row(&self) -> bool {
        matches!(
            self,
            Predictor::Weighted
                | Predictor::NorthEast
                | Predictor::AverageNorthAndNorthEast
                | Predictor::AverageAll
        )
    }
}

impl TryFrom<u32> for Predictor {
    type Error = Error;

    fn try_from(value: u32) -> Result<Self> {
        Self::from_u32(value).ok_or(Error::InvalidPredictor(value))
    }
}

#[derive(Debug, Clone, Copy, Default)]
pub struct PredictionData {
    pub left: i32,
    pub top: i32,
    pub toptop: i32,
    pub topleft: i32,
    pub topright: i32,
    pub leftleft: i32,
    pub toprightright: i32,
}

impl PredictionData {
    #[inline]
    pub fn update_for_interior_row(
        self,
        row_top: &[i32],
        row_toptop: &[i32],
        x: usize,
        cur: i32,
        needs_top: bool,
        needs_toptop: bool,
    ) -> PredictionData {
        debug_assert!(x > 1);
        debug_assert!(x + 2 < row_top.len());
        let left = cur;
        let top = self.topright;
        let topleft = self.top;
        let topright = self.toprightright;
        let leftleft = self.left;
        let toptop = if needs_toptop { row_toptop[x] } else { 0 };
        let toprightright = if needs_top { row_top[x + 2] } else { 0 };
        Self {
            left,
            top,
            toptop,
            topleft,
            topright,
            leftleft,
            toprightright,
        }
    }

    pub fn get_rows(row: &[i32], row_top: &[i32], row_toptop: &[i32], x: usize, y: usize) -> Self {
        let left = if x > 0 {
            row[x - 1]
        } else if y > 0 {
            row_top[0]
        } else {
            0
        };
        let top = if y > 0 { row_top[x] } else { left };
        let topleft = if x > 0 && y > 0 { row_top[x - 1] } else { left };
        let topright = if x + 1 < row.len() && y > 0 {
            row_top[x + 1]
        } else {
            top
        };
        let leftleft = if x > 1 { row[x - 2] } else { left };
        let toptop = if y > 1 { row_toptop[x] } else { top };
        let toprightright = if x + 2 < row.len() && y > 0 {
            row_top[x + 2]
        } else {
            topright
        };
        Self {
            left,
            top,
            toptop,
            topleft,
            topright,
            leftleft,
            toprightright,
        }
    }

    pub fn get(rect: &Image<i32>, x: usize, y: usize) -> Self {
        Self::get_rows(
            rect.row(y),
            rect.row(y.saturating_sub(1)),
            rect.row(y.saturating_sub(2)),
            x,
            y,
        )
    }

    #[allow(clippy::too_many_arguments)]
    pub fn get_with_neighbors(
        rect: &Image<i32>,
        rect_left: Option<&Image<i32>>,
        rect_top: Option<&Image<i32>>,
        rect_top_left: Option<&Image<i32>>,
        rect_right: Option<&Image<i32>>,
        rect_top_right: Option<&Image<i32>>,
        x: usize,
        y: usize,
        xsize: usize,
        ysize: usize,
    ) -> Self {
        let left = if x > 0 {
            rect.row(y)[x - 1]
        } else if let Some(l) = rect_left {
            l.row(y)[xsize - 1]
        } else if y > 0 {
            rect.row(y - 1)[0]
        } else if let Some(t) = rect_top {
            t.row(ysize - 1)[0]
        } else {
            0
        };
        let top = if y > 0 {
            rect.row(y - 1)[x]
        } else if let Some(t) = rect_top {
            t.row(ysize - 1)[x]
        } else {
            left
        };
        let topleft = if x > 0 {
            if y > 0 {
                rect.row(y - 1)[x - 1]
            } else if let Some(t) = rect_top {
                t.row(ysize - 1)[x - 1]
            } else {
                left
            }
        } else if y > 0 {
            if let Some(l) = rect_left {
                l.row(y - 1)[xsize - 1]
            } else {
                left
            }
        } else if let Some(tl) = rect_top_left {
            tl.row(ysize - 1)[xsize - 1]
        } else {
            left
        };
        let topright = if x + 1 < rect.size().0 {
            if y > 0 {
                rect.row(y - 1)[x + 1]
            } else if let Some(t) = rect_top {
                t.row(ysize - 1)[x + 1]
            } else {
                top
            }
        } else if y > 0 {
            if let Some(r) = rect_right {
                r.row(y - 1)[0]
            } else {
                top
            }
        } else if let Some(tr) = rect_top_right {
            tr.row(ysize - 1)[0]
        } else {
            top
        };
        let leftleft = if x > 1 {
            rect.row(y)[x - 2]
        } else if let Some(l) = rect_left {
            l.row(y)[xsize + x - 2]
        } else {
            left
        };
        let toptop = if y > 1 {
            rect.row(y - 2)[x]
        } else if let Some(t) = rect_top {
            t.row(ysize + y - 2)[x]
        } else {
            top
        };
        let toprightright = if x + 2 < rect.size().0 {
            if y > 0 {
                rect.row(y - 1)[x + 2]
            } else if let Some(t) = rect_top {
                t.row(ysize - 1)[x + 2]
            } else {
                topright
            }
        } else if y > 0 {
            if let Some(r) = rect_right {
                r.row(y - 1)[x + 2 - rect.size().0]
            } else {
                topright
            }
        } else if let Some(tr) = rect_top_right {
            tr.row(ysize - 1)[x + 2 - rect.size().0]
        } else {
            topright
        };
        Self {
            left,
            top,
            toptop,
            topleft,
            topright,
            leftleft,
            toprightright,
        }
    }
}

pub fn clamped_gradient(left: i64, top: i64, topleft: i64) -> i64 {
    // Same code/logic as libjxl.
    let min = left.min(top);
    let max = left.max(top);
    let grad = left + top - topleft;
    let grad_clamp_max = if topleft < min { max } else { grad };
    if topleft > max { min } else { grad_clamp_max }
}

impl Predictor {
    pub const NUM_PREDICTORS: u32 = Predictor::AverageAll as u32 + 1;

    #[inline]
    pub fn predict_one(
        &self,
        PredictionData {
            left,
            top,
            toptop,
            topleft,
            topright,
            leftleft,
            toprightright,
        }: PredictionData,
        wp_pred: i64,
    ) -> i64 {
        match self {
            Predictor::Zero => 0,
            Predictor::West => left as i64,
            Predictor::North => top as i64,
            Predictor::Select => Self::select(left as i64, top as i64, topleft as i64),
            Predictor::Gradient => clamped_gradient(left as i64, top as i64, topleft as i64),
            Predictor::Weighted => wp_pred,
            Predictor::WestWest => leftleft as i64,
            Predictor::NorthEast => topright as i64,
            Predictor::NorthWest => topleft as i64,
            Predictor::AverageWestAndNorth => (top as i64 + left as i64) / 2,
            Predictor::AverageWestAndNorthWest => (left as i64 + topleft as i64) / 2,
            Predictor::AverageNorthAndNorthWest => (top as i64 + topleft as i64) / 2,
            Predictor::AverageNorthAndNorthEast => (top as i64 + topright as i64) / 2,
            Predictor::AverageAll => {
                (6 * top as i64 - 2 * toptop as i64
                    + 7 * left as i64
                    + leftleft as i64
                    + toprightright as i64
                    + 3 * topright as i64
                    + 8)
                    / 16
            }
        }
    }

    fn select(left: i64, top: i64, topleft: i64) -> i64 {
        let p = left + top - topleft;
        if (p - left).abs() < (p - top).abs() {
            left
        } else {
            top
        }
    }
}

const NUM_PREDICTORS: usize = 4;
const PRED_EXTRA_BITS: i64 = 3;
const PREDICTION_ROUND: i64 = ((1 << PRED_EXTRA_BITS) >> 1) - 1;
// Allows to approximate division by a number from 1 to 64.
//  for (int i = 0; i < 64; i++) divlookup[i] = (1 << 24) / (i + 1);
const DIVLOOKUP: [u32; 64] = [
    16777216, 8388608, 5592405, 4194304, 3355443, 2796202, 2396745, 2097152, 1864135, 1677721,
    1525201, 1398101, 1290555, 1198372, 1118481, 1048576, 986895, 932067, 883011, 838860, 798915,
    762600, 729444, 699050, 671088, 645277, 621378, 599186, 578524, 559240, 541200, 524288, 508400,
    493447, 479349, 466033, 453438, 441505, 430185, 419430, 409200, 399457, 390167, 381300, 372827,
    364722, 356962, 349525, 342392, 335544, 328965, 322638, 316551, 310689, 305040, 299593, 294337,
    289262, 284359, 279620, 275036, 270600, 266305, 262144,
];

#[inline(always)]
fn add_bits(x: i32) -> i64 {
    (x as i64) << PRED_EXTRA_BITS
}

#[inline(always)]
fn error_weight(x: u32, maxweight: u32) -> u32 {
    let shift = floor_log2_nonzero(x as u64 + 1) as i32 - 5;
    if shift < 0 {
        4u32 + maxweight * DIVLOOKUP[x as usize & 63]
    } else {
        4u32 + ((maxweight * DIVLOOKUP[(x as usize >> shift) & 63]) >> shift)
    }
}

#[inline(always)]
fn weighted_average(pixels: &[i64; NUM_PREDICTORS], weights: &mut [u32; NUM_PREDICTORS]) -> i64 {
    let log_weight = floor_log2_nonzero(weights.iter().fold(0u64, |sum, el| sum + *el as u64));
    let weight_sum = weights.iter_mut().fold(0, |sum, el| {
        *el >>= log_weight - 4;
        sum + *el
    });
    let sum = weights
        .iter()
        .enumerate()
        .fold(((weight_sum >> 1) - 1) as i64, |sum, (i, weight)| {
            sum + pixels[i] * *weight as i64
        });
    (sum * DIVLOOKUP[(weight_sum - 1) as usize] as i64) >> 24
}

#[derive(Debug)]
pub struct WeightedPredictorState {
    prediction: [i64; NUM_PREDICTORS],
    pred: i64,
    // Position-major layout: errors for same position are contiguous
    // Layout: [pos0: p0,p1,p2,p3] [pos1: p0,p1,p2,p3] ...
    pred_errors_buffer: Vec<u32>,
    error: Vec<i32>,
    wp_header: WeightedHeader,
}

impl WeightedPredictorState {
    pub fn new(wp_header: &WeightedHeader, xsize: usize) -> WeightedPredictorState {
        let num_errors = (xsize + 2) * 2;
        WeightedPredictorState {
            prediction: [0; NUM_PREDICTORS],
            pred: 0,
            // Position-major layout: errors for same position are contiguous
            // Layout: [pos0: p0,p1,p2,p3] [pos1: p0,p1,p2,p3] ...
            // This gives better cache locality when accessing all predictors for a position
            pred_errors_buffer: vec![0; num_errors * NUM_PREDICTORS],
            error: vec![0; num_errors],
            wp_header: wp_header.clone(),
        }
    }

    /// Get all predictor errors for a given position (contiguous in memory)
    #[inline(always)]
    fn get_errors_at_pos(&self, pos: usize) -> &[u32; NUM_PREDICTORS] {
        let start = pos * NUM_PREDICTORS;
        self.pred_errors_buffer[start..start + NUM_PREDICTORS]
            .try_into()
            .unwrap()
    }

    /// Get mutable reference to all predictor errors for a given position
    #[inline(always)]
    fn get_errors_at_pos_mut(&mut self, pos: usize) -> &mut [u32; NUM_PREDICTORS] {
        let start = pos * NUM_PREDICTORS;
        (&mut self.pred_errors_buffer[start..start + NUM_PREDICTORS])
            .try_into()
            .unwrap()
    }

    pub fn save_state(&self, wp_image: &mut Image<i32>, xsize: usize) {
        wp_image
            .row_mut(0)
            .copy_from_slice(&self.error[xsize + 2..]);
    }

    pub fn restore_state(&mut self, wp_image: &Image<i32>, xsize: usize) {
        self.error[xsize + 2..].copy_from_slice(wp_image.row(0));
    }

    #[inline(always)]
    pub fn update_errors(&mut self, correct_val: i32, pos: (usize, usize), xsize: usize) {
        let (cur_row, prev_row) = if pos.1 & 1 != 0 {
            (0, xsize + 2)
        } else {
            (xsize + 2, 0)
        };
        let val = add_bits(correct_val);
        self.error[cur_row + pos.0] = (self.pred - val) as i32;

        // Compute errors for all predictors
        let mut errs = [0u32; NUM_PREDICTORS];
        for (err, &pred) in errs.iter_mut().zip(self.prediction.iter()) {
            *err = (((pred - val).abs() + PREDICTION_ROUND) >> PRED_EXTRA_BITS) as u32;
        }

        // Write to current position (contiguous access)
        *self.get_errors_at_pos_mut(cur_row + pos.0) = errs;

        // Update previous row position (contiguous access)
        let prev_errors = self.get_errors_at_pos_mut(prev_row + pos.0 + 1);
        for i in 0..NUM_PREDICTORS {
            prev_errors[i] = prev_errors[i].wrapping_add(errs[i]);
        }
    }

    #[inline(always)]
    pub fn predict_and_property(
        &mut self,
        pos: (usize, usize),
        xsize: usize,
        data: &PredictionData,
    ) -> (i64, i32) {
        let (cur_row, prev_row) = if pos.1 & 1 != 0 {
            (0, xsize + 2)
        } else {
            (xsize + 2, 0)
        };
        let pos_n = prev_row + pos.0;
        let pos_ne = if pos.0 < xsize - 1 { pos_n + 1 } else { pos_n };
        let pos_nw = if pos.0 > 0 { pos_n - 1 } else { pos_n };
        // Get errors at the 3 neighboring positions (contiguous access per position)
        let errors_n = self.get_errors_at_pos(pos_n);
        let errors_ne = self.get_errors_at_pos(pos_ne);
        let errors_nw = self.get_errors_at_pos(pos_nw);

        let mut weights = [0u32; NUM_PREDICTORS];
        for i in 0..NUM_PREDICTORS {
            weights[i] = error_weight(
                errors_n[i]
                    .wrapping_add(errors_ne[i])
                    .wrapping_add(errors_nw[i]),
                self.wp_header.w(i).unwrap(),
            );
        }
        let n = add_bits(data.top);
        let w = add_bits(data.left);
        let ne = add_bits(data.topright);
        let nw = add_bits(data.topleft);
        let nn = add_bits(data.toptop);

        let te_w = if pos.0 == 0 {
            0
        } else {
            self.error[cur_row + pos.0 - 1] as i64
        };
        let te_n = self.error[pos_n] as i64;
        let te_nw = self.error[pos_nw] as i64;
        let sum_wn = te_n + te_w;
        let te_ne = self.error[pos_ne] as i64;

        let mut p = te_w;
        if te_n.abs() > p.abs() {
            p = te_n;
        }
        if te_nw.abs() > p.abs() {
            p = te_nw;
        }
        if te_ne.abs() > p.abs() {
            p = te_ne;
        }

        self.prediction[0] = w + ne - n;
        self.prediction[1] = n - (((sum_wn + te_ne) * self.wp_header.p1c as i64) >> 5);
        self.prediction[2] = w - (((sum_wn + te_nw) * self.wp_header.p2c as i64) >> 5);
        self.prediction[3] = n
            - ((te_nw * (self.wp_header.p3ca as i64)
                + (te_n * (self.wp_header.p3cb as i64))
                + (te_ne * (self.wp_header.p3cc as i64))
                + ((nn - n) * (self.wp_header.p3cd as i64))
                + ((nw - w) * (self.wp_header.p3ce as i64)))
                >> 5);

        self.pred = weighted_average(&self.prediction, &mut weights);

        if ((te_n ^ te_w) | (te_n ^ te_nw)) <= 0 {
            let mx = w.max(ne.max(n));
            let mn = w.min(ne.min(n));
            self.pred = mn.max(mx.min(self.pred));
        }
        ((self.pred + PREDICTION_ROUND) >> PRED_EXTRA_BITS, p as i32)
    }
}

#[cfg(test)]
mod tests {
    use crate::headers::modular::{GroupHeader, WeightedHeader};

    use super::{PredictionData, WeightedPredictorState};

    struct SimpleRandom {
        out: i64,
    }

    impl SimpleRandom {
        fn new() -> SimpleRandom {
            SimpleRandom { out: 1 }
        }
        fn next(&mut self) -> i64 {
            self.out = self.out * 48271 % 0x7fffffff;
            self.out
        }
    }

    fn step(
        rng: &mut SimpleRandom,
        state: &mut WeightedPredictorState,
        xsize: usize,
        ysize: usize,
    ) -> (i64, i32) {
        let pos = (rng.next() as usize % xsize, rng.next() as usize % ysize);
        let res = state.predict_and_property(
            pos,
            xsize,
            &PredictionData {
                top: rng.next() as i32 % 256,
                left: rng.next() as i32 % 256,
                topright: rng.next() as i32 % 256,
                topleft: rng.next() as i32 % 256,
                toptop: rng.next() as i32 % 256,
                leftleft: 0,
                toprightright: 0,
            },
        );
        state.update_errors((rng.next() % 256) as i32, pos, xsize);
        res
    }

    #[test]
    fn predict_and_update_errors() {
        let mut rng = SimpleRandom::new();
        let header = GroupHeader {
            use_global_tree: false,
            wp_header: WeightedHeader {
                all_default: true,
                p1c: rng.next() as u32 % 32,
                p2c: rng.next() as u32 % 32,
                p3ca: rng.next() as u32 % 32,
                p3cb: rng.next() as u32 % 32,
                p3cc: rng.next() as u32 % 32,
                p3cd: rng.next() as u32 % 32,
                p3ce: rng.next() as u32 % 32,
                w0: rng.next() as u32 % 16,
                w1: rng.next() as u32 % 16,
                w2: rng.next() as u32 % 16,
                w3: rng.next() as u32 % 16,
            },
            transforms: Vec::new(),
        };
        let xsize = 8;
        let ysize = 8;
        let mut state = WeightedPredictorState::new(&header.wp_header, xsize);
        // The golden number results are generated by using the libjxl predictor with the same input numbers.
        assert_eq!(step(&mut rng, &mut state, xsize, ysize), (135i64, 0i32));
        assert_eq!(step(&mut rng, &mut state, xsize, ysize), (110i64, -60i32));
        assert_eq!(step(&mut rng, &mut state, xsize, ysize), (165i64, 0i32));
        assert_eq!(step(&mut rng, &mut state, xsize, ysize), (153i64, -60i32));
    }
}
