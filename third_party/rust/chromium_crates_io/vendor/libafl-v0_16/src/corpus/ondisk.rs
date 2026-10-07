//! The [`OnDiskCorpus`] stores all [`Testcase`]s to disk.
//!
//! It _never_ keeps any of them in memory.
//! This is a good solution for solutions that are never reused, or for *very* memory-constraint environments.
//! For any other occasions, consider using [`CachedOnDiskCorpus`]
//! which stores a certain number of [`Testcase`]s in memory and removes additional ones in a FIFO manner.

use alloc::string::String;
use core::{
    cell::{Ref, RefCell, RefMut},
    time::Duration,
};
use std::path::{Path, PathBuf};

use libafl_bolts::serdeany::SerdeAnyMap;
use serde::{Deserialize, Serialize};

use crate::{
    Error,
    corpus::{CachedOnDiskCorpus, Corpus, CorpusId, EnableDisableCorpus, HasTestcase, Testcase},
    inputs::Input,
};

/// Options for the the format of the on-disk metadata
#[derive(Default, Debug, Clone, Serialize, Deserialize)]
pub enum OnDiskMetadataFormat {
    /// A binary-encoded postcard
    Postcard,
    /// JSON
    Json,
    /// JSON formatted for readability
    #[default]
    JsonPretty,
    /// The same as [`OnDiskMetadataFormat::JsonPretty`], but compressed
    #[cfg(feature = "gzip")]
    JsonGzip,
}

/// The [`Testcase`] metadata that'll be stored to disk
#[derive(Debug, Serialize)]
pub struct OnDiskMetadata<'a> {
    /// The dynamic metadata [`SerdeAnyMap`] stored to disk
    pub metadata: &'a SerdeAnyMap,
    /// The exec time for this [`Testcase`]
    pub exec_time: &'a Option<Duration>,
    /// The executions of this [`Testcase`]
    pub executions: &'a u64,
}

/// A corpus able to store [`Testcase`]s to disk, and load them from disk, when they are being used.
///
/// Metadata is written to a `.<filename>.metadata` file in the same folder by default.
#[derive(Default, Serialize, Deserialize, Clone, Debug)]
pub struct OnDiskCorpus<I> {
    /// The root directory backing this corpus
    dir_path: PathBuf,
    /// We wrapp a cached corpus and set its size to 1.
    inner: CachedOnDiskCorpus<I>,
}

impl<I> Corpus<I> for OnDiskCorpus<I>
where
    I: Input,
{
    /// Returns the number of all enabled entries
    #[inline]
    fn count(&self) -> usize {
        self.inner.count()
    }

    /// Returns the number of all disabled entries
    fn count_disabled(&self) -> usize {
        self.inner.count_disabled()
    }

    /// Returns the number of all entries
    #[inline]
    fn count_all(&self) -> usize {
        self.inner.count_all()
    }

    /// Add an enabled testcase to the corpus and return its index
    #[inline]
    fn add(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error> {
        self.inner.add(testcase)
    }

    /// Add a disabled testcase to the corpus and return its index
    #[inline]
    fn add_disabled(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error> {
        self.inner.add_disabled(testcase)
    }

    /// Replaces the testcase at the given idx
    #[inline]
    fn replace(&mut self, id: CorpusId, testcase: Testcase<I>) -> Result<Testcase<I>, Error> {
        self.inner.replace(id, testcase)
    }

    /// Removes an entry from the corpus, returning it if it was present; considers both enabled and disabled testcases
    #[inline]
    fn remove(&mut self, id: CorpusId) -> Result<Testcase<I>, Error> {
        self.inner.remove(id)
    }

    /// Get by id; will check the disabled corpus if not available in the enabled
    #[inline]
    fn get(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        self.inner.get(id)
    }

    /// Get by id; considers both enabled and disabled testcases
    #[inline]
    fn get_from_all(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        self.inner.get_from_all(id)
    }

    /// Current testcase scheduled
    #[inline]
    fn current(&self) -> &Option<CorpusId> {
        self.inner.current()
    }

    /// Current testcase scheduled (mutable)
    #[inline]
    fn current_mut(&mut self) -> &mut Option<CorpusId> {
        self.inner.current_mut()
    }

    #[inline]
    fn next(&self, id: CorpusId) -> Option<CorpusId> {
        self.inner.next(id)
    }

    /// Peek the next free corpus id
    #[inline]
    fn peek_free_id(&self) -> CorpusId {
        self.inner.peek_free_id()
    }

    #[inline]
    fn prev(&self, id: CorpusId) -> Option<CorpusId> {
        self.inner.prev(id)
    }

    #[inline]
    fn first(&self) -> Option<CorpusId> {
        self.inner.first()
    }

    #[inline]
    fn last(&self) -> Option<CorpusId> {
        self.inner.last()
    }

    /// Get the nth corpus id; considers only enabled testcases
    #[inline]
    fn nth(&self, nth: usize) -> CorpusId {
        self.inner.nth(nth)
    }
    /// Get the nth corpus id; considers both enabled and disabled testcases
    #[inline]
    fn nth_from_all(&self, nth: usize) -> CorpusId {
        self.inner.nth_from_all(nth)
    }

    #[inline]
    fn load_input_into(&self, testcase: &mut Testcase<I>) -> Result<(), Error> {
        self.inner.load_input_into(testcase)
    }

    #[inline]
    fn store_input_from(&self, testcase: &Testcase<I>) -> Result<(), Error> {
        self.inner.store_input_from(testcase)
    }
}

impl<I> HasTestcase<I> for OnDiskCorpus<I>
where
    I: Input,
{
    fn testcase(&self, id: CorpusId) -> Result<Ref<'_, Testcase<I>>, Error> {
        Ok(self.get(id)?.borrow())
    }

    fn testcase_mut(&self, id: CorpusId) -> Result<RefMut<'_, Testcase<I>>, Error> {
        Ok(self.get(id)?.borrow_mut())
    }
}

impl<I> EnableDisableCorpus for OnDiskCorpus<I>
where
    I: Input,
{
    #[inline]
    fn disable(&mut self, id: CorpusId) -> Result<(), Error> {
        self.inner.disable(id)
    }

    #[inline]
    fn enable(&mut self, id: CorpusId) -> Result<(), Error> {
        self.inner.enable(id)
    }
}

impl<I> OnDiskCorpus<I> {
    /// Creates an [`OnDiskCorpus`].
    ///
    /// This corpus stores all testcases to disk.
    ///
    /// By default, it stores metadata for each [`Testcase`] as prettified json.
    /// Metadata will be written to a file named `.<testcase>.metadata`
    /// The metadata may include objective reason, specific information for a fuzz job, and more.
    ///
    /// To pick a different metadata format, use [`OnDiskCorpus::with_meta_format`].
    ///
    /// Will error, if [`std::fs::create_dir_all()`] failed for `dir_path`.
    pub fn new<P>(dir_path: P) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        Self::with_meta_format_and_prefix(
            dir_path.as_ref(),
            Some(OnDiskMetadataFormat::JsonPretty),
            None,
            true,
        )
    }

    /// Creates the [`OnDiskCorpus`] with a filename prefix.
    ///
    /// Will error, if [`std::fs::create_dir_all()`] failed for `dir_path`.
    pub fn with_prefix<P>(dir_path: P, prefix: Option<String>) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        Self::with_meta_format_and_prefix(
            dir_path.as_ref(),
            Some(OnDiskMetadataFormat::JsonPretty),
            prefix,
            true,
        )
    }

    /// Creates the [`OnDiskCorpus`] specifying the format in which `Metadata` will be saved to disk.
    ///
    /// Will error, if [`std::fs::create_dir_all()`] failed for `dir_path`.
    pub fn with_meta_format<P>(
        dir_path: P,
        meta_format: OnDiskMetadataFormat,
    ) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        Self::with_meta_format_and_prefix(dir_path.as_ref(), Some(meta_format), None, true)
    }

    /// Creates an [`OnDiskCorpus`] that will not store .metadata files
    ///
    /// Will error, if [`std::fs::create_dir_all()`] failed for `dir_path`.
    pub fn no_meta<P>(dir_path: P) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        Self::with_meta_format_and_prefix(dir_path.as_ref(), None, None, true)
    }

    /// Creates a new corpus at the given (non-generic) path with the given optional `meta_format`
    /// and `prefix`.
    ///
    /// Will error, if [`std::fs::create_dir_all()`] failed for `dir_path`.
    pub fn with_meta_format_and_prefix(
        dir_path: &Path,
        meta_format: Option<OnDiskMetadataFormat>,
        prefix: Option<String>,
        locking: bool,
    ) -> Result<Self, Error> {
        Ok(OnDiskCorpus {
            dir_path: dir_path.into(),
            inner: CachedOnDiskCorpus::with_meta_format_and_prefix(
                dir_path,
                1,
                meta_format,
                prefix,
                locking,
            )?,
        })
    }

    /// Path to the corpus directory associated with this corpus
    pub fn dir_path(&self) -> &PathBuf {
        &self.dir_path
    }
}
