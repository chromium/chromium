//! Inputs are the actual contents sent to a target for each exeuction.

pub mod bytes;
pub use bytes::BytesInput;

pub mod value;
pub use value::ValueInput;

pub mod encoded;
pub use encoded::*;

pub mod gramatron;
pub use gramatron::*;

pub mod generalized;
pub use generalized::*;

pub mod bytessub;
pub use bytessub::BytesSubInput;

#[cfg(feature = "multipart_inputs")]
pub mod multi;
#[cfg(feature = "multipart_inputs")]
pub use multi::*;
#[cfg(feature = "multipart_inputs")]
pub mod list;
#[cfg(feature = "multipart_inputs")]
pub use list::*;

#[cfg(feature = "nautilus")]
pub mod nautilus;

use alloc::{
    boxed::Box,
    string::String,
    vec::{Drain, Splice, Vec},
};
use core::{
    clone::Clone,
    fmt::Debug,
    hash::Hash,
    marker::PhantomData,
    ops::{DerefMut, RangeBounds},
};
#[cfg(feature = "std")]
use std::{fs::File, io::Read, path::Path};

#[cfg(feature = "std")]
use libafl_bolts::fs::write_file_atomic;
use libafl_bolts::{
    Error, HasLen, generic_hash_std,
    ownedref::{OwnedMutSlice, OwnedSlice},
    subrange::{SubRangeMutSlice, SubRangeSlice},
};
#[cfg(feature = "nautilus")]
pub use nautilus::*;
use serde::{Deserialize, Serialize};

use crate::corpus::CorpusId;

/// An input for the target
#[cfg(not(feature = "std"))]
pub trait Input: Clone + Serialize + serde::de::DeserializeOwned + Debug + Hash {
    /// Write this input to the file
    fn to_file<P>(&self, _path: P) -> Result<(), Error> {
        Err(Error::not_implemented("Not supported in no_std"))
    }

    /// Write this input to the file
    fn from_file<P>(_path: P) -> Result<Self, Error> {
        Err(Error::not_implemented("Not supprted in no_std"))
    }

    /// Generate a name for this input
    fn generate_name(&self, _id: Option<CorpusId>) -> String {
        format!("{:016x}", generic_hash_std(self))
    }
}

/// An input for the target
#[cfg(feature = "std")]
pub trait Input: Clone + Serialize + serde::de::DeserializeOwned + Debug + Hash {
    /// Write this input to the file
    fn to_file<P>(&self, path: P) -> Result<(), Error>
    where
        P: AsRef<Path>,
    {
        write_file_atomic(path, &postcard::to_allocvec(self)?)
    }

    /// Load the content of this input from a file
    fn from_file<P>(path: P) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        let mut file = File::open(path)?;
        let mut bytes = vec![];
        file.read_to_end(&mut bytes)?;
        Ok(postcard::from_bytes(&bytes)?)
    }

    /// Generate a name for this input, the user is responsible for making each name of testcase unique.
    fn generate_name(&self, _id: Option<CorpusId>) -> String {
        format!("{:016x}", generic_hash_std(self))
    }
}

/// Convert between two input types using the given state
pub trait InputConverter {
    /// What to convert from
    type From;
    /// Destination type
    type To;

    /// Convert the src type to the dest
    fn convert(&mut self, input: Self::From) -> Result<Self::To, Error>;
}

/// This trait can transform any input to bytes, which can be sent to the target from a harness.
/// Converters that implement this trait auto-implement [`InputConverter`] for this `I` to [`BytesInput`].
pub trait ToTargetBytes<I> {
    /// Transform to bytes
    fn to_target_bytes<'a>(&mut self, input: &'a I) -> OwnedSlice<'a, u8>;
}

/// An [`InputConverter`] wrapper that converts anything implementing [`ToTargetBytes`] to a [`BytesInput`].
#[derive(Debug)]
pub struct TargetBytesInputConverter<I, T> {
    to_bytes_converter: T,
    phantom: PhantomData<I>,
}

impl<I, T> InputConverter for TargetBytesInputConverter<I, T>
where
    T: ToTargetBytes<I>,
{
    type From = I;
    type To = BytesInput;

    fn convert(&mut self, input: Self::From) -> Result<Self::To, Error> {
        Ok(BytesInput::new(
            self.to_bytes_converter.to_target_bytes(&input).to_vec(),
        ))
    }
}

impl<I, T> TargetBytesInputConverter<I, T> {
    /// Create a new [`TargetBytesInputConverter`] from the given [`ToTargetBytes`] fn, that will convert target bytes to a [`BytesInput`].
    pub fn new(to_target_bytes_converter: T) -> Self {
        Self {
            to_bytes_converter: to_target_bytes_converter,
            phantom: PhantomData,
        }
    }
}

impl<I, T> From<T> for TargetBytesInputConverter<I, T> {
    fn from(to_bytes_converter: T) -> Self {
        Self::new(to_bytes_converter)
    }
}

/// `None` type to satisfy the type infearence in an `Option`
#[macro_export]
macro_rules! none_input_converter {
    () => {
        None::<$crate::inputs::ClosureInputConverter<_, _>>
    };
}

/// An input for tests, mainly. There is no real use much else.
#[derive(Copy, Clone, Serialize, Deserialize, Debug, Default, Hash)]
pub struct NopInput {}

impl NopInput {
    /// Creates a new [`NopInput`]
    #[must_use]
    pub fn new() -> Self {
        Self {}
    }
}

impl Input for NopInput {}
impl HasTargetBytes for NopInput {
    fn target_bytes(&self) -> OwnedSlice<'_, u8> {
        OwnedSlice::from(vec![0])
    }
}

impl HasLen for NopInput {
    fn len(&self) -> usize {
        0
    }
}

/// `InputConverter` that uses a closure to convert
pub struct ClosureInputConverter<F, T>
where
    F: Input,
    T: Input,
{
    convert_cb: Box<dyn FnMut(F) -> Result<T, Error>>,
}

impl<F, T> Debug for ClosureInputConverter<F, T>
where
    F: Input,
    T: Input,
{
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("ClosureInputConverter")
            .finish_non_exhaustive()
    }
}

impl<F, T> ClosureInputConverter<F, T>
where
    F: Input,
    T: Input,
{
    /// Create a new converter using two closures, use None to forbid the conversion or the conversion back
    #[must_use]
    pub fn new(convert_cb: Box<dyn FnMut(F) -> Result<T, Error>>) -> Self {
        Self { convert_cb }
    }
}

impl<F, T> InputConverter for ClosureInputConverter<F, T>
where
    F: Input,
    T: Input,
{
    type From = F;
    type To = T;

    fn convert(&mut self, input: Self::From) -> Result<Self::To, Error> {
        (self.convert_cb)(input)
    }
}

// TODO change this to fn target_bytes(&self, buffer: &mut Vec<u8>) -> &[u8];
/// Has a byte representation intended for the target.
/// Can be represented with a vector of bytes.
/// This representation is not necessarily deserializable.
/// Instead, it can be used as bytes input for a target
pub trait HasTargetBytes {
    /// Target bytes, that can be written to a target
    fn target_bytes(&self) -> OwnedSlice<'_, u8>;
}

/// Contains mutable bytes
pub trait HasMutatorBytes: HasLen {
    /// The bytes
    fn mutator_bytes(&self) -> &[u8];

    /// The bytes to mutate
    fn mutator_bytes_mut(&mut self) -> &mut [u8];

    /// Creates a [`SubRangeSlice`] from this input, that can be used to slice a byte array.
    fn sub_bytes<R>(&self, range: R) -> SubRangeSlice<'_, u8>
    where
        R: RangeBounds<usize>,
    {
        SubRangeSlice::new(OwnedSlice::from(self.mutator_bytes()), range)
    }

    /// Creates a [`SubRangeMutSlice`] from this input, that can be used to slice a byte array.
    fn sub_bytes_mut<R>(&mut self, range: R) -> SubRangeMutSlice<'_, u8>
    where
        R: RangeBounds<usize>,
    {
        SubRangeMutSlice::new(OwnedMutSlice::from(self.mutator_bytes_mut()), range)
    }

    /// Creates a [`BytesSubInput`] from this input, that can be used for local mutations.
    fn sub_input<R>(&mut self, range: R) -> BytesSubInput<'_, Self>
    where
        R: RangeBounds<usize>,
    {
        BytesSubInput::new(self, range)
    }
}

impl HasMutatorBytes for Vec<u8> {
    fn mutator_bytes(&self) -> &[u8] {
        self.as_ref()
    }

    fn mutator_bytes_mut(&mut self) -> &mut [u8] {
        self.as_mut()
    }
}

/// A wrapper type that allows us to use mutators for Mutators for `&mut `[`Vec`].
#[deprecated(since = "0.15.0", note = "Use &mut Vec<u8> directly")]
pub type MutVecInput<'a> = &'a mut Vec<u8>;

impl HasMutatorBytes for &'_ mut Vec<u8> {
    fn mutator_bytes(&self) -> &[u8] {
        self
    }

    fn mutator_bytes_mut(&mut self) -> &mut [u8] {
        self
    }
}

/// Contains resizable bytes
pub trait ResizableMutator<T> {
    /// Resize the mutator content to a given new size.
    /// Use `value` to fill new slots in case the buffer grows.
    /// See [`Vec::splice`].
    fn resize(&mut self, new_len: usize, value: T);

    /// Extends the given buffer with an iterator. See [`alloc::vec::Vec::extend`]
    fn extend<'a, I: IntoIterator<Item = &'a T>>(&mut self, iter: I)
    where
        T: 'a;

    /// Splices the given target values according to [`Vec::splice`]'s rules
    fn splice<R, I>(&mut self, range: R, replace_with: I) -> Splice<'_, I::IntoIter>
    where
        R: RangeBounds<usize>,
        I: IntoIterator<Item = T>;

    /// Drains the given target value according to [`Vec::drain`]'s rules
    fn drain<R>(&mut self, range: R) -> Drain<'_, T>
    where
        R: RangeBounds<usize>;
}

impl<T> ResizableMutator<T> for Vec<T>
where
    T: Copy + 'static,
{
    fn resize(&mut self, new_len: usize, value: T) {
        <Vec<T>>::resize(self, new_len, value);
    }

    fn extend<'a, I: IntoIterator<Item = &'a T>>(&mut self, iter: I) {
        <Vec<T> as Extend<I::Item>>::extend(self, iter);
    }

    fn splice<R, I>(&mut self, range: R, replace_with: I) -> Splice<'_, I::IntoIter>
    where
        R: RangeBounds<usize>,
        I: IntoIterator<Item = T>,
    {
        <Vec<T>>::splice(self, range, replace_with)
    }

    fn drain<R>(&mut self, range: R) -> Drain<'_, T>
    where
        R: RangeBounds<usize>,
    {
        <Vec<T>>::drain(self, range)
    }
}

impl ResizableMutator<u8> for &mut Vec<u8> {
    fn resize(&mut self, new_len: usize, value: u8) {
        self.deref_mut().resize(new_len, value);
    }

    fn extend<'b, I: IntoIterator<Item = &'b u8>>(&mut self, iter: I) {
        <Vec<u8> as Extend<I::Item>>::extend(self, iter);
    }

    fn splice<R, I>(&mut self, range: R, replace_with: I) -> Splice<'_, I::IntoIter>
    where
        R: RangeBounds<usize>,
        I: IntoIterator<Item = u8>,
    {
        self.deref_mut().splice::<R, I>(range, replace_with)
    }

    fn drain<R>(&mut self, range: R) -> Drain<'_, u8>
    where
        R: RangeBounds<usize>,
    {
        self.deref_mut().drain(range)
    }
}

#[derive(Debug, Copy, Clone, Default)]
/// Basic `NopToTargetBytes` with just one type that is not converting
pub struct NopToTargetBytes;

impl NopToTargetBytes {
    /// Creates a new [`NopToTargetBytes`]
    #[must_use]
    pub fn new() -> Self {
        Self
    }
}

impl<I> ToTargetBytes<I> for NopToTargetBytes
where
    I: HasTargetBytes + Debug,
{
    fn to_target_bytes<'a>(&mut self, input: &'a I) -> OwnedSlice<'a, u8> {
        input.target_bytes()
    }
}
