// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    f32::consts::{FRAC_1_SQRT_2, PI, SQRT_2},
    iter::{self, zip},
    ops,
};

use crate::{
    bit_reader::BitReader,
    entropy_coding::decode::{Histograms, SymbolReader, unpack_signed},
    error::{Error, Result},
    frame::color_correlation_map::ColorCorrelationParams,
    util::{CeilLog2, NewWithCapacity, fast_cos, fast_erff, tracing_wrappers::*},
};
const MAX_NUM_CONTROL_POINTS: u32 = 1 << 20;
const MAX_NUM_CONTROL_POINTS_PER_PIXEL_RATIO: u32 = 2;
const DELTA_LIMIT: i64 = 1 << 30;
const SPLINE_POS_LIMIT: isize = 1 << 23;

const QUANTIZATION_ADJUSTMENT_CONTEXT: usize = 0;
const STARTING_POSITION_CONTEXT: usize = 1;
const NUM_SPLINES_CONTEXT: usize = 2;
const NUM_CONTROL_POINTS_CONTEXT: usize = 3;
const CONTROL_POINTS_CONTEXT: usize = 4;
const DCT_CONTEXT: usize = 5;
const NUM_SPLINE_CONTEXTS: usize = 6;
const DESIRED_RENDERING_DISTANCE: f32 = 1.0;

#[derive(Debug, Clone, Copy, Default)]
pub struct Point {
    pub x: f32,
    pub y: f32,
}

impl Point {
    fn new(x: f32, y: f32) -> Self {
        Point { x, y }
    }
    fn abs(&self) -> f32 {
        self.x.hypot(self.y)
    }
}

impl PartialEq for Point {
    fn eq(&self, other: &Self) -> bool {
        (self.x - other.x).abs() < 1e-3 && (self.y - other.y).abs() < 1e-3
    }
}

impl ops::Add<Point> for Point {
    type Output = Point;
    fn add(self, rhs: Point) -> Point {
        Point {
            x: self.x + rhs.x,
            y: self.y + rhs.y,
        }
    }
}

impl ops::Sub<Point> for Point {
    type Output = Point;
    fn sub(self, rhs: Point) -> Point {
        Point {
            x: self.x - rhs.x,
            y: self.y - rhs.y,
        }
    }
}

impl ops::Mul<f32> for Point {
    type Output = Point;
    fn mul(self, rhs: f32) -> Point {
        Point {
            x: self.x * rhs,
            y: self.y * rhs,
        }
    }
}

impl ops::Div<f32> for Point {
    type Output = Point;
    fn div(self, rhs: f32) -> Point {
        let inv = 1.0 / rhs;
        Point {
            x: self.x * inv,
            y: self.y * inv,
        }
    }
}

#[derive(Default, Debug)]
pub struct Spline {
    control_points: Vec<Point>,
    // X, Y, B.
    color_dct: [Dct32; 3],
    // Splines are drawn by normalized Gaussian splatting. This controls the
    // Gaussian's parameter along the spline.
    sigma_dct: Dct32,
    // The estimated area in pixels covered by the spline.
    estimated_area_reached: u64,
}

impl Spline {
    pub fn validate_adjacent_point_coincidence(&self) -> Result<()> {
        if let Some(((index, p0), p1)) = zip(
            self.control_points
                .iter()
                .take(self.control_points.len() - 1)
                .enumerate(),
            self.control_points.iter().skip(1),
        )
        .find(|((_, p0), p1)| **p0 == **p1)
        {
            return Err(Error::SplineAdjacentCoincidingControlPoints(
                index,
                *p0,
                index + 1,
                *p1,
            ));
        }
        Ok(())
    }
}

#[derive(Debug, Default, Clone)]
pub struct QuantizedSpline {
    // Double delta-encoded.
    pub control_points: Vec<(i64, i64)>,
    pub color_dct: [[i32; 32]; 3],
    pub sigma_dct: [i32; 32],
}

fn inv_adjusted_quant(adjustment: i32) -> f32 {
    if adjustment >= 0 {
        1.0 / (1.0 + 0.125 * adjustment as f32)
    } else {
        1.0 - 0.125 * adjustment as f32
    }
}

fn validate_spline_point_pos<T: num_traits::ToPrimitive>(x: T, y: T) -> Result<()> {
    let xi = x.to_i32().unwrap();
    let yi = y.to_i32().unwrap();
    let ok_range = -(1i32 << 23)..(1i32 << 23);
    if !ok_range.contains(&xi) {
        return Err(Error::SplinesPointOutOfRange(
            Point {
                x: xi as f32,
                y: yi as f32,
            },
            xi,
            ok_range,
        ));
    }
    if !ok_range.contains(&yi) {
        return Err(Error::SplinesPointOutOfRange(
            Point {
                x: xi as f32,
                y: yi as f32,
            },
            yi,
            ok_range,
        ));
    }
    Ok(())
}

const CHANNEL_WEIGHT: [f32; 4] = [0.0042, 0.075, 0.07, 0.3333];

fn area_limit(image_size: u64) -> u64 {
    // Use saturating arithmetic to prevent overflow
    1024u64
        .saturating_mul(image_size)
        .saturating_add(1u64 << 32)
        .min(1u64 << 42)
}

impl QuantizedSpline {
    #[instrument(level = "debug", skip(br), ret, err)]
    pub fn read(
        br: &mut BitReader,
        splines_histograms: &Histograms,
        splines_reader: &mut SymbolReader,
        max_control_points: u32,
        total_num_control_points: &mut u32,
    ) -> Result<QuantizedSpline> {
        let num_control_points =
            splines_reader.read_unsigned(splines_histograms, br, NUM_CONTROL_POINTS_CONTEXT);
        *total_num_control_points += num_control_points;
        if *total_num_control_points > max_control_points {
            return Err(Error::SplinesTooManyControlPoints(
                *total_num_control_points,
                max_control_points,
            ));
        }
        let mut control_points = Vec::new_with_capacity(num_control_points as usize)?;
        for _ in 0..num_control_points {
            let x =
                splines_reader.read_signed(splines_histograms, br, CONTROL_POINTS_CONTEXT) as i64;
            let y =
                splines_reader.read_signed(splines_histograms, br, CONTROL_POINTS_CONTEXT) as i64;
            control_points.push((x, y));
            // Add check that double deltas are not outrageous (not in spec).
            let max_delta_delta = x.abs().max(y.abs());
            if max_delta_delta >= DELTA_LIMIT {
                return Err(Error::SplinesDeltaLimit(max_delta_delta, DELTA_LIMIT));
            }
        }
        // Decode DCTs and populate the QuantizedSpline struct
        let mut color_dct = [[0; 32]; 3];
        let mut sigma_dct = [0; 32];

        let mut decode_dct = |dct: &mut [i32; 32]| -> Result<()> {
            for value in dct.iter_mut() {
                *value = splines_reader.read_signed(splines_histograms, br, DCT_CONTEXT);
            }
            Ok(())
        };

        for channel in &mut color_dct {
            decode_dct(channel)?;
        }
        decode_dct(&mut sigma_dct)?;

        Ok(QuantizedSpline {
            control_points,
            color_dct,
            sigma_dct,
        })
    }

    pub fn dequantize(
        &self,
        starting_point: &Point,
        quantization_adjustment: i32,
        y_to_x: f32,
        y_to_b: f32,
        image_size: u64,
    ) -> Result<Spline> {
        let area_limit = area_limit(image_size);

        let mut result = Spline {
            control_points: Vec::new_with_capacity(self.control_points.len() + 1)?,
            ..Default::default()
        };

        let px = starting_point.x.round();
        let py = starting_point.y.round();
        validate_spline_point_pos(px, py)?;

        let mut current_x = px as i32;
        let mut current_y = py as i32;
        result
            .control_points
            .push(Point::new(current_x as f32, current_y as f32));

        let mut current_delta_x = 0i32;
        let mut current_delta_y = 0i32;
        let mut manhattan_distance = 0u64;

        for &(dx, dy) in &self.control_points {
            current_delta_x += dx as i32;
            current_delta_y += dy as i32;
            validate_spline_point_pos(current_delta_x, current_delta_y)?;

            manhattan_distance +=
                current_delta_x.unsigned_abs() as u64 + current_delta_y.unsigned_abs() as u64;

            if manhattan_distance > area_limit {
                return Err(Error::SplinesDistanceTooLarge(
                    manhattan_distance,
                    area_limit,
                ));
            }

            current_x += current_delta_x;
            current_y += current_delta_y;
            validate_spline_point_pos(current_x, current_y)?;

            result
                .control_points
                .push(Point::new(current_x as f32, current_y as f32));
        }

        let inv_quant = inv_adjusted_quant(quantization_adjustment);

        for (c, weight) in CHANNEL_WEIGHT.iter().enumerate().take(3) {
            for i in 0..32 {
                let inv_dct_factor = if i == 0 { FRAC_1_SQRT_2 } else { 1.0 };
                result.color_dct[c].0[i] =
                    self.color_dct[c][i] as f32 * inv_dct_factor * weight * inv_quant;
            }
        }

        for i in 0..32 {
            result.color_dct[0].0[i] += y_to_x * result.color_dct[1].0[i];
            result.color_dct[2].0[i] += y_to_b * result.color_dct[1].0[i];
        }

        let mut width_estimate = 0;
        let mut color = [0u64; 3];

        for (c, color_val) in color.iter_mut().enumerate() {
            for i in 0..32 {
                *color_val += (inv_quant * self.color_dct[c][i].abs() as f32).ceil() as u64;
            }
        }

        color[0] += y_to_x.abs().ceil() as u64 * color[1];
        color[2] += y_to_b.abs().ceil() as u64 * color[1];

        let max_color = color[0].max(color[1]).max(color[2]);
        let logcolor = 1u64.max((1u64 + max_color).ceil_log2());

        let weight_limit =
            (((area_limit as f32 / logcolor as f32) / manhattan_distance.max(1) as f32).sqrt())
                .ceil();

        for i in 0..32 {
            let inv_dct_factor = if i == 0 { FRAC_1_SQRT_2 } else { 1.0 };
            result.sigma_dct.0[i] =
                self.sigma_dct[i] as f32 * inv_dct_factor * CHANNEL_WEIGHT[3] * inv_quant;

            let weight_f = (inv_quant * self.sigma_dct[i].abs() as f32).ceil();
            let weight = weight_limit.min(weight_f.max(1.0)) as u64;
            width_estimate += weight * weight * logcolor;
        }

        result.estimated_area_reached = width_estimate * manhattan_distance;

        Ok(result)
    }
}

#[derive(Debug, Clone, Copy, Default)]
struct SplineSegment {
    center_x: f32,
    center_y: f32,
    maximum_distance: f32,
    inv_sigma: f32,
    sigma_over_4_times_intensity: f32,
    color: [f32; 3],
}

#[derive(Debug, Default, Clone)]
pub struct Splines {
    pub quantization_adjustment: i32,
    pub splines: Vec<QuantizedSpline>,
    pub starting_points: Vec<Point>,
    segments: Vec<SplineSegment>,
    segment_indices: Vec<usize>,
    segment_y_start: Vec<u64>,
}

fn draw_centripetal_catmull_rom_spline(points: &[Point]) -> Result<Vec<Point>> {
    if points.is_empty() {
        return Ok(vec![]);
    }
    if points.len() == 1 {
        return Ok(vec![points[0]]);
    }
    const NUM_POINTS: usize = 16;
    // Create a view of points with one prepended and one appended point.
    let extended_points = iter::once(points[0] + (points[0] - points[1]))
        .chain(points.iter().cloned())
        .chain(iter::once(
            points[points.len() - 1] + (points[points.len() - 1] - points[points.len() - 2]),
        ));
    // Pair each point with the sqrt of the distance to the next point.
    let points_and_deltas = extended_points
        .chain(iter::once(Point::default()))
        .scan(Point::default(), |previous, p| {
            let result = Some((*previous, (p - *previous).abs().sqrt()));
            *previous = p;
            result
        })
        .skip(1);
    // Window the points with a [Point; 4] window.
    let windowed_points = points_and_deltas
        .scan([(Point::default(), 0.0); 4], |window, p| {
            (window[0], window[1], window[2], window[3]) =
                (window[1], window[2], window[3], (p.0, p.1));
            Some([window[0], window[1], window[2], window[3]])
        })
        .skip(3);
    // Create the points necessary per window, and flatten the result.
    let result = windowed_points
        .flat_map(|p| {
            let mut window_result = [Point::default(); NUM_POINTS];
            window_result[0] = p[1].0;
            let mut t = [0.0; 4];
            for k in 0..3 {
                // TODO(from libjxl): Restrict d[k] with reasonable limit and spec it.
                t[k + 1] = t[k] + p[k].1;
            }
            for (i, window_point) in window_result.iter_mut().enumerate().skip(1) {
                let tt = p[0].1 + ((i as f32) / (NUM_POINTS as f32)) * p[1].1;
                let mut a = [Point::default(); 3];
                for k in 0..3 {
                    // TODO(from libjxl): Reciprocal multiplication would be faster.
                    a[k] = p[k].0 + (p[k + 1].0 - p[k].0) * ((tt - t[k]) / p[k].1);
                }
                let mut b = [Point::default(); 2];
                for k in 0..2 {
                    b[k] = a[k] + (a[k + 1] - a[k]) * ((tt - t[k]) / (p[k].1 + p[k + 1].1));
                }
                *window_point = b[0] + (b[1] - b[0]) * ((tt - t[1]) / p[1].1);
            }
            window_result
        })
        .chain(iter::once(points[points.len() - 1]))
        .collect();
    Ok(result)
}

fn for_each_equally_spaced_point<F: FnMut(Point, f32)>(
    points: &[Point],
    desired_distance: f32,
    mut f: F,
) {
    if points.is_empty() {
        return;
    }
    let mut accumulated_distance = 0.0;
    f(points[0], desired_distance);
    if points.len() == 1 {
        return;
    }
    for index in 0..(points.len() - 1) {
        let mut current = points[index];
        let next = points[index + 1];
        let segment = next - current;
        let segment_length = segment.abs();
        let unit_step = segment / segment_length;
        if accumulated_distance + segment_length >= desired_distance {
            current = current + unit_step * (desired_distance - accumulated_distance);
            f(current, desired_distance);
            accumulated_distance -= desired_distance;
        }
        accumulated_distance += segment_length;
        while accumulated_distance >= desired_distance {
            current = current + unit_step * desired_distance;
            f(current, desired_distance);
            accumulated_distance -= desired_distance;
        }
    }
    f(points[points.len() - 1], accumulated_distance);
}

/// Precomputed multipliers for DCT: PI / 32.0 * i for i in 0..32
const DCT_MULTIPLIERS: [f32; 32] = [
    PI / 32.0 * 0.0,
    PI / 32.0 * 1.0,
    PI / 32.0 * 2.0,
    PI / 32.0 * 3.0,
    PI / 32.0 * 4.0,
    PI / 32.0 * 5.0,
    PI / 32.0 * 6.0,
    PI / 32.0 * 7.0,
    PI / 32.0 * 8.0,
    PI / 32.0 * 9.0,
    PI / 32.0 * 10.0,
    PI / 32.0 * 11.0,
    PI / 32.0 * 12.0,
    PI / 32.0 * 13.0,
    PI / 32.0 * 14.0,
    PI / 32.0 * 15.0,
    PI / 32.0 * 16.0,
    PI / 32.0 * 17.0,
    PI / 32.0 * 18.0,
    PI / 32.0 * 19.0,
    PI / 32.0 * 20.0,
    PI / 32.0 * 21.0,
    PI / 32.0 * 22.0,
    PI / 32.0 * 23.0,
    PI / 32.0 * 24.0,
    PI / 32.0 * 25.0,
    PI / 32.0 * 26.0,
    PI / 32.0 * 27.0,
    PI / 32.0 * 28.0,
    PI / 32.0 * 29.0,
    PI / 32.0 * 30.0,
    PI / 32.0 * 31.0,
];

/// Precomputed cosine values for DCT at a given t value.
/// Computed once and reused for all 4 DCT evaluations (3 color channels + sigma).
struct PrecomputedCosines([f32; 32]);

impl PrecomputedCosines {
    /// Precompute cosines for a given t value.
    /// Call this once per point, then use with continuous_idct_fast for each DCT.
    #[inline]
    fn new(t: f32) -> Self {
        let tandhalf = t + 0.5;
        PrecomputedCosines(core::array::from_fn(|i| {
            fast_cos(DCT_MULTIPLIERS[i] * tandhalf)
        }))
    }
}

#[derive(Default, Clone, Copy, Debug)]
struct Dct32([f32; 32]);

impl Dct32 {
    /// Fast continuous IDCT using precomputed cosines.
    /// This avoids recomputing 32 cosines for each of the 4 DCT calls per point.
    #[inline]
    fn continuous_idct_fast(&self, precomputed: &PrecomputedCosines) -> f32 {
        // Compute dot product of coeffs and precomputed cosines
        // Using iterator for auto-vectorization
        zip(self.0, precomputed.0)
            .map(|(coeff, cos)| coeff * cos)
            .sum::<f32>()
            * SQRT_2
    }
}

impl Splines {
    #[cfg(test)]
    pub fn create(
        quantization_adjustment: i32,
        splines: Vec<QuantizedSpline>,
        starting_points: Vec<Point>,
    ) -> Splines {
        Splines {
            quantization_adjustment,
            splines,
            starting_points,
            segments: vec![],
            segment_indices: vec![],
            segment_y_start: vec![],
        }
    }
    pub fn draw_segments(&self, row: &mut [&mut [f32]], row_pos: (usize, usize), xsize: usize) {
        let first_segment_index_pos = self.segment_y_start[row_pos.1];
        let last_segment_index_pos = self.segment_y_start[row_pos.1 + 1];
        for segment_index_pos in first_segment_index_pos..last_segment_index_pos {
            self.draw_segment(
                row,
                row_pos,
                xsize,
                &self.segments[self.segment_indices[segment_index_pos as usize]],
            );
        }
    }
    fn draw_segment(
        &self,
        row: &mut [&mut [f32]],
        row_pos: (usize, usize),
        xsize: usize,
        segment: &SplineSegment,
    ) {
        let (x0, y) = row_pos;
        let x1 = x0 + xsize;
        let clamped_x0 = x0.max((segment.center_x - segment.maximum_distance).round() as usize);
        // one-past-the-end
        let clamped_x1 = x1.min((segment.center_x + segment.maximum_distance).round() as usize + 1);
        for x in clamped_x0..clamped_x1 {
            self.draw_segment_at(row, (x, y), x0, segment);
        }
    }
    fn draw_segment_at(
        &self,
        row: &mut [&mut [f32]],
        pixel_pos: (usize, usize),
        row_x0: usize,
        segment: &SplineSegment,
    ) {
        let (x, y) = pixel_pos;
        let inv_sigma = segment.inv_sigma;
        let half = 0.5f32;
        let one_over_2s2 = 0.353_553_38_f32;
        let sigma_over_4_times_intensity = segment.sigma_over_4_times_intensity;
        let dx = x as f32 - segment.center_x;
        let dy = y as f32 - segment.center_y;
        let sqd = dx * dx + dy * dy;
        let distance = sqd.sqrt();
        let one_dimensional_factor = fast_erff((distance * half + one_over_2s2) * inv_sigma)
            - fast_erff((distance * half - one_over_2s2) * inv_sigma);
        let local_intensity =
            sigma_over_4_times_intensity * one_dimensional_factor * one_dimensional_factor;
        for (channel_index, row) in row.iter_mut().enumerate() {
            let cm = segment.color[channel_index];
            let inp = row[x - row_x0];
            row[x - row_x0] = cm * local_intensity + inp;
        }
    }

    fn add_segment(
        &mut self,
        center: &Point,
        intensity: f32,
        color: [f32; 3],
        sigma: f32,
        high_precision: bool,
        segments_by_y: &mut Vec<(u64, usize)>,
    ) {
        if sigma.is_infinite()
            || sigma == 0.0
            || (1.0 / sigma).is_infinite()
            || intensity.is_infinite()
        {
            return;
        }
        let distance_exp: f32 = if high_precision { 5.0 } else { 3.0 };
        let max_color = [0.01, color[0], color[1], color[2]]
            .iter()
            .map(|chan| (chan * intensity).abs())
            .max_by(|a, b| a.total_cmp(b))
            .unwrap();
        let max_distance =
            (-2.0 * sigma * sigma * (0.1f32.ln() * distance_exp - max_color.ln())).sqrt();
        let segment = SplineSegment {
            center_x: center.x,
            center_y: center.y,
            color,
            inv_sigma: 1.0 / sigma,
            sigma_over_4_times_intensity: 0.25 * sigma * intensity,
            maximum_distance: max_distance,
        };
        let y0 = (center.y - max_distance).round() as i64;
        let y1 = (center.y + max_distance).round() as i64 + 1;
        for y in 0.max(y0)..y1 {
            segments_by_y.push((y as u64, self.segments.len()));
        }
        self.segments.push(segment);
    }

    fn add_segments_from_points(
        &mut self,
        spline: &Spline,
        points_to_draw: &[(Point, f32)],
        length: f32,
        desired_distance: f32,
        high_precision: bool,
        segments_by_y: &mut Vec<(u64, usize)>,
    ) {
        let inv_length = 1.0 / length;
        for (point_index, (point, multiplier)) in points_to_draw.iter().enumerate() {
            let progress = (point_index as f32 * desired_distance * inv_length).min(1.0);
            let t = (32.0 - 1.0) * progress;

            // Precompute cosines once for this point (saves 3x cosine computations)
            let precomputed = PrecomputedCosines::new(t);

            // Use precomputed cosines for all 4 DCT evaluations
            let mut color = [0.0; 3];
            for (index, coeffs) in spline.color_dct.iter().enumerate() {
                color[index] = coeffs.continuous_idct_fast(&precomputed);
            }
            let sigma = spline.sigma_dct.continuous_idct_fast(&precomputed);

            self.add_segment(
                point,
                *multiplier,
                color,
                sigma,
                high_precision,
                segments_by_y,
            );
        }
    }

    pub fn initialize_draw_cache(
        &mut self,
        image_xsize: u64,
        image_ysize: u64,
        color_correlation_params: &ColorCorrelationParams,
        high_precision: bool,
    ) -> Result<()> {
        let mut total_estimated_area_reached = 0u64;
        let mut splines = Vec::new();
        // Use saturating_mul to prevent overflow with malicious image dimensions
        let image_area = image_xsize.saturating_mul(image_ysize);
        let area_limit = area_limit(image_area);
        for (index, qspline) in self.splines.iter().enumerate() {
            let spline = qspline.dequantize(
                &self.starting_points[index],
                self.quantization_adjustment,
                color_correlation_params.y_to_x_lf(),
                color_correlation_params.y_to_b_lf(),
                image_area,
            )?;
            total_estimated_area_reached += spline.estimated_area_reached;
            if total_estimated_area_reached > area_limit {
                return Err(Error::SplinesAreaTooLarge(
                    total_estimated_area_reached,
                    area_limit,
                ));
            }
            spline.validate_adjacent_point_coincidence()?;
            splines.push(spline);
        }

        if total_estimated_area_reached
            > (8 * image_xsize * image_ysize + (1u64 << 25)).min(1u64 << 30)
        {
            warn!(
                "Large total_estimated_area_reached, expect slower decoding:{}",
                total_estimated_area_reached
            );
        }

        let mut segments_by_y = Vec::new();

        self.segments.clear();
        for spline in splines {
            let mut points_to_draw = Vec::<(Point, f32)>::new();
            let intermediate_points = draw_centripetal_catmull_rom_spline(&spline.control_points)?;
            for_each_equally_spaced_point(
                &intermediate_points,
                DESIRED_RENDERING_DISTANCE,
                |p, d| points_to_draw.push((p, d)),
            );
            let length = (points_to_draw.len() as isize - 2) as f32 * DESIRED_RENDERING_DISTANCE
                + points_to_draw[points_to_draw.len() - 1].1;
            if length <= 0.0 {
                continue;
            }
            self.add_segments_from_points(
                &spline,
                &points_to_draw,
                length,
                DESIRED_RENDERING_DISTANCE,
                high_precision,
                &mut segments_by_y,
            );
        }

        // TODO(from libjxl): Consider linear sorting here.
        segments_by_y.sort_by_key(|segment| segment.0);

        self.segment_indices.clear();
        self.segment_indices.try_reserve(segments_by_y.len())?;
        self.segment_indices.resize(segments_by_y.len(), 0);

        self.segment_y_start.clear();
        self.segment_y_start.try_reserve(image_ysize as usize + 1)?;
        self.segment_y_start.resize(image_ysize as usize + 1, 0);

        for (i, segment) in segments_by_y.iter().enumerate() {
            self.segment_indices[i] = segment.1;
            let y = segment.0;
            if y < image_ysize {
                self.segment_y_start[y as usize + 1] += 1;
            }
        }
        for y in 0..image_ysize {
            self.segment_y_start[y as usize + 1] += self.segment_y_start[y as usize];
        }
        Ok(())
    }

    #[instrument(level = "debug", skip(br), ret, err)]
    pub fn read(br: &mut BitReader, num_pixels: u32) -> Result<Splines> {
        trace!(pos = br.total_bits_read());
        let splines_histograms = Histograms::decode(NUM_SPLINE_CONTEXTS, br, true)?;
        let mut splines_reader = SymbolReader::new(&splines_histograms, br, None)?;
        let num_splines = splines_reader
            .read_unsigned(&splines_histograms, br, NUM_SPLINES_CONTEXT)
            .saturating_add(1);
        let max_control_points =
            MAX_NUM_CONTROL_POINTS.min(num_pixels / MAX_NUM_CONTROL_POINTS_PER_PIXEL_RATIO);
        if num_splines > max_control_points {
            return Err(Error::SplinesTooMany(num_splines, max_control_points));
        }

        let mut starting_points = Vec::new();
        let mut last_x = 0;
        let mut last_y = 0;
        for i in 0..num_splines {
            let unsigned_x =
                splines_reader.read_unsigned(&splines_histograms, br, STARTING_POSITION_CONTEXT);
            let unsigned_y =
                splines_reader.read_unsigned(&splines_histograms, br, STARTING_POSITION_CONTEXT);

            let (x, y) = if i != 0 {
                (
                    unpack_signed(unsigned_x) as isize + last_x,
                    unpack_signed(unsigned_y) as isize + last_y,
                )
            } else {
                (unsigned_x as isize, unsigned_y as isize)
            };
            // It is not in spec, but reasonable limit to avoid overflows.
            let max_coordinate = x.abs().max(y.abs());
            if max_coordinate >= SPLINE_POS_LIMIT {
                return Err(Error::SplinesCoordinatesLimit(
                    max_coordinate,
                    SPLINE_POS_LIMIT,
                ));
            }

            starting_points.push(Point {
                x: x as f32,
                y: y as f32,
            });

            last_x = x;
            last_y = y;
        }

        let quantization_adjustment =
            splines_reader.read_signed(&splines_histograms, br, QUANTIZATION_ADJUSTMENT_CONTEXT);

        let mut splines = Vec::new();
        let mut num_control_points = 0u32;
        for _ in 0..num_splines {
            splines.push(QuantizedSpline::read(
                br,
                &splines_histograms,
                &mut splines_reader,
                max_control_points,
                &mut num_control_points,
            )?);
        }
        splines_reader.check_final_state(&splines_histograms, br)?;
        Ok(Splines {
            quantization_adjustment,
            splines,
            starting_points,
            ..Splines::default()
        })
    }
}

#[cfg(test)]
#[allow(clippy::excessive_precision)]
mod test_splines {
    use std::{f32::consts::SQRT_2, iter::zip};
    use test_log::test;

    use crate::{
        error::{Error, Result},
        features::spline::SplineSegment,
        frame::color_correlation_map::ColorCorrelationParams,
        util::test::{assert_all_almost_abs_eq, assert_almost_abs_eq, assert_almost_eq},
    };

    use super::{
        DCT_MULTIPLIERS, DESIRED_RENDERING_DISTANCE, Dct32, Point, PrecomputedCosines,
        QuantizedSpline, Spline, Splines, draw_centripetal_catmull_rom_spline,
        for_each_equally_spaced_point,
    };
    use crate::util::fast_cos;

    impl Dct32 {
        /// Original continuous_idct for testing - validates correctness against golden values.
        fn continuous_idct(&self, t: f32) -> f32 {
            let tandhalf = t + 0.5;
            zip(DCT_MULTIPLIERS, self.0)
                .map(|(multiplier, coeff)| SQRT_2 * coeff * fast_cos(multiplier * tandhalf))
                .sum()
        }
    }

    #[test]
    fn dequantize() -> Result<(), Error> {
        // Golden data generated by libjxl.
        let quantized_and_dequantized = [
            (
                QuantizedSpline {
                    control_points: vec![
                        (109, 105),
                        (-247, -261),
                        (168, 427),
                        (-46, -360),
                        (-61, 181),
                    ],
                    color_dct: [
                        [
                            12223, 9452, 5524, 16071, 1048, 17024, 14833, 7690, 21952, 2405, 2571,
                            2190, 1452, 2500, 18833, 1667, 5857, 21619, 1310, 20000, 10429, 11667,
                            7976, 18786, 12976, 18548, 14786, 12238, 8667, 3405, 19929, 8429,
                        ],
                        [
                            177, 712, 127, 999, 969, 356, 105, 12, 1132, 309, 353, 415, 1213, 156,
                            988, 524, 316, 1100, 64, 36, 816, 1285, 183, 889, 839, 1099, 79, 1316,
                            287, 105, 689, 841,
                        ],
                        [
                            780, -201, -38, -695, -563, -293, -88, 1400, -357, 520, 979, 431, -118,
                            590, -971, -127, 157, 206, 1266, 204, -320, -223, 704, -687, -276,
                            -716, 787, -1121, 40, 292, 249, -10,
                        ],
                    ],
                    sigma_dct: [
                        139, 65, 133, 5, 137, 272, 88, 178, 71, 256, 254, 82, 126, 252, 152, 53,
                        281, 15, 8, 209, 285, 156, 73, 56, 36, 287, 86, 244, 270, 94, 224, 156,
                    ],
                },
                Spline {
                    control_points: vec![
                        Point { x: 109.0, y: 54.0 },
                        Point { x: 218.0, y: 159.0 },
                        Point { x: 80.0, y: 3.0 },
                        Point { x: 110.0, y: 274.0 },
                        Point { x: 94.0, y: 185.0 },
                        Point { x: 17.0, y: 277.0 },
                    ],
                    color_dct: [
                        Dct32([
                            36.300457,
                            39.69839859,
                            23.20079994,
                            67.49819946,
                            4.401599884,
                            71.50080109,
                            62.29859924,
                            32.29800034,
                            92.19839478,
                            10.10099983,
                            10.79819965,
                            9.197999954,
                            6.098399639,
                            10.5,
                            79.09859467,
                            7.001399517,
                            24.59939957,
                            90.79979706,
                            5.501999855,
                            84.0,
                            43.80179977,
                            49.00139999,
                            33.49919891,
                            78.90119934,
                            54.49919891,
                            77.90159607,
                            62.10119629,
                            51.39959717,
                            36.40139771,
                            14.30099964,
                            83.70179749,
                            35.40179825,
                        ]),
                        Dct32([
                            9.386842728,
                            53.40000153,
                            9.525000572,
                            74.92500305,
                            72.67500305,
                            26.70000076,
                            7.875000477,
                            0.9000000358,
                            84.90000153,
                            23.17500114,
                            26.47500038,
                            31.12500191,
                            90.9750061,
                            11.70000076,
                            74.1000061,
                            39.30000305,
                            23.70000076,
                            82.5,
                            4.800000191,
                            2.700000048,
                            61.20000076,
                            96.37500763,
                            13.72500038,
                            66.67500305,
                            62.92500305,
                            82.42500305,
                            5.925000191,
                            98.70000458,
                            21.52500153,
                            7.875000477,
                            51.67500305,
                            63.07500076,
                        ]),
                        Dct32([
                            47.99487305,
                            39.33000183,
                            6.865000725,
                            26.27500153,
                            33.2650032,
                            6.190000534,
                            1.715000629,
                            98.90000153,
                            59.91000366,
                            59.57500458,
                            95.00499725,
                            61.29500198,
                            82.71500397,
                            53.0,
                            6.130004883,
                            30.41000366,
                            34.69000244,
                            96.91999817,
                            93.4200058,
                            16.97999954,
                            38.80000305,
                            80.76500702,
                            63.00499725,
                            18.5850029,
                            43.60500336,
                            32.30500412,
                            61.01499939,
                            20.23000336,
                            24.32500076,
                            28.31500053,
                            69.10500336,
                            62.375,
                        ]),
                    ],
                    sigma_dct: Dct32([
                        32.75933838,
                        21.66449928,
                        44.32889938,
                        1.666499972,
                        45.66209793,
                        90.6576004,
                        29.33039856,
                        59.32740021,
                        23.66429901,
                        85.32479858,
                        84.6581955,
                        27.33059883,
                        41.99580002,
                        83.99160004,
                        50.66159821,
                        17.66489983,
                        93.65729523,
                        4.999499798,
                        2.666399956,
                        69.65969849,
                        94.9905014,
                        51.99480057,
                        24.33090019,
                        18.66479874,
                        11.99880028,
                        95.65709686,
                        28.66379929,
                        81.32519531,
                        89.99099731,
                        31.3302002,
                        74.65919495,
                        51.99480057,
                    ]),
                    estimated_area_reached: 19843491681,
                },
            ),
            (
                QuantizedSpline {
                    control_points: vec![
                        (24, -32),
                        (-178, -7),
                        (226, 151),
                        (121, -172),
                        (-184, 39),
                        (-201, -182),
                        (301, 404),
                    ],
                    color_dct: [
                        [
                            5051, 6881, 5238, 1571, 9952, 19762, 2048, 13524, 16405, 2310, 1286,
                            4714, 16857, 21429, 12500, 15524, 1857, 5595, 6286, 17190, 15405,
                            20738, 310, 16071, 10952, 16286, 15571, 8452, 6929, 3095, 9905, 5690,
                        ],
                        [
                            899, 1059, 836, 388, 1291, 247, 235, 203, 1073, 747, 1283, 799, 356,
                            1281, 1231, 561, 477, 720, 309, 733, 1013, 477, 779, 1183, 32, 1041,
                            1275, 367, 88, 1047, 321, 931,
                        ],
                        [
                            -78, 244, -883, 943, -682, 752, 107, 262, -75, 557, -202, -575, -231,
                            -731, -605, 732, 682, 650, 592, -14, -1035, 913, -188, -95, 286, -574,
                            -509, 67, 86, -1056, 592, 380,
                        ],
                    ],
                    sigma_dct: [
                        308, 8, 125, 7, 119, 237, 209, 60, 277, 215, 126, 186, 90, 148, 211, 136,
                        188, 142, 140, 124, 272, 140, 274, 165, 24, 209, 76, 254, 185, 83, 11, 141,
                    ],
                },
                Spline {
                    control_points: vec![
                        Point { x: 172.0, y: 309.0 },
                        Point { x: 196.0, y: 277.0 },
                        Point { x: 42.0, y: 238.0 },
                        Point { x: 114.0, y: 350.0 },
                        Point { x: 307.0, y: 290.0 },
                        Point { x: 316.0, y: 269.0 },
                        Point { x: 124.0, y: 66.0 },
                        Point { x: 233.0, y: 267.0 },
                    ],
                    color_dct: [
                        Dct32([
                            15.00070381,
                            28.90019989,
                            21.99959946,
                            6.598199844,
                            41.79839706,
                            83.00039673,
                            8.601599693,
                            56.80079651,
                            68.90100098,
                            9.701999664,
                            5.401199818,
                            19.79879951,
                            70.79940033,
                            90.00180054,
                            52.5,
                            65.20079803,
                            7.799399853,
                            23.49899864,
                            26.40119934,
                            72.19799805,
                            64.7009964,
                            87.09959412,
                            1.301999927,
                            67.49819946,
                            45.99839783,
                            68.40119934,
                            65.39820099,
                            35.49839783,
                            29.10179901,
                            12.9989996,
                            41.60099792,
                            23.89799881,
                        ]),
                        Dct32([
                            47.67667389,
                            79.42500305,
                            62.70000076,
                            29.10000038,
                            96.82500458,
                            18.52500153,
                            17.625,
                            15.22500038,
                            80.4750061,
                            56.02500153,
                            96.2250061,
                            59.92500305,
                            26.70000076,
                            96.07500458,
                            92.32500458,
                            42.07500076,
                            35.77500153,
                            54.00000381,
                            23.17500114,
                            54.97500229,
                            75.9750061,
                            35.77500153,
                            58.42500305,
                            88.7250061,
                            2.400000095,
                            78.07500458,
                            95.625,
                            27.52500153,
                            6.600000381,
                            78.52500153,
                            24.07500076,
                            69.82500458,
                        ]),
                        Dct32([
                            43.81587219,
                            96.50500488,
                            0.8899993896,
                            95.11000061,
                            49.0850029,
                            71.16500092,
                            25.11499977,
                            33.56500244,
                            75.2250061,
                            95.01499939,
                            82.08500671,
                            19.67500305,
                            10.53000069,
                            44.90500259,
                            49.9750061,
                            93.31500244,
                            83.51499939,
                            99.5,
                            64.61499786,
                            53.99500275,
                            3.525009155,
                            99.68499756,
                            45.2650032,
                            82.07500458,
                            22.42000008,
                            37.89500427,
                            59.99499893,
                            32.21500015,
                            12.62000084,
                            4.605003357,
                            65.51499939,
                            96.42500305,
                        ]),
                    ],
                    sigma_dct: Dct32([
                        72.58903503,
                        2.666399956,
                        41.66249847,
                        2.333099842,
                        39.66270065,
                        78.99209595,
                        69.65969849,
                        19.99799919,
                        92.32409668,
                        71.65950012,
                        41.99580002,
                        61.9937973,
                        29.99699974,
                        49.32839966,
                        70.32630157,
                        45.3288002,
                        62.66040039,
                        47.32859802,
                        46.66199875,
                        41.32920074,
                        90.6576004,
                        46.66199875,
                        91.32419586,
                        54.99449921,
                        7.999199867,
                        69.65969849,
                        25.3307991,
                        84.6581955,
                        61.66049957,
                        27.66390038,
                        3.66629982,
                        46.99530029,
                    ]),
                    estimated_area_reached: 25829781306,
                },
            ),
            (
                QuantizedSpline {
                    control_points: vec![
                        (157, -89),
                        (-244, 41),
                        (-58, 168),
                        (429, -185),
                        (-361, 198),
                        (230, -269),
                        (-416, 203),
                        (167, 65),
                        (460, -344),
                    ],
                    color_dct: [
                        [
                            5691, 15429, 1000, 2524, 5595, 4048, 18881, 1357, 14381, 3952, 22595,
                            15167, 20857, 2500, 905, 14548, 5452, 19500, 19143, 9643, 10929, 6048,
                            9476, 7143, 11952, 21524, 6643, 22310, 15500, 11476, 5310, 10452,
                        ],
                        [
                            470, 880, 47, 1203, 1295, 211, 475, 8, 907, 528, 325, 1145, 769, 1035,
                            633, 905, 57, 72, 1216, 780, 1, 696, 47, 637, 843, 580, 1144, 477, 669,
                            479, 256, 643,
                        ],
                        [
                            1169, -301, 1041, -725, -43, -22, 774, 134, -822, 499, 456, -287, -713,
                            -776, 76, 449, 750, 580, -207, -643, 956, -426, 377, -64, 101, -250,
                            -164, 259, 169, -240, 430, -22,
                        ],
                    ],
                    sigma_dct: [
                        354, 5, 75, 56, 140, 226, 84, 187, 151, 70, 257, 288, 137, 99, 100, 159,
                        79, 176, 59, 210, 278, 68, 171, 65, 230, 263, 69, 199, 107, 107, 170, 202,
                    ],
                },
                Spline {
                    control_points: vec![
                        Point { x: 100.0, y: 186.0 },
                        Point { x: 257.0, y: 97.0 },
                        Point { x: 170.0, y: 49.0 },
                        Point { x: 25.0, y: 169.0 },
                        Point { x: 309.0, y: 104.0 },
                        Point { x: 232.0, y: 237.0 },
                        Point { x: 385.0, y: 101.0 },
                        Point { x: 122.0, y: 168.0 },
                        Point { x: 26.0, y: 300.0 },
                        Point { x: 390.0, y: 88.0 },
                    ],
                    color_dct: [
                        Dct32([
                            16.90140724,
                            64.80179596,
                            4.199999809,
                            10.60079956,
                            23.49899864,
                            17.00160027,
                            79.30019379,
                            5.699399948,
                            60.40019608,
                            16.59840012,
                            94.89899445,
                            63.70139694,
                            87.59939575,
                            10.5,
                            3.80099988,
                            61.10159683,
                            22.89839935,
                            81.8999939,
                            80.40059662,
                            40.50059891,
                            45.90179825,
                            25.40159988,
                            39.79919815,
                            30.00059891,
                            50.19839859,
                            90.40079498,
                            27.90059853,
                            93.70199585,
                            65.09999847,
                            48.19919968,
                            22.30200005,
                            43.89839935,
                        ]),
                        Dct32([
                            24.92551422,
                            66.0,
                            3.525000095,
                            90.2250061,
                            97.12500763,
                            15.82500076,
                            35.625,
                            0.6000000238,
                            68.02500153,
                            39.60000229,
                            24.37500191,
                            85.875,
                            57.67500305,
                            77.625,
                            47.47500229,
                            67.875,
                            4.275000095,
                            5.400000095,
                            91.20000458,
                            58.50000381,
                            0.07500000298,
                            52.20000076,
                            3.525000095,
                            47.77500153,
                            63.22500229,
                            43.5,
                            85.80000305,
                            35.77500153,
                            50.17500305,
                            35.92500305,
                            19.20000076,
                            48.22500229,
                        ]),
                        Dct32([
                            82.78805542,
                            44.93000031,
                            76.39500427,
                            39.4750061,
                            94.11500549,
                            14.2850008,
                            89.80500031,
                            9.980000496,
                            10.48500061,
                            74.52999878,
                            56.29500198,
                            65.78500366,
                            7.765003204,
                            23.30500031,
                            52.79500198,
                            99.30500031,
                            56.77500153,
                            46.0,
                            76.71000671,
                            13.49000549,
                            66.99499512,
                            22.38000107,
                            29.91499901,
                            43.29500198,
                            70.2950058,
                            26.0,
                            74.31999969,
                            53.90499878,
                            62.00500488,
                            19.12500381,
                            49.30000305,
                            46.68500137,
                        ]),
                    ],
                    sigma_dct: Dct32([
                        83.43025208,
                        1.666499972,
                        24.99749947,
                        18.66479874,
                        46.66199875,
                        75.32579803,
                        27.99720001,
                        62.32709885,
                        50.32830048,
                        23.33099937,
                        85.65809631,
                        95.99040222,
                        45.66209793,
                        32.99670029,
                        33.32999802,
                        52.99469757,
                        26.33069992,
                        58.66079712,
                        19.66469955,
                        69.99299622,
                        92.65740204,
                        22.6644001,
                        56.99430084,
                        21.66449928,
                        76.65899658,
                        87.65789795,
                        22.99769974,
                        66.3266983,
                        35.6631012,
                        35.6631012,
                        56.6609993,
                        67.32659912,
                    ]),
                    estimated_area_reached: 47263284396,
                },
            ),
        ];
        for (quantized, want_dequantized) in quantized_and_dequantized {
            let got_dequantized = quantized.dequantize(
                &want_dequantized.control_points[0],
                0,
                0.0,
                1.0,
                2u64 << 30,
            )?;
            assert_eq!(
                got_dequantized.control_points.len(),
                want_dequantized.control_points.len()
            );
            assert_all_almost_abs_eq(
                got_dequantized
                    .control_points
                    .iter()
                    .map(|p| p.x)
                    .collect::<Vec<f32>>(),
                want_dequantized
                    .control_points
                    .iter()
                    .map(|p| p.x)
                    .collect::<Vec<f32>>(),
                1e-6,
            );
            assert_all_almost_abs_eq(
                got_dequantized
                    .control_points
                    .iter()
                    .map(|p| p.y)
                    .collect::<Vec<f32>>(),
                want_dequantized
                    .control_points
                    .iter()
                    .map(|p| p.y)
                    .collect::<Vec<f32>>(),
                1e-6,
            );
            for index in 0..got_dequantized.color_dct.len() {
                assert_all_almost_abs_eq(
                    got_dequantized.color_dct[index].0,
                    want_dequantized.color_dct[index].0,
                    1e-4,
                );
            }
            assert_all_almost_abs_eq(
                got_dequantized.sigma_dct.0,
                want_dequantized.sigma_dct.0,
                1e-4,
            );
            assert_eq!(
                got_dequantized.estimated_area_reached,
                want_dequantized.estimated_area_reached,
            );
        }
        Ok(())
    }

    #[test]
    fn centripetal_catmull_rom_spline() -> Result<(), Error> {
        let control_points = vec![Point { x: 1.0, y: 2.0 }, Point { x: 4.0, y: 3.0 }];
        let want_result = [
            Point { x: 1.0, y: 2.0 },
            Point {
                x: 1.187500119,
                y: 2.0625,
            },
            Point { x: 1.375, y: 2.125 },
            Point {
                x: 1.562499881,
                y: 2.1875,
            },
            Point {
                x: 1.750000119,
                y: 2.25,
            },
            Point {
                x: 1.9375,
                y: 2.3125,
            },
            Point { x: 2.125, y: 2.375 },
            Point {
                x: 2.312500238,
                y: 2.4375,
            },
            Point {
                x: 2.500000238,
                y: 2.5,
            },
            Point {
                x: 2.6875,
                y: 2.5625,
            },
            Point {
                x: 2.875000477,
                y: 2.625,
            },
            Point {
                x: 3.062499762,
                y: 2.6875,
            },
            Point { x: 3.25, y: 2.75 },
            Point {
                x: 3.4375,
                y: 2.8125,
            },
            Point {
                x: 3.624999762,
                y: 2.875,
            },
            Point {
                x: 3.812500238,
                y: 2.9375,
            },
            Point { x: 4.0, y: 3.0 },
        ];
        let got_result = draw_centripetal_catmull_rom_spline(&control_points)?;
        assert_all_almost_abs_eq(
            got_result.iter().map(|p| p.x).collect::<Vec<f32>>(),
            want_result.iter().map(|p| p.x).collect::<Vec<f32>>(),
            1e-10,
        );
        Ok(())
    }

    #[test]
    fn equally_spaced_points() -> Result<(), Error> {
        let desired_rendering_distance = 10.0f32;
        let segments = [
            Point { x: 0.0, y: 0.0 },
            Point { x: 5.0, y: 0.0 },
            Point { x: 35.0, y: 0.0 },
            Point { x: 35.0, y: 10.0 },
        ];
        let want_results = [
            (Point { x: 0.0, y: 0.0 }, desired_rendering_distance),
            (Point { x: 10.0, y: 0.0 }, desired_rendering_distance),
            (Point { x: 20.0, y: 0.0 }, desired_rendering_distance),
            (Point { x: 30.0, y: 0.0 }, desired_rendering_distance),
            (Point { x: 35.0, y: 5.0 }, desired_rendering_distance),
            (Point { x: 35.0, y: 10.0 }, 5.0f32),
        ];
        let mut got_results = Vec::<(Point, f32)>::new();
        for_each_equally_spaced_point(&segments, desired_rendering_distance, |p, d| {
            got_results.push((p, d))
        });
        assert_all_almost_abs_eq(
            got_results.iter().map(|(p, _)| p.x).collect::<Vec<f32>>(),
            want_results.iter().map(|(p, _)| p.x).collect::<Vec<f32>>(),
            1e-9,
        );
        assert_all_almost_abs_eq(
            got_results.iter().map(|(p, _)| p.y).collect::<Vec<f32>>(),
            want_results.iter().map(|(p, _)| p.y).collect::<Vec<f32>>(),
            1e-9,
        );
        assert_all_almost_abs_eq(
            got_results.iter().map(|(_, d)| *d).collect::<Vec<f32>>(),
            want_results.iter().map(|(_, d)| *d).collect::<Vec<f32>>(),
            1e-9,
        );
        Ok(())
    }

    #[test]
    fn dct32() -> Result<(), Error> {
        let mut dct = Dct32::default();
        for (i, coeff) in dct.0.iter_mut().enumerate() {
            *coeff = 0.05f32 * i as f32;
        }
        // Golden numbers come from libjxl.
        let want_out = [
            16.7353153229,
            -18.6041717529,
            7.9931735992,
            -7.1250801086,
            4.6699867249,
            -4.3367614746,
            3.2450540066,
            -3.0694460869,
            2.4446771145,
            -2.3350939751,
            1.9243829250,
            -1.8484034538,
            1.5531382561,
            -1.4964176416,
            1.2701368332,
            -1.2254891396,
            1.0434474945,
            -1.0067725182,
            0.8544843197,
            -0.8232427835,
            0.6916543841,
            -0.6642799377,
            0.5473306179,
            -0.5226536393,
            0.4161090851,
            -0.3933961987,
            0.2940555215,
            -0.2726306915,
            0.1781132221,
            -0.1574717760,
            0.0656886101,
            -0.0454511642,
        ];
        for (t, want) in want_out.iter().enumerate() {
            let got_out = dct.continuous_idct(t as f32);
            assert_almost_abs_eq(got_out, *want, 1e-4);
        }
        Ok(())
    }

    #[test]
    fn dct32_fast_matches_original() {
        // Verify that continuous_idct_fast produces the same results as continuous_idct
        let mut dct = Dct32::default();
        for (i, coeff) in dct.0.iter_mut().enumerate() {
            *coeff = 0.05f32 * i as f32;
        }

        for t in 0..32 {
            let t_val = t as f32;
            let original = dct.continuous_idct(t_val);
            let precomputed = PrecomputedCosines::new(t_val);
            let fast = dct.continuous_idct_fast(&precomputed);
            assert_almost_abs_eq(fast, original, 1e-5);
        }
    }

    fn verify_segment_almost_equal(seg1: &SplineSegment, seg2: &SplineSegment) {
        assert_almost_eq(seg1.center_x, seg2.center_x, 1e-2, 1e-4);
        assert_almost_eq(seg1.center_y, seg2.center_y, 1e-2, 1e-4);
        for (got, want) in zip(seg1.color.iter(), seg2.color.iter()) {
            assert_almost_eq(*got, *want, 1e-2, 1e-4);
        }
        assert_almost_eq(seg1.inv_sigma, seg2.inv_sigma, 1e-2, 1e-4);
        assert_almost_eq(seg1.maximum_distance, seg2.maximum_distance, 1e-2, 1e-4);
        assert_almost_eq(
            seg1.sigma_over_4_times_intensity,
            seg2.sigma_over_4_times_intensity,
            1e-2,
            1e-4,
        );
    }

    #[test]
    fn spline_segments_add_segment() -> Result<(), Error> {
        let mut splines = Splines::default();
        let mut segments_by_y = Vec::<(u64, usize)>::new();

        splines.add_segment(
            &Point { x: 10.0, y: 20.0 },
            0.5,
            [0.5, 0.6, 0.7],
            0.8,
            true,
            &mut segments_by_y,
        );
        // Golden numbers come from libjxl.
        let want_segment = SplineSegment {
            center_x: 10.0,
            center_y: 20.0,
            color: [0.5, 0.6, 0.7],
            inv_sigma: 1.25,
            maximum_distance: 3.65961,
            sigma_over_4_times_intensity: 0.1,
        };
        assert_eq!(splines.segments.len(), 1);
        verify_segment_almost_equal(&splines.segments[0], &want_segment);
        let want_segments_by_y = [
            (16, 0),
            (17, 0),
            (18, 0),
            (19, 0),
            (20, 0),
            (21, 0),
            (22, 0),
            (23, 0),
            (24, 0),
        ];
        for (got, want) in zip(segments_by_y.iter(), want_segments_by_y.iter()) {
            assert_eq!(got.0, want.0);
            assert_eq!(got.1, want.1);
        }
        Ok(())
    }

    #[test]
    fn spline_segments_add_segments_from_points() -> Result<(), Error> {
        let mut splines = Splines::default();
        let mut segments_by_y = Vec::<(u64, usize)>::new();
        let mut color_dct = [Dct32::default(); 3];
        for (channel_index, channel_dct) in color_dct.iter_mut().enumerate() {
            for (coeff_index, coeff) in channel_dct.0.iter_mut().enumerate() {
                *coeff = 0.1 * channel_index as f32 + 0.05 * coeff_index as f32;
            }
        }
        let mut sigma_dct = Dct32::default();
        for (coeff_index, coeff) in sigma_dct.0.iter_mut().enumerate() {
            *coeff = 0.06 * coeff_index as f32;
        }
        let spline = Spline {
            control_points: vec![],
            color_dct,
            sigma_dct,
            estimated_area_reached: 0,
        };
        let points_to_draw = vec![
            (Point { x: 10.0, y: 20.0 }, 1.0),
            (Point { x: 11.0, y: 21.0 }, 1.0),
            (Point { x: 12.0, y: 21.0 }, 1.0),
        ];
        splines.add_segments_from_points(
            &spline,
            &points_to_draw,
            SQRT_2 + 1.0,
            DESIRED_RENDERING_DISTANCE,
            true,
            &mut segments_by_y,
        );
        // Golden numbers come from libjxl.
        let want_segments = [
            SplineSegment {
                center_x: 10.0,
                center_y: 20.0,
                color: [16.73531532, 19.68646049, 22.63760757],
                inv_sigma: 0.04979490861,
                maximum_distance: 108.6400299,
                sigma_over_4_times_intensity: 5.020593643,
            },
            SplineSegment {
                center_x: 11.0,
                center_y: 21.0,
                color: [-0.8199231625, -0.7960500717, -0.7721766233],
                inv_sigma: -1.016355753,
                maximum_distance: 4.680418015,
                sigma_over_4_times_intensity: -0.2459768653,
            },
            SplineSegment {
                center_x: 12.0,
                center_y: 21.0,
                color: [-0.7767754197, -0.7544237971, -0.7320720553],
                inv_sigma: -1.072811365,
                maximum_distance: 4.423510075,
                sigma_over_4_times_intensity: -0.2330325693,
            },
        ];
        assert_eq!(splines.segments.len(), want_segments.len());
        for (got, want) in zip(splines.segments.iter(), want_segments.iter()) {
            verify_segment_almost_equal(got, want);
        }
        let want_segments_by_y: Vec<(u64, usize)> = (0..=129)
            .map(|c| (c, 0))
            .chain((16..=26).map(|c| (c, 1)))
            .chain((17..=25).map(|c| (c, 2)))
            .collect();
        for (got, want) in zip(segments_by_y.iter(), want_segments_by_y.iter()) {
            assert_eq!(got.0, want.0);
            assert_eq!(got.1, want.1);
        }
        Ok(())
    }

    #[test]
    fn init_draw_cache() -> Result<(), Error> {
        let mut splines = Splines {
            splines: vec![
                QuantizedSpline {
                    control_points: vec![
                        (109, 105),
                        (-247, -261),
                        (168, 427),
                        (-46, -360),
                        (-61, 181),
                    ],
                    color_dct: [
                        [
                            12223, 9452, 5524, 16071, 1048, 17024, 14833, 7690, 21952, 2405, 2571,
                            2190, 1452, 2500, 18833, 1667, 5857, 21619, 1310, 20000, 10429, 11667,
                            7976, 18786, 12976, 18548, 14786, 12238, 8667, 3405, 19929, 8429,
                        ],
                        [
                            177, 712, 127, 999, 969, 356, 105, 12, 1132, 309, 353, 415, 1213, 156,
                            988, 524, 316, 1100, 64, 36, 816, 1285, 183, 889, 839, 1099, 79, 1316,
                            287, 105, 689, 841,
                        ],
                        [
                            780, -201, -38, -695, -563, -293, -88, 1400, -357, 520, 979, 431, -118,
                            590, -971, -127, 157, 206, 1266, 204, -320, -223, 704, -687, -276,
                            -716, 787, -1121, 40, 292, 249, -10,
                        ],
                    ],
                    sigma_dct: [
                        139, 65, 133, 5, 137, 272, 88, 178, 71, 256, 254, 82, 126, 252, 152, 53,
                        281, 15, 8, 209, 285, 156, 73, 56, 36, 287, 86, 244, 270, 94, 224, 156,
                    ],
                },
                QuantizedSpline {
                    control_points: vec![
                        (24, -32),
                        (-178, -7),
                        (226, 151),
                        (121, -172),
                        (-184, 39),
                        (-201, -182),
                        (301, 404),
                    ],
                    color_dct: [
                        [
                            5051, 6881, 5238, 1571, 9952, 19762, 2048, 13524, 16405, 2310, 1286,
                            4714, 16857, 21429, 12500, 15524, 1857, 5595, 6286, 17190, 15405,
                            20738, 310, 16071, 10952, 16286, 15571, 8452, 6929, 3095, 9905, 5690,
                        ],
                        [
                            899, 1059, 836, 388, 1291, 247, 235, 203, 1073, 747, 1283, 799, 356,
                            1281, 1231, 561, 477, 720, 309, 733, 1013, 477, 779, 1183, 32, 1041,
                            1275, 367, 88, 1047, 321, 931,
                        ],
                        [
                            -78, 244, -883, 943, -682, 752, 107, 262, -75, 557, -202, -575, -231,
                            -731, -605, 732, 682, 650, 592, -14, -1035, 913, -188, -95, 286, -574,
                            -509, 67, 86, -1056, 592, 380,
                        ],
                    ],
                    sigma_dct: [
                        308, 8, 125, 7, 119, 237, 209, 60, 277, 215, 126, 186, 90, 148, 211, 136,
                        188, 142, 140, 124, 272, 140, 274, 165, 24, 209, 76, 254, 185, 83, 11, 141,
                    ],
                },
            ],
            starting_points: vec![Point { x: 10.0, y: 20.0 }, Point { x: 5.0, y: 40.0 }],
            ..Default::default()
        };
        splines.initialize_draw_cache(
            1 << 15,
            1 << 15,
            &ColorCorrelationParams {
                color_factor: 1,
                base_correlation_x: 0.0,
                base_correlation_b: 0.0,
                ytox_lf: 0,
                ytob_lf: 0,
            },
            true,
        )?;
        assert_eq!(splines.segments.len(), 1940);
        let want_segments_sample = [
            (
                22,
                SplineSegment {
                    center_x: 25.77652359,
                    center_y: 35.33295059,
                    color: [-524.996582, -509.9048462, 43.3883667],
                    inv_sigma: -0.00197347207,
                    maximum_distance: 3021.377197,
                    sigma_over_4_times_intensity: -126.6802902,
                },
            ),
            (
                474,
                SplineSegment {
                    center_x: -16.45600891,
                    center_y: 78.81845856,
                    color: [-117.6707535, -133.5515594, 343.5632629],
                    inv_sigma: -0.002631845651,
                    maximum_distance: 2238.376221,
                    sigma_over_4_times_intensity: -94.9903717,
                },
            ),
            (
                835,
                SplineSegment {
                    center_x: -71.93701172,
                    center_y: 230.0635529,
                    color: [44.79507446, 298.9411621, -395.3574524],
                    inv_sigma: 0.01869126037,
                    maximum_distance: 316.4499207,
                    sigma_over_4_times_intensity: 13.3752346,
                },
            ),
            (
                1066,
                SplineSegment {
                    center_x: -126.2593002,
                    center_y: -22.97857094,
                    color: [-136.4196625, 194.757019, -98.18778992],
                    inv_sigma: 0.007531851064,
                    maximum_distance: 769.2540283,
                    sigma_over_4_times_intensity: 33.19237137,
                },
            ),
            (
                1328,
                SplineSegment {
                    center_x: 73.70871735,
                    center_y: 56.31413269,
                    color: [-13.44394779, 162.6139221, 93.78419495],
                    inv_sigma: 0.003664178308,
                    maximum_distance: 1572.710327,
                    sigma_over_4_times_intensity: 68.2281189,
                },
            ),
            (
                1545,
                SplineSegment {
                    center_x: 77.48892975,
                    center_y: -92.33877563,
                    color: [-220.6807556, 66.13040924, -32.26184082],
                    inv_sigma: 0.03166157752,
                    maximum_distance: 183.6748352,
                    sigma_over_4_times_intensity: 7.89600563,
                },
            ),
            (
                1774,
                SplineSegment {
                    center_x: -16.43594933,
                    center_y: -144.8626556,
                    color: [57.31535339, -46.36843109, 92.14952087],
                    inv_sigma: -0.01524505392,
                    maximum_distance: 371.4827271,
                    sigma_over_4_times_intensity: -16.39876175,
                },
            ),
            (
                1929,
                SplineSegment {
                    center_x: 61.19338608,
                    center_y: -10.70717049,
                    color: [-69.78807068, 300.6082458, -476.5135803],
                    inv_sigma: 0.003229281865,
                    maximum_distance: 1841.37854,
                    sigma_over_4_times_intensity: 77.41659546,
                },
            ),
        ];
        for (index, segment) in want_segments_sample {
            verify_segment_almost_equal(&segment, &splines.segments[index]);
        }
        Ok(())
    }
}
