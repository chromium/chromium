//! Compression of events passed between a broker and clients.
//! Currently we use the gzip compression algorithm for its fast decompression performance.

use alloc::vec::Vec;
use core::fmt::Debug;

use miniz_oxide::{
    deflate::{CompressionLevel, compress_to_vec},
    inflate::decompress_to_vec,
};

use crate::Error;

/// Compression for your stream compression needs.
#[derive(Debug)]
pub struct GzipCompressor {
    /// If less bytes than threshold are being passed to `compress`, the payload is not getting compressed.
    threshold: usize,
}

impl GzipCompressor {
    /// If the buffer is at least larger as large as the `threshold` value, we compress the buffer.
    /// When given a `threshold` of `0`, the `GzipCompressor` will always compress.
    #[must_use]
    pub fn with_threshold(threshold: usize) -> Self {
        Self { threshold }
    }

    /// Create a [`GzipCompressor`] that will always compress
    #[must_use]
    pub fn new() -> Self {
        Self { threshold: 0 }
    }
}

impl Default for GzipCompressor {
    fn default() -> Self {
        Self::new()
    }
}

impl GzipCompressor {
    /// Compression.
    /// If the buffer is smaller than the threshold of this compressor, `None` will be returned.
    /// Else, the buffer is compressed.
    #[must_use]
    pub fn maybe_compress(&self, buf: &[u8]) -> Option<Vec<u8>> {
        if buf.len() >= self.threshold {
            //compress if the buffer is large enough
            Some(self.compress(buf))
        } else {
            None
        }
    }

    /// Force compression.
    /// Will ignore the preset threshold, and always compress.
    #[must_use]
    pub fn compress(&self, buf: &[u8]) -> Vec<u8> {
        compress_to_vec(buf, CompressionLevel::BestSpeed as u8)
    }

    /// Decompression.
    pub fn decompress(&self, buf: &[u8]) -> Result<Vec<u8>, Error> {
        let decompressed = decompress_to_vec(buf);

        match decompressed {
            Ok(buf) => Ok(buf),
            Err(_) => Err(Error::compression()),
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::compress::GzipCompressor;

    #[test]
    fn test_compression() {
        let compressor = GzipCompressor::with_threshold(1);
        assert!(
            compressor
                .decompress(&compressor.maybe_compress(&[1u8; 1024]).unwrap())
                .unwrap()
                == vec![1u8; 1024]
        );
    }

    #[test]
    fn test_threshold() {
        let compressor = GzipCompressor::with_threshold(1024);
        assert!(compressor.maybe_compress(&[1u8; 1023]).is_none());
        assert!(compressor.maybe_compress(&[1u8; 1024]).is_some());
    }
}
