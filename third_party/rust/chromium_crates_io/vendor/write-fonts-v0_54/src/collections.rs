//! helpers related to collection types

use std::collections::BTreeSet;

#[cfg(feature = "tables")]
use crate::{NullableOffsetMarker, OffsetMarker};

/// A helper trait for array-like fields, where we need to know
/// the length in order to populate another field.
pub trait HasLen {
    fn len(&self) -> usize;
}

impl<T> HasLen for [T] {
    fn len(&self) -> usize {
        self.len()
    }
}

impl<T> HasLen for BTreeSet<T> {
    fn len(&self) -> usize {
        self.len()
    }
}

impl<T> HasLen for Vec<T> {
    fn len(&self) -> usize {
        self.len()
    }
}

impl<T: HasLen> HasLen for Option<T> {
    fn len(&self) -> usize {
        match &self {
            Some(t) => t.len(),
            None => 0,
        }
    }
}

#[cfg(feature = "tables")]
impl<T: HasLen, const N: usize> HasLen for OffsetMarker<T, N> {
    fn len(&self) -> usize {
        T::len(self)
    }
}

#[cfg(feature = "tables")]
impl<T: HasLen, const N: usize> HasLen for NullableOffsetMarker<T, N> {
    fn len(&self) -> usize {
        self.as_ref().map(HasLen::len).unwrap_or(0)
    }
}
