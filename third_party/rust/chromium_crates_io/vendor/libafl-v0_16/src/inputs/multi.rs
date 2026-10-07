//! An input composed of multiple parts identified by a key.

use alloc::{string::String, vec::Vec};
use core::{fmt::Debug, hash::Hash};

use serde::{Serialize, de::DeserializeOwned};

use crate::{
    corpus::CorpusId,
    inputs::{Input, ListInput},
};

/// An input composed of multiple parts, each identified by a key.
///
/// It relies on a list to store the keys and parts. Keys may appear multiple times.
pub type MultipartInput<I, K> = ListInput<(K, I)>;

impl<I, K> Input for MultipartInput<I, K>
where
    I: Input,
    K: PartialEq + Debug + Serialize + DeserializeOwned + Clone + Hash,
{
    fn generate_name(&self, id: Option<CorpusId>) -> String {
        self.parts()
            .iter()
            .map(|(_k, i)| i.generate_name(id))
            .collect::<Vec<_>>()
            .join(",")
    }
}

/// Trait for types that provide a way to access parts by key.
pub trait Keyed<K, V> {
    /// Get the keys of the parts of this input.
    ///
    /// Keys may appear multiple times if they are used multiple times in the input.
    fn keys<'a>(&'a self) -> impl Iterator<Item = &'a K>
    where
        K: 'a;

    /// Get a reference to each part with the provided key along with its index.
    fn with_key<'a, 'b>(&'b self, key: &'a K) -> impl Iterator<Item = (usize, &'b V)> + 'a
    where
        'b: 'a,
        V: 'b;

    /// Gets a mutable reference to each part with the provided key along with its index.
    fn with_key_mut<'a, 'b>(
        &'b mut self,
        key: &'a K,
    ) -> impl Iterator<Item = (usize, &'b mut V)> + 'a
    where
        'b: 'a,
        V: 'b;
}

impl<I, K> Keyed<K, I> for MultipartInput<I, K>
where
    K: PartialEq,
{
    fn keys<'a>(&'a self) -> impl Iterator<Item = &'a K>
    where
        K: 'a,
    {
        self.parts().iter().map(|(k, _)| k)
    }

    fn with_key<'a, 'b>(&'b self, key: &'a K) -> impl Iterator<Item = (usize, &'b I)> + 'a
    where
        'b: 'a,
        I: 'b,
    {
        self.parts()
            .iter()
            .enumerate()
            .filter_map(move |(i, (k, input))| (key == k).then_some((i, input)))
    }

    fn with_key_mut<'a, 'b>(
        &'b mut self,
        key: &'a K,
    ) -> impl Iterator<Item = (usize, &'b mut I)> + 'a
    where
        'b: 'a,
        I: 'b,
    {
        self.parts_mut()
            .iter_mut()
            .enumerate()
            .filter_map(move |(i, (k, input))| (key == k).then_some((i, input)))
    }
}
