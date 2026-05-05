// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::ops::{Deref, DerefMut, Range, RangeBounds};

use smallvec::SmallVec;

use crate::audio::conv::FromSample;
use crate::audio::sample::{Sample, SampleBytes};
use crate::errors::Result;

use super::{
    Audio, AudioBufferBytes, AudioBytes, AudioMut, AudioSlice, AudioSliceMut, AudioSpec,
    Interleaved, Position,
};
use super::{AudioPlanes, AudioPlanesMut, util::*};

/// The maximum number of audio plane slices that will be stored on the stack before storing the
/// slices on the heap.
const MAX_STACK_PLANE_SLICES: usize = 8; // Upto 7.1 audio.

/// A dynamically sized container for multi-channel planar audio for a known sample format.
///
/// An `AudioBuffer` is characterized by a total capacity, and an audio specification describing the
/// audio channels and sample rate. The capacity of an `AudioBuffer` is the fixed number of audio
/// frames the buffer may store without growing.
///
/// Note, `AudioBuffer` does not automatically grow its capacity. All increases in capacity must
/// be manually requested.
///
/// `AudioBuffer` stores each audio channel in separate audio planes. An audio plane is a
/// continguous vector of audio samples. The samples across all audio planes at any one particular
/// index form an audio frame. The order of audio planes, and thus the order of samples in an
/// audio frame, is a canonical order based on the channels mask.
///
/// ## Audio Plane Ordering
///
/// The order and indexing of the audio planes in an audio buffer is based on the channel
/// representation specified in the audio specification.
///
///   * If [`Channels::Positioned`](super::Channels::Positioned), the audio buffer contains one
///     audio plane for each bit set in the specified [`Position`](super::Position) bitmask. The
///     order of audio planes is based on the position bits set to `1` from the least-significant
///     bit (LSB) to most-significant bit (MSB) in the mask. This order is known as the canonical
///     channel order. The audio buffer may be indexed by a position mask containing exactly one
///     position, or a canonical index.
///   * If [`Channels::Discrete`](super::Channels::Discrete), the total number of discrete channels
///     (`n`) is specified. The audio buffer contains one audio plane for discrete channel indicies
///     `0..n`. The buffer may be indexed by a discrete channel index, `i`, in the range `0..n`.
///   * If [`Channels::Ambisonic`](super::Channels::Ambisonic), the Ambisonic order (`n`) is
///     specified. The audio buffer contains `m = (1 + n) ^ 2` audio planes, one for each Ambisonic
///     component. The order of audio planes is based on the Ambisonic Channel Number (ACN). The
///     audio buffer may be indexed by the ACN of the desired channel, `k`, in the range `0..m`.
///   * If [`Channels::Custom`](super::Channels::Custom), a list of `n`
///     [`ChannelLabel`](super::ChannelLabel)s is specified. Each channel label describes the
///     disposition of the channel. The audio buffer contains `n` audio planes in the same order
///     as specified in the list. The audio buffer may be indexed by an index, `i`, in the range
///     `0..n`.
///
/// ## Realtime Safety
///
/// Unless explictly noted, `AudioBuffer` never allocates after instantiation.
#[derive(Clone, Default)]
pub struct AudioBuffer<S: Sample> {
    spec: AudioSpec,
    planes: SmallVec<[Vec<S>; 3]>, // Keep on-stack upto 2.1 audio.
    num_frames: usize,
    capacity: usize,
}

impl<S: Sample> AudioBuffer<S> {
    /// Instantiate a new `AudioBuffer` using the specified signal specification and an
    /// initial `capacity` in number of samples.
    pub fn new(spec: AudioSpec, capacity: usize) -> Self {
        let num_channels = spec.channels().count();

        // As a matter of practicality, it is not possible to allocate more than usize::MAX bytes
        // of audio samples.
        assert!(
            capacity <= (usize::MAX / (std::mem::size_of::<S>() * num_channels)),
            "capacity too large"
        );

        // Create a vector containing silence for the audio plane.
        let mut planes = SmallVec::<[Vec<S>; 3]>::with_capacity(num_channels);

        planes.resize_with(num_channels, || vec![S::MID; capacity]);

        AudioBuffer { spec, planes, num_frames: 0, capacity }
    }

    /// Returns `true` if the `AudioBuffer` is unused.
    ///
    /// An unused `AudioBuffer` has either a capacity of 0, or no channels.
    pub fn is_unused(&self) -> bool {
        self.capacity == 0 || self.spec.channels.count() == 0
    }

    /// Gets the total capacity of the buffer. The capacity is the maximum number of audio frames
    /// the buffer can store.
    pub fn capacity(&self) -> usize {
        self.capacity
    }

    /// Clears all audio frames.
    pub fn clear(&mut self) {
        self.num_frames = 0;
    }

    /// Grows the capacity of the buffer if the new capacity is larger than the current capacity.
    ///
    /// # Realtime Safety
    ///
    /// This function will allocate if `new_capacity` exceeds the current capacity.
    pub fn grow_capacity(&mut self, new_capacity: usize) {
        if self.capacity < new_capacity {
            for plane in &mut self.planes {
                plane.resize(new_capacity, S::MID);
            }
            self.capacity = new_capacity;
        }
    }

    /// Get an immutable slice of the buffer over `range`.
    pub fn slice<R: RangeBounds<usize>>(&self, range: R) -> AudioSlice<'_, S> {
        let bound = 0..self.num_frames;

        AudioSlice::new(&self.spec, &self.planes, get_sub_range(range, &bound))
    }

    /// Get a mutable slice of the buffer over `range`.
    pub fn slice_mut<R: RangeBounds<usize>>(&mut self, range: R) -> AudioSliceMut<'_, S> {
        let bound = 0..self.num_frames;

        AudioSliceMut::new(&self.spec, &mut self.planes, get_sub_range(range, &bound))
    }

    /// Append the frames from the audio buffer `src` to this buffer.
    ///
    /// # Panics
    ///
    /// Panics if `src` does not have the same specification, or the combined length would exceed
    /// the capacity of this buffer.
    pub fn append<Sin, Src>(&mut self, src: &Src)
    where
        Sin: Sample,
        S: Sample + FromSample<Sin>,
        Src: Audio<Sin>,
    {
        // Ensure the audio formats are identical.
        assert!(self.spec() == src.spec(), "audio specifications are not identical");

        // The number of frames after appending.
        let start = self.num_frames;
        let end = self.num_frames + src.frames();

        // Ensure the new length is not greater than the capacity.
        assert!(end <= self.capacity, "combined length would exceed capacity");

        for (dst, src) in self.planes.iter_mut().zip(src.iter_planes()) {
            // Dispatch to a common helper function.
            copy_to_slice(&src[start..end], &mut dst[start..end]);
        }

        // Commit appended frames.
        self.num_frames = end;
    }

    /// Resizes the buffer such that the number of frames is `new_len`. New frames are copies of
    /// `frame`.
    ///
    /// This function does not modify the maximum capacity of the buffer. If `new_len` is larger
    /// than the current number of frames in the buffer, the buffer will be padded with copies of
    /// `frame`. If `new_len` is smaller than the current number of frames, the buffer will be
    /// truncated.
    ///
    /// # Panics
    ///
    /// Panics if `new_len` exceeds the buffer capacity.
    pub fn resize(&mut self, new_len: usize, frame: &[S]) {
        // Ensure the new length is not greater than the capacity.
        assert!(new_len <= self.capacity, "new length would exceed capacity");

        // The provided frame to repeat must have a sample per frame,
        assert!(frame.len() == self.planes.len(), "provided frame is too small");

        // Truncates if `new_len` is smaller than `num_frames`.
        self.truncate(new_len);

        if self.num_frames < new_len {
            // Extend buffer to `new_len`, filling with `sample`.
            for (plane, &sample) in self.planes.iter_mut().zip(frame) {
                for s in &mut plane[self.num_frames..new_len] {
                    *s = sample;
                }
            }

            self.num_frames = new_len;
        }
    }

    /// Resizes the buffer such that the number of frames is `new_len`. New frames repeat `sample`
    /// for all planes.
    ///
    /// This function does not modify the maximum capacity of the buffer. If `new_len` is larger
    /// than the current number of frames in the buffer, the buffer will be padded with `sample`.
    /// If `new_len` is smaller than the current number of frames, the buffer will be truncated.
    ///
    /// # Panics
    ///
    /// Panics if `new_len` exceeds the buffer capacity.
    pub fn resize_with_sample(&mut self, new_len: usize, sample: S) {
        // Ensure the new length is not greater than the capacity.
        assert!(new_len <= self.capacity, "new length would exceed capacity");

        // Truncates if `new_len` is smaller than `num_frames`.
        self.truncate(new_len);

        if self.num_frames < new_len {
            // Extend buffer to `new_len`, filling with `sample`.
            for plane in &mut self.planes {
                for s in &mut plane[self.num_frames..new_len] {
                    *s = sample;
                }
            }

            self.num_frames = new_len;
        }
    }

    /// Resizes the buffer such that the number of frames is `new_len`. New frames are silent.
    ///
    /// This function does not modify the maximum capacity of the buffer. If `new_len` is larger
    /// than the current number of frames in the buffer, then the buffer will be padded with silent
    /// frames. If `new_len` is smaller than the current number of frames, the buffer will be
    /// truncated.
    ///
    /// # Panics
    ///
    /// Panics if `new_len` exceeds the buffer capacity.
    pub fn resize_with_silence(&mut self, new_len: usize) {
        self.resize_with_sample(new_len, S::MID);
    }

    /// Resizes the buffer such that the number of frames is `new_len`. New frames are left
    /// uninitialized and may contain stale data that should be overwritten.
    ///
    /// This function does not modify the maximum capacity of the buffer.
    ///
    /// # Panics
    ///
    /// Panics if `new_len` exceeds the buffer capacity.
    pub fn resize_uninit(&mut self, new_len: usize) {
        // Ensure the new length is not greater than the capacity.
        assert!(new_len <= self.capacity, "new length would exceed capacity");
        self.num_frames = new_len;
    }

    /// Renders a number of repeated frames.
    ///
    /// If `num_frames` is `None`, the remaining capacity will be used.
    ///
    /// Returns the number of frames rendered.
    ///
    /// # Panics
    ///
    /// Panics if `num_frames` is `Some` and would exceed the capacity of the buffer, or `frame`
    /// is not the same length as the number of planes.
    pub fn render(&mut self, num_frames: Option<usize>, frame: &[S]) -> usize {
        // The number of new frames to render.
        let num_new_frames = num_frames.unwrap_or(self.capacity - self.num_frames);

        // Do not render past the end of the audio buffer.
        assert!(self.num_frames + num_new_frames <= self.capacity(), "capacity will be exceeded");

        // The provided frame to repeat must have a sample per frame,
        assert!(frame.len() == self.planes.len(), "provided frame is too small");

        // Render repeated frames.
        for (plane, &s) in self.planes.iter_mut().zip(frame) {
            for sample in &mut plane[self.num_frames..self.num_frames + num_new_frames] {
                *sample = s;
            }
        }

        // Commit new frames.
        self.num_frames += num_new_frames;

        num_new_frames
    }

    /// Renders a number of frames using the provided render function.
    ///
    /// The number of frames to render is specified by `num_frames`. If `num_frames` is `None`, the
    /// remaining capacity will be used.
    ///
    /// The render function, `f`, is called once per frame and is expected to populate the next
    /// sample in each audio plane. The slice of samples for each audio plane cover all existing
    /// samples and the space for new samples. The index of the next sample to write is provided
    /// to the call. All samples beyond the write index should be considered uninitialized with
    /// stale data. The planes are in the canonical order. If the render function returns an error,
    /// the render operation is terminated and the error is returned. Only the samples written upto
    /// that point are committed.
    ///
    /// On success, the number of frames rendered is returned.
    ///
    /// # Realtime Safety
    ///
    /// If the number of planes exceeds 8, this function will allocate.
    ///
    /// # Panics
    ///
    /// Panics if `num_frames` is `Some` and would exceed the capacity of the buffer.
    pub fn render_with<F>(&mut self, num_frames: Option<usize>, mut f: F) -> Result<usize>
    where
        F: FnMut(usize, &mut [&mut [S]]) -> Result<()>,
    {
        // The number of new frames to render.
        let num_new_frames = num_frames.unwrap_or(self.capacity - self.num_frames);

        let end = self.num_frames + num_new_frames;

        // Do not render past the end of the audio buffer.
        assert!(end <= self.capacity, "capacity will be exceeded");

        // At this point, `num_new_frames` can be considered reserved but uninitialized. Create an
        // audio planes vector and fill each plane entry with a reference to the existing and
        // uninitialized samples in each plane respectively.
        let mut planes = MutableAudioPlaneSlices::new(&mut self.planes, 0..end);

        // Attempt to render frames one-by-one, exiting only if there is an error in the render
        // function.
        while self.num_frames < end {
            // Render frame.
            f(self.num_frames, &mut planes)?;

            // Commit frame.
            self.num_frames += 1;
        }

        Ok(num_new_frames - (end - self.num_frames))
    }

    /// Renders a number of silent frames.
    ///
    /// If `num_frames` is `None`, the remaining capacity will be used.
    ///
    /// Returns the number of frames rendered.
    ///
    /// # Panics
    ///
    /// Panics if `num_frames` is `Some` and would exceed the capacity of the buffer.
    pub fn render_silence(&mut self, num_frames: Option<usize>) -> usize {
        // The number of new frames to render.
        let num_new_frames = num_frames.unwrap_or(self.capacity - self.num_frames);

        // Do not render past the end of the audio buffer.
        assert!(self.num_frames + num_new_frames <= self.capacity(), "capacity will be exceeded");

        // Render silence.
        for plane in &mut self.planes {
            for sample in &mut plane[self.num_frames..self.num_frames + num_new_frames] {
                *sample = S::MID;
            }
        }

        // Commit new frames.
        self.num_frames += num_new_frames;

        num_new_frames
    }

    /// Renders an uninitialized number of frames.
    ///
    /// This is a cheap operation and simply advances the frame counter. The underlying samples are
    /// not modified and should be overwritten.
    ///
    /// If `num_frames` is `None`, the remaining capacity will be used.
    ///
    /// Returns the number of frames rendered.
    ///
    /// # Panics
    ///
    /// Panics if `num_frames` is `Some` and would exceed the capacity of the buffer.
    pub fn render_uninit(&mut self, num_frames: Option<usize>) -> usize {
        // The number of new frames to render.
        let num_new_frames = num_frames.unwrap_or(self.capacity - self.num_frames);

        // Do not render past the end of the audio buffer.
        assert!(self.num_frames + num_new_frames <= self.capacity(), "capacity will be exceeded");

        // Commit new frames.
        self.num_frames += num_new_frames;

        num_new_frames
    }

    /// Shifts the contents of the buffer back by the number of frames specified. The leading frames
    /// are dropped from the buffer.
    pub fn shift(&mut self, shift: usize) {
        if shift >= self.num_frames {
            self.clear();
        }
        else if shift > 0 {
            // Shift the samples down in each plane.
            for plane in &mut self.planes {
                plane.copy_within(shift..self.num_frames, 0);
            }
            self.num_frames -= shift;
        }
    }

    /// Truncates the buffer to the number of frames specified. If the number of frames in the
    /// buffer is less-than the number of frames specified, then this function does nothing.
    pub fn truncate(&mut self, num_frames: usize) {
        if num_frames < self.num_frames {
            self.num_frames = num_frames;
        }
    }

    /// Trims frames from the start and end of the buffer.
    pub fn trim(&mut self, start: usize, end: usize) {
        // First, trim the end to reduce the number of frames have to be shifted when the front is
        // trimmed.
        self.truncate(self.frames().saturating_sub(end));

        // Second, trim the start.
        self.shift(start);
    }
}

impl<S: Sample> Audio<S> for AudioBuffer<S> {
    fn spec(&self) -> &AudioSpec {
        &self.spec
    }

    fn num_planes(&self) -> usize {
        self.planes.len()
    }

    fn is_empty(&self) -> bool {
        self.num_frames == 0
    }

    fn frames(&self) -> usize {
        self.num_frames
    }

    fn plane(&self, idx: usize) -> Option<&[S]> {
        self.planes.get(idx).map(|plane| &plane[0..self.num_frames])
    }

    fn plane_pair(&self, idx0: usize, idx1: usize) -> Option<(&[S], &[S])> {
        plane_pair_by_buffer_index(&self.planes, 0..self.num_frames, idx0, idx1)
    }

    fn iter_planes(&self) -> AudioPlanes<'_, S> {
        AudioPlanes::new(&self.planes, 0..self.num_frames)
    }

    fn iter_interleaved(&self) -> Interleaved<'_, S> {
        Interleaved::new(&self.planes, 0..self.num_frames)
    }

    fn copy_to_slice_interleaved<Sout, Dst>(&self, dst: Dst)
    where
        Sout: Sample + FromSample<S>,
        Dst: AsMut<[Sout]>,
    {
        // Dispatch to common helper.
        copy_to_slice_interleaved(&self.planes, 0..self.num_frames, dst);
    }
}

impl<S: Sample> AudioMut<S> for AudioBuffer<S> {
    fn plane_mut(&mut self, idx: usize) -> Option<&mut [S]> {
        self.planes.get_mut(idx).map(|plane| &mut plane[0..self.num_frames])
    }

    fn plane_pair_mut(&mut self, idx0: usize, idx1: usize) -> Option<(&mut [S], &mut [S])> {
        plane_pair_by_buffer_index_mut(&mut self.planes, 0..self.num_frames, idx0, idx1)
    }

    fn iter_planes_mut(&mut self) -> AudioPlanesMut<'_, S> {
        AudioPlanesMut::new(&mut self.planes, 0..self.num_frames)
    }

    fn copy_from_slice_interleaved<Sin, Src>(&mut self, src: &Src)
    where
        Sin: Sample,
        S: Sample + FromSample<Sin>,
        Src: AsRef<[Sin]>,
    {
        // Dispatch to common helper.
        copy_from_slice_interleaved(src, 0..self.num_frames, &mut self.planes);
    }
}

impl<S: Sample + SampleBytes> AudioBytes<S> for AudioBuffer<S> {
    fn copy_bytes_interleaved_as<Sout, Dst>(&self, dst: Dst)
    where
        Sout: SampleBytes + FromSample<S>,
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_interleaved::<Sout, _, _, _, _>(&self.planes, 0..self.num_frames, convert, dst)
    }

    fn copy_bytes_planar_as<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: SampleBytes + FromSample<S>,
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_planar::<Sout, _, _, _, _>(&self.planes, 0..self.num_frames, convert, dst)
    }

    fn copy_bytes_interleaved<Dst>(&self, dst: Dst)
    where
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_interleaved::<S, _, _, _, _>(&self.planes, 0..self.num_frames, identity, dst)
    }

    fn copy_bytes_planar<Dst>(&self, dst: &mut [Dst])
    where
        Dst: AsMut<[u8]>,
    {
        // Dispatch to common helper.
        copy_bytes_planar::<S, _, _, _, _>(&self.planes, 0..self.num_frames, identity, dst)
    }
}

impl<S: Sample + SampleBytes> AudioBufferBytes<S> for AudioBuffer<S> {
    fn max_byte_len_per_plane_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes,
    {
        std::mem::size_of::<Sout::RawType>() * self.capacity()
    }

    fn max_byte_len_per_plane(&self) -> usize {
        std::mem::size_of::<S::RawType>() * self.capacity()
    }
}

impl<S: Sample> std::ops::Index<Position> for AudioBuffer<S> {
    type Output = [S];

    fn index(&self, index: Position) -> &Self::Output {
        self.plane_by_position(index).unwrap()
    }
}

impl<S: Sample> std::ops::IndexMut<Position> for AudioBuffer<S> {
    fn index_mut(&mut self, index: Position) -> &mut Self::Output {
        self.plane_by_position_mut(index).unwrap()
    }
}

impl<S: Sample> std::ops::Index<usize> for AudioBuffer<S> {
    type Output = [S];

    fn index(&self, index: usize) -> &Self::Output {
        self.plane(index).unwrap()
    }
}

impl<S: Sample> std::ops::IndexMut<usize> for AudioBuffer<S> {
    fn index_mut(&mut self, index: usize) -> &mut Self::Output {
        self.plane_mut(index).unwrap()
    }
}

/// Storage for mutable sub-slices of audio planes.
#[derive(Default)]
struct MutableAudioPlaneSlices<'a, S: Sample> {
    planes: SmallVec<[&'a mut [S]; MAX_STACK_PLANE_SLICES]>,
}

impl<'a, S: Sample> MutableAudioPlaneSlices<'a, S> {
    /// Instantiate from a slice of audio plane slices.
    fn new<Src>(src: &'a mut [Src], bound: Range<usize>) -> Self
    where
        Src: AsMut<[S]>,
    {
        let mut writable: MutableAudioPlaneSlices<'_, S> = Default::default();

        for plane in src {
            writable.planes.push(&mut plane.as_mut()[bound.clone()]);
        }

        writable
    }
}

impl<'a, S: Sample> Deref for MutableAudioPlaneSlices<'a, S> {
    type Target = [&'a mut [S]];

    fn deref(&self) -> &Self::Target {
        &self.planes
    }
}

impl<S: Sample> DerefMut for MutableAudioPlaneSlices<'_, S> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.planes
    }
}
