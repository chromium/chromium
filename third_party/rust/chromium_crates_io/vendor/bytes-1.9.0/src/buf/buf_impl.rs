#[cfg(feature = "std")]
use crate::buf::{reader, Reader};
use crate::buf::{take, Chain, Take};
#[cfg(feature = "std")]
use crate::{min_u64_usize, saturating_sub_usize_u64};
use crate::{panic_advance, panic_does_not_fit};

#[cfg(feature = "std")]
use std::io::IoSlice;

use alloc::boxed::Box;

macro_rules! buf_get_impl {
    ($this:ident, $typ:tt::$conv:tt) => {{
        const SIZE: usize = core::mem::size_of::<$typ>();

        if $this.remaining() < SIZE {
            panic_advance(SIZE, $this.remaining());
        }

        // try to convert directly from the bytes
        // this Option<ret> trick is to avoid keeping a borrow on self
        // when advance() is called (mut borrow) and to call bytes() only once
        let ret = $this
            .chunk()
            .get(..SIZE)
            .map(|src| unsafe { $typ::$conv(*(src as *const _ as *const [_; SIZE])) });

        if let Some(ret) = ret {
            // if the direct conversion was possible, advance and return
            $this.advance(SIZE);
            return ret;
        } else {
            // if not we copy the bytes in a temp buffer then convert
            let mut buf = [0; SIZE];
            $this.copy_to_slice(&mut buf); // (do the advance)
            return $typ::$conv(buf);
        }
    }};
    (le => $this:ident, $typ:tt, $len_to_read:expr) => {{
        const SIZE: usize = core::mem::size_of::<$typ>();

        // The same trick as above does not improve the best case speed.
        // It seems to be linked to the way the method is optimised by the compiler
        let mut buf = [0; SIZE];

        let subslice = match buf.get_mut(..$len_to_read) {
            Some(subslice) => subslice,
            None => panic_does_not_fit(SIZE, $len_to_read),
        };

        $this.copy_to_slice(subslice);
        return $typ::from_le_bytes(buf);
    }};
    (be => $this:ident, $typ:tt, $len_to_read:expr) => {{
        const SIZE: usize = core::mem::size_of::<$typ>();

        let slice_at = match SIZE.checked_sub($len_to_read) {
            Some(slice_at) => slice_at,
            None => panic_does_not_fit(SIZE, $len_to_read),
        };

        let mut buf = [0; SIZE];
        $this.copy_to_slice(&mut buf[slice_at..]);
        return $typ::from_be_bytes(buf);
    }};
}

// https://en.wikipedia.org/wiki/Sign_extension
fn sign_extend(val: u64, nbytes: usize) -> i64 {
    let shift = (8 - nbytes) * 8;
    (val << shift) as i64 >> shift
}

/// Read bytes from a buffer.
///
/// A buffer stores bytes in memory such that read operations are infallible.
/// The underlying storage may or may not be in contiguous memory. A `Buf` value
/// is a cursor into the buffer. Reading from `Buf` advances the cursor
/// position. It can be thought of as an efficient `Iterator` for collections of
/// bytes.
///
/// The simplest `Buf` is a `&[u8]`.
///
/// ```
/// use bytes::Buf;
///
/// let mut buf = &b"hello world"[..];
///
/// assert_eq!(b'h', buf.get_u8());
/// assert_eq!(b'e', buf.get_u8());
/// assert_eq!(b'l', buf.get_u8());
///
/// let mut rest = [0; 8];
/// buf.copy_to_slice(&mut rest);
///
/// assert_eq!(&rest[..], &b"lo world"[..]);
/// ```
pub trait Buf {
    /// Returns the number of bytes between the current position and the end of
    /// the buffer.
    ///
    /// This value is greater than or equal to the length of the slice returned
    /// by `chunk()`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"hello world"[..];
    ///
    /// assert_eq!(buf.remaining(), 11);
    ///
    /// buf.get_u8();
    ///
    /// assert_eq!(buf.remaining(), 10);
    /// ```
    ///
    /// # Implementer notes
    ///
    /// Implementations of `remaining` should ensure that the return value does
    /// not change unless a call is made to `advance` or any other function that
    /// is documented to change the `Buf`'s current position.
    fn remaining(&self) -> usize;

    /// Returns a slice starting at the current position and of length between 0
    /// and `Buf::remaining()`. Note that this *can* return a shorter slice (this
    /// allows non-continuous internal representation).
    ///
    /// This is a lower level function. Most operations are done with other
    /// functions.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"hello world"[..];
    ///
    /// assert_eq!(buf.chunk(), &b"hello world"[..]);
    ///
    /// buf.advance(6);
    ///
    /// assert_eq!(buf.chunk(), &b"world"[..]);
    /// ```
    ///
    /// # Implementer notes
    ///
    /// This function should never panic. `chunk()` should return an empty
    /// slice **if and only if** `remaining()` returns 0. In other words,
    /// `chunk()` returning an empty slice implies that `remaining()` will
    /// return 0 and `remaining()` returning 0 implies that `chunk()` will
    /// return an empty slice.
    // The `chunk` method was previously called `bytes`. This alias makes the rename
    // more easily discoverable.
    #[cfg_attr(docsrs, doc(alias = "bytes"))]
    fn chunk(&self) -> &[u8];

    /// Fills `dst` with potentially multiple slices starting at `self`'s
    /// current position.
    ///
    /// If the `Buf` is backed by disjoint slices of bytes, `chunk_vectored` enables
    /// fetching more than one slice at once. `dst` is a slice of `IoSlice`
    /// references, enabling the slice to be directly used with [`writev`]
    /// without any further conversion. The sum of the lengths of all the
    /// buffers in `dst` will be less than or equal to `Buf::remaining()`.
    ///
    /// The entries in `dst` will be overwritten, but the data **contained** by
    /// the slices **will not** be modified. If `chunk_vectored` does not fill every
    /// entry in `dst`, then `dst` is guaranteed to contain all remaining slices
    /// in `self.
    ///
    /// This is a lower level function. Most operations are done with other
    /// functions.
    ///
    /// # Implementer notes
    ///
    /// This function should never panic. Once the end of the buffer is reached,
    /// i.e., `Buf::remaining` returns 0, calls to `chunk_vectored` must return 0
    /// without mutating `dst`.
    ///
    /// Implementations should also take care to properly handle being called
    /// with `dst` being a zero length slice.
    ///
    /// [`writev`]: http://man7.org/linux/man-pages/man2/readv.2.html
    #[cfg(feature = "std")]
    #[cfg_attr(docsrs, doc(cfg(feature = "std")))]
    fn chunks_vectored<'a>(&'a self, dst: &mut [IoSlice<'a>]) -> usize {
        if dst.is_empty() {
            return 0;
        }

        if self.has_remaining() {
            dst[0] = IoSlice::new(self.chunk());
            1
        } else {
            0
        }
    }

    /// Advance the internal cursor of the Buf
    ///
    /// The next call to `chunk()` will return a slice starting `cnt` bytes
    /// further into the underlying buffer.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"hello world"[..];
    ///
    /// assert_eq!(buf.chunk(), &b"hello world"[..]);
    ///
    /// buf.advance(6);
    ///
    /// assert_eq!(buf.chunk(), &b"world"[..]);
    /// ```
    ///
    /// # Panics
    ///
    /// This function **may** panic if `cnt > self.remaining()`.
    ///
    /// # Implementer notes
    ///
    /// It is recommended for implementations of `advance` to panic if `cnt >
    /// self.remaining()`. If the implementation does not panic, the call must
    /// behave as if `cnt == self.remaining()`.
    ///
    /// A call with `cnt == 0` should never panic and be a no-op.
    fn advance(&mut self, cnt: usize);

    /// Returns true if there are any more bytes to consume
    ///
    /// This is equivalent to `self.remaining() != 0`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"a"[..];
    ///
    /// assert!(buf.has_remaining());
    ///
    /// buf.get_u8();
    ///
    /// assert!(!buf.has_remaining());
    /// ```
    fn has_remaining(&self) -> bool {
        self.remaining() > 0
    }

    /// Copies bytes from `self` into `dst`.
    ///
    /// The cursor is advanced by the number of bytes copied. `self` must have
    /// enough remaining bytes to fill `dst`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"hello world"[..];
    /// let mut dst = [0; 5];
    ///
    /// buf.copy_to_slice(&mut dst);
    /// assert_eq!(&b"hello"[..], &dst);
    /// assert_eq!(6, buf.remaining());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if `self.remaining() < dst.len()`.
    fn copy_to_slice(&mut self, mut dst: &mut [u8]) {
        if self.remaining() < dst.len() {
            panic_advance(dst.len(), self.remaining());
        }

        while !dst.is_empty() {
            let src = self.chunk();
            let cnt = usize::min(src.len(), dst.len());

            dst[..cnt].copy_from_slice(&src[..cnt]);
            dst = &mut dst[cnt..];

            self.advance(cnt);
        }
    }

    /// Gets an unsigned 8 bit integer from `self`.
    ///
    /// The current position is advanced by 1.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x08 hello"[..];
    /// assert_eq!(8, buf.get_u8());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is no more remaining data in `self`.
    fn get_u8(&mut self) -> u8 {
        if self.remaining() < 1 {
            panic_advance(1, 0);
        }
        let ret = self.chunk()[0];
        self.advance(1);
        ret
    }

    /// Gets a signed 8 bit integer from `self`.
    ///
    /// The current position is advanced by 1.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x08 hello"[..];
    /// assert_eq!(8, buf.get_i8());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is no more remaining data in `self`.
    fn get_i8(&mut self) -> i8 {
        if self.remaining() < 1 {
            panic_advance(1, 0);
        }
        let ret = self.chunk()[0] as i8;
        self.advance(1);
        ret
    }

    /// Gets an unsigned 16 bit integer from `self` in big-endian byte order.
    ///
    /// The current position is advanced by 2.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x08\x09 hello"[..];
    /// assert_eq!(0x0809, buf.get_u16());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u16(&mut self) -> u16 {
        buf_get_impl!(self, u16::from_be_bytes);
    }

    /// Gets an unsigned 16 bit integer from `self` in little-endian byte order.
    ///
    /// The current position is advanced by 2.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x09\x08 hello"[..];
    /// assert_eq!(0x0809, buf.get_u16_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u16_le(&mut self) -> u16 {
        buf_get_impl!(self, u16::from_le_bytes);
    }

    /// Gets an unsigned 16 bit integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by 2.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x08\x09 hello",
    ///     false => b"\x09\x08 hello",
    /// };
    /// assert_eq!(0x0809, buf.get_u16_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u16_ne(&mut self) -> u16 {
        buf_get_impl!(self, u16::from_ne_bytes);
    }

    /// Gets a signed 16 bit integer from `self` in big-endian byte order.
    ///
    /// The current position is advanced by 2.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x08\x09 hello"[..];
    /// assert_eq!(0x0809, buf.get_i16());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i16(&mut self) -> i16 {
        buf_get_impl!(self, i16::from_be_bytes);
    }

    /// Gets a signed 16 bit integer from `self` in little-endian byte order.
    ///
    /// The current position is advanced by 2.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x09\x08 hello"[..];
    /// assert_eq!(0x0809, buf.get_i16_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i16_le(&mut self) -> i16 {
        buf_get_impl!(self, i16::from_le_bytes);
    }

    /// Gets a signed 16 bit integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by 2.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x08\x09 hello",
    ///     false => b"\x09\x08 hello",
    /// };
    /// assert_eq!(0x0809, buf.get_i16_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i16_ne(&mut self) -> i16 {
        buf_get_impl!(self, i16::from_ne_bytes);
    }

    /// Gets an unsigned 32 bit integer from `self` in the big-endian byte order.
    ///
    /// The current position is advanced by 4.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x08\x09\xA0\xA1 hello"[..];
    /// assert_eq!(0x0809A0A1, buf.get_u32());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u32(&mut self) -> u32 {
        buf_get_impl!(self, u32::from_be_bytes);
    }

    /// Gets an unsigned 32 bit integer from `self` in the little-endian byte order.
    ///
    /// The current position is advanced by 4.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\xA1\xA0\x09\x08 hello"[..];
    /// assert_eq!(0x0809A0A1, buf.get_u32_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u32_le(&mut self) -> u32 {
        buf_get_impl!(self, u32::from_le_bytes);
    }

    /// Gets an unsigned 32 bit integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by 4.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x08\x09\xA0\xA1 hello",
    ///     false => b"\xA1\xA0\x09\x08 hello",
    /// };
    /// assert_eq!(0x0809A0A1, buf.get_u32_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u32_ne(&mut self) -> u32 {
        buf_get_impl!(self, u32::from_ne_bytes);
    }

    /// Gets a signed 32 bit integer from `self` in big-endian byte order.
    ///
    /// The current position is advanced by 4.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x08\x09\xA0\xA1 hello"[..];
    /// assert_eq!(0x0809A0A1, buf.get_i32());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i32(&mut self) -> i32 {
        buf_get_impl!(self, i32::from_be_bytes);
    }

    /// Gets a signed 32 bit integer from `self` in little-endian byte order.
    ///
    /// The current position is advanced by 4.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\xA1\xA0\x09\x08 hello"[..];
    /// assert_eq!(0x0809A0A1, buf.get_i32_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i32_le(&mut self) -> i32 {
        buf_get_impl!(self, i32::from_le_bytes);
    }

    /// Gets a signed 32 bit integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by 4.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x08\x09\xA0\xA1 hello",
    ///     false => b"\xA1\xA0\x09\x08 hello",
    /// };
    /// assert_eq!(0x0809A0A1, buf.get_i32_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i32_ne(&mut self) -> i32 {
        buf_get_impl!(self, i32::from_ne_bytes);
    }

    /// Gets an unsigned 64 bit integer from `self` in big-endian byte order.
    ///
    /// The current position is advanced by 8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x01\x02\x03\x04\x05\x06\x07\x08 hello"[..];
    /// assert_eq!(0x0102030405060708, buf.get_u64());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u64(&mut self) -> u64 {
        buf_get_impl!(self, u64::from_be_bytes);
    }

    /// Gets an unsigned 64 bit integer from `self` in little-endian byte order.
    ///
    /// The current position is advanced by 8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x08\x07\x06\x05\x04\x03\x02\x01 hello"[..];
    /// assert_eq!(0x0102030405060708, buf.get_u64_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u64_le(&mut self) -> u64 {
        buf_get_impl!(self, u64::from_le_bytes);
    }

    /// Gets an unsigned 64 bit integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by 8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x01\x02\x03\x04\x05\x06\x07\x08 hello",
    ///     false => b"\x08\x07\x06\x05\x04\x03\x02\x01 hello",
    /// };
    /// assert_eq!(0x0102030405060708, buf.get_u64_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u64_ne(&mut self) -> u64 {
        buf_get_impl!(self, u64::from_ne_bytes);
    }

    /// Gets a signed 64 bit integer from `self` in big-endian byte order.
    ///
    /// The current position is advanced by 8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x01\x02\x03\x04\x05\x06\x07\x08 hello"[..];
    /// assert_eq!(0x0102030405060708, buf.get_i64());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i64(&mut self) -> i64 {
        buf_get_impl!(self, i64::from_be_bytes);
    }

    /// Gets a signed 64 bit integer from `self` in little-endian byte order.
    ///
    /// The current position is advanced by 8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x08\x07\x06\x05\x04\x03\x02\x01 hello"[..];
    /// assert_eq!(0x0102030405060708, buf.get_i64_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i64_le(&mut self) -> i64 {
        buf_get_impl!(self, i64::from_le_bytes);
    }

    /// Gets a signed 64 bit integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by 8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x01\x02\x03\x04\x05\x06\x07\x08 hello",
    ///     false => b"\x08\x07\x06\x05\x04\x03\x02\x01 hello",
    /// };
    /// assert_eq!(0x0102030405060708, buf.get_i64_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i64_ne(&mut self) -> i64 {
        buf_get_impl!(self, i64::from_ne_bytes);
    }

    /// Gets an unsigned 128 bit integer from `self` in big-endian byte order.
    ///
    /// The current position is advanced by 16.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16 hello"[..];
    /// assert_eq!(0x01020304050607080910111213141516, buf.get_u128());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u128(&mut self) -> u128 {
        buf_get_impl!(self, u128::from_be_bytes);
    }

    /// Gets an unsigned 128 bit integer from `self` in little-endian byte order.
    ///
    /// The current position is advanced by 16.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x16\x15\x14\x13\x12\x11\x10\x09\x08\x07\x06\x05\x04\x03\x02\x01 hello"[..];
    /// assert_eq!(0x01020304050607080910111213141516, buf.get_u128_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u128_le(&mut self) -> u128 {
        buf_get_impl!(self, u128::from_le_bytes);
    }

    /// Gets an unsigned 128 bit integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by 16.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16 hello",
    ///     false => b"\x16\x15\x14\x13\x12\x11\x10\x09\x08\x07\x06\x05\x04\x03\x02\x01 hello",
    /// };
    /// assert_eq!(0x01020304050607080910111213141516, buf.get_u128_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_u128_ne(&mut self) -> u128 {
        buf_get_impl!(self, u128::from_ne_bytes);
    }

    /// Gets a signed 128 bit integer from `self` in big-endian byte order.
    ///
    /// The current position is advanced by 16.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16 hello"[..];
    /// assert_eq!(0x01020304050607080910111213141516, buf.get_i128());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i128(&mut self) -> i128 {
        buf_get_impl!(self, i128::from_be_bytes);
    }

    /// Gets a signed 128 bit integer from `self` in little-endian byte order.
    ///
    /// The current position is advanced by 16.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x16\x15\x14\x13\x12\x11\x10\x09\x08\x07\x06\x05\x04\x03\x02\x01 hello"[..];
    /// assert_eq!(0x01020304050607080910111213141516, buf.get_i128_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i128_le(&mut self) -> i128 {
        buf_get_impl!(self, i128::from_le_bytes);
    }

    /// Gets a signed 128 bit integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by 16.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16 hello",
    ///     false => b"\x16\x15\x14\x13\x12\x11\x10\x09\x08\x07\x06\x05\x04\x03\x02\x01 hello",
    /// };
    /// assert_eq!(0x01020304050607080910111213141516, buf.get_i128_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_i128_ne(&mut self) -> i128 {
        buf_get_impl!(self, i128::from_ne_bytes);
    }

    /// Gets an unsigned n-byte integer from `self` in big-endian byte order.
    ///
    /// The current position is advanced by `nbytes`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x01\x02\x03 hello"[..];
    /// assert_eq!(0x010203, buf.get_uint(3));
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_uint(&mut self, nbytes: usize) -> u64 {
        buf_get_impl!(be => self, u64, nbytes);
    }

    /// Gets an unsigned n-byte integer from `self` in little-endian byte order.
    ///
    /// The current position is advanced by `nbytes`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x03\x02\x01 hello"[..];
    /// assert_eq!(0x010203, buf.get_uint_le(3));
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_uint_le(&mut self, nbytes: usize) -> u64 {
        buf_get_impl!(le => self, u64, nbytes);
    }

    /// Gets an unsigned n-byte integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by `nbytes`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x01\x02\x03 hello",
    ///     false => b"\x03\x02\x01 hello",
    /// };
    /// assert_eq!(0x010203, buf.get_uint_ne(3));
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`, or
    /// if `nbytes` is greater than 8.
    fn get_uint_ne(&mut self, nbytes: usize) -> u64 {
        if cfg!(target_endian = "big") {
            self.get_uint(nbytes)
        } else {
            self.get_uint_le(nbytes)
        }
    }

    /// Gets a signed n-byte integer from `self` in big-endian byte order.
    ///
    /// The current position is advanced by `nbytes`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x01\x02\x03 hello"[..];
    /// assert_eq!(0x010203, buf.get_int(3));
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`, or
    /// if `nbytes` is greater than 8.
    fn get_int(&mut self, nbytes: usize) -> i64 {
        sign_extend(self.get_uint(nbytes), nbytes)
    }

    /// Gets a signed n-byte integer from `self` in little-endian byte order.
    ///
    /// The current position is advanced by `nbytes`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x03\x02\x01 hello"[..];
    /// assert_eq!(0x010203, buf.get_int_le(3));
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`, or
    /// if `nbytes` is greater than 8.
    fn get_int_le(&mut self, nbytes: usize) -> i64 {
        sign_extend(self.get_uint_le(nbytes), nbytes)
    }

    /// Gets a signed n-byte integer from `self` in native-endian byte order.
    ///
    /// The current position is advanced by `nbytes`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x01\x02\x03 hello",
    ///     false => b"\x03\x02\x01 hello",
    /// };
    /// assert_eq!(0x010203, buf.get_int_ne(3));
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`, or
    /// if `nbytes` is greater than 8.
    fn get_int_ne(&mut self, nbytes: usize) -> i64 {
        if cfg!(target_endian = "big") {
            self.get_int(nbytes)
        } else {
            self.get_int_le(nbytes)
        }
    }

    /// Gets an IEEE754 single-precision (4 bytes) floating point number from
    /// `self` in big-endian byte order.
    ///
    /// The current position is advanced by 4.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x3F\x99\x99\x9A hello"[..];
    /// assert_eq!(1.2f32, buf.get_f32());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_f32(&mut self) -> f32 {
        f32::from_bits(self.get_u32())
    }

    /// Gets an IEEE754 single-precision (4 bytes) floating point number from
    /// `self` in little-endian byte order.
    ///
    /// The current position is advanced by 4.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x9A\x99\x99\x3F hello"[..];
    /// assert_eq!(1.2f32, buf.get_f32_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_f32_le(&mut self) -> f32 {
        f32::from_bits(self.get_u32_le())
    }

    /// Gets an IEEE754 single-precision (4 bytes) floating point number from
    /// `self` in native-endian byte order.
    ///
    /// The current position is advanced by 4.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x3F\x99\x99\x9A hello",
    ///     false => b"\x9A\x99\x99\x3F hello",
    /// };
    /// assert_eq!(1.2f32, buf.get_f32_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_f32_ne(&mut self) -> f32 {
        f32::from_bits(self.get_u32_ne())
    }

    /// Gets an IEEE754 double-precision (8 bytes) floating point number from
    /// `self` in big-endian byte order.
    ///
    /// The current position is advanced by 8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x3F\xF3\x33\x33\x33\x33\x33\x33 hello"[..];
    /// assert_eq!(1.2f64, buf.get_f64());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_f64(&mut self) -> f64 {
        f64::from_bits(self.get_u64())
    }

    /// Gets an IEEE754 double-precision (8 bytes) floating point number from
    /// `self` in little-endian byte order.
    ///
    /// The current position is advanced by 8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf = &b"\x33\x33\x33\x33\x33\x33\xF3\x3F hello"[..];
    /// assert_eq!(1.2f64, buf.get_f64_le());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_f64_le(&mut self) -> f64 {
        f64::from_bits(self.get_u64_le())
    }

    /// Gets an IEEE754 double-precision (8 bytes) floating point number from
    /// `self` in native-endian byte order.
    ///
    /// The current position is advanced by 8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut buf: &[u8] = match cfg!(target_endian = "big") {
    ///     true => b"\x3F\xF3\x33\x33\x33\x33\x33\x33 hello",
    ///     false => b"\x33\x33\x33\x33\x33\x33\xF3\x3F hello",
    /// };
    /// assert_eq!(1.2f64, buf.get_f64_ne());
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if there is not enough remaining data in `self`.
    fn get_f64_ne(&mut self) -> f64 {
        f64::from_bits(self.get_u64_ne())
    }

    /// Consumes `len` bytes inside self and returns new instance of `Bytes`
    /// with this data.
    ///
    /// This function may be optimized by the underlying type to avoid actual
    /// copies. For example, `Bytes` implementation will do a shallow copy
    /// (ref-count increment).
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let bytes = (&b"hello world"[..]).copy_to_bytes(5);
    /// assert_eq!(&bytes[..], &b"hello"[..]);
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if `len > self.remaining()`.
    fn copy_to_bytes(&mut self, len: usize) -> crate::Bytes {
        use super::BufMut;

        if self.remaining() < len {
            panic_advance(len, self.remaining());
        }

        let mut ret = crate::BytesMut::with_capacity(len);
        ret.put(self.take(len));
        ret.freeze()
    }

    /// Creates an adaptor which will read at most `limit` bytes from `self`.
    ///
    /// This function returns a new instance of `Buf` which will read at most
    /// `limit` bytes.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::{Buf, BufMut};
    ///
    /// let mut buf = b"hello world"[..].take(5);
    /// let mut dst = vec![];
    ///
    /// dst.put(&mut buf);
    /// assert_eq!(dst, b"hello");
    ///
    /// let mut buf = buf.into_inner();
    /// dst.clear();
    /// dst.put(&mut buf);
    /// assert_eq!(dst, b" world");
    /// ```
    fn take(self, limit: usize) -> Take<Self>
    where
        Self: Sized,
    {
        take::new(self, limit)
    }

    /// Creates an adaptor which will chain this buffer with another.
    ///
    /// The returned `Buf` instance will first consume all bytes from `self`.
    /// Afterwards the output is equivalent to the output of next.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::Buf;
    ///
    /// let mut chain = b"hello "[..].chain(&b"world"[..]);
    ///
    /// let full = chain.copy_to_bytes(11);
    /// assert_eq!(full.chunk(), b"hello world");
    /// ```
    fn chain<U: Buf>(self, next: U) -> Chain<Self, U>
    where
        Self: Sized,
    {
        Chain::new(self, next)
    }

    /// Creates an adaptor which implements the `Read` trait for `self`.
    ///
    /// This function returns a new value which implements `Read` by adapting
    /// the `Read` trait functions to the `Buf` trait functions. Given that
    /// `Buf` operations are infallible, none of the `Read` functions will
    /// return with `Err`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::{Bytes, Buf};
    /// use std::io::Read;
    ///
    /// let buf = Bytes::from("hello world");
    ///
    /// let mut reader = buf.reader();
    /// let mut dst = [0; 1024];
    ///
    /// let num = reader.read(&mut dst).unwrap();
    ///
    /// assert_eq!(11, num);
    /// assert_eq!(&dst[..11], &b"hello world"[..]);
    /// ```
    #[cfg(feature = "std")]
    #[cfg_attr(docsrs, doc(cfg(feature = "std")))]
    fn reader(self) -> Reader<Self>
    where
        Self: Sized,
    {
        reader::new(self)
    }
}

macro_rules! deref_forward_buf {
    () => {
        #[inline]
        fn remaining(&self) -> usize {
            (**self).remaining()
        }

        #[inline]
        fn chunk(&self) -> &[u8] {
            (**self).chunk()
        }

        #[cfg(feature = "std")]
        #[inline]
        fn chunks_vectored<'b>(&'b self, dst: &mut [IoSlice<'b>]) -> usize {
            (**self).chunks_vectored(dst)
        }

        #[inline]
        fn advance(&mut self, cnt: usize) {
            (**self).advance(cnt)
        }

        #[inline]
        fn has_remaining(&self) -> bool {
            (**self).has_remaining()
        }

        #[inline]
        fn copy_to_slice(&mut self, dst: &mut [u8]) {
            (**self).copy_to_slice(dst)
        }

        #[inline]
        fn get_u8(&mut self) -> u8 {
            (**self).get_u8()
        }

        #[inline]
        fn get_i8(&mut self) -> i8 {
            (**self).get_i8()
        }

        #[inline]
        fn get_u16(&mut self) -> u16 {
            (**self).get_u16()
        }

        #[inline]
        fn get_u16_le(&mut self) -> u16 {
            (**self).get_u16_le()
        }

        #[inline]
        fn get_u16_ne(&mut self) -> u16 {
            (**self).get_u16_ne()
        }

        #[inline]
        fn get_i16(&mut self) -> i16 {
            (**self).get_i16()
        }

        #[inline]
        fn get_i16_le(&mut self) -> i16 {
            (**self).get_i16_le()
        }

        #[inline]
        fn get_i16_ne(&mut self) -> i16 {
            (**self).get_i16_ne()
        }

        #[inline]
        fn get_u32(&mut self) -> u32 {
            (**self).get_u32()
        }

        #[inline]
        fn get_u32_le(&mut self) -> u32 {
            (**self).get_u32_le()
        }

        #[inline]
        fn get_u32_ne(&mut self) -> u32 {
            (**self).get_u32_ne()
        }

        #[inline]
        fn get_i32(&mut self) -> i32 {
            (**self).get_i32()
        }

        #[inline]
        fn get_i32_le(&mut self) -> i32 {
            (**self).get_i32_le()
        }

        #[inline]
        fn get_i32_ne(&mut self) -> i32 {
            (**self).get_i32_ne()
        }

        #[inline]
        fn get_u64(&mut self) -> u64 {
            (**self).get_u64()
        }

        #[inline]
        fn get_u64_le(&mut self) -> u64 {
            (**self).get_u64_le()
        }

        #[inline]
        fn get_u64_ne(&mut self) -> u64 {
            (**self).get_u64_ne()
        }

        #[inline]
        fn get_i64(&mut self) -> i64 {
            (**self).get_i64()
        }

        #[inline]
        fn get_i64_le(&mut self) -> i64 {
            (**self).get_i64_le()
        }

        #[inline]
        fn get_i64_ne(&mut self) -> i64 {
            (**self).get_i64_ne()
        }

        #[inline]
        fn get_uint(&mut self, nbytes: usize) -> u64 {
            (**self).get_uint(nbytes)
        }

        #[inline]
        fn get_uint_le(&mut self, nbytes: usize) -> u64 {
            (**self).get_uint_le(nbytes)
        }

        #[inline]
        fn get_uint_ne(&mut self, nbytes: usize) -> u64 {
            (**self).get_uint_ne(nbytes)
        }

        #[inline]
        fn get_int(&mut self, nbytes: usize) -> i64 {
            (**self).get_int(nbytes)
        }

        #[inline]
        fn get_int_le(&mut self, nbytes: usize) -> i64 {
            (**self).get_int_le(nbytes)
        }

        #[inline]
        fn get_int_ne(&mut self, nbytes: usize) -> i64 {
            (**self).get_int_ne(nbytes)
        }

        #[inline]
        fn copy_to_bytes(&mut self, len: usize) -> crate::Bytes {
            (**self).copy_to_bytes(len)
        }
    };
}

impl<T: Buf + ?Sized> Buf for &mut T {
    deref_forward_buf!();
}

impl<T: Buf + ?Sized> Buf for Box<T> {
    deref_forward_buf!();
}

impl Buf for &[u8] {
    #[inline]
    fn remaining(&self) -> usize {
        self.len()
    }

    #[inline]
    fn chunk(&self) -> &[u8] {
        self
    }

    #[inline]
    fn advance(&mut self, cnt: usize) {
        if self.len() < cnt {
            panic_advance(cnt, self.len());
        }

        *self = &self[cnt..];
    }

    #[inline]
    fn copy_to_slice(&mut self, dst: &mut [u8]) {
        if self.len() < dst.len() {
            panic_advance(dst.len(), self.len());
        }

        dst.copy_from_slice(&self[..dst.len()]);
        self.advance(dst.len());
    }
}

#[cfg(feature = "std")]
impl<T: AsRef<[u8]>> Buf for std::io::Cursor<T> {
    #[inline]
    fn remaining(&self) -> usize {
        saturating_sub_usize_u64(self.get_ref().as_ref().len(), self.position())
    }

    #[inline]
    fn chunk(&self) -> &[u8] {
        let slice = self.get_ref().as_ref();
        let pos = min_u64_usize(self.position(), slice.len());
        &slice[pos..]
    }

    #[inline]
    fn advance(&mut self, cnt: usize) {
        let len = self.get_ref().as_ref().len();
        let pos = self.position();

        // We intentionally allow `cnt == 0` here even if `pos > len`.
        let max_cnt = saturating_sub_usize_u64(len, pos);
        if cnt > max_cnt {
            panic_advance(cnt, max_cnt);
        }

        // This will not overflow because either `cnt == 0` or the sum is not
        // greater than `len`.
        self.set_position(pos + cnt as u64);
    }
}

// The existence of this function makes the compiler catch if the Buf
// trait is "object-safe" or not.
fn _assert_trait_object(_b: &dyn Buf) {}
