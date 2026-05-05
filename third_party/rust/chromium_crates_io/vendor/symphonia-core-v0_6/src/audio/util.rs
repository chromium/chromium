// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::ops::{Bound, Range, RangeBounds};

use crate::audio::conv::{FromSample, IntoSample};
use crate::audio::sample::{Sample, SampleBytes};

/// Get a pair of planes.
pub fn plane_pair_by_buffer_index<S: Sample>(
    planes: &[Vec<S>],
    range: Range<usize>,
    idx0: usize,
    idx1: usize,
) -> Option<(&[S], &[S])> {
    // Both channels in the pair must be unique.
    assert!(idx0 != idx1, "buffer indicies cannot be the same");

    // Neither of the plane indicies may exceed the number of planes.
    let num_planes = planes.len();

    if idx0 < num_planes && idx1 < num_planes {
        if idx0 < idx1 {
            let (a, b) = planes.split_at(idx1);

            Some((&a[idx0][range.clone()], &b[0][range.clone()]))
        }
        else {
            let (a, b) = planes.split_at(idx0);

            Some((&b[0][range.clone()], &a[idx1][range.clone()]))
        }
    }
    else {
        // Either one or both plane indicies are out of range.
        None
    }
}

pub fn plane_pair_by_buffer_index_mut<S: Sample>(
    planes: &mut [Vec<S>],
    range: Range<usize>,
    idx0: usize,
    idx1: usize,
) -> Option<(&mut [S], &mut [S])> {
    // Both channels in the pair must be unique.
    assert!(idx0 != idx1, "buffer indicies cannot be the same");

    // Neither of the plane indicies may exceed the number of planes.
    let num_planes = planes.len();

    if idx0 < num_planes && idx1 < num_planes {
        if idx0 < idx1 {
            let (a, b) = planes.split_at_mut(idx1);

            Some((&mut a[idx0][range.clone()], &mut b[0][range.clone()]))
        }
        else {
            let (a, b) = planes.split_at_mut(idx0);

            Some((&mut b[0][range.clone()], &mut a[idx1][range.clone()]))
        }
    }
    else {
        // Either one or both plane indicies are out of range.
        None
    }
}

/// Get a sub-range of the range `bound` specified by relative range, `range`.
///
/// Panics if the start or end indicies are out-of-bounds, or overflows the underlying `usize`.
pub fn get_sub_range<R>(range: R, bound: &Range<usize>) -> Range<usize>
where
    R: RangeBounds<usize>,
{
    const fn panic_start_index_overflow() -> usize {
        panic!("attempted to index audio buffer slice from after maximum usize")
    }

    const fn panic_end_index_overflow() -> usize {
        panic!("attempted to index audio buffer slice slice up to maximum usize")
    }

    // The length of the bound.
    let len = bound.len();

    // Compute start index relative to bound.
    let start = match range.start_bound() {
        Bound::Included(&start) => start,
        Bound::Excluded(start) => start.checked_add(1).unwrap_or_else(panic_start_index_overflow),
        Bound::Unbounded => 0,
    };

    // Compute end index relative to bound.
    let end = match range.end_bound() {
        Bound::Included(end) => end.checked_add(1).unwrap_or_else(panic_end_index_overflow),
        Bound::Excluded(&end) => end,
        Bound::Unbounded => len,
    };

    // Validate the sub-range indicies form a valid range and do not exceed the bounding range.
    assert!(start <= end, "audio buffer slice index start {start} is beyond end {end}");
    assert!(end <= len, "audio buffer slice end {end} index is out of range {len}");

    // Compute the absolute start index of the sub-range in the bounding range.
    let start = start.checked_add(bound.start).unwrap_or_else(panic_start_index_overflow);

    // Compute the absolute end index of the sub-range in the bounding range.
    let end = end.checked_add(bound.start).unwrap_or_else(panic_end_index_overflow);

    Range { start, end }
}

#[inline(never)]
pub fn copy_to_slice_interleaved<Sin, Sout, Src, Dst>(
    src: &[Src],
    bound: Range<usize>,
    mut dst: Dst,
) where
    Sin: Sample,
    Sout: Sample + FromSample<Sin>,
    Src: AsRef<[Sin]>,
    Dst: AsMut<[Sout]>,
{
    let dst = dst.as_mut();

    let num_planes = src.len();

    assert!(
        dst.len() == num_planes * bound.len(),
        "destination slice does not match number of samples"
    );

    let mut i = 0;

    // The pattern below may be extended to copy planes in triplets to further improve performance.
    // However, the binary will become bloated because this function is heavily parameterized.

    // Copy planes in pairs when possible, it is substantially faster.
    while num_planes - i > 1 {
        let src0 = &src[i + 0].as_ref()[bound.clone()];
        let src1 = &src[i + 1].as_ref()[bound.clone()];

        for ((&s0, &s1), d) in src0.iter().zip(src1).zip(dst.chunks_exact_mut(num_planes)) {
            d[i + 0] = s0.into_sample();
            d[i + 1] = s1.into_sample();
        }

        i += 2
    }

    // Copy the final plane.
    while i < num_planes {
        let src0 = &src[i].as_ref()[bound.clone()];

        for (&s0, d) in src0.iter().zip(dst.chunks_exact_mut(num_planes)) {
            d[i] = s0.into_sample();
        }

        i += 1;
    }
}

#[inline(never)]
pub fn copy_from_slice_interleaved<Sin, Sout, Src, Dst>(
    src: Src,
    bound: Range<usize>,
    dst: &mut [Dst],
) where
    Sin: Sample,
    Sout: Sample + FromSample<Sin>,
    Src: AsRef<[Sin]>,
    Dst: AsMut<[Sout]>,
{
    let src = src.as_ref();

    let num_planes = dst.len();

    assert!(
        src.len() == num_planes * bound.len(),
        "source slice length does not match expected number of samples"
    );

    let mut i = 0;

    // The pattern below may be extended to copy planes in triplets to further improve performance.
    // However, the binary will become bloated because this function is heavily parameterized.

    // Copy planes in pairs when possible, it is substantially faster.
    while num_planes - i > 1 {
        let (l, r) = dst.split_at_mut(i + 1);

        let dst0 = &mut l.last_mut().unwrap().as_mut()[bound.clone()];
        let dst1 = &mut r.first_mut().unwrap().as_mut()[bound.clone()];

        for ((d0, d1), s) in dst0.iter_mut().zip(dst1).zip(src.chunks_exact(num_planes)) {
            *d0 = s[i + 0].into_sample();
            *d1 = s[i + 1].into_sample();
        }

        i += 2
    }

    // Copy the final plane.
    while i < num_planes {
        let dst0 = &mut dst[i].as_mut()[bound.clone()];

        for (d0, s) in dst0.iter_mut().zip(src.chunks_exact(num_planes)) {
            *d0 = s[i].into_sample();
        }

        i += 1;
    }
}

#[inline(always)]
pub fn convert<Sin: Sample, Sout: SampleBytes + FromSample<Sin>>(s: Sin) -> Sout {
    s.into_sample()
}

#[inline(always)]
pub fn identity<Sin: Sample + SampleBytes>(s: Sin) -> Sin {
    s
}

#[inline(never)]
pub fn copy_to_slice<Sin, Sout>(src: &[Sin], dst: &mut [Sout])
where
    Sin: Sample,
    Sout: Sample + FromSample<Sin>,
{
    assert!(src.len() == dst.len(), "destination slice does not match number of samples");

    for (d, s) in dst.iter_mut().zip(src) {
        *d = (*s).into_sample();
    }
}

#[inline(never)]
pub fn copy_bytes_interleaved<Sout, Sin, Src, F, Dst>(
    src: &[Src],
    bound: Range<usize>,
    f: F,
    mut dst: Dst,
) where
    Sout: SampleBytes,
    Sin: Sample,
    Src: AsRef<[Sin]>,
    F: Fn(Sin) -> Sout,
    Dst: AsMut<[u8]>,
{
    // Safety: This will never panic because `Sin::RawType` is bound by the `ByteAligned` marker
    // trait. This trait is only implemented for types that have 1-byte alignment. Therefore,
    // since `RawType` can only be assigned one of these types, `RawType` must also have 1-byte
    // alignment. In practice, this means that `RawType` will always be an array of `u8`. Therefore,
    // it is safe to cast between `&[u8]` and `&[RawType]`.
    let dst: &mut [Sout::RawType] = bytemuck::cast_slice_mut(dst.as_mut());

    let num_planes = src.len();

    // Check destination length after casting since the length could change per documentation.
    assert!(
        dst.len() == num_planes * bound.len(),
        "destination slice does not match number of samples"
    );

    let mut i = 0;

    // The pattern below may be extended to copy planes in triplets to further improve performance.
    // However, the binary will become bloated because this function is heavily parameterized.

    // Copy planes in pairs when possible, it is substantially faster.
    while num_planes - i > 1 {
        let src0 = &src[i + 0].as_ref()[bound.clone()];
        let src1 = &src[i + 1].as_ref()[bound.clone()];

        for ((&s0, &s1), d) in src0.iter().zip(src1).zip(dst.chunks_exact_mut(num_planes)) {
            d[i + 0] = f(s0).to_ne_sample_bytes();
            d[i + 1] = f(s1).to_ne_sample_bytes();
        }

        i += 2
    }

    // Copy the final plane.
    while i < num_planes {
        let src0 = &src[i].as_ref()[bound.clone()];

        for (&s0, d) in src0.iter().zip(dst.chunks_exact_mut(num_planes)) {
            d[i] = f(s0).to_ne_sample_bytes();
        }

        i += 1;
    }
}

#[inline(never)]
pub fn copy_bytes_planar<Sout, Sin, Src, F, Dst>(
    src: &[Src],
    bound: Range<usize>,
    f: F,
    dst: &mut [Dst],
) where
    Sout: SampleBytes,
    Sin: Sample,
    Src: AsRef<[Sin]>,
    F: Fn(Sin) -> Sout,
    Dst: AsMut<[u8]>,
{
    assert!(dst.len() == src.len(), "expected {} destination slices", src.len());

    for (src, dst) in src.iter().zip(dst) {
        let src = &src.as_ref()[bound.clone()];

        // Safety: See note in copy_bytes_interleaved.
        let dst: &mut [Sout::RawType] = bytemuck::cast_slice_mut(dst.as_mut());

        // Check destination length after casting since the length could change per documentation.
        assert!(src.len() == dst.len(), "destination slice does not match number of samples");

        for (&s, d) in src.iter().zip(dst) {
            *d = f(s).to_ne_sample_bytes();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::copy_from_slice_interleaved;

    #[test]
    fn verify_copy_from_slice_interleaved() {
        let src = vec![
            0.00, -1.0, 0.5, // 1st frame
            0.25, -0.75, 0.25, // 2nd frame
            0.50, -0.50, 0.0, // 3rd frame
            0.75, -0.25, -0.25, // 4th frame
            1.00, 0.00, -0.5, // 5th frame
        ];

        // Copy to entire destination slice.
        {
            let mut dst = vec![vec![0.0; 5]; 3];
            copy_from_slice_interleaved(&src, 0..5, &mut dst);
            assert_eq!(dst[0], [0.0, 0.25, 0.5, 0.75, 1.0]);
            assert_eq!(dst[1], [-1.0, -0.75, -0.5, -0.25, 0.0]);
            assert_eq!(dst[2], [0.5, 0.25, 0.0, -0.25, -0.5]);
        }

        // Copy to sub-slice of destination slice.
        {
            let mut dst = vec![vec![0.0; 5]; 3];
            copy_from_slice_interleaved(&src[3..12], 1..4, &mut dst);
            assert_eq!(dst[0], [0.0, 0.25, 0.5, 0.75, 0.0]);
            assert_eq!(dst[1], [0.0, -0.75, -0.5, -0.25, 0.0]);
            assert_eq!(dst[2], [0.0, 0.25, 0.0, -0.25, 0.0]);
        }
    }
}
