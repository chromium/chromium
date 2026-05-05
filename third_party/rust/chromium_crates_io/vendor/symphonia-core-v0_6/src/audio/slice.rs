// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::ops::{Range, RangeBounds};

use crate::audio::conv::FromSample;
use crate::audio::sample::{Sample, SampleBytes};

use super::util::*;
use super::{
    Audio, AudioBytes, AudioMut, AudioPlanes, AudioPlanesMut, AudioSpec, Interleaved, Position,
};

/// An immutable slice of planar audio.
pub struct AudioSlice<'a, S: Sample> {
    spec: &'a AudioSpec,
    planes: &'a [Vec<S>],
    range: Range<usize>,
}

impl<'a, S: Sample> AudioSlice<'a, S> {
    pub(super) fn new(spec: &'a AudioSpec, planes: &'a [Vec<S>], range: Range<usize>) -> Self {
        AudioSlice { spec, planes, range }
    }

    /// Get an immutable sub-slice of this slice over `range`.
    pub fn slice<R: RangeBounds<usize>>(&'a self, range: R) -> AudioSlice<'a, S> {
        AudioSlice::new(self.spec, self.planes, get_sub_range(range, &self.range))
    }
}

impl<S: Sample> Audio<S> for AudioSlice<'_, S> {
    fn spec(&self) -> &AudioSpec {
        self.spec
    }

    fn num_planes(&self) -> usize {
        self.planes.len()
    }

    fn is_empty(&self) -> bool {
        self.range.is_empty()
    }

    fn frames(&self) -> usize {
        self.range.len()
    }

    fn plane(&self, idx: usize) -> Option<&[S]> {
        self.planes.get(idx).map(|plane| &plane[self.range.clone()])
    }

    fn plane_pair(&self, idx0: usize, idx1: usize) -> Option<(&[S], &[S])> {
        plane_pair_by_buffer_index(self.planes, self.range.clone(), idx0, idx1)
    }

    fn iter_planes(&self) -> AudioPlanes<'_, S> {
        AudioPlanes::new(self.planes, self.range.clone())
    }

    fn iter_interleaved(&self) -> Interleaved<'_, S> {
        Interleaved::new(self.planes, self.range.clone())
    }

    fn copy_to_slice_interleaved<Sout, Dst>(&self, dst: Dst)
    where
        Sout: Sample + FromSample<S>,
        Dst: AsMut<[Sout]>,
    {
        // Dispatch to common helper.
        copy_to_slice_interleaved(self.planes, self.range.clone(), dst)
    }
}

impl<S: Sample + SampleBytes> AudioBytes<S> for AudioSlice<'_, S> {
    fn copy_bytes_interleaved_as<Sout, Dst>(&self, dst: Dst)
    where
        Sout: SampleBytes + FromSample<S>,
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_interleaved::<Sout, _, _, _, _>(self.planes, self.range.clone(), convert, dst)
    }

    fn copy_bytes_planar_as<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: SampleBytes + FromSample<S>,
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_planar::<Sout, _, _, _, _>(self.planes, self.range.clone(), convert, dst)
    }

    fn copy_bytes_interleaved<Dst>(&self, dst: Dst)
    where
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_interleaved::<S, _, _, _, _>(self.planes, self.range.clone(), identity, dst)
    }

    fn copy_bytes_planar<Dst>(&self, dst: &mut [Dst])
    where
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_planar::<S, _, _, _, _>(self.planes, self.range.clone(), identity, dst)
    }
}

impl<S: Sample> std::ops::Index<Position> for AudioSlice<'_, S> {
    type Output = [S];

    fn index(&self, index: Position) -> &Self::Output {
        self.plane_by_position(index).unwrap()
    }
}

impl<S: Sample> std::ops::Index<usize> for AudioSlice<'_, S> {
    type Output = [S];

    fn index(&self, index: usize) -> &Self::Output {
        self.plane(index).unwrap()
    }
}

/// A mutable slice of planar audio.
pub struct AudioSliceMut<'a, S: Sample> {
    spec: &'a AudioSpec,
    planes: &'a mut [Vec<S>],
    range: Range<usize>,
}

impl<'a, S: Sample> AudioSliceMut<'a, S> {
    pub(super) fn new(spec: &'a AudioSpec, planes: &'a mut [Vec<S>], range: Range<usize>) -> Self {
        AudioSliceMut { spec, planes, range }
    }

    /// Get an immutable sub-slice of this slice over `range`.
    pub fn slice<R: RangeBounds<usize>>(&'a self, range: R) -> AudioSlice<'a, S> {
        AudioSlice::new(self.spec, self.planes, get_sub_range(range, &self.range))
    }

    /// Get a mutable sub-slice of this slice over `range`.
    pub fn slice_mut<R: RangeBounds<usize>>(&'a mut self, range: R) -> AudioSliceMut<'a, S> {
        AudioSliceMut::new(self.spec, self.planes, get_sub_range(range, &self.range))
    }
}

impl<S: Sample> Audio<S> for AudioSliceMut<'_, S> {
    fn spec(&self) -> &AudioSpec {
        self.spec
    }

    fn num_planes(&self) -> usize {
        self.planes.len()
    }

    fn is_empty(&self) -> bool {
        self.range.is_empty()
    }

    fn frames(&self) -> usize {
        self.range.len()
    }

    fn plane(&self, idx: usize) -> Option<&[S]> {
        self.planes.get(idx).map(|plane| &plane[self.range.clone()])
    }

    fn plane_pair(&self, idx0: usize, idx1: usize) -> Option<(&[S], &[S])> {
        plane_pair_by_buffer_index(self.planes, self.range.clone(), idx0, idx1)
    }

    fn iter_planes(&self) -> AudioPlanes<'_, S> {
        AudioPlanes::new(self.planes, self.range.clone())
    }

    fn iter_interleaved(&self) -> Interleaved<'_, S> {
        Interleaved::new(self.planes, self.range.clone())
    }

    fn copy_to_slice_interleaved<Sout, Dst>(&self, dst: Dst)
    where
        Sout: Sample + FromSample<S>,
        Dst: AsMut<[Sout]>,
    {
        // Dispatch to common helper.
        copy_to_slice_interleaved(self.planes, self.range.clone(), dst)
    }
}

impl<S: Sample> AudioMut<S> for AudioSliceMut<'_, S> {
    fn plane_mut(&mut self, idx: usize) -> Option<&mut [S]> {
        self.planes.get_mut(idx).map(|plane| &mut plane[self.range.clone()])
    }

    fn plane_pair_mut(&mut self, idx0: usize, idx1: usize) -> Option<(&mut [S], &mut [S])> {
        plane_pair_by_buffer_index_mut(self.planes, self.range.clone(), idx0, idx1)
    }

    fn iter_planes_mut(&mut self) -> AudioPlanesMut<'_, S> {
        AudioPlanesMut::new(self.planes, self.range.clone())
    }

    fn copy_from_slice_interleaved<Sin, Src>(&mut self, src: &Src)
    where
        Sin: Sample,
        S: Sample + FromSample<Sin>,
        Src: AsRef<[Sin]>,
    {
        // Dispatch to common helper.
        copy_from_slice_interleaved(src, self.range.clone(), self.planes);
    }
}

impl<S: Sample + SampleBytes> AudioBytes<S> for AudioSliceMut<'_, S> {
    fn copy_bytes_interleaved_as<Sout, Dst>(&self, dst: Dst)
    where
        Sout: SampleBytes + FromSample<S>,
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_interleaved::<Sout, _, _, _, _>(self.planes, self.range.clone(), convert, dst)
    }

    fn copy_bytes_planar_as<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: SampleBytes + FromSample<S>,
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_planar::<Sout, _, _, _, _>(self.planes, self.range.clone(), convert, dst)
    }

    fn copy_bytes_interleaved<Dst>(&self, dst: Dst)
    where
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_interleaved::<S, _, _, _, _>(self.planes, self.range.clone(), identity, dst)
    }

    fn copy_bytes_planar<Dst>(&self, dst: &mut [Dst])
    where
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_planar::<S, _, _, _, _>(self.planes, self.range.clone(), identity, dst)
    }
}

impl<S: Sample> std::ops::Index<Position> for AudioSliceMut<'_, S> {
    type Output = [S];

    fn index(&self, index: Position) -> &Self::Output {
        self.plane_by_position(index).unwrap()
    }
}

impl<S: Sample> std::ops::IndexMut<Position> for AudioSliceMut<'_, S> {
    fn index_mut(&mut self, index: Position) -> &mut Self::Output {
        self.plane_by_position_mut(index).unwrap()
    }
}

impl<S: Sample> std::ops::Index<usize> for AudioSliceMut<'_, S> {
    type Output = [S];

    fn index(&self, index: usize) -> &Self::Output {
        self.plane(index).unwrap()
    }
}

impl<S: Sample> std::ops::IndexMut<usize> for AudioSliceMut<'_, S> {
    fn index_mut(&mut self, index: usize) -> &mut Self::Output {
        self.plane_mut(index).unwrap()
    }
}
