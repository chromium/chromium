//! This module defines trait shared across different `LibAFL` modules

use alloc::boxed::Box;
use core::any::type_name;

#[cfg(feature = "nautilus")]
pub mod nautilus;

use libafl_bolts::{
    Error,
    serdeany::{NamedSerdeAnyMap, SerdeAny, SerdeAnyMap},
};
/// Trait for elements offering metadata
pub trait HasMetadata {
    /// A map, storing all metadata
    fn metadata_map(&self) -> &SerdeAnyMap;
    /// A map, storing all metadata (mutable)
    fn metadata_map_mut(&mut self) -> &mut SerdeAnyMap;

    /// Add a metadata to the metadata map
    #[inline]
    fn add_metadata<M>(&mut self, meta: M)
    where
        M: SerdeAny,
    {
        self.metadata_map_mut().insert(meta);
    }

    /// Add a metadata to the metadata map
    /// Returns error if the metadata is already there
    #[inline]
    fn try_add_metadata<M>(&mut self, meta: M) -> Result<(), Error>
    where
        M: SerdeAny,
    {
        self.metadata_map_mut().try_insert(meta)
    }

    /// Gets metadata, or inserts it using the given construction function `default`
    fn metadata_or_insert_with<M>(&mut self, default: impl FnOnce() -> M) -> &mut M
    where
        M: SerdeAny,
    {
        self.metadata_map_mut().get_or_insert_with::<M>(default)
    }

    /// Remove a metadata from the metadata map
    #[inline]
    fn remove_metadata<M>(&mut self) -> Option<Box<M>>
    where
        M: SerdeAny,
    {
        self.metadata_map_mut().remove::<M>()
    }

    /// Check for a metadata
    ///
    /// # Note
    /// For performance reasons, you likely want to use [`Self::metadata_or_insert_with`] instead
    #[inline]
    fn has_metadata<M>(&self) -> bool
    where
        M: SerdeAny,
    {
        self.metadata_map().get::<M>().is_some()
    }

    /// To get metadata
    #[inline]
    fn metadata<M>(&self) -> Result<&M, Error>
    where
        M: SerdeAny,
    {
        self.metadata_map()
            .get::<M>()
            .ok_or_else(|| Error::key_not_found(format!("{} not found", type_name::<M>())))
    }

    /// To get mutable metadata
    #[inline]
    fn metadata_mut<M>(&mut self) -> Result<&mut M, Error>
    where
        M: SerdeAny,
    {
        self.metadata_map_mut()
            .get_mut::<M>()
            .ok_or_else(|| Error::key_not_found(format!("{} not found", type_name::<M>())))
    }
}

/// Trait for elements offering named metadata
pub trait HasNamedMetadata {
    /// A map, storing all metadata
    fn named_metadata_map(&self) -> &NamedSerdeAnyMap;
    /// A map, storing all metadata (mutable)
    fn named_metadata_map_mut(&mut self) -> &mut NamedSerdeAnyMap;

    /// Add a metadata to the metadata map
    #[inline]
    fn add_named_metadata<M>(&mut self, name: &str, meta: M)
    where
        M: SerdeAny,
    {
        self.named_metadata_map_mut().insert(name, meta);
    }

    /// Add a metadata to the metadata map
    /// Return an error if there already is the metadata with the same name
    #[inline]
    fn add_named_metadata_checked<M>(&mut self, name: &str, meta: M) -> Result<(), Error>
    where
        M: SerdeAny,
    {
        self.named_metadata_map_mut().try_insert(name, meta)
    }

    /// Add a metadata to the metadata map
    #[inline]
    fn remove_named_metadata<M>(&mut self, name: &str) -> Option<Box<M>>
    where
        M: SerdeAny,
    {
        self.named_metadata_map_mut().remove::<M>(name)
    }

    /// Gets metadata, or inserts it using the given construction function `default`
    fn named_metadata_or_insert_with<M>(
        &mut self,
        name: &str,
        default: impl FnOnce() -> M,
    ) -> &mut M
    where
        M: SerdeAny,
    {
        self.named_metadata_map_mut()
            .get_or_insert_with::<M>(name, default)
    }

    /// Check for a metadata
    ///
    /// # Note
    /// You likely want to use [`Self::named_metadata_or_insert_with`] for performance reasons.
    #[inline]
    fn has_named_metadata<M>(&self, name: &str) -> bool
    where
        M: SerdeAny,
    {
        self.named_metadata_map().contains::<M>(name)
    }

    /// To get named metadata
    #[inline]
    fn named_metadata<M>(&self, name: &str) -> Result<&M, Error>
    where
        M: SerdeAny,
    {
        self.named_metadata_map()
            .get::<M>(name)
            .ok_or_else(|| Error::key_not_found(format!("{} not found", type_name::<M>())))
    }

    /// To get mutable named metadata
    #[inline]
    fn named_metadata_mut<M>(&mut self, name: &str) -> Result<&mut M, Error>
    where
        M: SerdeAny,
    {
        self.named_metadata_map_mut()
            .get_mut::<M>(name)
            .ok_or_else(|| Error::key_not_found(format!("{} not found", type_name::<M>())))
    }
}
