//! Interpolate Untouched Points
//!
//! This module contains code for optimizing variable glyph deltas, by removing
//! deltas that can be interpolated.
//!
//! See [Inferred deltas for un-referenced point numbers][spec] for more information.
//!
//! [spec]: https://learn.microsoft.com/en-us/typography/opentype/spec/gvar#inferred-deltas-for-un-referenced-point-numbers

use std::collections::HashSet;

use super::GlyphDelta;
use crate::{round::OtRound, util::WrappingGet};

use kurbo::{Point, Vec2};

const NUM_PHANTOM_POINTS: usize = 4;

/// For the outline given in `coords`, with contour endpoints given
/// `ends`, optimize a set of delta values `deltas` within error `tolerance`.
///
/// For each delta in the input, returns an [`GlyphDelta`] with a flag
/// indicating whether or not it can be interpolated.
///
/// See:
/// * <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Lib/fontTools/varLib/iup.py#L470>
/// * <https://learn.microsoft.com/en-us/typography/opentype/spec/gvar#inferred-deltas-for-un-referenced-point-numbers>
pub fn iup_delta_optimize(
    deltas: Vec<Vec2>,
    coords: Vec<Point>,
    tolerance: f64,
    contour_ends: &[usize],
) -> Result<Vec<GlyphDelta>, IupError> {
    let num_coords = coords.len();
    if num_coords < NUM_PHANTOM_POINTS {
        return Err(IupError::NotEnoughCoords(num_coords));
    }
    if deltas.len() != coords.len() {
        return Err(IupError::DeltaCoordLengthMismatch {
            num_deltas: deltas.len(),
            num_coords: coords.len(),
        });
    }

    let mut contour_ends = contour_ends.to_vec();
    contour_ends.sort();

    let expected_num_coords = contour_ends
        .last()
        .copied()
        .map(|v| v + 1)
        .unwrap_or_default()
        + NUM_PHANTOM_POINTS;
    if num_coords != expected_num_coords {
        return Err(IupError::CoordEndsMismatch {
            num_coords,
            expected_num_coords,
        });
    }

    for offset in (1..=4).rev() {
        contour_ends.push(num_coords.saturating_sub(offset));
    }

    let mut result = Vec::with_capacity(num_coords);
    let mut start = 0;
    let mut deltas = deltas;
    let mut coords = coords;
    for end in contour_ends {
        let contour = iup_contour_optimize(
            &mut deltas[start..=end],
            &mut coords[start..=end],
            tolerance,
        )?;
        result.extend_from_slice(&contour);
        assert_eq!(contour.len() + start, end + 1);
        start = end + 1;
    }
    Ok(result)
}

#[derive(Clone, Debug)]
pub enum IupError {
    DeltaCoordLengthMismatch {
        num_deltas: usize,
        num_coords: usize,
    },
    NotEnoughCoords(usize),
    CoordEndsMismatch {
        num_coords: usize,
        expected_num_coords: usize,
    },
    AchievedInvalidState(String),
}

/// Check if IUP _might_ be possible. If not then we *must* encode the value at this index.
///
/// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Lib/fontTools/varLib/iup.py#L238-L290>
fn must_encode_at(deltas: &[Vec2], coords: &[Point], tolerance: f64, at: usize) -> bool {
    let ld = *deltas.wrapping_prev(at);
    let d = deltas[at];
    let nd = *deltas.wrapping_next(at);
    let lc = *coords.wrapping_prev(at);
    let c = coords[at];
    let nc = *coords.wrapping_next(at);

    for axis in [Axis2D::X, Axis2D::Y] {
        let (ldj, lcj) = (ld.get(axis), lc.get(axis));
        let (dj, cj) = (d.get(axis), c.get(axis));
        let (ndj, ncj) = (nd.get(axis), nc.get(axis));
        let (c1, c2, d1, d2) = if lcj <= ncj {
            (lcj, ncj, ldj, ndj)
        } else {
            (ncj, lcj, ndj, ldj)
        };
        // If the two coordinates are the same, then the interpolation
        // algorithm produces the same delta if both deltas are equal,
        // and zero if they differ.
        match (c1, c2) {
            _ if c1 == c2 => {
                if (d1 - d2).abs() > tolerance && dj.abs() > tolerance {
                    return true;
                }
            }
            _ if c1 <= cj && cj <= c2 => {
                // and c1 != c2
                // If coordinate for current point is between coordinate of adjacent
                // points on the two sides, but the delta for current point is NOT
                // between delta for those adjacent points (considering tolerance
                // allowance), then there is no way that current point can be IUP-ed.
                if !(d1.min(d2) - tolerance <= dj && dj <= d1.max(d2) + tolerance) {
                    return true;
                }
            }
            _ => {
                // cj < c1 or c2 < cj
                // Otherwise, the delta should either match the closest, or have the
                // same sign as the interpolation of the two deltas.
                if d1 != d2 && dj.abs() > tolerance {
                    if cj < c1 {
                        if ((dj - d1).abs() > tolerance) && (dj - tolerance < d1) != (d1 < d2) {
                            return true;
                        }
                    } else if ((dj - d2).abs() > tolerance) && (d2 < dj + tolerance) != (d1 < d2) {
                        return true;
                    }
                }
            }
        }
    }
    false
}

/// Indices of deltas that must be encoded explicitly because they can't be interpolated.
///
/// These deltas must be encoded explicitly. That allows us the dynamic
/// programming solution clear stop points which speeds it up considerably.
///
/// Rust port of <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Lib/fontTools/varLib/iup.py#L218>
fn iup_must_encode(
    deltas: &[Vec2],
    coords: &[Point],
    tolerance: f64,
) -> Result<HashSet<usize>, IupError> {
    Ok((0..deltas.len())
        .rev()
        .filter(|i| must_encode_at(deltas, coords, tolerance, *i))
        .collect())
}

// TODO: use for might_iup? - take point and vec and coord
#[derive(Copy, Clone, Debug)]
enum Axis2D {
    X,
    Y,
}

/// Some of the fonttools code loops over 0,1 to access x/y.
///
/// Attempt to provide a nice way to do the same in Rust.
trait Coord {
    fn get(&self, axis: Axis2D) -> f64;
    fn set(&mut self, coord: Axis2D, value: f64);
}

impl Coord for Point {
    fn get(&self, axis: Axis2D) -> f64 {
        match axis {
            Axis2D::X => self.x,
            Axis2D::Y => self.y,
        }
    }

    fn set(&mut self, axis: Axis2D, value: f64) {
        match axis {
            Axis2D::X => self.x = value,
            Axis2D::Y => self.y = value,
        }
    }
}

impl Coord for Vec2 {
    fn get(&self, axis: Axis2D) -> f64 {
        match axis {
            Axis2D::X => self.x,
            Axis2D::Y => self.y,
        }
    }

    fn set(&mut self, axis: Axis2D, value: f64) {
        match axis {
            Axis2D::X => self.x = value,
            Axis2D::Y => self.y = value,
        }
    }
}

/// Given two reference coordinates `rc1` & `rc2` and their respective
/// delta vectors `rd1` & `rd2`, returns interpolated deltas for the set of
/// coordinates `coords`.
///
/// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Lib/fontTools/varLib/iup.py#L53>
fn iup_segment(coords: &[Point], rc1: Point, rd1: Vec2, rc2: Point, rd2: Vec2) -> Vec<Vec2> {
    // rc1 = reference coord 1
    // rd1 = reference delta 1

    let n = coords.len();
    let mut result = vec![Vec2::default(); n];
    for axis in [Axis2D::X, Axis2D::Y] {
        let c1 = rc1.get(axis);
        let c2 = rc2.get(axis);
        let d1 = rd1.get(axis);
        let d2 = rd2.get(axis);

        if c1 == c2 {
            let value = if d1 == d2 { d1 } else { 0.0 };
            for r in result.iter_mut() {
                r.set(axis, value);
            }
            continue;
        }

        let (c1, c2, d1, d2) = if c1 > c2 {
            (c2, c1, d2, d1) // flip
        } else {
            (c1, c2, d1, d2) // nop
        };

        // # c1 < c2
        let scale = (d2 - d1) / (c2 - c1);
        for (idx, point) in coords.iter().enumerate() {
            let c = point.get(axis);
            let d = if c <= c1 {
                d1
            } else if c >= c2 {
                d2
            } else {
                // Interpolate
                d1 + (c - c1) * scale
            };
            result[idx].set(axis, d);
        }
    }
    result
}

/// Checks if the deltas for points at `i` and `j` (`i < j`) be
/// used to interpolate deltas for points in between them within
/// provided error tolerance
///
/// See [iup_contour_optimize_dp] comments for context on from/to range restrictions.
///
/// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Lib/fontTools/varLib/iup.py#L187>
fn can_iup_in_between(
    deltas: &[Vec2],
    coords: &[Point],
    tolerance: f64,
    from: isize,
    to: isize,
) -> Result<bool, IupError> {
    if from < -1 || to <= from || to - from < 2 {
        return Err(IupError::AchievedInvalidState(format!(
            "bad from/to: {from}..{to}"
        )));
    }
    // from is >= -1 so from + 1 is a valid usize
    // to > from so to is a valid usize
    // from -1 is taken to mean the last entry
    let to = to as usize;
    let (rc1, rd1) = if from < 0 {
        (*coords.last().unwrap(), *deltas.last().unwrap())
    } else {
        (coords[from as usize], deltas[from as usize])
    };
    let iup_values = iup_segment(
        &coords[(from + 1) as usize..to],
        rc1,
        rd1,
        coords[to],
        deltas[to],
    );

    let real_values = &deltas[(from + 1) as usize..to];

    // compute this once here instead of in the loop
    let tolerance_sq = tolerance.powi(2);
    Ok(real_values
        .iter()
        .zip(iup_values)
        .all(|(d, i)| (*d - i).hypot2() <= tolerance_sq))
}

/// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Lib/fontTools/varLib/iup.py#L327>
fn iup_initial_lookback(deltas: &[Vec2]) -> usize {
    std::cmp::min(deltas.len(), 8usize) // no more than 8!
}

#[derive(Debug, PartialEq)]
struct OptimizeDpResult {
    costs: Vec<i32>,
    chain: Vec<Option<usize>>,
}

/// Straightforward Dynamic-Programming.  For each index i, find least-costly encoding of
/// points 0 to i where i is explicitly encoded.  We find this by considering all previous
/// explicit points j and check whether interpolation can fill points between j and i.
///
/// Note that solution always encodes last point explicitly.  Higher-level is responsible
/// for removing that restriction.
///
/// As major speedup, we stop looking further whenever we see a point we are certain requires explicit encoding.
/// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Lib/fontTools/varLib/iup.py#L308>
fn iup_contour_optimize_dp(
    deltas: &[Vec2],
    coords: &[Point],
    tolerance: f64,
    must_encode: &HashSet<usize>,
    lookback: usize,
) -> Result<OptimizeDpResult, IupError> {
    let n = deltas.len();
    let lookback = lookback as isize;
    let mut costs = Vec::with_capacity(n);
    let mut chain: Vec<_> = (0..n)
        .map(|i| if i > 0 { Some(i - 1) } else { None })
        .collect();

    // n < 2 is degenerate
    if n < 2 {
        return Ok(OptimizeDpResult { costs, chain });
    }

    for i in 0..n {
        let mut best_cost = if i > 0 { costs[i - 1] } else { 0 } + 1;

        costs.push(best_cost);
        if i > 0 && must_encode.contains(&(i - 1)) {
            continue;
        }

        // python inner loop is j in range(i - 2, max(i - lookback, -2), -1)
        //      start at no less than -2, step -1 toward no less than -2
        //      lookback is either deltas.len() or deltas.len()/2 (because we tried repeating the contour)
        //          deltas must have more than 1 point to even try, so lookback at least 2
        // how does this play out? - slightly non-obviously one might argue
        // costs starts as {-1:0}
        // at i=0
        //      best_cost is set to costs[-1] + 1 which is 1
        //      costs[0] = best_cost, costs is {-1:0, 0:1}
        //      j in range(-2, max(0 - at least 2, -2), -1), so range(-2, -2, -1), which is empty
        // at i=1
        //      best_cost is set to costs[-1] + 1 which is 2
        //      costs[i] = best_cost, costs is {-1:0, 0:1, 1:2}
        //      j in range(-1, max(1 - at least 2, -2), -1), so it must be one of:
        //          range(-1, -2, -1), range(-1, -1, -1)
        //          only range(-1, -2, -1) has any values, -1
        //          when j = -1
        //              cost = costs[-1] + 1 will set cost to 1, which is < best_cost
        //              call can_iup_in_between for -1, 1
        // from i=2 onward we walk from >=0 to >=-2 non-inclusive, so can_iup_in_between
        // will only see indices >= -1. In Python -1 reads the last point, which must be encoded.

        // Python loops from high (inclusive) to low (exclusive) stepping -1
        // To match, we loop from low+1 to high+1 to exclude low and include high
        // and reverse to mimic the step
        let j_min = std::cmp::max(i as isize - lookback, -2);
        let j_max = i as isize - 2;
        for j in (j_min + 1..j_max + 1).rev() {
            let (cost, must_encode) = if j >= 0 {
                (costs[j as usize] + 1, must_encode.contains(&(j as usize)))
            } else {
                (1, false)
            };
            if cost < best_cost && can_iup_in_between(deltas, coords, tolerance, j, i as isize)? {
                best_cost = cost;
                costs[i] = best_cost;
                chain[i] = if j >= 0 { Some(j as usize) } else { None };
            }
            if must_encode {
                break;
            }
        }
    }

    Ok(OptimizeDpResult { costs, chain })
}

/// For contour with coordinates `coords`, optimize a set of delta values `deltas` within error `tolerance`.
///
/// Returns delta vector that has most number of None items instead of the input delta.
/// Returns a vector of [`GlyphDelta`]s with the maximal number marked as
/// 'optional'.
///
/// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Lib/fontTools/varLib/iup.py#L369>
fn iup_contour_optimize(
    deltas: &mut [Vec2],
    coords: &mut [Point],
    tolerance: f64,
) -> Result<Vec<GlyphDelta>, IupError> {
    if deltas.len() != coords.len() {
        return Err(IupError::DeltaCoordLengthMismatch {
            num_deltas: deltas.len(),
            num_coords: coords.len(),
        });
    }

    let n = deltas.len();

    // Get the easy cases out of the way
    // Easy: all points are the same or there are no points
    // This covers the case when there is only one point
    let Some(first_delta) = deltas.first() else {
        return Ok(Vec::new());
    };
    if deltas.iter().all(|d| d == first_delta) {
        if *first_delta == Vec2::ZERO {
            return Ok(vec![GlyphDelta::optional(0, 0); n]);
        }

        let (x, y) = first_delta.to_point().ot_round();
        // if all deltas are equal than the first is explicit and the rest
        // are interpolatable
        return Ok(std::iter::once(GlyphDelta::required(x, y))
            .chain(std::iter::repeat(GlyphDelta::optional(x, y)))
            .take(n)
            .collect());
    }

    // Solve the general problem using Dynamic Programming
    let must_encode = iup_must_encode(deltas, coords, tolerance)?;
    // The iup_contour_optimize_dp() routine returns the optimal encoding
    // solution given the constraint that the last point is always encoded.
    // To remove this constraint, we use two different methods, depending on
    // whether forced set is non-empty or not:

    // Debugging: Make the next if always take the second branch and observe
    // if the font size changes (reduced); that would mean the forced-set
    // has members it should not have.
    let encode = if !must_encode.is_empty() {
        // Setup for iup_contour_optimize_dp
        // We know at least one point *must* be encoded so rotate such that last point is encoded
        // rot must be > 0 so this is rightwards
        let mid = n - 1 - must_encode.iter().max().unwrap();
        deltas.rotate_right(mid);
        coords.rotate_right(mid);
        let must_encode: HashSet<usize> = must_encode.iter().map(|idx| (idx + mid) % n).collect();
        let dp_result = iup_contour_optimize_dp(
            deltas,
            coords,
            tolerance,
            &must_encode,
            iup_initial_lookback(deltas),
        )?;

        // Assemble solution
        let mut encode = HashSet::new();

        let mut i = n - 1;
        loop {
            encode.insert(i);
            i = match dp_result.chain[i] {
                Some(v) => v,
                None => break,
            };
        }

        if !encode.is_superset(&must_encode) {
            return Err(IupError::AchievedInvalidState(format!(
                "{encode:?} should contain {must_encode:?}"
            )));
        }

        deltas.rotate_left(mid);
        // The original fonttools version applies the IUP solution to the deltas and
        // *then* rotates the deltas list back, whereas here we have rotated the deltas *before*
        // applying the solution at the end, so we must rotate the solution as well
        // otherwise indices don't match as in https://github.com/googlefonts/fontations/issues/571
        // Cf. https://github.com/fonttools/fonttools/blob/dd783ff/Lib/fontTools/varLib/iup.py#L435-L437
        encode.iter().map(|idx| (idx + n - mid) % n).collect()
    } else {
        // Repeat the contour an extra time, solve the new case, then look for solutions of the
        // circular n-length problem in the solution for new linear case.  I cannot prove that
        // this always produces the optimal solution...
        let mut deltas_twice = Vec::with_capacity(2 * n);
        deltas_twice.extend_from_slice(deltas);
        deltas_twice.extend_from_slice(deltas);
        let mut coords_twice = Vec::with_capacity(2 * n);
        coords_twice.extend_from_slice(coords);
        coords_twice.extend_from_slice(coords);

        let dp_result = iup_contour_optimize_dp(
            &deltas_twice,
            &coords_twice,
            tolerance,
            &must_encode,
            iup_initial_lookback(deltas),
        )?;

        let mut best_sol = None;
        let mut best_cost = (n + 1) as i32;

        for start in n - 1..dp_result.costs.len() - 1 {
            // Assemble solution
            let mut solution = HashSet::new();
            let mut i = Some(start);

            // As in Python, this must be true when `i` is non-negative and `start - n` is negative
            while i > start.checked_sub(n) {
                let idx = i.unwrap();
                solution.insert(idx % n);
                i = dp_result.chain[idx];
            }

            // this should only be None for the first loop, when start is n - 1
            if i == start.checked_sub(n) {
                // Python reads [-1] to get 0, usize doesn't like that
                let cost = dp_result.costs[start]
                    - if n < start {
                        dp_result.costs[start - n]
                    } else {
                        0
                    };
                if cost <= best_cost {
                    best_sol = Some(solution);
                    best_cost = cost;
                }
            }
        }

        let encode = best_sol.ok_or(IupError::AchievedInvalidState(
            "No best solution identified".to_string(),
        ))?;

        if !encode.is_superset(&must_encode) {
            return Err(IupError::AchievedInvalidState(format!(
                "{encode:?} should contain {must_encode:?}"
            )));
        }

        encode
    };

    Ok(deltas
        .iter()
        .enumerate()
        .map(|(i, delta)| {
            let (x, y) = delta.to_point().ot_round();
            if encode.contains(&i) {
                GlyphDelta::required(x, y)
            } else {
                GlyphDelta::optional(x, y)
            }
        })
        .collect())
}

#[cfg(test)]
mod tests {
    use super::*;
    use pretty_assertions::assert_eq;

    struct IupScenario {
        deltas: Vec<Vec2>,
        coords: Vec<Point>,
        expected_must_encode: HashSet<usize>,
    }

    impl IupScenario {
        /// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Tests/varLib/iup_test.py#L113>
        fn assert_must_encode(&self) {
            assert_eq!(
                self.expected_must_encode,
                iup_must_encode(&self.deltas, &self.coords, f64::EPSILON).unwrap()
            );
        }

        /// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Tests/varLib/iup_test.py#L116-L120>
        fn assert_optimize_dp(&self) {
            let must_encode = iup_must_encode(&self.deltas, &self.coords, f64::EPSILON).unwrap();
            let lookback = iup_initial_lookback(&self.deltas);
            let r1 = iup_contour_optimize_dp(
                &self.deltas,
                &self.coords,
                f64::EPSILON,
                &must_encode,
                lookback,
            )
            .unwrap();
            let must_encode = HashSet::new();
            let r2 = iup_contour_optimize_dp(
                &self.deltas,
                &self.coords,
                f64::EPSILON,
                &must_encode,
                lookback,
            )
            .unwrap();

            assert_eq!(r1, r2);
        }

        /// No Python equivalent
        fn assert_optimize_contour(&self) {
            let mut deltas = self.deltas.clone();
            let mut coords = self.coords.clone();
            iup_contour_optimize(&mut deltas, &mut coords, f64::EPSILON).unwrap();
        }
    }

    /// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Tests/varLib/iup_test.py#L15>
    fn iup_scenario1() -> IupScenario {
        IupScenario {
            deltas: vec![(0.0, 0.0).into()],
            coords: vec![(1.0, 2.0).into()],
            expected_must_encode: HashSet::new(),
        }
    }

    /// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Tests/varLib/iup_test.py#L16>
    fn iup_scenario2() -> IupScenario {
        IupScenario {
            deltas: vec![(0.0, 0.0).into(), (0.0, 0.0).into(), (0.0, 0.0).into()],
            coords: vec![(1.0, 2.0).into(), (3.0, 2.0).into(), (2.0, 3.0).into()],
            expected_must_encode: HashSet::new(),
        }
    }

    /// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Tests/varLib/iup_test.py#L17-L21>
    fn iup_scenario3() -> IupScenario {
        IupScenario {
            deltas: vec![
                (1.0, 1.0).into(),
                (-1.0, 1.0).into(),
                (-1.0, -1.0).into(),
                (1.0, -1.0).into(),
            ],
            coords: vec![
                (0.0, 0.0).into(),
                (2.0, 0.0).into(),
                (2.0, 2.0).into(),
                (0.0, 2.0).into(),
            ],
            expected_must_encode: HashSet::new(),
        }
    }

    /// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Tests/varLib/iup_test.py#L22-L52>
    fn iup_scenario4() -> IupScenario {
        IupScenario {
            deltas: vec![
                (-1.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (0.0, 0.0).into(),
                (0.0, 0.0).into(),
                (0.0, 0.0).into(),
                (0.0, 0.0).into(),
                (0.0, 0.0).into(),
                (0.0, 0.0).into(),
                (-1.0, 0.0).into(),
            ],
            coords: vec![
                (-35.0, -152.0).into(),
                (-86.0, -101.0).into(),
                (-50.0, -65.0).into(),
                (0.0, -116.0).into(),
                (51.0, -65.0).into(),
                (86.0, -99.0).into(),
                (35.0, -151.0).into(),
                (87.0, -202.0).into(),
                (51.0, -238.0).into(),
                (-1.0, -187.0).into(),
                (-53.0, -239.0).into(),
                (-88.0, -205.0).into(),
            ],
            expected_must_encode: HashSet::from([11]),
        }
    }

    /// <https://github.com/fonttools/fonttools/blob/6a13bdc2e668334b04466b288d31179df1cff7be/Tests/varLib/iup_test.py#L53-L108>
    fn iup_scenario5() -> IupScenario {
        IupScenario {
            deltas: vec![
                (0.0, 0.0).into(),
                (1.0, 0.0).into(),
                (2.0, 0.0).into(),
                (2.0, 0.0).into(),
                (0.0, 0.0).into(),
                (1.0, 0.0).into(),
                (3.0, 0.0).into(),
                (3.0, 0.0).into(),
                (2.0, 0.0).into(),
                (2.0, 0.0).into(),
                (0.0, 0.0).into(),
                (0.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (-3.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (0.0, 0.0).into(),
                (0.0, 0.0).into(),
                (-2.0, 0.0).into(),
                (-2.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (-1.0, 0.0).into(),
                (-4.0, 0.0).into(),
            ],
            coords: vec![
                (330.0, 65.0).into(),
                (401.0, 65.0).into(),
                (499.0, 117.0).into(),
                (549.0, 225.0).into(),
                (549.0, 308.0).into(),
                (549.0, 422.0).into(),
                (549.0, 500.0).into(),
                (497.0, 600.0).into(),
                (397.0, 648.0).into(),
                (324.0, 648.0).into(),
                (271.0, 648.0).into(),
                (200.0, 620.0).into(),
                (165.0, 570.0).into(),
                (165.0, 536.0).into(),
                (165.0, 473.0).into(),
                (252.0, 407.0).into(),
                (355.0, 407.0).into(),
                (396.0, 407.0).into(),
                (396.0, 333.0).into(),
                (354.0, 333.0).into(),
                (249.0, 333.0).into(),
                (141.0, 268.0).into(),
                (141.0, 203.0).into(),
                (141.0, 131.0).into(),
                (247.0, 65.0).into(),
            ],
            expected_must_encode: HashSet::from([5, 15, 24]),
        }
    }

    /// The Python tests do not take the must encode empty branch of iup_contour_optimize,
    /// this test is meant to activate it by having enough points to be interesting and
    /// none of them must_encode.
    fn iup_scenario6() -> IupScenario {
        IupScenario {
            deltas: vec![
                (0.0, 0.0).into(),
                (1.0, 1.0).into(),
                (2.0, 2.0).into(),
                (3.0, 3.0).into(),
                (4.0, 4.0).into(),
                (5.0, 5.0).into(),
                (6.0, 6.0).into(),
                (7.0, 7.0).into(),
            ],
            coords: vec![
                (0.0, 0.0).into(),
                (10.0, 10.0).into(),
                (20.0, 20.0).into(),
                (30.0, 30.0).into(),
                (40.0, 40.0).into(),
                (50.0, 50.0).into(),
                (60.0, 60.0).into(),
                (70.0, 70.0).into(),
            ],
            expected_must_encode: HashSet::from([]),
        }
    }

    /// Another case with no must-encode items, this time a real one from a fontmake-rs test
    /// that was failing
    fn iup_scenario7() -> IupScenario {
        IupScenario {
            coords: vec![
                (242.0, 111.0),
                (314.0, 111.0),
                (314.0, 317.0),
                (513.0, 317.0),
                (513.0, 388.0),
                (314.0, 388.0),
                (314.0, 595.0),
                (242.0, 595.0),
                (242.0, 388.0),
                (43.0, 388.0),
                (43.0, 317.0),
                (242.0, 317.0),
                (0.0, 0.0),
                (557.0, 0.0),
                (0.0, 0.0),
                (0.0, 0.0),
            ]
            .into_iter()
            .map(|c| c.into())
            .collect(),
            deltas: vec![
                (-10.0, 0.0),
                (25.0, 0.0),
                (25.0, -18.0),
                (15.0, -18.0),
                (15.0, 18.0),
                (25.0, 18.0),
                (25.0, 1.0),
                (-10.0, 1.0),
                (-10.0, 18.0),
                (0.0, 18.0),
                (0.0, -18.0),
                (-10.0, -18.0),
                (0.0, 0.0),
                (15.0, 0.0),
                (0.0, 0.0),
                (0.0, 0.0),
            ]
            .into_iter()
            .map(|c| c.into())
            .collect(),
            expected_must_encode: HashSet::from([0]),
        }
    }

    /// From a fontmake-rs test that was failing (achieved invalid state)
    fn iup_scenario8() -> IupScenario {
        IupScenario {
            coords: vec![
                (131.0, 430.0),
                (131.0, 350.0),
                (470.0, 350.0),
                (470.0, 430.0),
                (131.0, 330.0),
            ]
            .into_iter()
            .map(|c| c.into())
            .collect(),
            deltas: vec![
                (-15.0, 115.0),
                (-15.0, 30.0),
                (124.0, 30.0),
                (124.0, 115.0),
                (-39.0, 26.0),
            ]
            .into_iter()
            .map(|c| c.into())
            .collect(),
            expected_must_encode: HashSet::from([0, 4]),
        }
    }

    #[test]
    fn iup_test_scenario01_must_encode() {
        iup_scenario1().assert_must_encode();
    }

    #[test]
    fn iup_test_scenario02_must_encode() {
        iup_scenario2().assert_must_encode();
    }

    #[test]
    fn iup_test_scenario03_must_encode() {
        iup_scenario3().assert_must_encode();
    }

    #[test]
    fn iup_test_scenario04_must_encode() {
        iup_scenario4().assert_must_encode();
    }

    #[test]
    fn iup_test_scenario05_must_encode() {
        iup_scenario5().assert_must_encode();
    }

    #[test]
    fn iup_test_scenario06_must_encode() {
        iup_scenario6().assert_must_encode();
    }

    #[test]
    fn iup_test_scenario07_must_encode() {
        iup_scenario7().assert_must_encode();
    }

    #[test]
    fn iup_test_scenario08_must_encode() {
        iup_scenario8().assert_must_encode();
    }

    #[test]
    fn iup_test_scenario01_optimize() {
        iup_scenario1().assert_optimize_dp();
    }

    #[test]
    fn iup_test_scenario02_optimize() {
        iup_scenario2().assert_optimize_dp();
    }

    #[test]
    fn iup_test_scenario03_optimize() {
        iup_scenario3().assert_optimize_dp();
    }

    #[test]
    fn iup_test_scenario04_optimize() {
        iup_scenario4().assert_optimize_dp();
    }

    #[test]
    fn iup_test_scenario05_optimize() {
        iup_scenario5().assert_optimize_dp();
    }

    #[test]
    fn iup_test_scenario06_optimize() {
        iup_scenario6().assert_optimize_dp();
    }

    #[test]
    fn iup_test_scenario07_optimize() {
        iup_scenario7().assert_optimize_dp();
    }

    #[test]
    fn iup_test_scenario08_optimize() {
        iup_scenario8().assert_optimize_dp();
    }

    #[test]
    fn iup_test_scenario01_optimize_contour() {
        iup_scenario1().assert_optimize_contour();
    }

    #[test]
    fn iup_test_scenario02_optimize_contour() {
        iup_scenario2().assert_optimize_contour();
    }

    #[test]
    fn iup_test_scenario03_optimize_contour() {
        iup_scenario3().assert_optimize_contour();
    }

    #[test]
    fn iup_test_scenario04_optimize_contour() {
        iup_scenario4().assert_optimize_contour();
    }

    #[test]
    fn iup_test_scenario05_optimize_contour() {
        iup_scenario5().assert_optimize_contour();
    }

    #[test]
    fn iup_test_scenario06_optimize_contour() {
        iup_scenario6().assert_optimize_contour();
    }

    #[test]
    fn iup_test_scenario07_optimize_contour() {
        iup_scenario7().assert_optimize_contour();
    }

    #[test]
    fn iup_test_scenario08_optimize_contour() {
        iup_scenario8().assert_optimize_contour();
    }

    // a helper to let us match the existing test format
    fn make_vec_of_options(deltas: &[GlyphDelta]) -> Vec<(usize, Option<Vec2>)> {
        deltas
            .iter()
            .enumerate()
            .map(|(i, delta)| {
                (
                    i,
                    delta
                        .required
                        .then_some(Vec2::new(delta.x as _, delta.y as _)),
                )
            })
            .collect()
    }

    #[test]
    fn iup_delta_optimize_oswald_glyph_two() {
        // https://github.com/googlefonts/fontations/issues/564
        let deltas: Vec<_> = vec![
            (0.0, 0.0),
            (41.0, 0.0),
            (41.0, 41.0),
            (60.0, 41.0),
            (22.0, -22.0),
            (27.0, -15.0),
            (38.0, -4.0),
            (44.0, 2.0),
            (44.0, -1.0),
            (44.0, 2.0),
            (29.0, 4.0),
            (18.0, 4.0),
            (9.0, 4.0),
            (-4.0, -4.0),
            (-11.0, -12.0),
            (-11.0, -10.0),
            (-11.0, -25.0),
            (44.0, -25.0),
            (44.0, -12.0),
            (44.0, -20.0),
            (39.0, -38.0),
            (26.0, -50.0),
            (16.0, -50.0),
            (-5.0, -50.0),
            (-13.0, -21.0),
            (-13.0, 1.0),
            (-13.0, 11.0),
            (-13.0, 16.0),
            (-13.0, 16.0),
            (-12.0, 19.0),
            (0.0, 42.0),
            (0.0, 0.0),
            (36.0, 0.0),
            (0.0, 0.0),
            (0.0, 0.0),
        ]
        .into_iter()
        .map(|c| c.into())
        .collect();
        let coords: Vec<_> = vec![
            (41.0, 0.0),
            (423.0, 0.0),
            (423.0, 90.0),
            (167.0, 90.0),
            (353.0, 374.0),
            (377.0, 410.0),
            (417.0, 478.0),
            (442.0, 556.0),
            (442.0, 608.0),
            (442.0, 706.0),
            (346.0, 817.0),
            (248.0, 817.0),
            (176.0, 817.0),
            (89.0, 759.0),
            (50.0, 654.0),
            (50.0, 581.0),
            (50.0, 553.0),
            (157.0, 553.0),
            (157.0, 580.0),
            (157.0, 619.0),
            (173.0, 687.0),
            (215.0, 729.0),
            (253.0, 729.0),
            (298.0, 729.0),
            (334.0, 665.0),
            (334.0, 609.0),
            (334.0, 564.0),
            (309.0, 495.0),
            (270.0, 433.0),
            (247.0, 397.0),
            (41.0, 76.0),
            (0.0, 0.0),
            (478.0, 0.0),
            (0.0, 0.0),
            (0.0, 0.0),
        ]
        .into_iter()
        .map(|c| c.into())
        .collect();

        // using fonttools varLib's default tolerance
        let tolerance = 0.5;
        // a single contour, minus the phantom points
        let contour_ends = vec![coords.len() - 1 - 4];

        let result = iup_delta_optimize(deltas.clone(), coords, tolerance, &contour_ends).unwrap();
        assert_eq!(result.len(), deltas.len());
        let result = make_vec_of_options(&result);

        assert_eq!(
            result,
            // this is what fonttools iup_delta_optimize returns and what we want to match
            vec![
                (0, None),
                (1, Some(Vec2 { x: 41.0, y: 0.0 })),
                (2, None),
                (3, Some(Vec2 { x: 60.0, y: 41.0 })),
                (4, Some(Vec2 { x: 22.0, y: -22.0 })),
                (5, Some(Vec2 { x: 27.0, y: -15.0 })),
                (6, Some(Vec2 { x: 38.0, y: -4.0 })),
                (7, Some(Vec2 { x: 44.0, y: 2.0 })),
                (8, Some(Vec2 { x: 44.0, y: -1.0 })),
                (9, Some(Vec2 { x: 44.0, y: 2.0 })),
                (10, Some(Vec2 { x: 29.0, y: 4.0 })),
                (11, Some(Vec2 { x: 18.0, y: 4.0 })),
                (12, Some(Vec2 { x: 9.0, y: 4.0 })),
                (13, Some(Vec2 { x: -4.0, y: -4.0 })),
                (14, Some(Vec2 { x: -11.0, y: -12.0 })),
                (15, Some(Vec2 { x: -11.0, y: -10.0 })),
                (16, None),
                (17, Some(Vec2 { x: 44.0, y: -25.0 })),
                (18, Some(Vec2 { x: 44.0, y: -12.0 })),
                (19, Some(Vec2 { x: 44.0, y: -20.0 })),
                (20, Some(Vec2 { x: 39.0, y: -38.0 })),
                (21, Some(Vec2 { x: 26.0, y: -50.0 })),
                (22, Some(Vec2 { x: 16.0, y: -50.0 })),
                (23, Some(Vec2 { x: -5.0, y: -50.0 })),
                (24, Some(Vec2 { x: -13.0, y: -21.0 })),
                (25, Some(Vec2 { x: -13.0, y: 1.0 })),
                (26, Some(Vec2 { x: -13.0, y: 11.0 })),
                (27, Some(Vec2 { x: -13.0, y: 16.0 })),
                (28, Some(Vec2 { x: -13.0, y: 16.0 })),
                (29, None),
                (30, Some(Vec2 { x: 0.0, y: 42.0 })),
                (31, None),
                (32, Some(Vec2 { x: 36.0, y: 0.0 })),
                (33, None),
                (34, None),
            ]
        )
    }

    #[test]
    fn iup_delta_optimize_gs_glyph_uppercase_c() {
        // https://github.com/googlefonts/fontations/issues/571
        let deltas: Vec<_> = vec![
            (2.0, 0.0),
            (4.0, 0.0),
            (8.0, -1.0),
            (10.0, -1.0),
            (10.0, 0.0),
            (-14.0, 25.0),
            (-8.0, 34.0),
            (-3.0, 38.0),
            (-5.0, 35.0),
            (-7.0, 35.0),
            (6.0, 35.0),
            (22.0, 27.0),
            (29.0, 11.0),
            (29.0, -1.0),
            (29.0, -13.0),
            (22.0, -29.0),
            (8.0, -37.0),
            (-3.0, -37.0),
            (0.0, -37.0),
            (1.0, -43.0),
            (-7.0, -41.0),
            (-19.0, -28.0),
            (8.0, 0.0),
            (8.0, 0.0),
            (6.0, 0.0),
            (4.0, 0.0),
            (2.0, 0.0),
            (0.0, 0.0),
            (-5.0, 0.0),
            (-8.0, 0.0),
            (-10.0, 0.0),
            (-10.0, 0.0),
            (-8.0, 0.0),
            (-5.0, 0.0),
            (-1.0, 0.0),
            (0.0, 0.0),
            (0.0, 0.0),
            (0.0, 0.0),
            (0.0, 0.0),
        ]
        .into_iter()
        .map(|c| c.into())
        .collect();

        let coords: Vec<_> = vec![
            (416.0, -16.0),
            (476.0, -16.0),
            (581.0, 17.0),
            (668.0, 75.0),
            (699.0, 112.0),
            (637.0, 172.0),
            (609.0, 139.0),
            (542.0, 91.0),
            (463.0, 65.0),
            (416.0, 65.0),
            (339.0, 65.0),
            (209.0, 137.0),
            (131.0, 269.0),
            (131.0, 358.0),
            (131.0, 448.0),
            (209.0, 579.0),
            (339.0, 651.0),
            (416.0, 651.0),
            (458.0, 651.0),
            (529.0, 631.0),
            (590.0, 589.0),
            (617.0, 556.0),
            (678.0, 615.0),
            (646.0, 652.0),
            (566.0, 704.0),
            (471.0, 732.0),
            (416.0, 732.0),
            (337.0, 732.0),
            (202.0, 675.0),
            (101.0, 574.0),
            (45.0, 438.0),
            (45.0, 279.0),
            (101.0, 142.0),
            (202.0, 41.0),
            (337.0, -16.0),
            (0.0, 0.0),
            (741.0, 0.0),
            (0.0, 0.0),
            (0.0, 0.0),
        ]
        .into_iter()
        .map(|c| c.into())
        .collect();

        // using fonttools varLib's default tolerance
        let tolerance = 0.5;
        // a single contour, minus the phantom points
        let contour_ends = vec![coords.len() - 1 - 4];

        let result = iup_delta_optimize(deltas.clone(), coords, tolerance, &contour_ends).unwrap();
        let result = make_vec_of_options(&result);

        assert_eq!(
            result,
            vec![
                (0, None),
                (1, Some(Vec2 { x: 4.0, y: 0.0 })),
                (2, Some(Vec2 { x: 8.0, y: -1.0 })),
                (3, Some(Vec2 { x: 10.0, y: -1.0 })),
                (4, Some(Vec2 { x: 10.0, y: 0.0 })),
                (5, Some(Vec2 { x: -14.0, y: 25.0 })),
                (6, Some(Vec2 { x: -8.0, y: 34.0 })),
                (7, Some(Vec2 { x: -3.0, y: 38.0 })),
                (8, Some(Vec2 { x: -5.0, y: 35.0 })),
                (9, Some(Vec2 { x: -7.0, y: 35.0 })),
                (10, Some(Vec2 { x: 6.0, y: 35.0 })),
                (11, Some(Vec2 { x: 22.0, y: 27.0 })),
                (12, Some(Vec2 { x: 29.0, y: 11.0 })),
                (13, None),
                (14, Some(Vec2 { x: 29.0, y: -13.0 })),
                (15, Some(Vec2 { x: 22.0, y: -29.0 })),
                (16, Some(Vec2 { x: 8.0, y: -37.0 })),
                (17, Some(Vec2 { x: -3.0, y: -37.0 })),
                (18, Some(Vec2 { x: 0.0, y: -37.0 })),
                (19, Some(Vec2 { x: 1.0, y: -43.0 })),
                (20, Some(Vec2 { x: -7.0, y: -41.0 })),
                (21, Some(Vec2 { x: -19.0, y: -28.0 })),
                (22, Some(Vec2 { x: 8.0, y: 0.0 })),
                (23, Some(Vec2 { x: 8.0, y: 0.0 })),
                (24, None),
                (25, Some(Vec2 { x: 4.0, y: 0.0 })),
                (26, None),
                (27, None),
                (28, None),
                (29, None),
                (30, None),
                (31, Some(Vec2 { x: -10.0, y: 0.0 })),
                (32, None),
                (33, None),
                (34, None),
                (35, None),
                (36, None),
                (37, None),
                (38, None),
            ]
        )
    }

    // https://github.com/googlefonts/fontations/issues/662
    // this bug seems to be triggered when deltas and coords are equal?
    // test case based on s.cp in googlesans
    #[test]
    fn bug_662() {
        let deltas = vec![
            Vec2 { x: 73.0, y: 0.0 },
            Vec2 { x: 158.0, y: 448.0 },
            Vec2 { x: 154.0, y: 448.0 },
            Vec2 { x: 0.0, y: 0.0 },
            Vec2 { x: 765.0, y: 0.0 },
            Vec2 { x: 0.0, y: 0.0 },
            Vec2 { x: 0.0, y: 0.0 },
        ];

        let coords = [
            (73.0, 0.0),
            (158.0, 448.0),
            (154.0, 448.0),
            (0.0, 0.0),
            (765.0, 0.0),
            (0.0, 0.0),
            (0.0, 0.0),
        ]
        .into_iter()
        .map(|(x, y)| Point::new(x, y))
        .collect::<Vec<_>>();

        let contour_ends = vec![2];

        iup_delta_optimize(deltas, coords, 0.5, &contour_ends).unwrap();
    }

    // https://github.com/googlefonts/fontc/issues/1116
    // This test case triggered an error caused by a slight semantic difference
    // that was introduced from us transitioning to unsigned values in porting
    // the Python original.
    #[test]
    fn bug_fontc_1116() {
        let mut deltas = vec![
            Vec2 { x: 0.0, y: 0.0 },
            Vec2 { x: 0.0, y: 3.0 },
            Vec2 { x: 0.0, y: 0.0 },
            Vec2 { x: 0.0, y: 0.0 },
            Vec2 { x: 2.0, y: 3.0 },
            Vec2 { x: 2.0, y: 0.0 },
            Vec2 { x: 1.0, y: 3.0 },
            Vec2 { x: 1.0, y: 0.0 },
            Vec2 { x: 0.0, y: 0.0 },
            Vec2 { x: 0.0, y: 1.0 },
            Vec2 { x: 1.0, y: 0.0 },
            Vec2 { x: 1.0, y: 0.0 },
        ];
        let mut coords = [
            (0.0, 0.0),
            (0.0, 3.0),
            (0.0, 0.0),
            (0.0, 0.0),
            (2.0, 3.0),
            (2.0, 0.0),
            (1.0, 3.0),
            (1.0, 0.0),
            (0.0, 0.0),
            (0.0, 1.0),
            (1.0, 0.0),
            (1.0, 0.0),
        ]
        .into_iter()
        .map(|(x, y)| Point::new(x, y))
        .collect::<Vec<_>>();

        iup_contour_optimize(&mut deltas, &mut coords, 0.5).unwrap();
    }
}
