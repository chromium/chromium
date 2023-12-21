//! Traits for interpreting font data

use types::{BigEndian, FixedSize, Scalar, Tag};

use crate::font_data::FontData;

/// A type that can be read from raw table data.
///
/// This trait is implemented for all font tables that are self-describing: that
/// is, tables that do not require any external state in order to interpret their
/// underlying bytes. (Tables that require external state implement
/// [`FontReadWithArgs`] instead)
pub trait FontRead<'a>: Sized {
    /// Read an instace of `Self` from the provided data, performing validation.
    ///
    /// In the case of a table, this method is responsible for ensuring the input
    /// data is consistent: this means ensuring that any versioned fields are
    /// present as required by the version, and that any array lengths are not
    /// out-of-bounds.
    fn read(data: FontData<'a>) -> Result<Self, ReadError>;
}

//NOTE: this is separate so that it can be a super trait of FontReadWithArgs and
//ComputeSize, without them needing to know about each other? I'm not sure this
//is necessary, but I don't know the full heirarchy of traits I'm going to need
//yet, so this seems... okay?

/// A trait for a type that needs additional arguments to be read.
pub trait ReadArgs {
    type Args: Copy;
}

/// A trait for types that require external data in order to be constructed.
///
/// You should not need to use this directly; it is intended to be used from
/// generated code. Any type that requires external arguments also has a custom
/// `read` constructor where you can pass those arguments like normal.
pub trait FontReadWithArgs<'a>: Sized + ReadArgs {
    /// read an item, using the provided args.
    ///
    /// If successful, returns a new item of this type, and the number of bytes
    /// used to construct it.
    ///
    /// If a type requires multiple arguments, they will be passed as a tuple.
    fn read_with_args(data: FontData<'a>, args: &Self::Args) -> Result<Self, ReadError>;
}

// a blanket impl of ReadArgs/FontReadWithArgs for general FontRead types.
//
// This is used by ArrayOfOffsets/ArrayOfNullableOffsets to provide a common
// interface for regardless of whether a type has args.
impl<'a, T: FontRead<'a>> ReadArgs for T {
    type Args = ();
}

impl<'a, T: FontRead<'a>> FontReadWithArgs<'a> for T {
    fn read_with_args(data: FontData<'a>, _: &Self::Args) -> Result<Self, ReadError> {
        Self::read(data)
    }
}

/// A trait for tables that have multiple possible formats.
pub trait Format<T> {
    /// The format value for this table.
    const FORMAT: T;
}

/// A type that can compute its size at runtime, based on some input.
///
/// For types with a constant size, see [`FixedSize`] and
/// for types which store their size inline, see [`VarSize`].
pub trait ComputeSize: ReadArgs {
    /// Compute the number of bytes required to represent this type.
    fn compute_size(args: &Self::Args) -> usize;
}

/// A trait for types that have variable length.
///
/// As a rule, these types have an initial length field.
///
/// For types with a constant size, see [`FixedSize`] and
/// for types which can pre-compute their size, see [`ComputeSize`].
pub trait VarSize {
    /// The type of the first (length) field of the item.
    ///
    /// When reading this type, we will read this value first, and use it to
    /// determine the total length.
    type Size: Scalar + Into<u32>;

    #[doc(hidden)]
    fn read_len_at(data: FontData, pos: usize) -> Option<usize> {
        let asu32 = data.read_at::<Self::Size>(pos).ok()?.into();
        Some(asu32 as usize + Self::Size::RAW_BYTE_LEN)
    }
}

/// A marker trait for types that can read from a big-endian buffer without copying.
///
/// This is used as a trait bound on certain methods on [`FontData`] (such as
/// [`FontData::read_ref_at`] and [`FontData::read_array`]) in order to ensure
/// that those methods are only used with types that uphold certain safety
/// guarantees.
///
/// WARNING: Do not implement this trait manually. Implementations are created
/// where appropriate during code generation, and there should be no conditions
/// under which this trait could be implemented, but cannot be implemented by
/// codegen.
///
/// # Safety
///
/// If a type `T` implements `FromBytes` then unsafe code may assume that it is
/// safe to interpret any sequence of bytes with length equal to
/// `std::mem::size_of::<T>()` as `T`.
///
/// we additionally ensure the following conditions:
///
/// - the type must have no internal padding
/// - `std::mem::align_of::<T>() == 1`
/// - for structs, the type is `repr(packed)` and `repr(C)`, and all fields are
///   also `FromBytes`
///
/// In practice, this trait is only implemented for `u8`, `BigEndian<T>`,
/// and for structs where all fields are those base types.
pub unsafe trait FromBytes: FixedSize + sealed::Sealed {
    /// You should not be implementing this trait!
    #[doc(hidden)]
    fn this_trait_should_only_be_implemented_in_generated_code();
}

// a sealed trait. see <https://rust-lang.github.io/api-guidelines/future-proofing.html>
pub(crate) mod sealed {
    pub trait Sealed {}
}

impl sealed::Sealed for u8 {}
// SAFETY: any byte can be interpreted as any other byte
unsafe impl FromBytes for u8 {
    fn this_trait_should_only_be_implemented_in_generated_code() {}
}

impl<T: Scalar> sealed::Sealed for BigEndian<T> {}
// SAFETY: BigEndian<T> is always wrapper around a transparent fixed-size byte array
unsafe impl<T: Scalar> FromBytes for BigEndian<T> {
    fn this_trait_should_only_be_implemented_in_generated_code() {}
}

/// An error that occurs when reading font data
#[derive(Debug, Clone)]
pub enum ReadError {
    OutOfBounds,
    // i64 is flexible enough to store any value we might encounter
    InvalidFormat(i64),
    InvalidSfnt(u32),
    InvalidTtc(Tag),
    InvalidCollectionIndex(u32),
    InvalidArrayLen,
    ValidationError,
    NullOffset,
    TableIsMissing(Tag),
    MetricIsMissing(Tag),
    MalformedData(&'static str),
}

impl std::fmt::Display for ReadError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            ReadError::OutOfBounds => write!(f, "An offset was out of bounds"),
            ReadError::InvalidFormat(x) => write!(f, "Invalid format '{x}'"),
            ReadError::InvalidSfnt(ver) => write!(f, "Invalid sfnt version 0x{ver:08X}"),
            ReadError::InvalidTtc(tag) => write!(f, "Invalid ttc tag {tag}"),
            ReadError::InvalidCollectionIndex(ix) => {
                write!(f, "Invalid index {ix} for font collection")
            }
            ReadError::InvalidArrayLen => {
                write!(f, "Specified array length not a multiple of item size")
            }
            ReadError::ValidationError => write!(f, "A validation error occured"),
            ReadError::NullOffset => write!(f, "An offset was unexpectedly null"),
            ReadError::TableIsMissing(tag) => write!(f, "the {tag} table is missing"),
            ReadError::MetricIsMissing(tag) => write!(f, "the {tag} metric is missing"),
            ReadError::MalformedData(msg) => write!(f, "Malformed data: '{msg}'"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for ReadError {}
