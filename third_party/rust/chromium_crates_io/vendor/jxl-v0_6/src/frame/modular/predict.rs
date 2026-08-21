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
    #[inline(always)]
    pub fn update_for_interior_row(
        self,
        row_top: &[i32],
        row_toptop: &[i32],
        x: usize,
        cur: i32,
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
        let toprightright = row_top[x + 2];
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

    #[inline]
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

#[derive(Debug)]
pub struct WeightedPredictorState {
    prediction: [i64; NUM_PREDICTORS],
    pred: i64,
    // Safety invariant:
    // - computing `(xsize + 1) * 2` does not overflow
    // - `pred_errors_buffer.len() == (xsize + 1) * 2`
    // - `error.len() == (xsize + 1) * 2`
    // Note that (at least as of June 2026) the use of unsafe code
    // that needs these invariants seems to provide meaningful speedups.
    xsize: usize,
    pred_errors_buffer: Vec<[u32; NUM_PREDICTORS]>,
    // Note: we store errors in positions [1..=xsize], and error[0] == 0.
    // (this is not a safety invariant)
    error: Vec<i32>,
    w: [u8; 4],
    p1c: u8,
    p2c: u8,
    p3c: [u8; 5],
}

impl WeightedPredictorState {
    pub fn new(wp_header: &WeightedHeader, xsize: usize) -> WeightedPredictorState {
        let num_errors = xsize.checked_add(1).unwrap().checked_mul(2).unwrap();
        WeightedPredictorState {
            prediction: [0; NUM_PREDICTORS],
            pred: 0,
            // Safety note: safety invariant is true by construction (we checked no
            // overflows above)
            xsize,
            pred_errors_buffer: vec![[0; NUM_PREDICTORS]; num_errors],
            error: vec![0; num_errors],
            // These casts are lossless because w fits in 4 bits and p fits in 5 by
            // bitstream constraints.
            w: [
                wp_header.w0 as u8,
                wp_header.w1 as u8,
                wp_header.w2 as u8,
                wp_header.w3 as u8,
            ],
            p1c: wp_header.p1c as u8,
            p2c: wp_header.p2c as u8,
            p3c: [
                wp_header.p3ca as u8,
                wp_header.p3cb as u8,
                wp_header.p3cc as u8,
                wp_header.p3cd as u8,
                wp_header.p3ce as u8,
            ],
        }
    }

    pub fn save_state(&self, wp_image: &mut Image<i32>) {
        let row_stride = self.xsize + 1;
        wp_image
            .row_mut(0)
            .copy_from_slice(&self.error[0..row_stride]);
        let src = &self.pred_errors_buffer;
        let [d0, d1, d2, d3] = wp_image.distinct_full_rows_mut([1, 2, 3, 4]);
        for ((((d0, d1), d2), d3), &s) in d0
            .iter_mut()
            .zip(d1.iter_mut())
            .zip(d2.iter_mut())
            .zip(d3.iter_mut())
            .zip(src)
        {
            *d0 = s[0] as i32;
            *d1 = s[1] as i32;
            *d2 = s[2] as i32;
            *d3 = s[3] as i32;
        }
    }

    pub fn restore_state(&mut self, wp_image: &Image<i32>) {
        let row_stride = self.xsize + 1;
        self.error[0..row_stride].copy_from_slice(wp_image.row(0));
        let s0 = wp_image.row(1);
        let s1 = wp_image.row(2);
        let s2 = wp_image.row(3);
        let s3 = wp_image.row(4);
        let dst = &mut self.pred_errors_buffer;
        for (d, (&s0, (&s1, (&s2, &s3)))) in dst
            .iter_mut()
            .zip(s0.iter().zip(s1.iter().zip(s2.iter().zip(s3.iter()))))
        {
            *d = [s0 as u32, s1 as u32, s2 as u32, s3 as u32];
        }
    }

    // Note: optimizations in this function are a bit finnicky.
    #[allow(unsafe_code)]
    #[inline(always)]
    pub fn predict_and_property(
        &mut self,
        pos: (usize, usize),
        data: &PredictionData,
    ) -> (i64, i32) {
        assert!(pos.0 < self.xsize);
        // Safety note: the index arithmetic in this function is guaranteed
        // not to overflow thanks to the safety invariant and the check on `pos.0`
        // above.
        // The debug_assert! are documentation of safety-relevant properties in
        // code form.
        let (cur_row, prev_row) = if pos.1 & 1 != 0 {
            (0, self.xsize + 1)
        } else {
            (self.xsize + 1, 0)
        };
        // Safety note: guaranteed to be < self.xsize.
        let pos_ne = if pos.0 + 1 < self.xsize {
            pos.0 + 1
        } else {
            pos.0
        };
        // Safety note: guaranteed to be < self.xsize.
        let pos_nw = pos.0.saturating_sub(1);

        debug_assert!(prev_row + pos.0 < self.pred_errors_buffer.len());
        // SAFETY: prev_row <= xsize + 1, so the index is < 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.pred_error_buffers.len() == 2*(xsize+1)`).
        let err_n = unsafe { self.pred_errors_buffer.get_unchecked(prev_row + pos.0) };
        debug_assert!(prev_row + pos_ne < self.pred_errors_buffer.len());
        // SAFETY: prev_row <= xsize + 1, so the index is < 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.pred_error_buffers.len() == 2*(xsize+1)`).
        let err_ne = unsafe { self.pred_errors_buffer.get_unchecked(prev_row + pos_ne) };
        debug_assert!(prev_row + pos_nw < self.pred_errors_buffer.len());
        // SAFETY: prev_row <= xsize + 1, so the index is < 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.pred_error_buffers.len() == 2*(xsize+1)`).
        let err_nw = unsafe { self.pred_errors_buffer.get_unchecked(prev_row + pos_nw) };

        let err0 = err_n[0].wrapping_add(err_ne[0]).wrapping_add(err_nw[0]);
        let err1 = err_n[1].wrapping_add(err_ne[1]).wrapping_add(err_nw[1]);
        let err2 = err_n[2].wrapping_add(err_ne[2]).wrapping_add(err_nw[2]);
        let err3 = err_n[3].wrapping_add(err_ne[3]).wrapping_add(err_nw[3]);

        let shift0 = (err0 as u64 + 1).ilog2().saturating_sub(5);
        let shift1 = (err1 as u64 + 1).ilog2().saturating_sub(5);
        let shift2 = (err2 as u64 + 1).ilog2().saturating_sub(5);
        let shift3 = (err3 as u64 + 1).ilog2().saturating_sub(5);

        debug_assert!(err0 >> shift0 < 64);
        // SAFETY: x >> ((x+1).ilog2().saturating_sub(5)) < 64 for any x (see `error_weight_bounds`).
        // (Note: the compiler doesn't seem to realize this)
        let div0 = unsafe { *DIVLOOKUP.get_unchecked(err0 as usize >> shift0) };
        debug_assert!(err1 >> shift1 < 64);
        // SAFETY: same as div0
        let div1 = unsafe { *DIVLOOKUP.get_unchecked(err1 as usize >> shift1) };
        debug_assert!(err2 >> shift2 < 64);
        // SAFETY: same as div0
        let div2 = unsafe { *DIVLOOKUP.get_unchecked(err2 as usize >> shift2) };
        debug_assert!(err3 >> shift3 < 64);
        // SAFETY: same as div0
        let div3 = unsafe { *DIVLOOKUP.get_unchecked(err3 as usize >> shift3) };

        let w0 = 4u32 + ((self.w[0] as u32 * div0) >> shift0);
        let w1 = 4u32 + ((self.w[1] as u32 * div1) >> shift1);
        let w2 = 4u32 + ((self.w[2] as u32 * div2) >> shift2);
        let w3 = 4u32 + ((self.w[3] as u32 * div3) >> shift3);
        // Safety note: we know that w0, w1, w2, w3 >= 4 because u8 * DIVLOOKUP[i] + 4 never
        // overflows.
        debug_assert!(w0 >= 4);
        debug_assert!(w1 >= 4);
        debug_assert!(w2 >= 4);
        debug_assert!(w3 >= 4);

        // Note: this might access what's morally equivalent to position -1,
        // but that value is guaranteed to be 0.

        debug_assert!(cur_row + pos.0 < self.error.len());
        // SAFETY: cur_row <= xsize + 1, so the index is < 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.error.len() == 2*(xsize+1)`).
        let te_w = unsafe { *self.error.get_unchecked(cur_row + pos.0) as i64 };
        debug_assert!(prev_row + 1 + pos.0 < self.error.len());
        // SAFETY: prev_row <= xsize + 1, so the index is <= 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.error.len() == 2*(xsize+1)`).
        let te_n = unsafe { *self.error.get_unchecked(prev_row + 1 + pos.0) as i64 };
        debug_assert!(prev_row + 1 + pos_nw < self.error.len());
        // SAFETY: prev_row <= xsize + 1, so the index is <= 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.error.len() == 2*(xsize+1)`).
        let te_nw = unsafe { *self.error.get_unchecked(prev_row + 1 + pos_nw) as i64 };
        let sum_wn = te_n + te_w;
        debug_assert!(prev_row + 1 + pos_ne < self.error.len());
        // SAFETY: prev_row <= xsize + 1, so the index is <= 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.error.len() == 2*(xsize+1)`).
        let te_ne = unsafe { *self.error.get_unchecked(prev_row + 1 + pos_ne) as i64 };

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

        let n = add_bits(data.top);
        let w = add_bits(data.left);
        let ne = add_bits(data.topright);
        let nw = add_bits(data.topleft);
        let nn = add_bits(data.toptop);

        let p0 = w + ne - n;
        let p1 = n - (((sum_wn + te_ne) * self.p1c as i64) >> 5);
        let p2 = w - (((sum_wn + te_nw) * self.p2c as i64) >> 5);
        let p3 = n
            - ((te_nw * (self.p3c[0] as i64)
                + (te_n * (self.p3c[1] as i64))
                + (te_ne * (self.p3c[2] as i64))
                + ((nn - n) * (self.p3c[3] as i64))
                + ((nw - w) * (self.p3c[4] as i64)))
                >> 5);

        let log_weight = floor_log2_nonzero(w0 as u64 + w1 as u64 + w2 as u64 + w3 as u64);
        debug_assert!(log_weight >= 4);
        // Safety note: due to all weights being >= 4, their sum is >= 16 so the base2 log
        // is at least 4. Thus, the subtractions below do not overflow.

        let w0s = w0 as i64 >> (log_weight - 4);
        let w1s = w1 as i64 >> (log_weight - 4);
        let w2s = w2 as i64 >> (log_weight - 4);
        let w3s = w3 as i64 >> (log_weight - 4);

        let weight_sum = w0s + w1s + w2s + w3s;
        debug_assert!(weight_sum > 0);
        // Safety note: weight_sum > 0.
        // Let wM be the largest of w0, w1, w2, w3. Then, their sum is at most 4*wM,
        // so log_weight <= wM.ilog2() + 2. It follows that log_weight - 4 < wM.ilog2(),
        // which guarantees that the shift to compute wMs does not return 0.
        debug_assert!(weight_sum <= 64);
        // Safety note: weight_sum <= 64.
        // Note that w0s + w1s + w2s + w3s <= (w0 + w1 + w2 + w3) >> (log_weight - 4) since
        // shifting rounds down. This last quantity is at most 64 (in fact, at most 31),
        // as checked in scaled_weight_bounds (note that 16 <= w0 + w1 + w2 + w3 <= 4 * u32::MAX)

        let sum = (weight_sum >> 1) - 1 + w0s * p0 + w1s * p1 + w2s * p2 + w3s * p3;

        // SAFETY: given that weight_sum > 0 and weight_sum <= 64, weight_sum - 1 is
        // in bounds of DIVLOOKUP.
        let mut pred =
            (sum * unsafe { *DIVLOOKUP.get_unchecked((weight_sum - 1) as usize) } as i64) >> 24;

        if ((te_n ^ te_w) | (te_n ^ te_nw)) <= 0 {
            let mx = w.max(ne.max(n));
            let mn = w.min(ne.min(n));
            pred = mn.max(mx.min(pred));
        }
        self.prediction = [p0, p1, p2, p3];
        self.pred = pred;
        ((pred + PREDICTION_ROUND) >> PRED_EXTRA_BITS, p as i32)
    }

    #[allow(unsafe_code)]
    #[inline(always)]
    pub fn update_errors(&mut self, correct_val: i32, pos: (usize, usize)) {
        assert!(pos.0 < self.xsize);
        let (cur_row, prev_row) = if pos.1 & 1 != 0 {
            (0, self.xsize + 1)
        } else {
            (self.xsize + 1, 0)
        };
        let val = add_bits(correct_val);
        debug_assert!(cur_row + pos.0 + 1 < self.error.len());
        // SAFETY: cur_row <= xsize + 1, so the index is <= 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.error.len() == 2*(xsize+1)`).
        unsafe { *self.error.get_unchecked_mut(cur_row + pos.0 + 1) = (self.pred - val) as i32 };

        // Compute errors for all predictors
        let err0 =
            (((self.prediction[0] - val).abs() + PREDICTION_ROUND) >> PRED_EXTRA_BITS) as u32;
        let err1 =
            (((self.prediction[1] - val).abs() + PREDICTION_ROUND) >> PRED_EXTRA_BITS) as u32;
        let err2 =
            (((self.prediction[2] - val).abs() + PREDICTION_ROUND) >> PRED_EXTRA_BITS) as u32;
        let err3 =
            (((self.prediction[3] - val).abs() + PREDICTION_ROUND) >> PRED_EXTRA_BITS) as u32;

        debug_assert!(cur_row + pos.0 < self.pred_errors_buffer.len());
        // SAFETY: cur_row <= xsize + 1, so the index is < 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.pred_errors_buffer.len() == 2*(xsize+1)`).
        unsafe {
            *self.pred_errors_buffer.get_unchecked_mut(cur_row + pos.0) = [err0, err1, err2, err3];
        }

        debug_assert!(prev_row + pos.0 + 1 < self.pred_errors_buffer.len());
        // SAFETY: prev_row <= xsize + 1, so the index is <= 2*xsize + 1, which is in-bounds due to
        // the safety invariant (`self.pred_errors_buffer.len() == 2*(xsize+1)`).
        let prev_errors = unsafe {
            self.pred_errors_buffer
                .get_unchecked_mut(prev_row + pos.0 + 1)
        };

        prev_errors[0] = prev_errors[0].wrapping_add(err0);
        prev_errors[1] = prev_errors[1].wrapping_add(err1);
        prev_errors[2] = prev_errors[2].wrapping_add(err2);
        prev_errors[3] = prev_errors[3].wrapping_add(err3);
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        headers::modular::{GroupHeader, WeightedHeader},
        util::floor_log2_nonzero,
    };

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
        state.update_errors((rng.next() % 256) as i32, pos);
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

    #[test]
    fn error_weight_bounds() {
        for i in 0..u32::MAX {
            let shift = (i as u64 + 1).ilog2().saturating_sub(5);
            assert!(((i as usize) >> shift) < 64, "i = {i}, shift = {shift}");
        }
    }

    #[test]
    fn scaled_weight_bounds() {
        for i in 16..u32::MAX as u64 * 4 {
            let shift = floor_log2_nonzero(i) - 4;
            assert!((i >> shift) < 32, "i = {i}, shift = {shift}");
        }
    }
}
