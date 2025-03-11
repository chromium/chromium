// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Runtime `Skeleton` implementation for more efficient deserialization.

use super::reference;
use crate::provider::fields::Field;
use alloc::vec::Vec;
use zerovec::ZeroVec;

/// A skeleton that supports zero-copy deserialization.
#[derive(Debug, PartialEq, Clone)]
pub struct Skeleton<'data>(pub(crate) ZeroVec<'data, Field>);

impl From<reference::Skeleton> for Skeleton<'_> {
    fn from(input: reference::Skeleton) -> Self {
        let fields = input.fields_iter().copied().collect::<Vec<_>>();
        Self(ZeroVec::alloc_from_slice(&fields))
    }
}

impl<'data> From<ZeroVec<'data, Field>> for Skeleton<'data> {
    fn from(input: ZeroVec<'data, Field>) -> Self {
        Self(input)
    }
}

#[cfg(feature = "datagen")]
impl core::fmt::Display for Skeleton<'_> {
    fn fmt(&self, formatter: &mut core::fmt::Formatter) -> core::fmt::Result {
        use core::fmt::Write;
        for field in self.0.iter() {
            let ch: char = field.symbol.into();
            for _ in 0..field.length.to_len() {
                formatter.write_char(ch)?;
            }
        }
        Ok(())
    }
}
