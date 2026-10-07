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
pub trait InputConverter<S> {
    /// What to convert from
    type From;
    /// Destination type
    type To;

    /// Convert the src type to the dest
    fn convert(&mut self, state: &mut S, input: Self::From) -> Result<Self::To, Error>;
}

/// This trait can transform any input to bytes, which can be sent to the target from a harness.
pub trait FromTargetBytesConverter<I, S> {
    /// Convert a slice of bytes to an input
    fn convert_from_target_bytes(&mut self, state: &mut S, bytes: &[u8]) -> Result<I, Error>;
}

/// This trait can transform any input to bytes, which can be sent to the target from a harness.
/// Converters that implement this trait auto-implement [`InputConverter`] for this `I` to [`BytesInput`].
pub trait ToTargetBytesConverter<I, S> {
    /// Transform to bytes
    fn convert_to_target_bytes<'a>(&mut self, state: &mut S, input: &'a I) -> OwnedSlice<'a, u8>;
}

/// An [`InputConverter`] wrapper that converts anything implementing [`ToTargetBytesConverter`] to a [`BytesInput`].
#[derive(Debug)]
pub struct ToBytesInputConverter<I, T = BytesInputConverter> {
    to_bytes_converter: T,
    phantom: PhantomData<I>,
}

impl<I, S, T> InputConverter<S> for ToBytesInputConverter<I, T>
where
    T: ToTargetBytesConverter<I, S>,
{
    type From = I;
    type To = BytesInput;

    fn convert(&mut self, state: &mut S, input: Self::From) -> Result<Self::To, Error> {
        Ok(BytesInput::new(
            self.to_bytes_converter
                .convert_to_target_bytes(state, &input)
                .to_vec(),
        ))
    }
}

impl<I, S, T> ToTargetBytesConverter<I, S> for ToBytesInputConverter<I, T>
where
    T: ToTargetBytesConverter<I, S>,
{
    fn convert_to_target_bytes<'a>(&mut self, state: &mut S, input: &'a I) -> OwnedSlice<'a, u8> {
        self.to_bytes_converter
            .convert_to_target_bytes(state, input)
    }
}

impl<I, T> ToBytesInputConverter<I, T> {
    /// Create a new [`ToBytesInputConverter`] from the given [`ToTargetBytesConverter`] fn, that will convert target bytes to a [`BytesInput`].
    pub fn new(to_target_bytes_converter: T) -> Self {
        Self {
            to_bytes_converter: to_target_bytes_converter,
            phantom: PhantomData,
        }
    }
}

impl<I, T> From<T> for ToBytesInputConverter<I, T> {
    fn from(to_bytes_converter: T) -> Self {
        Self::new(to_bytes_converter)
    }
}

/// `None` type to satisfy the type infearence in an `Option`
#[macro_export]
macro_rules! none_input_converter {
    () => {
        None::<$crate::inputs::ClosureInputConverter<_, _, _>>
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

/// Callback for [`ClosureInputConverter`]
pub type ClosureInputCallback<S, F, T> = Box<dyn FnMut(&mut S, F) -> Result<T, Error>>;

/// `InputConverter` that uses a closure to convert
pub struct ClosureInputConverter<S, F, T>
where
    F: Input,
    T: Input,
{
    convert_cb: ClosureInputCallback<S, F, T>,
}

impl<S, F, T> Debug for ClosureInputConverter<S, F, T>
where
    F: Input,
    T: Input,
{
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("ClosureInputConverter")
            .finish_non_exhaustive()
    }
}

impl<S, F, T> ClosureInputConverter<S, F, T>
where
    F: Input,
    T: Input,
{
    /// Create a new converter using two closures, use None to forbid the conversion or the conversion back
    #[must_use]
    pub fn new(convert_cb: ClosureInputCallback<S, F, T>) -> Self {
        Self { convert_cb }
    }
}

impl<S, F, T> InputConverter<S> for ClosureInputConverter<S, F, T>
where
    F: Input,
    T: Input,
{
    type From = F;
    type To = T;

    fn convert(&mut self, state: &mut S, input: Self::From) -> Result<Self::To, Error> {
        (self.convert_cb)(state, input)
    }
}

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

/// Basic [`BytesInputConverter`] with just one type that is not converting
#[derive(Debug, Copy, Clone, Default)]
pub struct BytesInputConverter;

impl BytesInputConverter {
    /// Creates a new [`BytesInputConverter`]
    #[must_use]
    pub fn new() -> Self {
        Self
    }
}

impl<I, S> ToTargetBytesConverter<I, S> for BytesInputConverter
where
    I: HasTargetBytes,
{
    fn convert_to_target_bytes<'a>(&mut self, _state: &mut S, input: &'a I) -> OwnedSlice<'a, u8> {
        input.target_bytes()
    }
}

impl<I, S> FromTargetBytesConverter<I, S> for BytesInputConverter
where
    I: From<BytesInput>,
{
    fn convert_from_target_bytes(&mut self, _state: &mut S, bytes: &[u8]) -> Result<I, Error> {
        Ok(I::from(BytesInput::new(bytes.to_vec())))
    }
}

/// An [`InputConverter`] wrapper that converts a [`BytesInput`] to anything implementing [`FromTargetBytesConverter`].
#[derive(Debug, Clone, Copy)]
pub struct FromBytesInputConverter<I, T = BytesInputConverter> {
    from_bytes_converter: T,
    phantom: PhantomData<I>,
}

impl<I, S, T> InputConverter<S> for FromBytesInputConverter<I, T>
where
    T: FromTargetBytesConverter<I, S>,
{
    type From = BytesInput;
    type To = I;

    fn convert(&mut self, state: &mut S, input: Self::From) -> Result<Self::To, Error> {
        self.from_bytes_converter
            .convert_from_target_bytes(state, &input.target_bytes())
    }
}

impl<I, S> ToTargetBytesConverter<I, S> for FromBytesInputConverter<(), ()>
where
    I: HasTargetBytes,
{
    fn convert_to_target_bytes<'a>(&mut self, _state: &mut S, input: &'a I) -> OwnedSlice<'a, u8> {
        input.target_bytes()
    }
}

impl<I, S, T> FromTargetBytesConverter<I, S> for FromBytesInputConverter<I, T>
where
    T: FromTargetBytesConverter<I, S>,
{
    fn convert_from_target_bytes(&mut self, state: &mut S, bytes: &[u8]) -> Result<I, Error> {
        self.from_bytes_converter
            .convert_from_target_bytes(state, bytes)
    }
}

impl<I, T> FromBytesInputConverter<I, T> {
    /// Create a new [`FromBytesInputConverter`] from the given [`FromTargetBytesConverter`] fn, that will convert a [`BytesInput`] to target bytes.
    pub fn new(from_target_bytes_converter: T) -> Self {
        Self {
            from_bytes_converter: from_target_bytes_converter,
            phantom: PhantomData,
        }
    }
}

impl<I, T> From<T> for FromBytesInputConverter<I, T> {
    fn from(from_bytes_converter: T) -> Self {
        Self::new(from_bytes_converter)
    }
}

#[cfg(test)]
mod tests {
    use crate::inputs::{
        BytesInput, BytesInputConverter, FromBytesInputConverter, FromTargetBytesConverter,
        HasTargetBytes, InputConverter,
    };

    #[test]
    fn test_from_target_bytes() {
        let bytes = vec![1, 2, 3, 4];
        let mut nop = FromBytesInputConverter::new(BytesInputConverter::new());
        let res: BytesInput = nop.convert_from_target_bytes(&mut (), &bytes).unwrap();
        assert_eq!(&*res.target_bytes(), &bytes);

        let start_input = BytesInput::new(bytes.clone());
        let res2 = nop.convert(&mut (), start_input).unwrap();
        assert_eq!(&*res2.target_bytes(), &bytes);
    }
}
