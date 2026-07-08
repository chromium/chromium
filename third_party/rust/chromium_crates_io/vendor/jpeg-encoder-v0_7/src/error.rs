use alloc::fmt::Display;
use core::error::Error;

/// # The error type for encoding
#[derive(Debug)]
pub enum EncodingError {
    /// An invalid app segment number has been used
    InvalidAppSegment(u8),

    /// App segment exceeds maximum allowed data length
    AppSegmentTooLarge(usize),

    /// Color profile exceeds maximum allowed data length
    IccTooLarge(usize),

    /// Image data is too short
    BadImageData { length: usize, required: usize },

    /// Width or height is zero
    ZeroImageDimensions { width: u16, height: u16 },

    /// An io error occurred during writing
    #[cfg(feature = "std")]
    IoError(std::io::Error),

    /// An io error occurred during writing (Should be used in no_std cases instead of IoError)
    Write(alloc::string::String),
}

#[cfg(feature = "std")]
impl From<std::io::Error> for EncodingError {
    fn from(err: std::io::Error) -> EncodingError {
        EncodingError::IoError(err)
    }
}

impl Display for EncodingError {
    fn fmt(&self, f: &mut alloc::fmt::Formatter<'_>) -> alloc::fmt::Result {
        use EncodingError::*;
        match self {
            InvalidAppSegment(nr) => write!(f, "Invalid app segment number: {}", nr),
            AppSegmentTooLarge(length) => write!(
                f,
                "App segment exceeds maximum allowed data length of 65533: {}",
                length
            ),
            IccTooLarge(length) => write!(
                f,
                "ICC profile exceeds maximum allowed data length: {}",
                length
            ),
            BadImageData { length, required } => write!(
                f,
                "Image data too small for dimensions and color_type: {} need at least {}",
                length, required
            ),
            ZeroImageDimensions { width, height } => {
                write!(f, "Image dimensions must be non zero: {}x{}", width, height)
            }
            #[cfg(feature = "std")]
            IoError(err) => err.fmt(f),
            Write(err) => write!(f, "{}", err),
        }
    }
}

impl Error for EncodingError {
    #[cfg(feature = "std")]
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            EncodingError::IoError(err) => Some(err),
            _ => None,
        }
    }
}
