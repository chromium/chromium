// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `audio` module provides primitives for working with multi-channel audio generically across
//! sample formats.
//!
//! ## Standard Sample Formats
//!
//! The structs and traits in this module are generic across any type implementing the [`Sample`]
//! trait. However, a set of standard sample formats is defined. These standard sample formats are:
//! U8, S8, U16, S16, U24, S24, U32, S32, F32, and F64. These sample formats correspond to the
//! following underlying data types, respectively: `u8`, `i8`, `u16`, `i16`, `u24`, `i24`, `u32`,
//! `i32`, `f32`, and `f64`.
//!
//! ### Generic Wrappers
//!
//! To make handling varying sample formats easier, this module also implements a set of generic
//! wrappers around typed audio buffers and slices. These wrappers are implemented as enums whereby
//! each enumerator corresponds to one of the aforementioned standard sample formats. Functions on
//! the generic wrappers match closely with that of the typed interface and are dispatch to the
//! active enumerator.
use std::ops::Range;

mod buf;
mod channels;
mod generic;
mod slice;
mod util;

pub mod conv;
pub mod sample;

pub use buf::*;
pub use channels::*;
pub use generic::*;
pub use slice::*;

// Utilities are internal use-only.
use util::*;

use conv::FromSample;
use sample::{Sample, SampleBytes};

/// A specification defining the core characteristics of some audio.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct AudioSpec {
    /// The sample rate in Hz.
    rate: u32,
    /// The channels.
    channels: Channels,
}

impl AudioSpec {
    /// Create an audio specification from a sample rate in Hertz (Hz) and set of channels.
    pub fn new(rate: u32, channels: Channels) -> Self {
        AudioSpec { rate, channels }
    }

    /// Get the sample rate in Hz.
    pub fn rate(&self) -> u32 {
        self.rate
    }

    /// Get the channels.
    pub fn channels(&self) -> &Channels {
        &self.channels
    }
}

/// Iterator over immutable audio plane slices.
pub struct AudioPlanes<'a, S: Sample> {
    planes: &'a [Vec<S>],
    bound: Range<usize>,
}

impl<'a, S: Sample> AudioPlanes<'a, S> {
    fn new(planes: &'a [Vec<S>], bound: Range<usize>) -> Self {
        AudioPlanes { planes, bound }
    }
}

impl<'a, S: Sample> Iterator for AudioPlanes<'a, S> {
    type Item = &'a [S];

    fn next(&mut self) -> Option<Self::Item> {
        match self.planes.split_first() {
            Some((next, rest)) => {
                self.planes = rest;
                Some(&next[self.bound.clone()])
            }
            _ => None,
        }
    }
}

/// Iterator over mutable audio plane slices.
pub struct AudioPlanesMut<'a, S: Sample> {
    planes: &'a mut [Vec<S>],
    bound: Range<usize>,
}

impl<'a, S: Sample> AudioPlanesMut<'a, S> {
    fn new(planes: &'a mut [Vec<S>], bound: Range<usize>) -> Self {
        AudioPlanesMut { planes, bound }
    }
}

impl<'a, S: Sample> Iterator for AudioPlanesMut<'a, S> {
    type Item = &'a mut [S];

    fn next(&mut self) -> Option<Self::Item> {
        match std::mem::take(&mut self.planes).split_first_mut() {
            Some((next, rest)) => {
                self.planes = rest;
                Some(&mut next[self.bound.clone()])
            }
            _ => None,
        }
    }
}

/// Iterator over interleaved samples.
pub struct Interleaved<'a, S: Sample> {
    planes: &'a [Vec<S>],
    num_planes: usize,
    index: usize,
    end: usize,
}

impl<S: Sample> Interleaved<'_, S> {
    fn new(planes: &[Vec<S>], bound: Range<usize>) -> Interleaved<'_, S> {
        let num_planes = planes.len();
        let len = bound.len();

        let index = bound.start * num_planes;
        let end = index + len * num_planes;

        Interleaved { planes, num_planes, index, end }
    }
}

impl<S: Sample> Iterator for Interleaved<'_, S> {
    type Item = S;

    fn next(&mut self) -> Option<Self::Item> {
        if self.index < self.end {
            let frame = self.index / self.num_planes;
            let plane = self.index % self.num_planes;

            self.index += 1;

            return Some(self.planes[plane][frame]);
        }
        None
    }
}

/// Trait for accessing immutable planar audio.
pub trait Audio<S: Sample> {
    /// Get the audio specification.
    fn spec(&self) -> &AudioSpec;

    /// Get the total number of audio planes.
    fn num_planes(&self) -> usize;

    /// Returns `true` if there are no audio frames.
    fn is_empty(&self) -> bool;

    /// Gets the number of audio frames in the buffer.
    fn frames(&self) -> usize;

    /// Try to get an immutable slice to the audio plane at the canonical buffer index `idx`.
    fn plane(&self, idx: usize) -> Option<&[S]>;

    /// Try to get an immutable slice to the audio plane at position `pos`.
    ///
    /// # Panics
    ///
    /// Panics if `pos` contains more than one position.
    fn plane_by_position(&self, pos: Position) -> Option<&[S]> {
        self.spec()
            .channels()
            .get_canonical_index_for_positioned_channel(pos)
            .and_then(|idx| self.plane(idx))
    }

    /// Try to get immutable slices to a pair of audio planes at the canonical buffer indicies
    /// `idx0` and `idx1`.
    ///
    /// # Panics
    ///
    /// Panics if `idx0` and `idx1` are the same index.
    fn plane_pair(&self, idx0: usize, idx1: usize) -> Option<(&[S], &[S])>;

    /// Try to get an immutable slice to a pair of audio planes at positions `pos0` and `pos1`.
    ///
    /// # Panics
    ///
    /// Panics if `pos0` and `pos1` are the same position, or contain more than one position.
    fn plane_pair_by_position(&self, pos0: Position, pos1: Position) -> Option<(&[S], &[S])> {
        // The channel positions cannot be the same.
        assert!(pos0 != pos1, "channel positions cannot be the same");

        let channels = self.spec().channels();

        let idx0 = channels.get_canonical_index_for_positioned_channel(pos0);
        let idx1 = channels.get_canonical_index_for_positioned_channel(pos1);

        idx0.and_then(|idx0| idx1.map(|idx1| (idx0, idx1)))
            .and_then(|(idx0, idx1)| self.plane_pair(idx0, idx1))
    }

    /// Iterate over immutable slices to all planes.
    fn iter_planes(&self) -> AudioPlanes<'_, S>;

    /// Iterate over all samples in an interleaved order.
    fn iter_interleaved(&self) -> Interleaved<'_, S>;

    /// Get the total number of samples contained in all audio planes.
    fn samples_interleaved(&self) -> usize {
        self.num_planes() * self.frames()
    }

    /// Get the total number of samples contained in each audio plane.
    fn samples_planar(&self) -> usize {
        self.frames()
    }

    /// Copy all audio frames to a slice of samples in interleaved order.
    ///
    /// If the sample format of the slice differs a sample format conversion will occur. Samples are
    /// interleaved in canonical order.
    ///
    /// # Panics
    ///
    /// Panics if the length of the destination slice is not the correct length. Use
    /// [`samples_interleaved`](Self::samples_interleaved) to get the correct length.
    fn copy_to_slice_interleaved<Sout, Dst>(&self, dst: Dst)
    where
        Sout: Sample + FromSample<S>,
        Dst: AsMut<[Sout]>;

    /// Copy all audio planes to discrete slices.
    ///
    /// If the sample format of a slice differs a sampe format conversion will occur.
    ///
    /// # Panics
    ///
    /// Panics if the number of slices is not equal to the number of audio planes, or if a slice is
    /// not the correct length. Use [`num_planes`](Self::num_planes) to get the number of audio
    /// planes, and [`samples_planar`](Self::samples_planar) to get the length of each plane.
    fn copy_to_slice_planar<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: Sample + FromSample<S>,
        Dst: AsMut<[Sout]>,
    {
        assert!(
            dst.len() == self.num_planes(),
            "expected {} destination slices",
            self.num_planes()
        );

        for (src, dst) in self.iter_planes().zip(dst) {
            // Dispatch to a common copy function.
            copy_to_slice(src, dst.as_mut());
        }
    }

    /// Copy all audio frames to a vector of samples in interleaved order.
    ///
    /// This function takes a mutable reference to a vector. The vector is resized such that the
    /// length of the vector after the copy is the exact number of samples copied.
    ///
    /// A sample format conversion will occur if the sample format of the destination vector
    /// differs.
    ///
    /// # Realtime Safety
    ///
    /// This function allocates if the vector is not long enough. Allocations can be avoided if
    /// the vector has capacity reserved ahead of time.
    fn copy_to_vec_interleaved<Sout>(&self, dst: &mut Vec<Sout>)
    where
        Sout: Sample + FromSample<S>,
    {
        // Ensure the vector is long enough.
        dst.resize(self.samples_interleaved(), Sout::MID);
        self.copy_to_slice_interleaved(dst);
    }

    /// Copy all audio planes to discrete vectors.
    ///
    /// This function takes a mutable reference to a vector of vectors. The outer vector is resized
    /// to match the number of audio planes being copied. Each inner audio plane vector is then
    /// resized to match the number of samples copied per plane.
    ///
    /// A sample format conversion will occur if the sample format of the destination vectors
    /// differ.
    ///
    /// # Realtime Safety
    ///
    /// This function allocates if either the outer vector is not long enough, or if an audio plane
    /// vector is not long enough. Allocations can be avoided if all vectors have capacity reserved
    /// ahead of time.
    fn copy_to_vecs_planar<Sout>(&self, dst: &mut Vec<Vec<Sout>>)
    where
        Sout: Sample + FromSample<S>,
    {
        // Ensure there is one vector per plane.
        dst.resize(self.num_planes(), Default::default());

        // Ensure the vector for each plane is the correct length.
        for vec in dst.iter_mut() {
            vec.resize(self.samples_planar(), Sout::MID);
        }

        self.copy_to_slice_planar(dst);
    }
}

/// Trait for manipulating mutable planar audio.
pub trait AudioMut<S: Sample>: Audio<S> {
    /// Try to get a mutable slice to the audio plane at the canonical buffer index `idx`.
    fn plane_mut(&mut self, idx: usize) -> Option<&mut [S]>;

    /// Try to get a mutable slice to the audio plane at position `pos`.
    ///
    /// # Panics
    ///
    /// Panics if `pos` contains the same position.
    fn plane_by_position_mut(&mut self, pos: Position) -> Option<&mut [S]> {
        self.spec()
            .channels()
            .get_canonical_index_for_positioned_channel(pos)
            .and_then(move |idx| self.plane_mut(idx))
    }

    /// Try to get mutable slices to a pair of audio planes at the canonical buffer indicies `idx0`
    /// and `idx1`.
    ///
    /// # Panics
    ///
    /// Panics if `idx0` and `idx1` are the same index.
    fn plane_pair_mut(&mut self, idx0: usize, idx1: usize) -> Option<(&mut [S], &mut [S])>;

    /// Try to get mutable slices to a pair of audio planes at positions `pos0` and `pos1`.
    ///
    /// # Panics
    ///
    /// Panics if `pos0` and `pos1` are the same position, or contains the same position.
    fn plane_pair_by_position_mut(
        &mut self,
        pos0: Position,
        pos1: Position,
    ) -> Option<(&mut [S], &mut [S])> {
        // The channel positions cannot be the same.
        assert!(pos0 != pos1, "channel positions cannot be the same");

        let channels = self.spec().channels();

        let idx0 = channels.get_canonical_index_for_positioned_channel(pos0);
        let idx1 = channels.get_canonical_index_for_positioned_channel(pos1);

        idx0.and_then(|idx0| idx1.map(|idx1| (idx0, idx1)))
            .and_then(move |(idx0, idx1)| self.plane_pair_mut(idx0, idx1))
    }

    /// Iterate over mutable slices to all planes.
    fn iter_planes_mut(&mut self) -> AudioPlanesMut<'_, S>;

    /// Applies a transformation function over the samples of all planes.
    ///
    /// The order of traversal is not specified.
    fn apply<F>(&mut self, f: F)
    where
        F: Fn(S) -> S,
    {
        for plane in self.iter_planes_mut() {
            for sample in plane {
                *sample = f(*sample);
            }
        }
    }

    /// Copy audio from a source.
    ///
    /// # Panics
    ///
    /// Panics if:
    /// * The audio specification of this and the source are not identical.
    /// * The number of frames of both this and the source are not identical.
    fn copy_from<Sin, Src>(&mut self, src: &Src)
    where
        Sin: Sample,
        S: Sample + FromSample<Sin>,
        Src: Audio<Sin>,
    {
        assert!(self.spec() == src.spec(), "expected identical audio specifications");
        assert!(self.frames() == src.frames(), "expected identical number of frames");

        for (src, dst) in src.iter_planes().zip(self.iter_planes_mut()) {
            // Dispatch to a common helper function.
            copy_to_slice(src, dst);
        }
    }

    /// Copy audio from a slice of slices.
    ///
    /// # Panics
    ///
    /// Panics if:
    /// * The number of slices does not match the number of audio planes.
    /// * The number of samples in each slice does not match the number of frames.
    fn copy_from_slice_planar<Sin, Src>(&mut self, src: &[Src])
    where
        Sin: Sample,
        S: Sample + FromSample<Sin>,
        Src: AsRef<[Sin]>,
    {
        assert!(src.len() == self.num_planes(), "expected {} source slices", self.num_planes());

        for (src, dst) in src.iter().zip(self.iter_planes_mut()) {
            // Dispatch to a common copy function.
            copy_to_slice(src.as_ref(), dst);
        }
    }

    /// Copy audio from an interleaved slice of samples.
    fn copy_from_slice_interleaved<Sin, Src>(&mut self, src: &Src)
    where
        Sin: Sample,
        S: Sample + FromSample<Sin>,
        Src: AsRef<[Sin]>;
}

/// Trait for marshalling planar audio to byte buffers.
pub trait AudioBytes<S: Sample + SampleBytes>: Audio<S> {
    /// Get the length in bytes of all samples if converted to a new sample format.
    fn byte_len_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + FromSample<S>,
    {
        std::mem::size_of::<Sout::RawType>() * self.samples_interleaved()
    }

    /// Get the length in bytes of all samples in a single plane if converted to a new sample
    /// format.
    fn byte_len_per_plane_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + FromSample<S>,
    {
        std::mem::size_of::<Sout::RawType>() * self.samples_planar()
    }

    /// Get the length of bytes of a single interleaved audio frame if converted to a new sample
    /// format.
    fn byte_len_per_frame_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + FromSample<S>,
    {
        std::mem::size_of::<Sout::RawType>() * self.num_planes()
    }

    /// Copy interleaved audio to the destination byte slice after converting to a different sample
    /// format.
    ///
    /// The destination slice must be exactly the correct length to fit all audio frames. Use
    /// [`byte_len_as`](Self::byte_len_as) to determine this length.
    ///
    /// # Panics
    ///
    /// Panics if `dst` is not the expected length.
    fn copy_bytes_interleaved_as<Sout, Dst>(&self, dst: Dst)
    where
        Sout: SampleBytes + FromSample<S>,
        Dst: AsMut<[u8]>;

    /// Copy planar audio as bytes to a destination slice per plane after converting to a different
    /// sample format.
    ///
    /// There must be exactly one destination slice per audio plane.
    ///
    /// Each destination slice must be exactly the correct length to fit all audio samples. Use
    /// [`byte_len_per_plane_as`](Self::byte_len_per_plane_as) to determine this length.
    ///
    /// # Panics
    ///
    /// Panics if:
    /// * The length of `dst` is not exactly equal to the number of planes.
    /// * The length of each slice in `dst` is not the expected length.
    fn copy_bytes_planar_as<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: SampleBytes + FromSample<S>,
        Dst: AsMut<[u8]>;

    /// Get the length in bytes of all samples.
    fn byte_len(&self) -> usize {
        std::mem::size_of::<S::RawType>() * self.samples_interleaved()
    }

    /// Get the length in bytes of all samples in a single plane.
    fn byte_len_per_plane(&self) -> usize {
        std::mem::size_of::<S::RawType>() * self.samples_planar()
    }

    /// Get the length of bytes of a single interleaved audio frame.
    fn byte_len_per_frame(&self) -> usize {
        std::mem::size_of::<S::RawType>() * self.num_planes()
    }

    /// Copy interleaved audio to the destination byte slice.
    ///
    /// The destination slice must be exactly the correct length to fit all audio frames. Use
    /// [`byte_len`](Self::byte_len) to determine this length.
    ///
    /// # Panics
    ///
    /// Panics if `dst` is not the expected length.
    fn copy_bytes_interleaved<Dst>(&self, dst: Dst)
    where
        Dst: AsMut<[u8]>;

    /// Copy planar audio as bytes to a destination slice per plane.
    ///
    /// There must be exactly one destination slice per audio plane.
    ///
    /// Each destination slice must be exactly the correct length to fit all audio samples. Use
    /// [`byte_len_per_plane`](Self::byte_len_per_plane) to determine this length.
    ///
    /// # Panics
    ///
    /// Panics if:
    /// * The length of `dst` is not exactly equal to the number of planes.
    /// * The length of each slice in `dst` is not the expected length.
    fn copy_bytes_planar<Dst>(&self, dst: &mut [Dst])
    where
        Dst: AsMut<[u8]>;

    /// Copy interleaved audio to the destination byte vector.
    ///
    /// This function takes a mutable reference to a vector. The vector is resized such that the
    /// length of the vector after the copy is the exact number of samples copied.
    ///
    /// # Realtime Safety
    ///
    /// This function allocates if the vector is not long enough. Allocations can be avoided if
    /// the vector has capacity reserved ahead of time.
    fn copy_bytes_to_vec_interleaved(&self, dst: &mut Vec<u8>) {
        // Ensure the vector is long enough.
        dst.resize(self.byte_len(), 0);
        self.copy_bytes_interleaved(dst);
    }

    /// Copy interleaved audio to the destination byte vector after converting to a different sample
    /// format.
    ///
    /// This function takes a mutable reference to a vector. The vector is resized such that the
    /// length of the vector after the copy is the exact number of samples copied.
    ///
    /// # Realtime Safety
    ///
    /// This function allocates if the vector is not long enough. Allocations can be avoided if
    /// the vector has capacity reserved ahead of time.
    fn copy_bytes_to_vec_interleaved_as<Sout>(&self, dst: &mut Vec<u8>)
    where
        Sout: SampleBytes + FromSample<S>,
    {
        // Ensure the vector is long enough.
        dst.resize(self.byte_len_as::<Sout>(), 0);
        self.copy_bytes_interleaved_as::<Sout, _>(dst);
    }

    /// Copy audio planes as bytes to discrete byte vectors.
    ///
    /// This function takes a mutable reference to a vector of vectors. The outer vector is resized
    /// to match the number of audio planes being copied. Each inner byte vector is resized to
    /// match the number of bytes per plane.
    ///
    /// # Realtime Safety
    ///
    /// This function allocates if either the outer vector is not long enough, or if an audio plane
    /// vector is not long enough. Allocations can be avoided if all vectors have capacity reserved
    /// ahead of time.
    fn copy_bytes_to_vecs_planar(&self, dst: &mut Vec<Vec<u8>>) {
        // Ensure there is one vector per plane.
        dst.resize(self.num_planes(), Default::default());

        // Ensure the vector for each plane is the correct length.
        for vec in dst.iter_mut() {
            vec.resize(self.byte_len_per_plane(), 0);
        }

        self.copy_bytes_planar(dst);
    }

    /// Copy audio planes as bytes to discrete byte vectors after converting to a different sample
    /// format.
    ///
    /// This function takes a mutable reference to a vector of vectors. The outer vector is resized
    /// to match the number of audio planes being copied. Each inner byte vector is resized to
    /// match the number of bytes per plane.
    ///
    /// # Realtime Safety
    ///
    /// This function allocates if either the outer vector is not long enough, or if an audio plane
    /// vector is not long enough. Allocations can be avoided if all vectors have capacity reserved
    /// ahead of time.
    fn copy_bytes_to_vecs_planar_as<Sout>(&self, dst: &mut Vec<Vec<u8>>)
    where
        Sout: SampleBytes + FromSample<S>,
    {
        // Ensure there is one vector per plane.
        dst.resize(self.num_planes(), Default::default());

        // Ensure the vector for each plane is the correct length.
        for vec in dst.iter_mut() {
            vec.resize(self.byte_len_per_plane_as::<Sout>(), 0);
        }

        self.copy_bytes_planar_as::<Sout, _>(dst);
    }
}

/// Trait for querying the maximum capacity in bytes for dynamically sized audio storage.
pub trait AudioBufferBytes<S: Sample + SampleBytes>: AudioBytes<S> {
    /// Get the maximum possible length in bytes of all samples after converting to a different
    /// sample format.
    fn max_byte_len_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes,
    {
        self.max_byte_len_per_plane_as::<Sout>() * self.num_planes()
    }

    /// Get the maximum possible length in bytes of all samples in a single plane after converting
    /// to a different sample format.
    fn max_byte_len_per_plane_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes;

    /// Get the maximum possible length in bytes of all samples.
    fn max_byte_len(&self) -> usize {
        self.max_byte_len_per_plane() * self.num_planes()
    }

    /// Get the maximum possible length in bytes of all samples in a single plane.
    fn max_byte_len_per_plane(&self) -> usize;
}
