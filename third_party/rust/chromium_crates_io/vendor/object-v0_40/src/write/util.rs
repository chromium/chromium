use alloc::vec::Vec;
#[cfg(feature = "std")]
use std::{io, mem};

use crate::constants::Wrap;
use crate::endian::{Endian, U16, U32, U64};
use crate::pod::{Pod, bytes_of, bytes_of_slice};

/// Trait for writable buffer.
///
/// This is a low-level, append-only sink. Callers that need to know the current
/// write offset should wrap the buffer in a [`CountingBuffer`], which counts the
/// number of bytes written.
pub trait WritableBuffer {
    /// Reserves capacity for `size` bytes.
    ///
    /// When a writer calls this, it does so exactly once, before writing any bytes,
    /// with `size` equal to the exact total number of bytes it will write.
    /// A writer targeting a [`GrowableBuffer`] may skip this call.
    /// Whether a writer calls this is part of that writer's contract.
    fn reserve(&mut self, size: u64) -> Result<(), ()>;

    /// Writes the specified slice of bytes at the end of the buffer.
    fn write_bytes(&mut self, val: &[u8]);

    /// Writes `additional` zero bytes at the end of the buffer.
    fn write_zeros(&mut self, mut additional: u64) {
        while additional > 0 {
            let write_amt = additional.min(1024) as usize;
            self.write_bytes(&[0; 1024][..write_amt]);
            additional -= write_amt as u64;
        }
    }
}

/// Extension methods for [`WritableBuffer`].
///
/// These are provided as a separate trait so that they can be used with `?Sized`.
pub trait WritableBufferExt: WritableBuffer {
    /// Writes the specified `Pod` type at the end of the buffer.
    fn write_pod<T: Pod>(&mut self, val: &T) {
        self.write_bytes(bytes_of(val))
    }

    /// Writes the specified `Pod` slice at the end of the buffer.
    fn write_pod_slice<T: Pod>(&mut self, val: &[T]) {
        self.write_bytes(bytes_of_slice(val))
    }

    /// Write a `u16` with the specified endianness at the end of the buffer.
    fn write_u16<E, T>(&mut self, endian: E, val: T)
    where
        E: Endian,
        T: Wrap<Inner = u16> + Copy + 'static,
    {
        self.write_bytes(bytes_of(&U16::new(endian, val)))
    }

    /// Write a `u32` with the specified endianness at the end of the buffer.
    fn write_u32<E, T>(&mut self, endian: E, val: T)
    where
        E: Endian,
        T: Wrap<Inner = u32> + Copy + 'static,
    {
        self.write_bytes(bytes_of(&U32::new(endian, val)))
    }

    /// Write a `u64` with the specified endianness at the end of the buffer.
    fn write_u64<E, T>(&mut self, endian: E, val: T)
    where
        E: Endian,
        T: Wrap<Inner = u64> + Copy + 'static,
    {
        self.write_bytes(bytes_of(&U64::new(endian, val)))
    }
}

impl<W: WritableBuffer + ?Sized> WritableBufferExt for W {}

impl<W: WritableBuffer + ?Sized> WritableBuffer for &mut W {
    #[inline]
    fn reserve(&mut self, size: u64) -> Result<(), ()> {
        (**self).reserve(size)
    }

    #[inline]
    fn write_bytes(&mut self, val: &[u8]) {
        (**self).write_bytes(val)
    }

    #[inline]
    fn write_zeros(&mut self, additional: u64) {
        (**self).write_zeros(additional)
    }
}

/// A [`WritableBuffer`] that can grow its capacity while writing.
///
/// [`WritableBuffer::reserve`] may still be called but this is not required.
/// Writes will automatically increase the capacity of the buffer.
pub trait GrowableBuffer: WritableBuffer {
    /// Upcast to a [`WritableBuffer`] trait object.
    //
    // Manual upcast because trait upcasting coercion requires MSRV 1.86.
    fn as_writable(&mut self) -> &mut dyn WritableBuffer;
}

/// Wraps a [`WritableBuffer`] and counts the number of bytes written.
///
/// The user is responsible for calling [`WritableBuffer::reserve`] if required (either
/// before creating the `CountingBuffer`, or via `CountingBuffer`'s implementation).
pub struct CountingBuffer<W> {
    buffer: W,
    count: u64,
}

impl<W> core::fmt::Debug for CountingBuffer<W> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("CountingBuffer")
            .field("count", &self.count)
            .finish_non_exhaustive()
    }
}

impl<W> CountingBuffer<W> {
    /// Create a new `CountingBuffer` wrapping the given buffer, starting at count 0.
    pub fn new(buffer: W) -> Self {
        CountingBuffer { buffer, count: 0 }
    }

    /// Unwraps this `CountingBuffer` giving back the original buffer.
    pub fn into_inner(self) -> W {
        self.buffer
    }

    /// Returns the number of bytes that have been written.
    pub fn count(&self) -> u64 {
        self.count
    }
}

impl<W: WritableBuffer> CountingBuffer<W> {
    /// Writes zero bytes until the total count written reaches `new_len`.
    pub fn resize(&mut self, new_len: u64) {
        debug_assert!(new_len >= self.count);
        self.write_zeros(new_len.saturating_sub(self.count));
    }

    /// Writes zero bytes until the total count written is aligned to `size`.
    pub fn write_align(&mut self, size: u64) {
        self.resize(align(self.count, size));
    }
}

impl<W: WritableBuffer> WritableBuffer for CountingBuffer<W> {
    #[inline]
    fn reserve(&mut self, size: u64) -> Result<(), ()> {
        self.buffer.reserve(size)
    }

    #[inline]
    fn write_bytes(&mut self, val: &[u8]) {
        self.buffer.write_bytes(val);
        self.count += val.len() as u64;
    }

    #[inline]
    fn write_zeros(&mut self, additional: u64) {
        self.buffer.write_zeros(additional);
        self.count += additional;
    }
}

impl GrowableBuffer for Vec<u8> {
    fn as_writable(&mut self) -> &mut dyn WritableBuffer {
        self
    }
}

impl WritableBuffer for Vec<u8> {
    #[inline]
    fn reserve(&mut self, size: u64) -> Result<(), ()> {
        debug_assert!(self.is_empty());
        let size = usize::try_from(size).map_err(|_| ())?;
        self.reserve(size);
        Ok(())
    }

    #[inline]
    fn write_bytes(&mut self, val: &[u8]) {
        self.extend_from_slice(val)
    }

    #[inline]
    fn write_zeros(&mut self, additional: u64) {
        let new_len = self.len() + additional as usize;
        self.resize(new_len, 0);
    }
}

/// A fixed-size buffer.
///
/// `reserve` returns an error if the slice is too small. The slice is allowed to be
/// larger than needed, but trailing bytes are left untouched. Writing more bytes than
/// the slice can hold will panic.
impl WritableBuffer for &mut [u8] {
    #[inline]
    fn reserve(&mut self, size: u64) -> Result<(), ()> {
        if size > self.len() as u64 {
            return Err(());
        }
        Ok(())
    }

    #[inline]
    fn write_bytes(&mut self, val: &[u8]) {
        let (head, tail) = core::mem::take(self).split_at_mut(val.len());
        head.copy_from_slice(val);
        *self = tail;
    }

    #[inline]
    fn write_zeros(&mut self, additional: u64) {
        let (head, tail) = core::mem::take(self).split_at_mut(additional as usize);
        head.fill(0);
        *self = tail;
    }
}

/// A [`WritableBuffer`] that streams data to a [`Write`](std::io::Write) implementation.
///
/// [`Self::result`] must be called to determine if an I/O error occurred during writing.
/// Alternatively, [`Self::flush`] will both check for errors and flush.
///
/// It is advisable to use a buffered writer like [`BufWriter`](std::io::BufWriter)
/// instead of an unbuffered writer like [`File`](std::fs::File).
#[cfg(feature = "std")]
#[derive(Debug)]
pub struct StreamingBuffer<W> {
    writer: W,
    result: Result<(), io::Error>,
}

#[cfg(feature = "std")]
impl<W> StreamingBuffer<W> {
    /// Create a new `StreamingBuffer` backed by the given writer.
    pub fn new(writer: W) -> Self {
        StreamingBuffer {
            writer,
            result: Ok(()),
        }
    }

    /// Unwraps this [`StreamingBuffer`] giving back the original writer.
    pub fn into_inner(self) -> W {
        self.writer
    }

    /// Returns any error that occurred during writing.
    pub fn result(&mut self) -> Result<(), io::Error> {
        mem::replace(&mut self.result, Ok(()))
    }
}

#[cfg(feature = "std")]
impl<W: io::Write> StreamingBuffer<W> {
    /// Flushes after first checking if any error previously occurred during writing.
    pub fn flush(&mut self) -> Result<(), io::Error> {
        self.result()?;
        self.writer.flush()
    }
}

#[cfg(feature = "std")]
impl<W: io::Write> GrowableBuffer for StreamingBuffer<W> {
    fn as_writable(&mut self) -> &mut dyn WritableBuffer {
        self
    }
}

#[cfg(feature = "std")]
impl<W: io::Write> WritableBuffer for StreamingBuffer<W> {
    #[inline]
    fn reserve(&mut self, _size: u64) -> Result<(), ()> {
        Ok(())
    }

    #[inline]
    fn write_bytes(&mut self, val: &[u8]) {
        if self.result.is_ok() {
            self.result = self.writer.write_all(val);
        }
    }
}

/// Write an unsigned number using the LEB128 encoding to a buffer.
///
/// Returns the number of bytes written.
#[allow(dead_code)]
pub(crate) fn write_uleb128(buf: &mut Vec<u8>, mut val: u64) -> usize {
    let mut len = 0;
    loop {
        let mut byte = (val & 0x7f) as u8;
        val >>= 7;
        let done = val == 0;
        if !done {
            byte |= 0x80;
        }

        buf.push(byte);
        len += 1;

        if done {
            return len;
        }
    }
}

/// Write a signed number using the LEB128 encoding to a buffer.
///
/// Returns the number of bytes written.
#[allow(dead_code)]
pub(crate) fn write_sleb128(buf: &mut Vec<u8>, mut val: i64) -> usize {
    let mut len = 0;
    loop {
        let mut byte = val as u8;
        // Keep the sign bit for testing
        val >>= 6;
        let done = val == 0 || val == -1;
        if done {
            byte &= !0x80;
        } else {
            // Remove the sign bit
            val >>= 1;
            byte |= 0x80;
        }

        buf.push(byte);
        len += 1;

        if done {
            return len;
        }
    }
}

#[allow(dead_code)]
pub(crate) fn align_u32(offset: u32, size: u32) -> u32 {
    (offset + (size - 1)) & !(size - 1)
}

#[allow(dead_code)]
pub(crate) fn align(offset: u64, size: u64) -> u64 {
    (offset + (size - 1)) & !(size - 1)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn slice_buffer() {
        let mut data = [0u8; 4];
        assert_eq!((&mut data[..]).reserve(5), Err(()));
        assert_eq!((&mut data[..]).reserve(4), Ok(()));
        assert_eq!((&mut data[..]).reserve(3), Ok(()));

        let mut data = [0xffu8; 9];
        let mut slice = &mut data[..];
        let mut buffer = CountingBuffer::new(&mut slice);
        buffer.write_bytes(&[1, 2, 3]);
        assert_eq!(buffer.count(), 3);
        buffer.write_zeros(2);
        assert_eq!(buffer.count(), 5);
        buffer.write_align(4);
        assert_eq!(buffer.count(), 8);
        assert_eq!(data, [1, 2, 3, 0, 0, 0, 0, 0, 0xff]);
    }
}
