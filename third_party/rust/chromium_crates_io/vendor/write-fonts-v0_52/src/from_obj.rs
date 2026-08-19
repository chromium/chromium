//! Traits for converting from parsed font data to their compile equivalents

use std::collections::BTreeSet;

use read_fonts::{ArrayOfNullableOffsets, ArrayOfOffsets, FontData, FontRead, Offset, ReadError};
use types::{BigEndian, Scalar};

use crate::{NullableOffsetMarker, OffsetMarker};

/// A trait for types that can fully resolve themselves.
///
/// This means that any offsets held in this type are resolved relative to the
/// start of the table itself (and not some parent table)
pub trait FromTableRef<T>: FromObjRef<T> {
    fn from_table_ref(from: &T) -> Self {
        let data = FontData::new(&[]);
        Self::from_obj_ref(from, data)
    }
}

/// A trait for types that can resolve themselves when provided data to resolve offsets.
///
/// It is possible that the generated object is malformed; for instance offsets
/// may be null where it is not allowed. This can be checked by calling [`validate`][]
/// on the generated object.
///
/// This is implemented for the majority of parse types. Those that are the base
/// for offset data ignore the provided data and use their own.
///
/// [`validate`]: [crate::Validate::validate]
pub trait FromObjRef<T: ?Sized>: Sized {
    /// Convert `from` to an instance of `Self`, using the provided data to resolve offsets.
    fn from_obj_ref(from: &T, data: FontData) -> Self;
}

/// A conversion from a parsed font object type to an owned version, resolving
/// offsets.
///
/// You should avoid implementing this trait manually. Like [`std::convert::Into`],
/// it is provided as a blanket impl when you implement [`FromObjRef<T>`].
pub trait ToOwnedObj<T> {
    /// Convert this type into `T`, using the provided data to resolve any offsets.
    fn to_owned_obj(&self, data: FontData) -> T;
}

/// A conversion from a fully resolvable parsed font table to its owned equivalent.
///
/// As with [`ToOwnedObj`], you should not need to implement this manually.
pub trait ToOwnedTable<T>: ToOwnedObj<T> {
    fn to_owned_table(&self) -> T;
}

impl<U, T> ToOwnedObj<U> for T
where
    U: FromObjRef<T>,
{
    fn to_owned_obj(&self, data: FontData) -> U {
        U::from_obj_ref(self, data)
    }
}

impl<U, T> ToOwnedTable<U> for T
where
    U: FromTableRef<T>,
{
    fn to_owned_table(&self) -> U {
        U::from_table_ref(self)
    }
}

impl<T> FromObjRef<BigEndian<T>> for T
where
    T: Scalar,
    BigEndian<T>: Copy,
{
    fn from_obj_ref(from: &BigEndian<T>, _: FontData) -> Self {
        from.get()
    }
}

// we need this because we special case &[u8], eliding the BigEndian wrapper.
impl FromObjRef<u8> for u8 {
    fn from_obj_ref(from: &u8, _data: FontData) -> Self {
        *from
    }
}

impl<T, U> FromObjRef<&[U]> for Vec<T>
where
    T: FromObjRef<U>,
{
    fn from_obj_ref(from: &&[U], data: FontData) -> Self {
        from.iter().map(|item| item.to_owned_obj(data)).collect()
    }
}

impl<T, U> FromObjRef<&[U]> for BTreeSet<T>
where
    T: FromObjRef<U> + std::cmp::Ord,
{
    fn from_obj_ref(from: &&[U], data: FontData) -> Self {
        from.iter().map(|item| item.to_owned_obj(data)).collect()
    }
}

// A blanket impl to cover converting any Option<T> if T is convertible
impl<T: FromObjRef<U>, U> FromObjRef<Option<U>> for Option<T> {
    fn from_obj_ref(from: &Option<U>, data: FontData) -> Self {
        from.as_ref().map(|inner| T::from_obj_ref(inner, data))
    }
}

// A blanket impl to cover converting any Option<T> if T is convertible
impl<T: FromTableRef<U>, U> FromTableRef<Option<U>> for Option<T> {
    fn from_table_ref(from: &Option<U>) -> Self {
        from.as_ref().map(ToOwnedTable::to_owned_table)
    }
}

/* blanket impls converting resolved offsets to offsetmarkers */

impl<T: FromObjRef<U> + Default, U, const N: usize> FromObjRef<Result<U, ReadError>>
    for OffsetMarker<T, N>
{
    fn from_obj_ref(from: &Result<U, ReadError>, data: FontData) -> Self {
        match from {
            Err(_) => OffsetMarker::default(),
            Ok(table) => OffsetMarker::new(table.to_owned_obj(data)),
        }
    }
}

impl<T: FromObjRef<U>, U, const N: usize> FromObjRef<Option<Result<U, ReadError>>>
    for NullableOffsetMarker<T, N>
{
    fn from_obj_ref(from: &Option<Result<U, ReadError>>, data: FontData) -> Self {
        match from {
            Some(Ok(table)) => NullableOffsetMarker::new(Some(table.to_owned_obj(data))),
            _ => NullableOffsetMarker::new(None),
        }
    }
}

// used for bare offsets
impl<T: FromTableRef<U> + Default, U, const N: usize> FromTableRef<Result<U, ReadError>>
    for OffsetMarker<T, N>
{
    fn from_table_ref(from: &Result<U, ReadError>) -> Self {
        match from {
            Err(_) => OffsetMarker::default(),
            Ok(table) => OffsetMarker::new(table.to_owned_table()),
        }
    }
}

// convert bare nullable/versioned offsets to NullableOffsetMarker
impl<T: FromTableRef<U>, U, const N: usize> FromTableRef<Option<Result<U, ReadError>>>
    for NullableOffsetMarker<T, N>
{
    fn from_table_ref(from: &Option<Result<U, ReadError>>) -> Self {
        match from {
            Some(Ok(table)) => NullableOffsetMarker::new(Some(table.to_owned_table())),
            _ => NullableOffsetMarker::new(None),
        }
    }
}

// impls for converting arrays:
impl<'a, T, U, O, const N: usize> FromObjRef<ArrayOfOffsets<'a, U, O>> for Vec<OffsetMarker<T, N>>
where
    T: FromObjRef<U> + Default,
    U: FontRead<'a>,
    U::Args: 'static,
    O: Scalar + Offset,
{
    fn from_obj_ref(from: &ArrayOfOffsets<'a, U, O>, data: FontData) -> Self {
        from.iter()
            .map(|x| OffsetMarker::from_obj_ref(&x, data))
            .collect()
    }
}

impl<'a, T, U, O, const N: usize> FromObjRef<ArrayOfNullableOffsets<'a, U, O>>
    for Vec<NullableOffsetMarker<T, N>>
where
    T: FromObjRef<U> + Default,
    U: FontRead<'a>,
    U::Args: 'static,
    O: Scalar + Offset,
{
    fn from_obj_ref(from: &ArrayOfNullableOffsets<'a, U, O>, data: FontData) -> Self {
        from.iter()
            .map(|x| NullableOffsetMarker::from_obj_ref(&x, data))
            .collect()
    }
}

impl<'a, T, U, O, const N: usize> FromTableRef<ArrayOfOffsets<'a, U, O>> for Vec<OffsetMarker<T, N>>
where
    T: FromTableRef<U> + Default,
    U: FontRead<'a>,
    U::Args: 'static,
    O: Scalar + Offset,
{
}

impl<'a, T, U, O, const N: usize> FromTableRef<ArrayOfNullableOffsets<'a, U, O>>
    for Vec<NullableOffsetMarker<T, N>>
where
    T: FromObjRef<U> + Default,
    U: FontRead<'a>,
    U::Args: 'static,
    O: Scalar + Offset,
{
}
