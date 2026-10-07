//! The null corpus does not store any [`Testcase`]s.
use core::{cell::RefCell, marker::PhantomData};

use serde::{Deserialize, Serialize};

use crate::{
    Error,
    corpus::{Corpus, CorpusId, Testcase},
};

/// A corpus which does not store any [`Testcase`]s.
#[derive(Default, Serialize, Deserialize, Clone, Debug)]
pub struct NopCorpus<I> {
    empty: Option<CorpusId>,
    phantom: PhantomData<I>,
}

impl<I> Corpus<I> for NopCorpus<I> {
    /// Returns the number of all enabled entries
    #[inline]
    fn count(&self) -> usize {
        0
    }

    /// Returns the number of all disabled entries
    fn count_disabled(&self) -> usize {
        0
    }

    /// Returns the number of all entries
    #[inline]
    fn count_all(&self) -> usize {
        0
    }

    /// Add an enabled testcase to the corpus and return its index
    #[inline]
    fn add(&mut self, _testcase: Testcase<I>) -> Result<CorpusId, Error> {
        Err(Error::unsupported("Unsupported by NopCorpus"))
    }

    /// Add a disabled testcase to the corpus and return its index
    #[inline]
    fn add_disabled(&mut self, _testcase: Testcase<I>) -> Result<CorpusId, Error> {
        Err(Error::unsupported("Unsupported by NopCorpus"))
    }

    /// Replaces the testcase with the given id
    #[inline]
    fn replace(&mut self, _id: CorpusId, _testcase: Testcase<I>) -> Result<Testcase<I>, Error> {
        Err(Error::unsupported("Unsupported by NopCorpus"))
    }

    /// Removes an entry from the corpus, returning it if it was present; considers both enabled and disabled testcases
    #[inline]
    fn remove(&mut self, _id: CorpusId) -> Result<Testcase<I>, Error> {
        Err(Error::unsupported("Unsupported by NopCorpus"))
    }

    /// Get by id; considers only enabled testcases
    #[inline]
    fn get(&self, _id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        Err(Error::unsupported("Unsupported by NopCorpus"))
    }

    /// Get by id; considers both enabled and disabled testcases
    #[inline]
    fn get_from_all(&self, _id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        Err(Error::unsupported("Unsupported by NopCorpus"))
    }

    /// Current testcase scheduled
    #[inline]
    fn current(&self) -> &Option<CorpusId> {
        &self.empty
    }

    /// Current testcase scheduled (mutable)
    #[inline]
    fn current_mut(&mut self) -> &mut Option<CorpusId> {
        &mut self.empty
    }

    #[inline]
    fn next(&self, _id: CorpusId) -> Option<CorpusId> {
        None
    }

    /// Peek the next free corpus id
    #[inline]
    fn peek_free_id(&self) -> CorpusId {
        CorpusId::from(0_usize)
    }

    #[inline]
    fn prev(&self, _id: CorpusId) -> Option<CorpusId> {
        None
    }

    #[inline]
    fn first(&self) -> Option<CorpusId> {
        None
    }

    #[inline]
    fn last(&self) -> Option<CorpusId> {
        None
    }

    /// Get the nth corpus id; considers only enabled testcases
    #[inline]
    fn nth(&self, _nth: usize) -> CorpusId {
        CorpusId::from(0_usize)
    }

    /// Get the nth corpus id; considers both enabled and disabled testcases
    #[inline]
    fn nth_from_all(&self, _nth: usize) -> CorpusId {
        CorpusId::from(0_usize)
    }

    #[inline]
    fn load_input_into(&self, _testcase: &mut Testcase<I>) -> Result<(), Error> {
        Err(Error::unsupported("Unsupported by NopCorpus"))
    }

    #[inline]
    fn store_input_from(&self, _testcase: &Testcase<I>) -> Result<(), Error> {
        Err(Error::unsupported("Unsupported by NopCorpus"))
    }
}

impl<I> NopCorpus<I> {
    /// Creates a new [`NopCorpus`].
    #[must_use]
    pub fn new() -> Self {
        Self {
            empty: None,
            phantom: PhantomData {},
        }
    }
}
