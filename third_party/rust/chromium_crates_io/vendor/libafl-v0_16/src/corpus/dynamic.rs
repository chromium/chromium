//! Dynamic corpus that allows users to switch corpus types at runtime.

use core::{cell::RefCell, marker::PhantomData};

use libafl_bolts::Error;
use serde::{Deserialize, Serialize};

use crate::corpus::{Corpus, CorpusId, Testcase};

/// An dynamic corpus type accepting two types of corpus at runtime. This helps rustc better
/// reason about the bounds compared to dyn objects.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum DynamicCorpus<C1, C2, I> {
    /// Corpus1 implementation
    Corpus1(C1, PhantomData<I>),
    /// Corpus2 implementation
    Corpus2(C2, PhantomData<I>),
}

impl<C1, C2, I> DynamicCorpus<C1, C2, I>
where
    C1: Corpus<I>,
    C2: Corpus<I>,
{
    /// Create a `DynamicCorpus` with Corpus1 variant.
    pub fn corpus1(c: C1) -> Self {
        Self::Corpus1(c, PhantomData)
    }

    /// Create a `DynamicCorpus` with Corpus2 variant.
    pub fn corpus2(c: C2) -> Self {
        Self::Corpus2(c, PhantomData)
    }
}

impl<C1, C2, I> Corpus<I> for DynamicCorpus<C1, C2, I>
where
    C1: Corpus<I>,
    C2: Corpus<I>,
{
    fn peek_free_id(&self) -> CorpusId {
        match self {
            Self::Corpus1(c1, _) => c1.peek_free_id(),
            Self::Corpus2(c2, _) => c2.peek_free_id(),
        }
    }

    fn add(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error> {
        match self {
            Self::Corpus1(c1, _) => c1.add(testcase),
            Self::Corpus2(c2, _) => c2.add(testcase),
        }
    }

    fn add_disabled(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error> {
        match self {
            Self::Corpus1(c1, _) => c1.add_disabled(testcase),
            Self::Corpus2(c2, _) => c2.add_disabled(testcase),
        }
    }

    fn cloned_input_for_id(&self, idx: CorpusId) -> Result<I, Error>
    where
        I: Clone,
    {
        match self {
            Self::Corpus1(c1, _) => c1.cloned_input_for_id(idx),
            Self::Corpus2(c2, _) => c2.cloned_input_for_id(idx),
        }
    }

    fn count(&self) -> usize {
        match self {
            Self::Corpus1(c1, _) => c1.count(),
            Self::Corpus2(c2, _) => c2.count(),
        }
    }

    fn count_all(&self) -> usize {
        match self {
            Self::Corpus1(c1, _) => c1.count_all(),
            Self::Corpus2(c2, _) => c2.count_all(),
        }
    }

    fn count_disabled(&self) -> usize {
        match self {
            Self::Corpus1(c1, _) => c1.count_disabled(),
            Self::Corpus2(c2, _) => c2.count_disabled(),
        }
    }

    fn current(&self) -> &Option<CorpusId> {
        match self {
            Self::Corpus1(c1, _) => c1.current(),
            Self::Corpus2(c2, _) => c2.current(),
        }
    }

    fn current_mut(&mut self) -> &mut Option<CorpusId> {
        match self {
            Self::Corpus1(c1, _) => c1.current_mut(),
            Self::Corpus2(c2, _) => c2.current_mut(),
        }
    }

    fn first(&self) -> Option<CorpusId> {
        match self {
            Self::Corpus1(c1, _) => c1.first(),
            Self::Corpus2(c2, _) => c2.first(),
        }
    }

    fn get(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        match self {
            Self::Corpus1(c1, _) => c1.get(id),
            Self::Corpus2(c2, _) => c2.get(id),
        }
    }

    fn get_from_all(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        match self {
            Self::Corpus1(c1, _) => c1.get_from_all(id),
            Self::Corpus2(c2, _) => c2.get_from_all(id),
        }
    }

    fn is_empty(&self) -> bool {
        match self {
            Self::Corpus1(c1, _) => c1.is_empty(),
            Self::Corpus2(c2, _) => c2.is_empty(),
        }
    }

    fn last(&self) -> Option<CorpusId> {
        match self {
            Self::Corpus1(c1, _) => c1.last(),
            Self::Corpus2(c2, _) => c2.last(),
        }
    }

    fn load_input_into(&self, testcase: &mut Testcase<I>) -> Result<(), Error> {
        match self {
            Self::Corpus1(c1, _) => c1.load_input_into(testcase),
            Self::Corpus2(c2, _) => c2.load_input_into(testcase),
        }
    }

    fn next(&self, id: CorpusId) -> Option<CorpusId> {
        match self {
            Self::Corpus1(c1, _) => c1.next(id),
            Self::Corpus2(c2, _) => c2.next(id),
        }
    }

    fn nth(&self, nth: usize) -> CorpusId {
        match self {
            Self::Corpus1(c1, _) => c1.nth(nth),
            Self::Corpus2(c2, _) => c2.nth(nth),
        }
    }

    fn nth_from_all(&self, nth: usize) -> CorpusId {
        match self {
            Self::Corpus1(c1, _) => c1.nth_from_all(nth),
            Self::Corpus2(c2, _) => c2.nth_from_all(nth),
        }
    }

    fn prev(&self, id: CorpusId) -> Option<CorpusId> {
        match self {
            Self::Corpus1(c1, _) => c1.prev(id),
            Self::Corpus2(c2, _) => c2.prev(id),
        }
    }

    fn remove(&mut self, id: CorpusId) -> Result<Testcase<I>, Error> {
        match self {
            Self::Corpus1(c1, _) => c1.remove(id),
            Self::Corpus2(c2, _) => c2.remove(id),
        }
    }

    fn replace(&mut self, idx: CorpusId, testcase: Testcase<I>) -> Result<Testcase<I>, Error> {
        match self {
            Self::Corpus1(c1, _) => c1.replace(idx, testcase),
            Self::Corpus2(c2, _) => c2.replace(idx, testcase),
        }
    }

    fn store_input_from(&self, testcase: &Testcase<I>) -> Result<(), Error> {
        match self {
            Self::Corpus1(c1, _) => c1.store_input_from(testcase),
            Self::Corpus2(c2, _) => c2.store_input_from(testcase),
        }
    }
}
