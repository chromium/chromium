//! Possible ZIP compression methods.

use std::{fmt, io};

#[allow(deprecated)]
/// Identifies the storage format used to compress a file within a ZIP archive.
///
/// Each file's compression method is stored alongside it, allowing the
/// contents to be read without context.
///
/// When creating ZIP files, you may choose the method to use with
/// [`crate::write::FileOptions::compression_method`]
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
#[cfg_attr(fuzzing, derive(arbitrary::Arbitrary))]
#[non_exhaustive]
pub enum CompressionMethod {
    /// Store the file as is
    Stored,
    /// Compress the file using Deflate
    #[cfg(feature = "_deflate-any")]
    Deflated,
    /// Compress the file using Deflate64.
    /// Decoding deflate64 is supported but encoding deflate64 is not supported.
    #[cfg(feature = "deflate64")]
    Deflate64,
    /// Compress the file using BZIP2
    #[cfg(feature = "bzip2")]
    Bzip2,
    /// Encrypted using AES.
    ///
    /// The actual compression method has to be taken from the AES extra data field
    /// or from `ZipFileData`.
    #[cfg(feature = "aes-crypto")]
    Aes,
    /// Compress the file using ZStandard
    #[cfg(feature = "zstd")]
    Zstd,
    /// Compress the file using LZMA
    #[cfg(feature = "lzma")]
    Lzma,
    /// Compress the file using XZ
    #[cfg(feature = "xz")]
    Xz,
    /// Unsupported compression method
    #[cfg_attr(
        not(fuzzing),
        deprecated(since = "0.5.7", note = "use the constants instead")
    )]
    Unsupported(u16),
}
#[allow(deprecated, missing_docs)]
/// All compression methods defined for the ZIP format
impl CompressionMethod {
    pub const STORE: Self = CompressionMethod::Stored;
    pub const SHRINK: Self = CompressionMethod::Unsupported(1);
    pub const REDUCE_1: Self = CompressionMethod::Unsupported(2);
    pub const REDUCE_2: Self = CompressionMethod::Unsupported(3);
    pub const REDUCE_3: Self = CompressionMethod::Unsupported(4);
    pub const REDUCE_4: Self = CompressionMethod::Unsupported(5);
    pub const IMPLODE: Self = CompressionMethod::Unsupported(6);
    #[cfg(feature = "_deflate-any")]
    pub const DEFLATE: Self = CompressionMethod::Deflated;
    #[cfg(not(feature = "_deflate-any"))]
    pub const DEFLATE: Self = CompressionMethod::Unsupported(8);
    #[cfg(feature = "deflate64")]
    pub const DEFLATE64: Self = CompressionMethod::Deflate64;
    #[cfg(not(feature = "deflate64"))]
    pub const DEFLATE64: Self = CompressionMethod::Unsupported(9);
    pub const PKWARE_IMPLODE: Self = CompressionMethod::Unsupported(10);
    #[cfg(feature = "bzip2")]
    pub const BZIP2: Self = CompressionMethod::Bzip2;
    #[cfg(not(feature = "bzip2"))]
    pub const BZIP2: Self = CompressionMethod::Unsupported(12);
    #[cfg(not(feature = "lzma"))]
    pub const LZMA: Self = CompressionMethod::Unsupported(14);
    #[cfg(feature = "lzma")]
    pub const LZMA: Self = CompressionMethod::Lzma;
    pub const IBM_ZOS_CMPSC: Self = CompressionMethod::Unsupported(16);
    pub const IBM_TERSE: Self = CompressionMethod::Unsupported(18);
    pub const ZSTD_DEPRECATED: Self = CompressionMethod::Unsupported(20);
    #[cfg(feature = "zstd")]
    pub const ZSTD: Self = CompressionMethod::Zstd;
    #[cfg(not(feature = "zstd"))]
    pub const ZSTD: Self = CompressionMethod::Unsupported(93);
    pub const MP3: Self = CompressionMethod::Unsupported(94);
    #[cfg(feature = "xz")]
    pub const XZ: Self = CompressionMethod::Xz;
    #[cfg(not(feature = "xz"))]
    pub const XZ: Self = CompressionMethod::Unsupported(95);
    pub const JPEG: Self = CompressionMethod::Unsupported(96);
    pub const WAVPACK: Self = CompressionMethod::Unsupported(97);
    pub const PPMD: Self = CompressionMethod::Unsupported(98);
    #[cfg(feature = "aes-crypto")]
    pub const AES: Self = CompressionMethod::Aes;
    #[cfg(not(feature = "aes-crypto"))]
    pub const AES: Self = CompressionMethod::Unsupported(99);
}
impl CompressionMethod {
    pub(crate) const fn parse_from_u16(val: u16) -> Self {
        match val {
            0 => CompressionMethod::Stored,
            #[cfg(feature = "_deflate-any")]
            8 => CompressionMethod::Deflated,
            #[cfg(feature = "deflate64")]
            9 => CompressionMethod::Deflate64,
            #[cfg(feature = "bzip2")]
            12 => CompressionMethod::Bzip2,
            #[cfg(feature = "lzma")]
            14 => CompressionMethod::Lzma,
            #[cfg(feature = "xz")]
            95 => CompressionMethod::Xz,
            #[cfg(feature = "zstd")]
            93 => CompressionMethod::Zstd,
            #[cfg(feature = "aes-crypto")]
            99 => CompressionMethod::Aes,
            #[allow(deprecated)]
            v => CompressionMethod::Unsupported(v),
        }
    }

    /// Converts a u16 to its corresponding CompressionMethod
    #[deprecated(
        since = "0.5.7",
        note = "use a constant to construct a compression method"
    )]
    pub const fn from_u16(val: u16) -> CompressionMethod {
        Self::parse_from_u16(val)
    }

    pub(crate) const fn serialize_to_u16(self) -> u16 {
        match self {
            CompressionMethod::Stored => 0,
            #[cfg(feature = "_deflate-any")]
            CompressionMethod::Deflated => 8,
            #[cfg(feature = "deflate64")]
            CompressionMethod::Deflate64 => 9,
            #[cfg(feature = "bzip2")]
            CompressionMethod::Bzip2 => 12,
            #[cfg(feature = "aes-crypto")]
            CompressionMethod::Aes => 99,
            #[cfg(feature = "zstd")]
            CompressionMethod::Zstd => 93,
            #[cfg(feature = "lzma")]
            CompressionMethod::Lzma => 14,
            #[cfg(feature = "xz")]
            CompressionMethod::Xz => 95,
            #[allow(deprecated)]
            CompressionMethod::Unsupported(v) => v,
        }
    }

    /// Converts a CompressionMethod to a u16
    #[deprecated(
        since = "0.5.7",
        note = "to match on other compression methods, use a constant"
    )]
    pub const fn to_u16(self) -> u16 {
        self.serialize_to_u16()
    }
}

impl Default for CompressionMethod {
    fn default() -> Self {
        #[cfg(feature = "_deflate-any")]
        return CompressionMethod::Deflated;

        #[cfg(not(feature = "_deflate-any"))]
        return CompressionMethod::Stored;
    }
}

impl fmt::Display for CompressionMethod {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Just duplicate what the Debug format looks like, i.e, the enum key:
        write!(f, "{self:?}")
    }
}

/// The compression methods which have been implemented.
pub const SUPPORTED_COMPRESSION_METHODS: &[CompressionMethod] = &[
    CompressionMethod::Stored,
    #[cfg(feature = "_deflate-any")]
    CompressionMethod::Deflated,
    #[cfg(feature = "deflate64")]
    CompressionMethod::Deflate64,
    #[cfg(feature = "bzip2")]
    CompressionMethod::Bzip2,
    #[cfg(feature = "zstd")]
    CompressionMethod::Zstd,
    #[cfg(feature = "xz")]
    CompressionMethod::Xz,
];

pub(crate) enum Decompressor<R: io::BufRead> {
    Stored(R),
    #[cfg(feature = "_deflate-any")]
    Deflated(flate2::bufread::DeflateDecoder<R>),
    #[cfg(feature = "deflate64")]
    Deflate64(deflate64::Deflate64Decoder<R>),
    #[cfg(feature = "bzip2")]
    Bzip2(bzip2::bufread::BzDecoder<R>),
    #[cfg(feature = "zstd")]
    Zstd(zstd::Decoder<'static, R>),
    #[cfg(feature = "lzma")]
    Lzma(Box<crate::read::lzma::LzmaDecoder<R>>),
    #[cfg(feature = "xz")]
    Xz(xz2::bufread::XzDecoder<R>),
}

impl<R: io::BufRead> io::Read for Decompressor<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        match self {
            Decompressor::Stored(r) => r.read(buf),
            #[cfg(feature = "_deflate-any")]
            Decompressor::Deflated(r) => r.read(buf),
            #[cfg(feature = "deflate64")]
            Decompressor::Deflate64(r) => r.read(buf),
            #[cfg(feature = "bzip2")]
            Decompressor::Bzip2(r) => r.read(buf),
            #[cfg(feature = "zstd")]
            Decompressor::Zstd(r) => r.read(buf),
            #[cfg(feature = "lzma")]
            Decompressor::Lzma(r) => r.read(buf),
            #[cfg(feature = "xz")]
            Decompressor::Xz(r) => r.read(buf),
        }
    }
}

impl<R: io::BufRead> Decompressor<R> {
    pub fn new(reader: R, compression_method: CompressionMethod) -> crate::result::ZipResult<Self> {
        Ok(match compression_method {
            CompressionMethod::Stored => Decompressor::Stored(reader),
            #[cfg(feature = "_deflate-any")]
            CompressionMethod::Deflated => {
                Decompressor::Deflated(flate2::bufread::DeflateDecoder::new(reader))
            }
            #[cfg(feature = "deflate64")]
            CompressionMethod::Deflate64 => {
                Decompressor::Deflate64(deflate64::Deflate64Decoder::with_buffer(reader))
            }
            #[cfg(feature = "bzip2")]
            CompressionMethod::Bzip2 => Decompressor::Bzip2(bzip2::bufread::BzDecoder::new(reader)),
            #[cfg(feature = "zstd")]
            CompressionMethod::Zstd => Decompressor::Zstd(zstd::Decoder::with_buffer(reader)?),
            #[cfg(feature = "lzma")]
            CompressionMethod::Lzma => {
                Decompressor::Lzma(Box::new(crate::read::lzma::LzmaDecoder::new(reader)))
            }
            #[cfg(feature = "xz")]
            CompressionMethod::Xz => Decompressor::Xz(xz2::bufread::XzDecoder::new(reader)),
            _ => {
                return Err(crate::result::ZipError::UnsupportedArchive(
                    "Compression method not supported",
                ))
            }
        })
    }

    /// Consumes this decoder, returning the underlying reader.
    pub fn into_inner(self) -> R {
        match self {
            Decompressor::Stored(r) => r,
            #[cfg(feature = "_deflate-any")]
            Decompressor::Deflated(r) => r.into_inner(),
            #[cfg(feature = "deflate64")]
            Decompressor::Deflate64(r) => r.into_inner(),
            #[cfg(feature = "bzip2")]
            Decompressor::Bzip2(r) => r.into_inner(),
            #[cfg(feature = "zstd")]
            Decompressor::Zstd(r) => r.finish(),
            #[cfg(feature = "lzma")]
            Decompressor::Lzma(r) => r.into_inner(),
            #[cfg(feature = "xz")]
            Decompressor::Xz(r) => r.into_inner(),
        }
    }
}

#[cfg(test)]
mod test {
    use super::{CompressionMethod, SUPPORTED_COMPRESSION_METHODS};

    #[test]
    fn from_eq_to() {
        for v in 0..(u16::MAX as u32 + 1) {
            let from = CompressionMethod::parse_from_u16(v as u16);
            let to = from.serialize_to_u16() as u32;
            assert_eq!(v, to);
        }
    }

    #[test]
    fn to_eq_from() {
        fn check_match(method: CompressionMethod) {
            let to = method.serialize_to_u16();
            let from = CompressionMethod::parse_from_u16(to);
            let back = from.serialize_to_u16();
            assert_eq!(to, back);
        }

        for &method in SUPPORTED_COMPRESSION_METHODS {
            check_match(method);
        }
    }

    #[test]
    fn to_display_fmt() {
        fn check_match(method: CompressionMethod) {
            let debug_str = format!("{method:?}");
            let display_str = format!("{method}");
            assert_eq!(debug_str, display_str);
        }

        for &method in SUPPORTED_COMPRESSION_METHODS {
            check_match(method);
        }
    }
}
