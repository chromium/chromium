//! Errors that can be encounting by Encoding and Decoding.

/// Errors that can be encountered by encoding a type
#[non_exhaustive]
#[derive(Debug)]
pub enum EncodeError {
    /// The writer ran out of storage.
    UnexpectedEnd,

    /// The `RefCell<T>` is already borrowed
    RefCellAlreadyBorrowed {
        /// The inner borrow error
        inner: core::cell::BorrowError,
        /// the type name of the RefCell being encoded that is currently borrowed.
        type_name: &'static str,
    },

    /// An uncommon error occurred, see the inner text for more information
    Other(&'static str),

    /// An uncommon error occurred, see the inner text for more information
    #[cfg(feature = "alloc")]
    OtherString(alloc::string::String),

    /// A `std::path::Path` was being encoded but did not contain a valid `&str` representation
    #[cfg(feature = "std")]
    InvalidPathCharacters,

    /// The targeted writer encountered an `std::io::Error`
    #[cfg(feature = "std")]
    Io {
        /// The encountered error
        inner: std::io::Error,
        /// The amount of bytes that were written before the error occurred
        index: usize,
    },

    /// The encoder tried to encode a `Mutex` or `RwLock`, but the locking failed
    #[cfg(feature = "std")]
    LockFailed {
        /// The type name of the mutex for debugging purposes
        type_name: &'static str,
    },

    /// The encoder tried to encode a `SystemTime`, but it was before `SystemTime::UNIX_EPOCH`
    #[cfg(feature = "std")]
    InvalidSystemTime {
        /// The error that was thrown by the SystemTime
        inner: std::time::SystemTimeError,
        /// The SystemTime that caused the error
        time: std::boxed::Box<std::time::SystemTime>,
    },

    #[cfg(feature = "serde")]
    /// A serde-specific error that occurred while decoding.
    Serde(crate::features::serde::EncodeError),
}

impl core::fmt::Display for EncodeError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        // TODO: Improve this?
        write!(f, "{:?}", self)
    }
}

/// Errors that can be encountered by decoding a type
#[non_exhaustive]
#[derive(Debug)]
pub enum DecodeError {
    /// The reader reached its end but more bytes were expected.
    UnexpectedEnd {
        /// Gives an estimate of how many extra bytes are needed.
        ///
        /// **Note**: this is only an estimate and not indicative of the actual bytes needed.
        ///
        /// **Note**: Bincode has no look-ahead mechanism. This means that this will only return the amount of bytes to be read for the current action, and not take into account the entire data structure being read.
        additional: usize,
    },

    /// The given configuration limit was exceeded
    LimitExceeded,

    /// Invalid type was found. The decoder tried to read type `expected`, but found type `found` instead.
    InvalidIntegerType {
        /// The type that was being read from the reader
        expected: IntegerType,
        /// The type that was encoded in the data
        found: IntegerType,
    },

    /// The decoder tried to decode any of the `NonZero*` types but the value is zero
    NonZeroTypeIsZero {
        /// The type that was being read from the reader
        non_zero_type: IntegerType,
    },

    /// Invalid enum variant was found. The decoder tried to decode variant index `found`, but the variant index should be between `min` and `max`.
    UnexpectedVariant {
        /// The type name that was being decoded.
        type_name: &'static str,

        /// The variants that are allowed
        allowed: &'static AllowedEnumVariants,

        /// The index of the enum that the decoder encountered
        found: u32,
    },

    /// The decoder tried to decode a `str`, but an utf8 error was encountered.
    Utf8 {
        /// The inner error
        inner: core::str::Utf8Error,
    },

    /// The decoder tried to decode a `char` and failed. The given buffer contains the bytes that are read at the moment of failure.
    InvalidCharEncoding([u8; 4]),

    /// The decoder tried to decode a `bool` and failed. The given value is what is actually read.
    InvalidBooleanValue(u8),

    /// The decoder tried to decode an array of length `required`, but the binary data contained an array of length `found`.
    ArrayLengthMismatch {
        /// The length of the array required by the rust type.
        required: usize,
        /// The length of the array found in the binary format.
        found: usize,
    },

    /// The encoded value is outside of the range of the target usize type.
    ///
    /// This can happen if an usize was encoded on an architecture with a larger
    /// usize type and then decoded on an architecture with a smaller one. For
    /// example going from a 64 bit architecture to a 32 or 16 bit one may
    /// cause this error.
    OutsideUsizeRange(u64),

    /// Tried to decode an enum with no variants
    EmptyEnum {
        /// The type that was being decoded
        type_name: &'static str,
    },

    /// The decoder tried to decode a Duration and overflowed the number of seconds.
    InvalidDuration {
        /// The number of seconds in the duration.
        secs: u64,

        /// The number of nanoseconds in the duration, which when converted to seconds and added to
        /// `secs`, overflows a `u64`.
        nanos: u32,
    },

    /// The decoder tried to decode a SystemTime and overflowed
    InvalidSystemTime {
        /// The duration which could not have been added to
        /// [`UNIX_EPOCH`](std::time::SystemTime::UNIX_EPOCH)
        duration: core::time::Duration,
    },

    /// The decoder tried to decode a `CString`, but the incoming data contained a 0 byte
    #[cfg(feature = "std")]
    CStringNulError {
        /// Nul byte position
        position: usize,
    },

    /// The reader encountered an IO error but more bytes were expected.
    #[cfg(feature = "std")]
    Io {
        /// The IO error expected
        inner: std::io::Error,

        /// Gives an estimate of how many extra bytes are needed.
        ///
        /// **Note**: this is only an estimate and not indicative of the actual bytes needed.
        ///
        /// **Note**: Bincode has no look-ahead mechanism. This means that this will only return the amount of bytes to be read for the current action, and not take into account the entire data structure being read.
        additional: usize,
    },

    /// An uncommon error occurred, see the inner text for more information
    Other(&'static str),

    /// An uncommon error occurred, see the inner text for more information
    #[cfg(feature = "alloc")]
    OtherString(alloc::string::String),

    #[cfg(feature = "serde")]
    /// A serde-specific error that occurred while decoding.
    Serde(crate::features::serde::DecodeError),
}

impl core::fmt::Display for DecodeError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        // TODO: Improve this?
        write!(f, "{:?}", self)
    }
}

impl DecodeError {
    /// If the current error is `InvalidIntegerType`, change the `expected` and
    /// `found` values from `Ux` to `Ix`. This is needed to have correct error
    /// reporting in src/varint/decode_signed.rs since this calls
    /// src/varint/decode_unsigned.rs and needs to correct the `expected` and
    /// `found` types.
    pub(crate) fn change_integer_type_to_signed(self) -> DecodeError {
        match self {
            Self::InvalidIntegerType { expected, found } => Self::InvalidIntegerType {
                expected: expected.into_signed(),
                found: found.into_signed(),
            },
            other => other,
        }
    }
}

/// Indicates which enum variants are allowed
#[non_exhaustive]
#[derive(Debug, PartialEq, Eq)]
pub enum AllowedEnumVariants {
    /// All values between `min` and `max` (inclusive) are allowed
    #[allow(missing_docs)]
    Range { min: u32, max: u32 },
    /// Each one of these values is allowed
    Allowed(&'static [u32]),
}

/// Integer types. Used by [DecodeError]. These types have no purpose other than being shown in errors.
#[non_exhaustive]
#[derive(Debug, PartialEq, Eq)]
#[allow(missing_docs)]
pub enum IntegerType {
    U8,
    U16,
    U32,
    U64,
    U128,
    Usize,

    I8,
    I16,
    I32,
    I64,
    I128,
    Isize,

    Reserved,
}

impl IntegerType {
    /// Change the `Ux` value to the associated `Ix` value.
    /// Returns the old value if `self` is already `Ix`.
    pub(crate) const fn into_signed(self) -> Self {
        match self {
            Self::U8 => Self::I8,
            Self::U16 => Self::I16,
            Self::U32 => Self::I32,
            Self::U64 => Self::I64,
            Self::U128 => Self::I128,
            Self::Usize => Self::Isize,

            other => other,
        }
    }
}
