//! The `GeneralizedInput` is an input that ca be generalized to represent a rule, used by Grimoire

use alloc::vec::Vec;

use libafl_bolts::impl_serdeany;
use serde::{Deserialize, Serialize};

use crate::{
    Error, HasMetadata,
    corpus::Testcase,
    inputs::BytesInput,
    stages::mutational::{MutatedTransform, MutatedTransformPost},
};

/// An item of the generalized input
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq, Hash)]
pub enum GeneralizedItem {
    /// Real bytes
    Bytes(Vec<u8>),
    /// An insertion point
    Gap,
}

/// Metadata regarding the generalised content of an input
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct GeneralizedInputMetadata {
    generalized: Vec<GeneralizedItem>,
}

impl_serdeany!(GeneralizedInputMetadata);

impl GeneralizedInputMetadata {
    /// Fill the generalized vector from a slice of option (None -> Gap)
    #[must_use]
    pub fn generalized_from_options(v: &[Option<u8>]) -> Self {
        let mut generalized = vec![];
        let mut bytes = vec![];
        if v.first() != Some(&None) {
            generalized.push(GeneralizedItem::Gap);
        }
        for e in v {
            match e {
                None => {
                    if !bytes.is_empty() {
                        generalized.push(GeneralizedItem::Bytes(bytes.clone()));
                        bytes.clear();
                    }
                    generalized.push(GeneralizedItem::Gap);
                }
                Some(b) => {
                    bytes.push(*b);
                }
            }
        }
        if !bytes.is_empty() {
            generalized.push(GeneralizedItem::Bytes(bytes));
        }
        if generalized.last() != Some(&GeneralizedItem::Gap) {
            generalized.push(GeneralizedItem::Gap);
        }
        Self { generalized }
    }

    /// Get the size of the generalized
    #[must_use]
    pub fn generalized_len(&self) -> usize {
        let mut size = 0;
        for item in &self.generalized {
            match item {
                GeneralizedItem::Bytes(b) => size += b.len(),
                GeneralizedItem::Gap => size += 1,
            }
        }
        size
    }

    /// Convert generalized to bytes
    #[must_use]
    pub fn generalized_to_bytes(&self) -> Vec<u8> {
        self.generalized
            .iter()
            .filter_map(|item| match item {
                GeneralizedItem::Bytes(bytes) => Some(bytes),
                GeneralizedItem::Gap => None,
            })
            .flatten()
            .copied()
            .collect()
    }

    /// Get the generalized input
    #[must_use]
    pub fn generalized(&self) -> &[GeneralizedItem] {
        &self.generalized
    }

    /// Get the generalized input (mutable)
    pub fn generalized_mut(&mut self) -> &mut Vec<GeneralizedItem> {
        &mut self.generalized
    }
}

impl<S> MutatedTransform<BytesInput, S> for GeneralizedInputMetadata {
    type Post = Self;

    fn try_transform_from(base: &mut Testcase<BytesInput>, _state: &S) -> Result<Self, Error> {
        let meta = base
            .metadata_map()
            .get::<GeneralizedInputMetadata>()
            .ok_or_else(|| {
                Error::key_not_found(format!(
                    "Couldn't find the GeneralizedInputMetadata for corpus entry {base:?}",
                ))
            })
            .cloned()?;

        Ok(meta)
    }

    fn try_transform_into(self, _state: &S) -> Result<(BytesInput, Self::Post), Error> {
        Ok((BytesInput::from(self.generalized_to_bytes()), self))
    }
}

impl<S> MutatedTransformPost<S> for GeneralizedInputMetadata {}
