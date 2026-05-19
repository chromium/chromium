//! Corpuses contain the testcases, either in memory, on disk, or somewhere else.

use core::{cell::RefCell, fmt, marker::PhantomData};

use serde::{Deserialize, Serialize};

use crate::Error;

pub mod testcase;
pub use testcase::{HasTestcase, SchedulerTestcaseMetadata, Testcase};

pub mod inmemory;
pub use inmemory::InMemoryCorpus;

pub mod dynamic;
pub use dynamic::DynamicCorpus;

#[cfg(feature = "std")]
pub mod inmemory_ondisk;
#[cfg(feature = "std")]
pub use inmemory_ondisk::InMemoryOnDiskCorpus;

#[cfg(feature = "std")]
pub mod ondisk;
#[cfg(feature = "std")]
pub use ondisk::OnDiskCorpus;

#[cfg(feature = "std")]
pub mod cached;
#[cfg(feature = "std")]
pub use cached::CachedOnDiskCorpus;

#[cfg(feature = "cmin")]
pub mod minimizer;

pub mod nop;
#[cfg(feature = "cmin")]
pub use minimizer::*;
pub use nop::NopCorpus;

/// An abstraction for the index that identify a testcase in the corpus
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
#[repr(transparent)]
pub struct CorpusId(pub usize);

impl fmt::Display for CorpusId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl From<usize> for CorpusId {
    fn from(id: usize) -> Self {
        Self(id)
    }
}

impl From<u64> for CorpusId {
    fn from(id: u64) -> Self {
        Self(id as usize)
    }
}

impl From<CorpusId> for usize {
    /// Not that the `CorpusId` is not necessarily stable in the corpus (if we remove [`Testcase`]s, for example).
    fn from(id: CorpusId) -> Self {
        id.0
    }
}

/// Utility macro to call `Corpus::random_id`; fetches only enabled [`Testcase`]`s`
#[macro_export]
macro_rules! random_corpus_id {
    ($corpus:expr, $rand:expr) => {{
        let cnt = $corpus.count();
        #[cfg(debug_assertions)]
        let nth = $rand.below(core::num::NonZero::new(cnt).expect("Corpus may not be empty!"));
        // # Safety
        // This is a hot path. We try to be as fast as possible here.
        // In debug this is checked (see above.)
        // The worst that can happen is a wrong integer to get returned.
        // In this case, the call below will fail.
        #[cfg(not(debug_assertions))]
        let nth = $rand.below(unsafe { core::num::NonZero::new(cnt).unwrap_unchecked() });
        $corpus.nth(nth)
    }};
}

/// Utility macro to call `Corpus::random_id`; fetches both enabled and disabled [`Testcase`]`s`
/// Note: use `Corpus::get_from_all` as disabled entries are inaccessible from `Corpus::get`
#[macro_export]
macro_rules! random_corpus_id_with_disabled {
    ($corpus:expr, $rand:expr) => {{
        let cnt = $corpus.count_all();
        #[cfg(debug_assertions)]
        let nth = $rand.below(core::num::NonZero::new(cnt).expect("Corpus may not be empty!"));
        // # Safety
        // This is a hot path. We try to be as fast as possible here.
        // In debug this is checked (see above.)
        // The worst that can happen is a wrong integer to get returned.
        // In this case, the call below will fail.
        #[cfg(not(debug_assertions))]
        let nth = $rand.below(unsafe { core::num::NonZero::new(cnt).unwrap_unchecked() });
        $corpus.nth_from_all(nth)
    }};
}

/// Corpus with all current [`Testcase`]s, or solutions
pub trait Corpus<I>: Sized {
    /// Returns the number of all enabled entries
    fn count(&self) -> usize;

    /// Returns the number of all disabled entries
    fn count_disabled(&self) -> usize;

    /// Returns the number of elements including disabled entries
    fn count_all(&self) -> usize;

    /// Returns true, if no elements are in this corpus yet
    fn is_empty(&self) -> bool {
        self.count() == 0
    }

    /// Add an enabled testcase to the corpus and return its index
    fn add(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error>;

    /// Add a disabled testcase to the corpus and return its index
    fn add_disabled(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error>;

    /// Replaces the [`Testcase`] at the given idx, returning the existing.
    fn replace(&mut self, id: CorpusId, testcase: Testcase<I>) -> Result<Testcase<I>, Error>;

    /// Removes an entry from the corpus, returning it if it was present; considers both enabled and disabled testcases
    fn remove(&mut self, id: CorpusId) -> Result<Testcase<I>, Error>;

    /// Get by id; considers only enabled testcases
    fn get(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error>;

    /// Get by id; considers both enabled and disabled testcases
    fn get_from_all(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error>;

    /// Current testcase scheduled
    fn current(&self) -> &Option<CorpusId>;

    /// Current testcase scheduled (mutable)
    fn current_mut(&mut self) -> &mut Option<CorpusId>;

    /// Get the next corpus id
    fn next(&self, id: CorpusId) -> Option<CorpusId>;

    /// Peek the next free corpus id
    fn peek_free_id(&self) -> CorpusId;

    /// Get the prev corpus id
    fn prev(&self, id: CorpusId) -> Option<CorpusId>;

    /// Get the first inserted corpus id
    fn first(&self) -> Option<CorpusId>;

    /// Get the last inserted corpus id
    fn last(&self) -> Option<CorpusId>;

    /// An iterator over very active corpus id
    fn ids(&self) -> CorpusIdIterator<'_, Self, I> {
        CorpusIdIterator {
            corpus: self,
            cur: self.first(),
            cur_back: self.last(),
            phantom: PhantomData,
        }
    }

    /// Get the nth corpus id; considers only enabled testcases
    fn nth(&self, nth: usize) -> CorpusId {
        self.ids()
            .nth(nth)
            .unwrap_or_else(|| panic!("Failed to get the {nth} CorpusId"))
    }

    /// Get the nth corpus id; considers both enabled and disabled testcases
    fn nth_from_all(&self, nth: usize) -> CorpusId;

    /// Method to load the input for this [`Testcase`] from persistent storage,
    /// if necessary, and if was not already loaded (`== Some(input)`).
    /// After this call, `testcase.input()` must always return `Some(input)`.
    fn load_input_into(&self, testcase: &mut Testcase<I>) -> Result<(), Error>;

    /// Method to store the input of this `Testcase` to persistent storage, if necessary.
    fn store_input_from(&self, testcase: &Testcase<I>) -> Result<(), Error>;

    /// Loads the `Input` for a given [`CorpusId`] from the [`Corpus`], and returns the clone.
    fn cloned_input_for_id(&self, id: CorpusId) -> Result<I, Error>
    where
        I: Clone,
    {
        let mut testcase = self.get(id)?.borrow_mut();
        Ok(testcase.load_input(self)?.clone())
    }
}

/// Marker trait for corpus implementations that actually support enable/disable functionality
pub trait EnableDisableCorpus {
    /// Disables a testcase, moving it to the disabled map
    fn disable(&mut self, id: CorpusId) -> Result<(), Error>;

    /// Enables a testcase, moving it to the enabled map
    fn enable(&mut self, id: CorpusId) -> Result<(), Error>;
}

/// Trait for types which track the current corpus index
pub trait HasCurrentCorpusId {
    /// Set the current corpus index; we have started processing this corpus entry
    fn set_corpus_id(&mut self, id: CorpusId) -> Result<(), Error>;

    /// Clear the current corpus index; we are done with this entry
    fn clear_corpus_id(&mut self) -> Result<(), Error>;

    /// Fetch the current corpus index -- typically used after a state recovery or transfer
    fn current_corpus_id(&self) -> Result<Option<CorpusId>, Error>;
}

/// [`Iterator`] over the ids of a [`Corpus`]
#[derive(Debug)]
pub struct CorpusIdIterator<'a, C, I> {
    corpus: &'a C,
    cur: Option<CorpusId>,
    cur_back: Option<CorpusId>,
    phantom: PhantomData<I>,
}

impl<C, I> Iterator for CorpusIdIterator<'_, C, I>
where
    C: Corpus<I>,
{
    type Item = CorpusId;

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(cur) = self.cur {
            self.cur = self.corpus.next(cur);
            Some(cur)
        } else {
            None
        }
    }
}

impl<C, I> DoubleEndedIterator for CorpusIdIterator<'_, C, I>
where
    C: Corpus<I>,
{
    fn next_back(&mut self) -> Option<Self::Item> {
        if let Some(cur_back) = self.cur_back {
            self.cur_back = self.corpus.prev(cur_back);
            Some(cur_back)
        } else {
            None
        }
    }
}
