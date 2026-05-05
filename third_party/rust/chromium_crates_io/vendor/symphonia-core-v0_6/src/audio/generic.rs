// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::ops::RangeBounds;

use crate::audio::{
    conv::ConvertibleSample,
    sample::{SampleBytes, SampleFormat, i24, u24},
};

use super::{Audio, AudioBuffer, AudioBytes, AudioMut, AudioSlice, AudioSpec};

/// An owning wrapper for an [`AudioBuffer`] of any standard sample format.
///
/// Calls on this wrapper are dispatched to the underlying, wrapped, buffer and are semantically
/// identical.
pub enum GenericAudioBuffer {
    /// An unsigned 8-bit integer audio buffer.
    U8(AudioBuffer<u8>),
    /// An unsigned 16-bit integer audio buffer.
    U16(AudioBuffer<u16>),
    /// An unsigned 24-bit integer audio buffer.
    U24(AudioBuffer<u24>),
    /// An unsigned 32-bit integer audio buffer.
    U32(AudioBuffer<u32>),
    /// A signed 8-bit integer audio buffer.
    S8(AudioBuffer<i8>),
    /// A signed 16-bit integer audio buffer.
    S16(AudioBuffer<i16>),
    /// A signed 24-bit integer audio buffer.
    S24(AudioBuffer<i24>),
    /// A signed 32-bit integer audio buffer.
    S32(AudioBuffer<i32>),
    /// A single precision (32-bit) floating point audio buffer.
    F32(AudioBuffer<f32>),
    /// A double precision (64-bit) floating point audio buffer.
    F64(AudioBuffer<f64>),
}

macro_rules! impl_generic_func {
    ($own:expr, $buf:ident, $expr:expr) => {
        match $own {
            GenericAudioBuffer::U8($buf) => $expr,
            GenericAudioBuffer::U16($buf) => $expr,
            GenericAudioBuffer::U24($buf) => $expr,
            GenericAudioBuffer::U32($buf) => $expr,
            GenericAudioBuffer::S8($buf) => $expr,
            GenericAudioBuffer::S16($buf) => $expr,
            GenericAudioBuffer::S24($buf) => $expr,
            GenericAudioBuffer::S32($buf) => $expr,
            GenericAudioBuffer::F32($buf) => $expr,
            GenericAudioBuffer::F64($buf) => $expr,
        }
    };
}

impl GenericAudioBuffer {
    pub fn new(format: SampleFormat, spec: AudioSpec, capacity: usize) -> Self {
        match format {
            SampleFormat::U8 => GenericAudioBuffer::U8(AudioBuffer::new(spec, capacity)),
            SampleFormat::U16 => GenericAudioBuffer::U16(AudioBuffer::new(spec, capacity)),
            SampleFormat::U24 => GenericAudioBuffer::U24(AudioBuffer::new(spec, capacity)),
            SampleFormat::U32 => GenericAudioBuffer::U32(AudioBuffer::new(spec, capacity)),
            SampleFormat::S8 => GenericAudioBuffer::S8(AudioBuffer::new(spec, capacity)),
            SampleFormat::S16 => GenericAudioBuffer::S16(AudioBuffer::new(spec, capacity)),
            SampleFormat::S24 => GenericAudioBuffer::S24(AudioBuffer::new(spec, capacity)),
            SampleFormat::S32 => GenericAudioBuffer::S32(AudioBuffer::new(spec, capacity)),
            SampleFormat::F32 => GenericAudioBuffer::F32(AudioBuffer::new(spec, capacity)),
            SampleFormat::F64 => GenericAudioBuffer::F64(AudioBuffer::new(spec, capacity)),
        }
    }

    /// Get the audio specification.
    pub fn spec(&self) -> &AudioSpec {
        impl_generic_func!(self, buf, buf.spec())
    }

    /// Get the total number of audio planes.
    pub fn num_planes(&self) -> usize {
        impl_generic_func!(self, buf, buf.num_planes())
    }

    /// Returns `true` if there are no audio frames.
    pub fn is_empty(&self) -> bool {
        impl_generic_func!(self, buf, buf.is_empty())
    }

    /// Gets the number of audio frames in the buffer.
    pub fn frames(&self) -> usize {
        impl_generic_func!(self, buf, buf.frames())
    }

    /// Returns `true` if the `AudioBuffer` is unused.
    ///
    /// An unused `AudioBuffer` has either a capacity of 0, or no channels.
    pub fn is_unused(&self) -> bool {
        impl_generic_func!(self, buf, buf.is_unused())
    }

    /// Gets the total capacity of the buffer. The capacity is the maximum number of audio frames
    /// the buffer can store.
    pub fn capacity(&self) -> usize {
        impl_generic_func!(self, buf, buf.capacity())
    }

    /// Clears all audio frames.
    pub fn clear(&mut self) {
        impl_generic_func!(self, buf, buf.clear());
    }

    /// Grows the capacity of the buffer if the new capacity is larger than the current capacity.
    ///
    /// # Realtime Safety
    ///
    /// This function will allocate if `new_capacity` exceeds the current capacity.
    pub fn grow_capacity(&mut self, new_capacity: usize) {
        impl_generic_func!(self, buf, buf.grow_capacity(new_capacity))
    }

    /// Resizes the buffer such that the number of frames is `new_len`. New frames are silent.
    ///
    /// See [`AudioBuffer::resize_with_silence`] for full details.
    pub fn resize_with_silence(&mut self, new_len: usize) {
        impl_generic_func!(self, buf, buf.resize_with_silence(new_len))
    }

    /// Resizes the buffer such that the number of frames is `new_len`. New frames are left
    /// uninitialized and may contain stale data that should be overwritten.
    ///
    /// See [`AudioBuffer::resize_uninit`] for full details.
    pub fn resize_uninit(&mut self, new_len: usize) {
        impl_generic_func!(self, buf, buf.resize_uninit(new_len))
    }

    /// Renders a number of silent frames.
    ///
    /// See [`AudioBuffer::render_silence`] for full details.
    pub fn render_silence(&mut self, num_frames: Option<usize>) -> usize {
        impl_generic_func!(self, buf, buf.render_silence(num_frames))
    }

    /// Renders an uninitialized number of frames.
    ///
    /// See [`AudioBuffer::render_uninit`] for full details.
    pub fn render_uninit(&mut self, num_frames: Option<usize>) -> usize {
        impl_generic_func!(self, buf, buf.render_uninit(num_frames))
    }

    /// Shifts the contents of the buffer back by the number of frames specified. The leading frames
    /// are dropped from the buffer.
    pub fn shift(&mut self, shift: usize) {
        impl_generic_func!(self, buf, buf.shift(shift))
    }

    /// Get an immutable slice of the buffer over `range`.
    pub fn slice<R: RangeBounds<usize>>(&self, range: R) -> GenericAudioSlice<'_> {
        match self {
            GenericAudioBuffer::U8(buf) => GenericAudioSlice::U8(buf.slice(range)),
            GenericAudioBuffer::U16(buf) => GenericAudioSlice::U16(buf.slice(range)),
            GenericAudioBuffer::U24(buf) => GenericAudioSlice::U24(buf.slice(range)),
            GenericAudioBuffer::U32(buf) => GenericAudioSlice::U32(buf.slice(range)),
            GenericAudioBuffer::S8(buf) => GenericAudioSlice::S8(buf.slice(range)),
            GenericAudioBuffer::S16(buf) => GenericAudioSlice::S16(buf.slice(range)),
            GenericAudioBuffer::S24(buf) => GenericAudioSlice::S24(buf.slice(range)),
            GenericAudioBuffer::S32(buf) => GenericAudioSlice::S32(buf.slice(range)),
            GenericAudioBuffer::F32(buf) => GenericAudioSlice::F32(buf.slice(range)),
            GenericAudioBuffer::F64(buf) => GenericAudioSlice::F64(buf.slice(range)),
        }
    }

    /// Truncates the buffer to the number of frames specified. If the number of frames in the
    /// buffer is less-than the number of frames specified, then this function does nothing.
    pub fn truncate(&mut self, num_frames: usize) {
        impl_generic_func!(self, buf, buf.truncate(num_frames))
    }

    /// Trims frames from the start and end of the buffer.
    pub fn trim(&mut self, start: usize, end: usize) {
        impl_generic_func!(self, buf, buf.trim(start, end))
    }

    /// Get the total number of samples contained in all audio planes.
    pub fn samples_interleaved(&self) -> usize {
        self.num_planes() * self.frames()
    }

    /// Get the total number of samples contained in each audio plane.
    pub fn samples_planar(&self) -> usize {
        self.frames()
    }

    /// Copy audio to a mutable audio slice while doing any necessary sample format conversions.
    pub fn copy_to<Sout, Dst>(&self, dst: &mut Dst)
    where
        Sout: ConvertibleSample,
        Dst: AudioMut<Sout>,
    {
        impl_generic_func!(self, buf, dst.copy_from(buf));
    }

    /// Copy all audio frames to a slice of samples in interleaved order.
    ///
    /// See [`AudioBuffer::copy_to_slice_interleaved`] for full details.
    pub fn copy_to_slice_interleaved<Sout, Dst>(&self, dst: Dst)
    where
        Sout: ConvertibleSample,
        Dst: AsMut<[Sout]>,
    {
        impl_generic_func!(self, buf, buf.copy_to_slice_interleaved(dst))
    }

    /// Copy all audio planes to discrete slices.
    ///
    /// See [`AudioBuffer::copy_to_slice_planar`] for full details.
    pub fn copy_to_slice_planar<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: ConvertibleSample,
        Dst: AsMut<[Sout]>,
    {
        impl_generic_func!(self, buf, buf.copy_to_slice_planar(dst))
    }

    /// Copy all audio frames to a vector of samples in interleaved order.
    ///
    /// See [`AudioBuffer::copy_to_vec_interleaved`] for full details.
    pub fn copy_to_vec_interleaved<Sout>(&self, dst: &mut Vec<Sout>)
    where
        Sout: ConvertibleSample,
    {
        impl_generic_func!(self, buf, buf.copy_to_vec_interleaved(dst))
    }

    /// Copy all audio planes to discrete vectors.
    ///
    /// See [`AudioBuffer::copy_to_vecs_planar`] for full details.
    pub fn copy_to_vecs_planar<Sout>(&self, dst: &mut Vec<Vec<Sout>>)
    where
        Sout: ConvertibleSample,
    {
        impl_generic_func!(self, buf, buf.copy_to_vecs_planar(dst))
    }

    /// Copy interleaved audio to the destination byte slice after converting to a different sample
    /// format.
    ///
    /// See [`AudioBuffer::copy_bytes_interleaved_as`] for full details.
    pub fn copy_bytes_interleaved_as<Sout, Dst>(&self, dst: Dst)
    where
        Sout: SampleBytes + ConvertibleSample,
        Dst: AsMut<[u8]>,
    {
        impl_generic_func!(self, buf, buf.copy_bytes_interleaved_as::<Sout, _>(dst))
    }

    /// Copy planar audio as bytes to a destination slice per plane after converting to a different
    /// sample format.
    ///
    /// See [`AudioBuffer::copy_bytes_planar_as`] for full details.
    pub fn copy_bytes_planar_as<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: SampleBytes + ConvertibleSample,
        Dst: AsMut<[u8]>,
    {
        impl_generic_func!(self, buf, buf.copy_bytes_planar_as::<Sout, _>(dst))
    }

    /// Copy interleaved audio to the destination byte slice.
    ///
    /// See [`AudioBuffer::copy_bytes_interleaved`] for full details.
    pub fn copy_bytes_interleaved<Dst>(&self, dst: Dst)
    where
        Dst: AsMut<[u8]>,
    {
        impl_generic_func!(self, buf, buf.copy_bytes_interleaved(dst))
    }

    /// Copy planar audio as bytes to a destination slice per plane.
    ///
    /// See [`AudioBuffer::copy_bytes_planar`] for full details.
    pub fn copy_bytes_planar<Dst>(&self, dst: &mut [Dst])
    where
        Dst: AsMut<[u8]>,
    {
        impl_generic_func!(self, buf, buf.copy_bytes_planar(dst))
    }

    /// Copy interleaved audio to the destination byte vector.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vec_interleaved`] for full details.
    pub fn copy_bytes_to_vec_interleaved(&self, dst: &mut Vec<u8>) {
        impl_generic_func!(self, buf, buf.copy_bytes_to_vec_interleaved(dst))
    }

    /// Copy interleaved audio to the destination byte vector after converting to a different sample
    /// format.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vec_interleaved_as`] for full details.
    pub fn copy_bytes_to_vec_interleaved_as<Sout>(&self, dst: &mut Vec<u8>)
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_func!(self, buf, buf.copy_bytes_to_vec_interleaved_as::<Sout>(dst))
    }

    /// Copy audio planes as bytes to discrete byte vectors.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vecs_planar`] for full details.
    pub fn copy_bytes_to_vecs_planar(&self, dst: &mut Vec<Vec<u8>>) {
        impl_generic_func!(self, buf, buf.copy_bytes_to_vecs_planar(dst))
    }

    /// Copy audio planes as bytes to discrete byte vectors after converting to a different sample
    /// format.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vecs_planar_as`] for full details.
    pub fn copy_bytes_to_vecs_planar_as<Sout>(&self, dst: &mut Vec<Vec<u8>>)
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_func!(self, buf, buf.copy_bytes_to_vecs_planar_as::<Sout>(dst))
    }

    /// Get the length in bytes of all samples if converted to a new sample format.
    pub fn byte_len_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_func!(self, buf, buf.byte_len_as::<Sout>())
    }

    /// Get the length in bytes of all samples in a single plane if converted to a new sample
    /// format.
    pub fn byte_len_per_plane_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_func!(self, buf, buf.byte_len_per_plane_as::<Sout>())
    }

    /// Get the length of bytes of a single interleaved audio frame if converted to a new sample
    /// format.
    pub fn byte_len_per_frame_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_func!(self, buf, buf.byte_len_per_frame_as::<Sout>())
    }

    /// Get the length in bytes of all samples.
    pub fn byte_len(&self) -> usize {
        impl_generic_func!(self, buf, buf.byte_len())
    }

    /// Get the length in bytes of all samples in a single plane.
    pub fn byte_len_per_plane(&self) -> usize {
        impl_generic_func!(self, buf, buf.byte_len_per_plane())
    }

    /// Get the length of bytes of a single interleaved audio frame.
    pub fn byte_len_per_frame(&self) -> usize {
        impl_generic_func!(self, buf, buf.byte_len_per_frame())
    }
}

/// A non-owning immutable reference wrapper for an [`AudioBuffer`] of any standard sample format.
///
/// Calls on this wrapper are dispatched to the underlying, wrapped, buffer and are semantically
/// identical.
#[derive(Clone)]
pub enum GenericAudioBufferRef<'a> {
    /// An immutable unsigned 8-bit integer audio buffer reference.
    U8(&'a AudioBuffer<u8>),
    /// An immutable unsigned 16-bit integer audio buffer reference.
    U16(&'a AudioBuffer<u16>),
    /// An immutable unsigned 24-bit integer audio buffer reference.
    U24(&'a AudioBuffer<u24>),
    /// An immutable unsigned 32-bit integer audio buffer reference.
    U32(&'a AudioBuffer<u32>),
    /// An immutable signed 8-bit integer audio buffer reference.
    S8(&'a AudioBuffer<i8>),
    /// An immutable signed 16-bit integer audio buffer reference.
    S16(&'a AudioBuffer<i16>),
    /// An immutable signed 24-bit integer audio buffer reference.
    S24(&'a AudioBuffer<i24>),
    /// An immutable signed 32-bit integer audio buffer reference.
    S32(&'a AudioBuffer<i32>),
    /// An immutable single precision (32-bit) floating point audio buffer reference.
    F32(&'a AudioBuffer<f32>),
    /// An immutable double precision (64-bit) floating point audio buffer reference.
    F64(&'a AudioBuffer<f64>),
}

macro_rules! impl_generic_ref_func {
    ($var:expr, $buf:ident,$expr:expr) => {
        match $var {
            GenericAudioBufferRef::U8($buf) => $expr,
            GenericAudioBufferRef::U16($buf) => $expr,
            GenericAudioBufferRef::U24($buf) => $expr,
            GenericAudioBufferRef::U32($buf) => $expr,
            GenericAudioBufferRef::S8($buf) => $expr,
            GenericAudioBufferRef::S16($buf) => $expr,
            GenericAudioBufferRef::S24($buf) => $expr,
            GenericAudioBufferRef::S32($buf) => $expr,
            GenericAudioBufferRef::F32($buf) => $expr,
            GenericAudioBufferRef::F64($buf) => $expr,
        }
    };
}

impl GenericAudioBufferRef<'_> {
    /// Get the audio specification.
    pub fn spec(&self) -> &AudioSpec {
        impl_generic_ref_func!(self, buf, buf.spec())
    }

    /// Get the total number of audio planes.
    pub fn num_planes(&self) -> usize {
        impl_generic_ref_func!(self, buf, buf.num_planes())
    }

    /// Returns `true` if there are no audio frames.
    pub fn is_empty(&self) -> bool {
        impl_generic_ref_func!(self, buf, buf.is_empty())
    }

    /// Gets the number of audio frames in the buffer.
    pub fn frames(&self) -> usize {
        impl_generic_ref_func!(self, buf, buf.frames())
    }

    /// Returns `true` if the referenced `AudioBuffer` is unused.
    ///
    /// An unused `AudioBuffer` has either a capacity of 0, or no channels.
    pub fn is_unused(&self) -> bool {
        impl_generic_ref_func!(self, buf, buf.is_unused())
    }

    /// Gets the total capacity of the buffer. The capacity is the maximum number of audio frames
    /// the buffer can store.
    pub fn capacity(&self) -> usize {
        impl_generic_ref_func!(self, buf, buf.capacity())
    }

    /// Get the total number of samples contained in all audio planes.
    pub fn samples_interleaved(&self) -> usize {
        self.num_planes() * self.frames()
    }

    /// Get the total number of samples contained in each audio plane.
    pub fn samples_planar(&self) -> usize {
        self.frames()
    }

    /// Copy audio to a mutable audio slice while doing any necessary sample format conversions.
    pub fn copy_to<Sout, Dst>(&self, dst: &mut Dst)
    where
        Sout: ConvertibleSample,
        Dst: AudioMut<Sout>,
    {
        impl_generic_ref_func!(self, buf, dst.copy_from(*buf));
    }

    /// Copy all audio frames to a slice of samples in interleaved order.
    ///
    /// See [`AudioBuffer::copy_to_slice_interleaved`] for full details.
    pub fn copy_to_slice_interleaved<Sout, Dst>(&self, dst: Dst)
    where
        Sout: ConvertibleSample,
        Dst: AsMut<[Sout]>,
    {
        impl_generic_ref_func!(self, buf, buf.copy_to_slice_interleaved(dst))
    }

    /// Copy all audio planes to discrete slices.
    ///
    /// See [`AudioBuffer::copy_to_slice_planar`] for full details.
    pub fn copy_to_slice_planar<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: ConvertibleSample,
        Dst: AsMut<[Sout]>,
    {
        impl_generic_ref_func!(self, buf, buf.copy_to_slice_planar(dst))
    }

    /// Copy all audio frames to a vector of samples in interleaved order.
    ///
    /// See [`AudioBuffer::copy_to_vec_interleaved`] for full details.
    pub fn copy_to_vec_interleaved<Sout>(&self, dst: &mut Vec<Sout>)
    where
        Sout: ConvertibleSample,
    {
        impl_generic_ref_func!(self, buf, buf.copy_to_vec_interleaved(dst))
    }

    /// Copy all audio planes to discrete vectors.
    ///
    /// See [`AudioBuffer::copy_to_vecs_planar`] for full details.
    pub fn copy_to_vecs_planar<Sout>(&self, dst: &mut Vec<Vec<Sout>>)
    where
        Sout: ConvertibleSample,
    {
        impl_generic_ref_func!(self, buf, buf.copy_to_vecs_planar(dst))
    }

    /// Copy interleaved audio to the destination byte slice after converting to a different sample
    /// format.
    ///
    /// See [`AudioBuffer::copy_bytes_interleaved_as`] for full details.
    pub fn copy_bytes_interleaved_as<Sout, Dst>(&self, dst: Dst)
    where
        Sout: SampleBytes + ConvertibleSample,
        Dst: AsMut<[u8]>,
    {
        impl_generic_ref_func!(self, buf, buf.copy_bytes_interleaved_as::<Sout, _>(dst))
    }

    /// Copy planar audio as bytes to a destination slice per plane after converting to a different
    /// sample format.
    ///
    /// See [`AudioBuffer::copy_bytes_planar_as`] for full details.
    pub fn copy_bytes_planar_as<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: SampleBytes + ConvertibleSample,
        Dst: AsMut<[u8]>,
    {
        impl_generic_ref_func!(self, buf, buf.copy_bytes_planar_as::<Sout, _>(dst))
    }

    /// Copy interleaved audio to the destination byte slice.
    ///
    /// See [`AudioBuffer::copy_bytes_interleaved`] for full details.
    pub fn copy_bytes_interleaved<Dst>(&self, dst: Dst)
    where
        Dst: AsMut<[u8]>,
    {
        impl_generic_ref_func!(self, buf, buf.copy_bytes_interleaved(dst))
    }

    /// Copy planar audio as bytes to a destination slice per plane.
    ///
    /// See [`AudioBuffer::copy_bytes_planar`] for full details.
    pub fn copy_bytes_planar<Dst>(&self, dst: &mut [Dst])
    where
        Dst: AsMut<[u8]>,
    {
        impl_generic_ref_func!(self, buf, buf.copy_bytes_planar(dst))
    }

    /// Copy interleaved audio to the destination byte vector.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vec_interleaved`] for full details.
    pub fn copy_bytes_to_vec_interleaved(&self, dst: &mut Vec<u8>) {
        impl_generic_ref_func!(self, buf, buf.copy_bytes_to_vec_interleaved(dst))
    }

    /// Copy interleaved audio to the destination byte vector after converting to a different sample
    /// format.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vec_interleaved_as`] for full details.
    pub fn copy_bytes_to_vec_interleaved_as<Sout>(&self, dst: &mut Vec<u8>)
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_ref_func!(self, buf, buf.copy_bytes_to_vec_interleaved_as::<Sout>(dst))
    }

    /// Copy audio planes as bytes to discrete byte vectors.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vecs_planar`] for full details.
    pub fn copy_bytes_to_vecs_planar(&self, dst: &mut Vec<Vec<u8>>) {
        impl_generic_ref_func!(self, buf, buf.copy_bytes_to_vecs_planar(dst))
    }

    /// Copy audio planes as bytes to discrete byte vectors after converting to a different sample
    /// format.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vecs_planar_as`] for full details.
    pub fn copy_bytes_to_vecs_planar_as<Sout>(&self, dst: &mut Vec<Vec<u8>>)
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_ref_func!(self, buf, buf.copy_bytes_to_vecs_planar_as::<Sout>(dst))
    }

    /// Get the length in bytes of all samples if converted to a new sample format.
    pub fn byte_len_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_ref_func!(self, buf, buf.byte_len_as::<Sout>())
    }

    /// Get the length in bytes of all samples in a single plane if converted to a new sample
    /// format.
    pub fn byte_len_per_plane_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_ref_func!(self, buf, buf.byte_len_per_plane_as::<Sout>())
    }

    /// Get the length of bytes of a single interleaved audio frame if converted to a new sample
    /// format.
    pub fn byte_len_per_frame_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_ref_func!(self, buf, buf.byte_len_per_frame_as::<Sout>())
    }

    /// Get the length in bytes of all samples.
    pub fn byte_len(&self) -> usize {
        impl_generic_ref_func!(self, buf, buf.byte_len())
    }

    /// Get the length in bytes of all samples in a single plane.
    pub fn byte_len_per_plane(&self) -> usize {
        impl_generic_ref_func!(self, buf, buf.byte_len_per_plane())
    }

    /// Get the length of bytes of a single interleaved audio frame.
    pub fn byte_len_per_frame(&self) -> usize {
        impl_generic_ref_func!(self, buf, buf.byte_len_per_frame())
    }
}

/// A trait for generically borrowing an [`AudioBuffer`] by wrapping it in a
/// [`GenericAudioBufferRef`].
pub trait AsGenericAudioBufferRef {
    /// Get an immutable reference to the audio buffer as a generic audio buffer reference.
    fn as_generic_audio_buffer_ref(&self) -> GenericAudioBufferRef<'_>;
}

impl AsGenericAudioBufferRef for GenericAudioBuffer {
    fn as_generic_audio_buffer_ref(&self) -> GenericAudioBufferRef<'_> {
        impl_generic_func!(self, buf, buf.as_generic_audio_buffer_ref())
    }
}

// Implement AsicGenericAudioBufferRef for all AudioBuffers of standard sample formats.
macro_rules! impl_as_generic_audio_buffer_ref {
    ($fmt:ty, $ref:path) => {
        impl AsGenericAudioBufferRef for AudioBuffer<$fmt> {
            fn as_generic_audio_buffer_ref(&self) -> GenericAudioBufferRef<'_> {
                $ref(self)
            }
        }
    };
}

impl_as_generic_audio_buffer_ref!(u8, GenericAudioBufferRef::U8);
impl_as_generic_audio_buffer_ref!(u16, GenericAudioBufferRef::U16);
impl_as_generic_audio_buffer_ref!(u24, GenericAudioBufferRef::U24);
impl_as_generic_audio_buffer_ref!(u32, GenericAudioBufferRef::U32);
impl_as_generic_audio_buffer_ref!(i8, GenericAudioBufferRef::S8);
impl_as_generic_audio_buffer_ref!(i16, GenericAudioBufferRef::S16);
impl_as_generic_audio_buffer_ref!(i24, GenericAudioBufferRef::S24);
impl_as_generic_audio_buffer_ref!(i32, GenericAudioBufferRef::S32);
impl_as_generic_audio_buffer_ref!(f32, GenericAudioBufferRef::F32);
impl_as_generic_audio_buffer_ref!(f64, GenericAudioBufferRef::F64);

// Implement From for conversions between AudioBuffer and GenericAudioBuffer{Ref} for all
// standard sample formats.
macro_rules! impl_from_converions {
    ($fmt:ty, $own:path, $ref:path) => {
        // Infalliable conversion from AudioBuffer<S> to GenericAudioBuffer.
        impl From<AudioBuffer<$fmt>> for GenericAudioBuffer {
            fn from(value: AudioBuffer<$fmt>) -> Self {
                $own(value)
            }
        }

        // Falliable conversion from GenericAudioBuffer to AudioBuffer<S>.
        impl TryFrom<GenericAudioBuffer> for AudioBuffer<$fmt> {
            type Error = ();

            fn try_from(value: GenericAudioBuffer) -> Result<Self, Self::Error> {
                match value {
                    $own(buffer) => Ok(buffer),
                    _ => Err(()),
                }
            }
        }

        // Infalliable conversion from &AudioBuffer<S> to GenericAudioBufferRef.
        impl<'a> From<&'a AudioBuffer<$fmt>> for GenericAudioBufferRef<'a> {
            fn from(value: &'a AudioBuffer<$fmt>) -> Self {
                $ref(value)
            }
        }

        // Falliable conversion from GenericAudioBufferRef to &AudioBuffer<S>.
        impl<'a> TryFrom<GenericAudioBufferRef<'a>> for &'a AudioBuffer<$fmt> {
            type Error = ();

            fn try_from(value: GenericAudioBufferRef<'a>) -> Result<Self, Self::Error> {
                match value {
                    $ref(buffer) => Ok(buffer),
                    _ => Err(()),
                }
            }
        }
    };
}

impl_from_converions!(u8, GenericAudioBuffer::U8, GenericAudioBufferRef::U8);
impl_from_converions!(u16, GenericAudioBuffer::U16, GenericAudioBufferRef::U16);
impl_from_converions!(u24, GenericAudioBuffer::U24, GenericAudioBufferRef::U24);
impl_from_converions!(u32, GenericAudioBuffer::U32, GenericAudioBufferRef::U32);
impl_from_converions!(i8, GenericAudioBuffer::S8, GenericAudioBufferRef::S8);
impl_from_converions!(i16, GenericAudioBuffer::S16, GenericAudioBufferRef::S16);
impl_from_converions!(i24, GenericAudioBuffer::S24, GenericAudioBufferRef::S24);
impl_from_converions!(i32, GenericAudioBuffer::S32, GenericAudioBufferRef::S32);
impl_from_converions!(f32, GenericAudioBuffer::F32, GenericAudioBufferRef::F32);
impl_from_converions!(f64, GenericAudioBuffer::F64, GenericAudioBufferRef::F64);

/// An owning wrapper for an [`AudioSlice`] of any standard sample format.
///
/// Calls on this wrapper are dispatched to the underlying, wrapped, slice and are semantically
/// identical.
pub enum GenericAudioSlice<'a> {
    /// An unsigned 8-bit integer audio slice.
    U8(AudioSlice<'a, u8>),
    /// An unsigned 16-bit integer audio slice.
    U16(AudioSlice<'a, u16>),
    /// An unsigned 24-bit integer audio slice.
    U24(AudioSlice<'a, u24>),
    /// An unsigned 32-bit integer audio slice.
    U32(AudioSlice<'a, u32>),
    /// A signed 8-bit integer audio slice.
    S8(AudioSlice<'a, i8>),
    /// A signed 16-bit integer audio slice.
    S16(AudioSlice<'a, i16>),
    /// A signed 24-bit integer audio slice.
    S24(AudioSlice<'a, i24>),
    /// A signed 32-bit integer audio slice.
    S32(AudioSlice<'a, i32>),
    /// A single precision (32-bit) floating point audio slice.
    F32(AudioSlice<'a, f32>),
    /// A double precision (64-bit) floating point audio slice.
    F64(AudioSlice<'a, f64>),
}

macro_rules! impl_generic_slice_func {
    ($own:expr, $buf:ident, $expr:expr) => {
        match $own {
            GenericAudioSlice::U8($buf) => $expr,
            GenericAudioSlice::U16($buf) => $expr,
            GenericAudioSlice::U24($buf) => $expr,
            GenericAudioSlice::U32($buf) => $expr,
            GenericAudioSlice::S8($buf) => $expr,
            GenericAudioSlice::S16($buf) => $expr,
            GenericAudioSlice::S24($buf) => $expr,
            GenericAudioSlice::S32($buf) => $expr,
            GenericAudioSlice::F32($buf) => $expr,
            GenericAudioSlice::F64($buf) => $expr,
        }
    };
}

impl<'a> GenericAudioSlice<'a> {
    /// Get the audio specification.
    pub fn spec(&self) -> &AudioSpec {
        impl_generic_slice_func!(self, slice, slice.spec())
    }

    /// Get the total number of audio planes.
    pub fn num_planes(&self) -> usize {
        impl_generic_slice_func!(self, slice, slice.num_planes())
    }

    /// Returns `true` if there are no audio frames.
    pub fn is_empty(&self) -> bool {
        impl_generic_slice_func!(self, slice, slice.is_empty())
    }

    /// Gets the number of audio frames in the slice.
    pub fn frames(&self) -> usize {
        impl_generic_slice_func!(self, slice, slice.frames())
    }

    /// Get the total number of samples contained in all audio planes.
    pub fn samples_interleaved(&self) -> usize {
        self.num_planes() * self.frames()
    }

    /// Get the total number of samples contained in each audio plane.
    pub fn samples_planar(&self) -> usize {
        self.frames()
    }

    /// Copy audio to a mutable audio slice while doing any necessary sample format conversions.
    pub fn copy_to<Sout, Dst>(&self, dst: &mut Dst)
    where
        Sout: ConvertibleSample,
        Dst: AudioMut<Sout>,
    {
        impl_generic_slice_func!(self, slice, dst.copy_from(slice));
    }

    /// Copy all audio frames to a slice of samples in interleaved order.
    ///
    /// See [`AudioBuffer::copy_to_slice_interleaved`] for full details.
    pub fn copy_to_slice_interleaved<Sout, Dst>(&self, dst: Dst)
    where
        Sout: ConvertibleSample,
        Dst: AsMut<[Sout]>,
    {
        impl_generic_slice_func!(self, slice, slice.copy_to_slice_interleaved(dst))
    }

    /// Copy all audio planes to discrete slices.
    ///
    /// See [`AudioBuffer::copy_to_slice_planar`] for full details.
    pub fn copy_to_slice_planar<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: ConvertibleSample,
        Dst: AsMut<[Sout]>,
    {
        impl_generic_slice_func!(self, slice, slice.copy_to_slice_planar(dst))
    }

    /// Copy all audio frames to a vector of samples in interleaved order.
    ///
    /// See [`AudioBuffer::copy_to_vec_interleaved`] for full details.
    pub fn copy_to_vec_interleaved<Sout>(&self, dst: &mut Vec<Sout>)
    where
        Sout: ConvertibleSample,
    {
        impl_generic_slice_func!(self, slice, slice.copy_to_vec_interleaved(dst))
    }

    /// Copy all audio planes to discrete vectors.
    ///
    /// See [`AudioBuffer::copy_to_vecs_planar`] for full details.
    pub fn copy_to_vecs_planar<Sout>(&self, dst: &mut Vec<Vec<Sout>>)
    where
        Sout: ConvertibleSample,
    {
        impl_generic_slice_func!(self, slice, slice.copy_to_vecs_planar(dst))
    }

    /// Copy interleaved audio to the destination byte slice after converting to a different sample
    /// format.
    ///
    /// See [`AudioBuffer::copy_bytes_interleaved_as`] for full details.
    pub fn copy_bytes_interleaved_as<Sout, Dst>(&self, dst: Dst)
    where
        Sout: SampleBytes + ConvertibleSample,
        Dst: AsMut<[u8]>,
    {
        impl_generic_slice_func!(self, slice, slice.copy_bytes_interleaved_as::<Sout, _>(dst))
    }

    /// Copy planar audio as bytes to a destination slice per plane after converting to a different
    /// sample format.
    ///
    /// See [`AudioBuffer::copy_bytes_planar_as`] for full details.
    pub fn copy_bytes_planar_as<Sout, Dst>(&self, dst: &mut [Dst])
    where
        Sout: SampleBytes + ConvertibleSample,
        Dst: AsMut<[u8]>,
    {
        impl_generic_slice_func!(self, slice, slice.copy_bytes_planar_as::<Sout, _>(dst))
    }

    /// Copy interleaved audio to the destination byte slice.
    ///
    /// See [`AudioBuffer::copy_bytes_interleaved`] for full details.
    pub fn copy_bytes_interleaved<Dst>(&self, dst: Dst)
    where
        Dst: AsMut<[u8]>,
    {
        impl_generic_slice_func!(self, slice, slice.copy_bytes_interleaved(dst))
    }

    /// Copy planar audio as bytes to a destination slice per plane.
    ///
    /// See [`AudioBuffer::copy_bytes_planar`] for full details.
    pub fn copy_bytes_planar<Dst>(&self, dst: &mut [Dst])
    where
        Dst: AsMut<[u8]>,
    {
        impl_generic_slice_func!(self, slice, slice.copy_bytes_planar(dst))
    }

    /// Copy interleaved audio to the destination byte vector.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vec_interleaved`] for full details.
    pub fn copy_bytes_to_vec_interleaved(&self, dst: &mut Vec<u8>) {
        impl_generic_slice_func!(self, slice, slice.copy_bytes_to_vec_interleaved(dst))
    }

    /// Copy interleaved audio to the destination byte vector after converting to a different sample
    /// format.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vec_interleaved_as`] for full details.
    pub fn copy_bytes_to_vec_interleaved_as<Sout>(&self, dst: &mut Vec<u8>)
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_slice_func!(self, slice, slice.copy_bytes_to_vec_interleaved_as::<Sout>(dst))
    }

    /// Copy audio planes as bytes to discrete byte vectors.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vecs_planar`] for full details.
    pub fn copy_bytes_to_vecs_planar(&self, dst: &mut Vec<Vec<u8>>) {
        impl_generic_slice_func!(self, slice, slice.copy_bytes_to_vecs_planar(dst))
    }

    /// Copy audio planes as bytes to discrete byte vectors after converting to a different sample
    /// format.
    ///
    /// See [`AudioBuffer::copy_bytes_to_vecs_planar_as`] for full details.
    pub fn copy_bytes_to_vecs_planar_as<Sout>(&self, dst: &mut Vec<Vec<u8>>)
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_slice_func!(self, slice, slice.copy_bytes_to_vecs_planar_as::<Sout>(dst))
    }

    /// Get the length in bytes of all samples if converted to a new sample format.
    pub fn byte_len_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_slice_func!(self, slice, slice.byte_len_as::<Sout>())
    }

    /// Get the length in bytes of all samples in a single plane if converted to a new sample
    /// format.
    pub fn byte_len_per_plane_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_slice_func!(self, slice, slice.byte_len_per_plane_as::<Sout>())
    }

    /// Get the length of bytes of a single interleaved audio frame if converted to a new sample
    /// format.
    pub fn byte_len_per_frame_as<Sout>(&self) -> usize
    where
        Sout: SampleBytes + ConvertibleSample,
    {
        impl_generic_slice_func!(self, slice, slice.byte_len_per_frame_as::<Sout>())
    }

    /// Get the length in bytes of all samples.
    pub fn byte_len(&self) -> usize {
        impl_generic_slice_func!(self, slice, slice.byte_len())
    }

    /// Get the length in bytes of all samples in a single plane.
    pub fn byte_len_per_plane(&self) -> usize {
        impl_generic_slice_func!(self, slice, slice.byte_len_per_plane())
    }

    /// Get the length of bytes of a single interleaved audio frame.
    pub fn byte_len_per_frame(&self) -> usize {
        impl_generic_slice_func!(self, slice, slice.byte_len_per_frame())
    }
}
