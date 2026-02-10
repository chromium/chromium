//! Simple CRC bindings backed by miniz.c

use std::io;
use std::io::prelude::*;

#[cfg(not(feature = "zlib-rs"))]
pub use impl_crc32fast::Crc;

#[cfg(feature = "zlib-rs")]
pub use impl_zlib_rs::Crc;

#[cfg(not(feature = "zlib-rs"))]
mod impl_crc32fast {
    use crc32fast::Hasher;

    /// The CRC calculated by a [`CrcReader`].
    ///
    /// [`CrcReader`]: struct.CrcReader.html
    #[derive(Debug, Default)]
    pub struct Crc {
        amt: u32,
        hasher: Hasher,
    }

    impl Crc {
        /// Create a new CRC.
        pub fn new() -> Self {
            Self::default()
        }

        /// Returns the current crc32 checksum.
        pub fn sum(&self) -> u32 {
            self.hasher.clone().finalize()
        }

        /// The number of bytes that have been used to calculate the CRC.
        /// This value is only accurate if the amount is lower than 2<sup>32</sup>.
        pub fn amount(&self) -> u32 {
            self.amt
        }

        /// Update the CRC with the bytes in `data`.
        pub fn update(&mut self, data: &[u8]) {
            self.amt = self.amt.wrapping_add(data.len() as u32);
            self.hasher.update(data);
        }

        /// Reset the CRC.
        pub fn reset(&mut self) {
            self.amt = 0;
            self.hasher.reset();
        }

        /// Combine the CRC with the CRC for the subsequent block of bytes.
        pub fn combine(&mut self, additional_crc: &Self) {
            self.amt = self.amt.wrapping_add(additional_crc.amt);
            self.hasher.combine(&additional_crc.hasher);
        }
    }
}

#[cfg(feature = "zlib-rs")]
mod impl_zlib_rs {
    /// The CRC calculated by a [`CrcReader`].
    ///
    /// [`CrcReader`]: struct.CrcReader.html
    #[derive(Debug, Default)]
    pub struct Crc {
        consumed: u64,
        state: u32,
    }

    impl Crc {
        /// Create a new CRC.
        pub fn new() -> Self {
            Self::default()
        }

        /// Returns the current crc32 checksum.
        pub fn sum(&self) -> u32 {
            self.state
        }

        /// The number of bytes that have been used to calculate the CRC.
        /// This value is only accurate if the amount is lower than 2<sup>32</sup>.
        pub fn amount(&self) -> u32 {
            self.consumed as u32
        }

        /// Update the CRC with the bytes in `data`.
        pub fn update(&mut self, data: &[u8]) {
            self.consumed = self.consumed.wrapping_add(data.len() as u64);
            self.state = zlib_rs::crc32::crc32(self.state, data);
        }

        /// Reset the CRC.
        pub fn reset(&mut self) {
            self.consumed = 0;
            self.state = 0
        }

        /// Combine the CRC with the CRC for the subsequent block of bytes.
        pub fn combine(&mut self, additional_crc: &Self) {
            self.consumed = self.consumed.wrapping_add(additional_crc.consumed);
            self.state = zlib_rs::crc32::crc32_combine(
                self.state,
                additional_crc.state,
                additional_crc.consumed,
            );
        }
    }
}

/// A wrapper around a [`Read`] that calculates the CRC.
///
/// [`Read`]: https://doc.rust-lang.org/std/io/trait.Read.html
#[derive(Debug)]
pub struct CrcReader<R> {
    inner: R,
    crc: Crc,
}

impl<R: Read> CrcReader<R> {
    /// Create a new `CrcReader`.
    pub fn new(r: R) -> CrcReader<R> {
        CrcReader {
            inner: r,
            crc: Crc::new(),
        }
    }
}

impl<R> CrcReader<R> {
    /// Get the Crc for this `CrcReader`.
    pub fn crc(&self) -> &Crc {
        &self.crc
    }

    /// Get the reader that is wrapped by this `CrcReader`.
    pub fn into_inner(self) -> R {
        self.inner
    }

    /// Get the reader that is wrapped by this `CrcReader` by reference.
    pub fn get_ref(&self) -> &R {
        &self.inner
    }

    /// Get a mutable reference to the reader that is wrapped by this `CrcReader`.
    pub fn get_mut(&mut self) -> &mut R {
        &mut self.inner
    }

    /// Reset the Crc in this `CrcReader`.
    pub fn reset(&mut self) {
        self.crc.reset();
    }
}

impl<R: Read> Read for CrcReader<R> {
    fn read(&mut self, into: &mut [u8]) -> io::Result<usize> {
        let amt = self.inner.read(into)?;
        self.crc.update(&into[..amt]);
        Ok(amt)
    }
}

impl<R: BufRead> BufRead for CrcReader<R> {
    fn fill_buf(&mut self) -> io::Result<&[u8]> {
        self.inner.fill_buf()
    }
    fn consume(&mut self, amt: usize) {
        if let Ok(data) = self.inner.fill_buf() {
            self.crc.update(&data[..amt]);
        }
        self.inner.consume(amt);
    }
}

/// A wrapper around a [`Write`] that calculates the CRC.
///
/// [`Write`]: https://doc.rust-lang.org/std/io/trait.Write.html
#[derive(Debug)]
pub struct CrcWriter<W> {
    inner: W,
    crc: Crc,
}

impl<W> CrcWriter<W> {
    /// Get the Crc for this `CrcWriter`.
    pub fn crc(&self) -> &Crc {
        &self.crc
    }

    /// Get the writer that is wrapped by this `CrcWriter`.
    pub fn into_inner(self) -> W {
        self.inner
    }

    /// Get the writer that is wrapped by this `CrcWriter` by reference.
    pub fn get_ref(&self) -> &W {
        &self.inner
    }

    /// Get a mutable reference to the writer that is wrapped by this `CrcWriter`.
    pub fn get_mut(&mut self) -> &mut W {
        &mut self.inner
    }

    /// Reset the Crc in this `CrcWriter`.
    pub fn reset(&mut self) {
        self.crc.reset();
    }
}

impl<W: Write> CrcWriter<W> {
    /// Create a new `CrcWriter`.
    pub fn new(w: W) -> CrcWriter<W> {
        CrcWriter {
            inner: w,
            crc: Crc::new(),
        }
    }
}

impl<W: Write> Write for CrcWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let amt = self.inner.write(buf)?;
        self.crc.update(&buf[..amt]);
        Ok(amt)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

#[cfg(test)]
mod tests {
    use super::Crc;

    fn crc_of(data: &[u8]) -> Crc {
        let mut c = Crc::new();
        c.update(data);
        c
    }

    fn sum_of(data: &[u8]) -> u32 {
        crc_of(data).sum()
    }

    #[test]
    fn new_is_empty() {
        let c = Crc::new();
        assert_eq!(c.amount(), 0);
        assert_eq!(c.sum(), 0);
    }

    #[test]
    fn known_vector_hello() {
        assert_eq!(sum_of(b"hello"), 0x3610_A686);
    }

    #[test]
    fn known_vector_quick_brown_fox() {
        assert_eq!(
            sum_of(b"The quick brown fox jumps over the lazy dog"),
            0x414F_A339
        );
    }

    #[test]
    fn update_is_streaming() {
        let mut c = Crc::new();
        c.update(b"hello");
        c.update(b" ");
        c.update(b"world");

        assert_eq!(c.amount(), 11);
        assert_eq!(c.sum(), sum_of(b"hello world"));
    }

    #[test]
    fn reset_restores_initial_state() {
        let mut c = Crc::new();
        c.update(b"abc");
        assert_ne!(c.sum(), 0);
        assert_eq!(c.amount(), 3);

        c.reset();
        assert_eq!(c.amount(), 0);
        assert_eq!(c.sum(), 0);
    }

    #[test]
    fn combine_matches_concatenation() {
        let a = b"hello ";
        let b = b"world";

        let mut ca = crc_of(a);
        let cb = crc_of(b);

        ca.combine(&cb);

        dbg!(&ca);

        assert_eq!(ca.amount(), 11);
        assert_eq!(ca.sum(), sum_of(b"hello world"));
    }
}
