// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::fmt::Debug;

use byteorder::{ByteOrder, LittleEndian};

use crate::error::Error;

/// Reads bits from a sequence of bytes.
#[derive(Clone)]
pub struct BitReader<'a> {
    data: &'a [u8],
    bit_buf: u64,
    bits_in_buf: usize,
    initial_bytes: usize,
    overread_bytes: usize,
}

impl Debug for BitReader<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "BitReader{{ data: [{} bytes], bit_buf: {:0width$b}, total_bits_read: {} }}",
            self.data.len(),
            self.bit_buf,
            self.total_bits_read(),
            width = self.bits_in_buf
        )
    }
}

pub const MAX_BITS_PER_CALL: usize = 56;

impl<'a> BitReader<'a> {
    /// Constructs a BitReader for a given range of data.
    pub fn new(data: &[u8]) -> BitReader<'_> {
        BitReader {
            data,
            bit_buf: 0,
            bits_in_buf: 0,
            initial_bytes: data.len(),
            overread_bytes: 0,
        }
    }

    /// Reads `num` bits from the buffer without consuming them.
    #[inline]
    pub fn peek(&mut self, num: usize) -> u64 {
        debug_assert!(num <= MAX_BITS_PER_CALL);
        if self.bits_in_buf < num {
            self.refill();
        }
        self.bit_buf & ((1u64 << num) - 1)
    }

    /// Advances by `num` bits. Similar to `skip_bits`, but bits must be in the buffer.
    #[inline]
    pub fn consume(&mut self, num: usize) -> Result<(), Error> {
        let overread_bits = self.overread_bytes * 8;
        if self.bits_in_buf < num + overread_bits {
            let real_bits = self.bits_in_buf.saturating_sub(overread_bits);
            return Err(Error::OutOfBounds((num - real_bits).div_ceil(8)));
        }
        self.bit_buf >>= num;
        self.bits_in_buf -= num;
        Ok(())
    }

    #[inline(always)]
    pub fn consume_optimistic(&mut self, num: usize) {
        self.bit_buf >>= num;
        self.bits_in_buf -= num;
    }

    /// Reads `num` bits from the buffer.
    /// ```ignore
    /// let mut br = BitReader::new(&[0, 1]);
    /// assert_eq!(br.read(8)?, 0);
    /// assert_eq!(br.read(4)?, 1);
    /// assert_eq!(br.read(4)?, 0);
    /// assert_eq!(br.total_bits_read(), 16);
    /// assert!(br.read(1).is_err());
    /// ```
    #[inline]
    pub fn read(&mut self, num: usize) -> Result<u64, Error> {
        let ret = self.peek(num);
        self.consume(num)?;
        Ok(ret)
    }

    /// inline(never) wrapper around read, for cold code paths.
    #[inline(never)]
    pub fn read_noinline(&mut self, num: usize) -> Result<u64, Error> {
        self.read(num)
    }

    #[inline]
    pub fn read_optimistic(&mut self, num: usize) -> u64 {
        let ret = self.peek(num);
        self.consume_optimistic(num);
        ret
    }

    pub fn check_for_error(&self) -> Result<(), Error> {
        let total_read = self.total_bits_read();
        let initial_bits = self.initial_bytes * 8;
        if total_read > initial_bits {
            Err(Error::OutOfBounds(total_read - initial_bits))
        } else {
            Ok(())
        }
    }

    /// Returns the total number of bits that have been read or skipped.
    pub fn total_bits_read(&self) -> usize {
        (self.initial_bytes - self.data.len() + self.overread_bytes) * 8 - self.bits_in_buf
    }

    /// Returns the total number of bits that can still be read or skipped.
    pub fn total_bits_available(&self) -> usize {
        (self.data.len() * 8 + self.bits_in_buf).saturating_sub(self.overread_bytes * 8)
    }

    /// Skips `num` bits.
    /// ```ignore
    /// let mut br = BitReader::new(&[0, 1]);
    /// assert_eq!(br.read(8)?, 0);
    /// br.skip_bits(4)?;
    /// assert_eq!(br.total_bits_read(), 12);
    /// ```
    #[inline(never)]
    pub fn skip_bits(&mut self, mut n: usize) -> Result<(), Error> {
        let overread_bits = self.overread_bytes * 8;
        let real_bits_in_buf = self.bits_in_buf.saturating_sub(overread_bits);
        // Check if we can skip within the current buffer
        if n <= real_bits_in_buf {
            self.bits_in_buf -= n;
            self.bit_buf >>= n;
            return Ok(());
        }

        // Adjust the number of bits to skip and reset the buffer
        n -= real_bits_in_buf;
        self.bit_buf = 0;
        self.bits_in_buf = 0;
        self.overread_bytes = 0;

        // Check if the remaining bits to skip exceed the total bits in `data`
        let bits_available = self.data.len() * 8;
        if n > bits_available {
            self.data = &[];
            return Err(Error::OutOfBounds(n - bits_available));
        }

        // Skip bytes directly in `data`, then handle leftover bits
        self.data = &self.data[n / 8..];
        n %= 8;

        if n > 0 {
            self.refill();
            self.bit_buf >>= n;
            self.bits_in_buf -= n;
        }
        Ok(())
    }

    /// Return the number of bits
    pub fn bits_to_next_byte(&self) -> usize {
        let total_read = self.total_bits_read();
        let byte_boundary = total_read.div_ceil(8) * 8;
        byte_boundary - total_read
    }

    /// Jumps to the next byte boundary. The skipped bytes have to be 0.
    /// ```ignore
    /// let mut br = BitReader::new(&[0, 1]);
    /// assert_eq!(br.read(8)?, 0);
    /// br.skip_bits(4)?;
    /// br.jump_to_byte_boundary()?;
    /// assert_eq!(br.total_bits_read(), 16);
    /// ```
    #[inline(never)]
    pub fn jump_to_byte_boundary(&mut self) -> Result<(), Error> {
        if self.read(self.bits_to_next_byte())? != 0 {
            return Err(Error::NonZeroPadding);
        }
        Ok(())
    }

    #[inline]
    fn refill(&mut self) {
        // See Refill() in C++ code.
        if self.data.len() >= 8 {
            let bits = LittleEndian::read_u64(self.data);
            self.bit_buf |= bits << self.bits_in_buf;
            let read_bytes = (63 - self.bits_in_buf) >> 3;
            self.bits_in_buf |= 56;
            self.data = &self.data[read_bytes..];
            debug_assert!(56 <= self.bits_in_buf && self.bits_in_buf < 64);
        } else {
            self.refill_slow()
        }
    }

    #[inline(never)]
    fn refill_slow(&mut self) {
        while self.bits_in_buf < 56 {
            if self.data.is_empty() {
                let extra_bytes = (63 - self.bits_in_buf) >> 3;
                self.overread_bytes += extra_bytes;
                self.bits_in_buf |= 56;
                return;
            }
            self.bit_buf |= (self.data[0] as u64) << self.bits_in_buf;
            self.bits_in_buf += 8;
            self.data = &self.data[1..];
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_skip_bits_on_fresh_reader() {
        // This test checks if skip_bits works correctly on a fresh BitReader
        let data = [0x12, 0x34, 0x56, 0x78];
        let mut br = BitReader::new(&data);

        // Try to skip 1 bit on a fresh reader - this should work
        br.skip_bits(1)
            .expect("skip_bits should work on fresh reader");
        assert_eq!(br.total_bits_read(), 1);

        // Read the next 7 bits to complete the byte
        let val = br.read(7).expect("read should work");
        assert_eq!(val, 0x12 >> 1); // Should get the lower 7 bits of 0x12
    }
}
