//! In-memory corpus, keeps all test cases in memory at all times

use alloc::vec::Vec;
use core::cell::{Ref, RefCell, RefMut};

use serde::{Deserialize, Serialize};

use super::{EnableDisableCorpus, HasTestcase};
use crate::{
    Error,
    corpus::{Corpus, CorpusId, Testcase},
};

/// Keep track of the stored `Testcase` and the siblings ids (insertion order)
#[cfg(not(feature = "corpus_btreemap"))]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestcaseStorageItem<I> {
    /// The stored testcase
    pub testcase: RefCell<Testcase<I>>,
    /// Previously inserted id
    pub prev: Option<CorpusId>,
    /// Following inserted id
    pub next: Option<CorpusId>,
}

/// The map type in which testcases are stored (disable the feature `corpus_btreemap` to use a `HashMap` instead of `BTreeMap`)
#[derive(Default, Serialize, Deserialize, Clone, Debug)]
pub struct TestcaseStorageMap<I> {
    #[cfg(not(feature = "corpus_btreemap"))]
    /// A map of `CorpusId` to `TestcaseStorageItem`
    pub map: hashbrown::HashMap<CorpusId, TestcaseStorageItem<I>>,
    #[cfg(feature = "corpus_btreemap")]
    /// A map of `CorpusId` to `Testcase`.
    pub map: alloc::collections::btree_map::BTreeMap<CorpusId, RefCell<Testcase<I>>>,
    /// The keys in order (use `Vec::binary_search`)
    pub keys: Vec<CorpusId>,
    /// First inserted id
    #[cfg(not(feature = "corpus_btreemap"))]
    first_id: Option<CorpusId>,
    /// Last inserted id
    #[cfg(not(feature = "corpus_btreemap"))]
    last_id: Option<CorpusId>,
}

impl<I> TestcaseStorageMap<I> {
    /// Insert a key in the keys set
    fn insert_key(&mut self, id: CorpusId) {
        if let Err(idx) = self.keys.binary_search(&id) {
            self.keys.insert(idx, id);
        }
    }

    /// Remove a key from the keys set
    fn remove_key(&mut self, id: CorpusId) {
        if let Ok(idx) = self.keys.binary_search(&id) {
            self.keys.remove(idx);
        }
    }

    /// Replace a testcase given a `CorpusId`
    #[cfg(not(feature = "corpus_btreemap"))]
    pub fn replace(&mut self, id: CorpusId, testcase: Testcase<I>) -> Option<Testcase<I>> {
        match self.map.get_mut(&id) {
            Some(entry) => Some(entry.testcase.replace(testcase)),
            _ => None,
        }
    }

    /// Replace a testcase given a `CorpusId`
    #[cfg(feature = "corpus_btreemap")]
    pub fn replace(&mut self, id: CorpusId, testcase: Testcase<I>) -> Option<Testcase<I>> {
        self.map.get_mut(&id).map(|entry| entry.replace(testcase))
    }

    /// Remove a testcase given a [`CorpusId`]
    #[cfg(not(feature = "corpus_btreemap"))]
    pub fn remove(&mut self, id: CorpusId) -> Option<RefCell<Testcase<I>>> {
        match self.map.remove(&id) {
            Some(item) => {
                self.remove_key(id);
                match item.prev {
                    Some(prev) => {
                        self.map.get_mut(&prev).unwrap().next = item.next;
                    }
                    _ => {
                        // first elem
                        self.first_id = item.next;
                    }
                }
                match item.next {
                    Some(next) => {
                        self.map.get_mut(&next).unwrap().prev = item.prev;
                    }
                    _ => {
                        // last elem
                        self.last_id = item.prev;
                    }
                }
                Some(item.testcase)
            }
            _ => None,
        }
    }

    /// Remove a testcase given a [`CorpusId`]
    #[cfg(feature = "corpus_btreemap")]
    pub fn remove(&mut self, id: CorpusId) -> Option<RefCell<Testcase<I>>> {
        self.remove_key(id);
        self.map.remove(&id)
    }

    /// Get a testcase given a `CorpusId`
    #[cfg(not(feature = "corpus_btreemap"))]
    #[must_use]
    pub fn get(&self, id: CorpusId) -> Option<&RefCell<Testcase<I>>> {
        self.map.get(&id).as_ref().map(|x| &x.testcase)
    }

    /// Get a testcase given a `CorpusId`
    #[cfg(feature = "corpus_btreemap")]
    #[must_use]
    pub fn get(&self, id: CorpusId) -> Option<&RefCell<Testcase<I>>> {
        self.map.get(&id)
    }

    /// Get the next id given a `CorpusId` (creation order)
    #[cfg(not(feature = "corpus_btreemap"))]
    #[must_use]
    pub fn next(&self, id: CorpusId) -> Option<CorpusId> {
        match self.map.get(&id) {
            Some(item) => item.next,
            _ => None,
        }
    }

    /// Get the next id given a `CorpusId` (creation order)
    #[cfg(feature = "corpus_btreemap")]
    #[must_use]
    pub fn next(&self, id: CorpusId) -> Option<CorpusId> {
        // TODO see if using self.keys is faster
        let mut range = self
            .map
            .range((core::ops::Bound::Included(id), core::ops::Bound::Unbounded));
        if let Some((this_id, _)) = range.next() {
            if id != *this_id {
                return None;
            }
        }
        if let Some((next_id, _)) = range.next() {
            Some(*next_id)
        } else {
            None
        }
    }

    /// Get the previous id given a `CorpusId` (creation order)
    #[cfg(not(feature = "corpus_btreemap"))]
    #[must_use]
    pub fn prev(&self, id: CorpusId) -> Option<CorpusId> {
        match self.map.get(&id) {
            Some(item) => item.prev,
            _ => None,
        }
    }

    /// Get the previous id given a `CorpusId` (creation order)
    #[cfg(feature = "corpus_btreemap")]
    #[must_use]
    pub fn prev(&self, id: CorpusId) -> Option<CorpusId> {
        // TODO see if using self.keys is faster
        let mut range = self
            .map
            .range((core::ops::Bound::Unbounded, core::ops::Bound::Included(id)));
        if let Some((this_id, _)) = range.next_back() {
            if id != *this_id {
                return None;
            }
        }
        if let Some((prev_id, _)) = range.next_back() {
            Some(*prev_id)
        } else {
            None
        }
    }

    /// Get the first created id
    #[cfg(not(feature = "corpus_btreemap"))]
    #[must_use]
    pub fn first(&self) -> Option<CorpusId> {
        self.first_id
    }

    /// Get the first created id
    #[cfg(feature = "corpus_btreemap")]
    #[must_use]
    pub fn first(&self) -> Option<CorpusId> {
        self.map.iter().next().map(|x| *x.0)
    }

    /// Get the last created id
    #[cfg(not(feature = "corpus_btreemap"))]
    #[must_use]
    pub fn last(&self) -> Option<CorpusId> {
        self.last_id
    }

    /// Get the last created id
    #[cfg(feature = "corpus_btreemap")]
    #[must_use]
    pub fn last(&self) -> Option<CorpusId> {
        self.map.iter().next_back().map(|x| *x.0)
    }

    fn new() -> Self {
        Self {
            #[cfg(not(feature = "corpus_btreemap"))]
            map: hashbrown::HashMap::default(),
            #[cfg(feature = "corpus_btreemap")]
            map: alloc::collections::BTreeMap::default(),
            keys: Vec::default(),
            #[cfg(not(feature = "corpus_btreemap"))]
            first_id: None,
            #[cfg(not(feature = "corpus_btreemap"))]
            last_id: None,
        }
    }
}
/// Storage map for the testcases (used in `Corpus` implementations) with an incremental index
#[derive(Default, Serialize, Deserialize, Clone, Debug)]
pub struct TestcaseStorage<I> {
    /// The map in which enabled testcases are stored
    pub enabled: TestcaseStorageMap<I>,
    /// The map in which disabled testcases are stored
    pub disabled: TestcaseStorageMap<I>,
    /// The progressive id for both maps
    progressive_id: usize,
}

impl<I> TestcaseStorage<I> {
    /// Insert a testcase assigning a `CorpusId` to it
    pub fn insert(&mut self, testcase: RefCell<Testcase<I>>) -> CorpusId {
        self.insert_inner(testcase, false)
    }

    #[must_use]
    /// Peek the next free corpus id
    pub fn peek_free_id(&self) -> CorpusId {
        CorpusId::from(self.progressive_id)
    }

    /// Insert a testcase assigning a `CorpusId` to it
    pub fn insert_disabled(&mut self, testcase: RefCell<Testcase<I>>) -> CorpusId {
        self.insert_inner(testcase, true)
    }

    /// Insert a testcase assigning a `CorpusId` to it
    #[cfg(not(feature = "corpus_btreemap"))]
    fn insert_inner(&mut self, testcase: RefCell<Testcase<I>>, is_disabled: bool) -> CorpusId {
        let id = CorpusId::from(self.progressive_id);
        testcase.borrow_mut().set_corpus_id(Some(id));
        self.progressive_id += 1;
        let corpus = if is_disabled {
            &mut self.disabled
        } else {
            &mut self.enabled
        };
        let prev = if let Some(last_id) = corpus.last_id {
            corpus.map.get_mut(&last_id).unwrap().next = Some(id);
            Some(last_id)
        } else {
            None
        };
        if corpus.first_id.is_none() {
            corpus.first_id = Some(id);
        }
        corpus.last_id = Some(id);
        corpus.insert_key(id);
        corpus.map.insert(
            id,
            TestcaseStorageItem {
                testcase,
                prev,
                next: None,
            },
        );
        id
    }

    #[cfg(not(feature = "corpus_btreemap"))]
    fn insert_inner_with_id(
        &mut self,
        testcase: RefCell<Testcase<I>>,
        is_disabled: bool,
        id: CorpusId,
    ) -> Result<(), Error> {
        if self.progressive_id < id.into() {
            return Err(Error::illegal_state(
                "trying to insert a testcase with an id bigger than the internal Id counter",
            ));
        }
        let corpus = if is_disabled {
            &mut self.disabled
        } else {
            &mut self.enabled
        };
        let prev = if let Some(last_id) = corpus.last_id {
            corpus.map.get_mut(&last_id).unwrap().next = Some(id);
            Some(last_id)
        } else {
            None
        };
        if corpus.first_id.is_none() {
            corpus.first_id = Some(id);
        }
        corpus.last_id = Some(id);
        corpus.insert_key(id);
        corpus.map.insert(
            id,
            TestcaseStorageItem {
                testcase,
                prev,
                next: None,
            },
        );
        Ok(())
    }

    #[cfg(feature = "corpus_btreemap")]
    fn insert_inner_with_id(
        &mut self,
        testcase: RefCell<Testcase<I>>,
        is_disabled: bool,
        id: CorpusId,
    ) -> Result<(), Error> {
        if self.progressive_id < id.into() {
            return Err(Error::illegal_state(
                "trying to insert a testcase with an id bigger than the internal Id counter",
            ));
        }
        let corpus = if is_disabled {
            &mut self.disabled
        } else {
            &mut self.enabled
        };
        corpus.insert_key(id);
        corpus.map.insert(id, testcase);
        Ok(())
    }

    /// Insert a testcase assigning a `CorpusId` to it
    #[cfg(feature = "corpus_btreemap")]
    fn insert_inner(&mut self, testcase: RefCell<Testcase<I>>, is_disabled: bool) -> CorpusId {
        let id = CorpusId::from(self.progressive_id);
        self.progressive_id += 1;
        let corpus = if is_disabled {
            &mut self.disabled
        } else {
            &mut self.enabled
        };
        corpus.insert_key(id);
        corpus.map.insert(id, testcase);
        id
    }

    /// Create new `TestcaseStorage`
    #[must_use]
    pub fn new() -> Self {
        Self {
            enabled: TestcaseStorageMap::new(),
            disabled: TestcaseStorageMap::new(),
            progressive_id: 0,
        }
    }
}

/// A corpus handling all in memory.
#[derive(Default, Serialize, Deserialize, Clone, Debug)]
pub struct InMemoryCorpus<I> {
    storage: TestcaseStorage<I>,
    current: Option<CorpusId>,
}

impl<I> Corpus<I> for InMemoryCorpus<I> {
    /// Returns the number of all enabled entries
    #[inline]
    fn count(&self) -> usize {
        self.storage.enabled.map.len()
    }

    /// Returns the number of all disabled entries
    fn count_disabled(&self) -> usize {
        self.storage.disabled.map.len()
    }

    /// Returns the number of elements including disabled entries
    #[inline]
    fn count_all(&self) -> usize {
        self.storage
            .enabled
            .map
            .len()
            .saturating_add(self.storage.disabled.map.len())
    }

    /// Add an enabled testcase to the corpus and return its index
    #[inline]
    fn add(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error> {
        Ok(self.storage.insert(RefCell::new(testcase)))
    }

    /// Add a disabled testcase to the corpus and return its index
    #[inline]
    fn add_disabled(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error> {
        Ok(self.storage.insert_disabled(RefCell::new(testcase)))
    }

    /// Replaces the testcase at the given id
    #[inline]
    fn replace(&mut self, id: CorpusId, testcase: Testcase<I>) -> Result<Testcase<I>, Error> {
        self.storage.enabled.replace(id, testcase).ok_or_else(|| {
            Error::key_not_found(format!("Index {id} not found, could not replace."))
        })
    }

    /// Removes an entry from the corpus, returning it if it was present; considers both enabled and disabled testcases
    #[inline]
    fn remove(&mut self, id: CorpusId) -> Result<Testcase<I>, Error> {
        let mut testcase = self.storage.enabled.remove(id);
        if testcase.is_none() {
            testcase = self.storage.disabled.remove(id);
        }
        testcase
            .map(|x| x.take())
            .ok_or_else(|| Error::key_not_found(format!("Index {id} not found")))
    }

    /// Get by id; considers only enabled testcases
    #[inline]
    fn get(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        self.storage
            .enabled
            .get(id)
            .ok_or_else(|| Error::key_not_found(format!("Index {id} not found")))
    }
    /// Get by id; considers both enabled and disabled testcases
    #[inline]
    fn get_from_all(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        let mut testcase = self.storage.enabled.get(id);
        if testcase.is_none() {
            testcase = self.storage.disabled.get(id);
        }
        testcase.ok_or_else(|| Error::key_not_found(format!("Index {id} not found")))
    }

    /// Current testcase scheduled
    #[inline]
    fn current(&self) -> &Option<CorpusId> {
        &self.current
    }

    /// Current testcase scheduled (mutable)
    #[inline]
    fn current_mut(&mut self) -> &mut Option<CorpusId> {
        &mut self.current
    }

    #[inline]
    fn next(&self, id: CorpusId) -> Option<CorpusId> {
        self.storage.enabled.next(id)
    }

    /// Peek the next free corpus id
    #[inline]
    fn peek_free_id(&self) -> CorpusId {
        self.storage.peek_free_id()
    }

    #[inline]
    fn prev(&self, id: CorpusId) -> Option<CorpusId> {
        self.storage.enabled.prev(id)
    }

    #[inline]
    fn first(&self) -> Option<CorpusId> {
        self.storage.enabled.first()
    }

    #[inline]
    fn last(&self) -> Option<CorpusId> {
        self.storage.enabled.last()
    }

    /// Get the nth corpus id; considers only enabled testcases
    #[inline]
    fn nth(&self, nth: usize) -> CorpusId {
        self.storage.enabled.keys[nth]
    }

    /// Get the nth corpus id; considers both enabled and disabled testcases
    #[inline]
    fn nth_from_all(&self, nth: usize) -> CorpusId {
        let enabled_count = self.count();
        if nth >= enabled_count {
            return self.storage.disabled.keys[nth.saturating_sub(enabled_count)];
        }
        self.storage.enabled.keys[nth]
    }

    #[inline]
    fn load_input_into(&self, _: &mut Testcase<I>) -> Result<(), Error> {
        // Inputs never get evicted, nothing to load here.
        Ok(())
    }

    #[inline]
    fn store_input_from(&self, _: &Testcase<I>) -> Result<(), Error> {
        Ok(())
    }
}

impl<I> EnableDisableCorpus for InMemoryCorpus<I> {
    #[inline]
    fn disable(&mut self, id: CorpusId) -> Result<(), Error> {
        if let Some(testcase) = self.storage.enabled.remove(id) {
            self.storage.insert_inner_with_id(testcase, true, id)
        } else {
            Err(Error::key_not_found(format!(
                "Index {id} not found in enabled testcases"
            )))
        }
    }

    #[inline]
    fn enable(&mut self, id: CorpusId) -> Result<(), Error> {
        if let Some(testcase) = self.storage.disabled.remove(id) {
            self.storage.insert_inner_with_id(testcase, false, id)
        } else {
            Err(Error::key_not_found(format!(
                "Index {id} not found in disabled testcases"
            )))
        }
    }
}

impl<I> HasTestcase<I> for InMemoryCorpus<I> {
    fn testcase(&self, id: CorpusId) -> Result<Ref<'_, Testcase<I>>, Error> {
        Ok(self.get(id)?.borrow())
    }

    fn testcase_mut(&self, id: CorpusId) -> Result<RefMut<'_, Testcase<I>>, Error> {
        Ok(self.get(id)?.borrow_mut())
    }
}

impl<I> InMemoryCorpus<I> {
    /// Creates a new [`InMemoryCorpus`], keeping all [`Testcase`]`s` in memory.
    /// This is the simplest and fastest option, however test progress will be lost on exit or on OOM.
    #[must_use]
    pub fn new() -> Self {
        Self {
            storage: TestcaseStorage::new(),
            current: None,
        }
    }
}

#[cfg(test)]
#[cfg(not(feature = "corpus_btreemap"))]
mod tests {
    use super::*;
    use crate::{
        Error,
        corpus::Testcase,
        inputs::{HasMutatorBytes, bytes::BytesInput},
    };

    /// Helper function to create a corpus with predefined test cases
    #[cfg(not(feature = "corpus_btreemap"))]
    fn setup_corpus() -> (InMemoryCorpus<BytesInput>, Vec<CorpusId>) {
        let mut corpus = InMemoryCorpus::<BytesInput>::new();
        let mut ids = Vec::new();

        // Add initial test cases with distinct byte patterns ([1,2,3],[2,3,4],[3,4,5])
        for i in 0..3u8 {
            let input = BytesInput::new(vec![i + 1, i + 2, i + 3]);
            let tc_id = corpus.add(Testcase::new(input)).unwrap();
            ids.push(tc_id);
        }

        (corpus, ids)
    }

    /// Helper function to verify corpus counts
    #[cfg(not(feature = "corpus_btreemap"))]
    fn assert_corpus_counts(corpus: &InMemoryCorpus<BytesInput>, enabled: usize, disabled: usize) {
        let total = enabled + disabled; // if a testcase is not in the enabled map, then it's in the disabled one.
        assert_eq!(corpus.count(), enabled, "Wrong number of enabled testcases");
        assert_eq!(
            corpus.count_disabled(),
            disabled,
            "Wrong number of disabled testcases"
        );
        assert_eq!(corpus.count_all(), total, "Wrong total number of testcases");
    }

    #[test]
    #[cfg(not(feature = "corpus_btreemap"))]
    fn test_corpus_basic_operations() {
        let (corpus, ids) = setup_corpus();
        assert_corpus_counts(&corpus, 3, 0);

        for id in &ids {
            assert!(corpus.get(*id).is_ok(), "Failed to get testcase {id:?}");
            assert!(
                corpus.get_from_all(*id).is_ok(),
                "Failed to get testcase from all {id:?}"
            );
        }

        // Non-existent ID should fail
        let invalid_id = CorpusId(999);
        assert!(corpus.get(invalid_id).is_err());
        assert!(corpus.get_from_all(invalid_id).is_err());
    }

    #[test]
    #[cfg(not(feature = "corpus_btreemap"))]
    fn test_corpus_disable_enable() -> Result<(), Error> {
        let (mut corpus, ids) = setup_corpus();
        let invalid_id = CorpusId(999);

        corpus.disable(ids[1])?;
        assert_corpus_counts(&corpus, 2, 1);

        // Verify disabled testcase is not in enabled list but is in all list
        assert!(
            corpus.get(ids[1]).is_err(),
            "Disabled testcase should not be accessible via get()"
        );
        assert!(
            corpus.get_from_all(ids[1]).is_ok(),
            "Disabled testcase should be accessible via get_from_all()"
        );

        // Other testcases are still accessible
        assert!(corpus.get(ids[0]).is_ok());
        assert!(corpus.get(ids[2]).is_ok());

        corpus.enable(ids[1])?;
        assert_corpus_counts(&corpus, 3, 0);

        // Verify all testcases are accessible from the enabled map again
        for id in &ids {
            assert!(corpus.get(*id).is_ok());
        }

        // Corner cases
        assert!(
            corpus.disable(ids[1]).is_ok(),
            "Should be able to disable testcase"
        );
        assert!(
            corpus.disable(ids[1]).is_err(),
            "Should not be able to disable already disabled testcase"
        );
        assert!(
            corpus.enable(ids[0]).is_err(),
            "Should not be able to enable already enabled testcase"
        );
        assert!(
            corpus.disable(invalid_id).is_err(),
            "Should not be able to disable non-existent testcase"
        );
        assert!(
            corpus.enable(invalid_id).is_err(),
            "Should not be able to enable non-existent testcase"
        );

        Ok(())
    }

    #[test]
    #[cfg(not(feature = "corpus_btreemap"))]
    fn test_corpus_operations_after_disabled() -> Result<(), Error> {
        let (mut corpus, ids) = setup_corpus();

        corpus.disable(ids[0])?;
        assert_corpus_counts(&corpus, 2, 1);

        let removed = corpus.remove(ids[0])?;
        let removed_data = removed.input().as_ref().unwrap().mutator_bytes();
        assert_eq!(
            removed_data,
            &vec![1, 2, 3],
            "Removed testcase has incorrect data"
        );
        assert_corpus_counts(&corpus, 2, 0);

        let removed = corpus.remove(ids[1])?;
        let removed_data = removed.input().as_ref().unwrap().mutator_bytes();
        assert_eq!(
            removed_data,
            &vec![2, 3, 4],
            "Removed testcase has incorrect data"
        );
        assert_corpus_counts(&corpus, 1, 0);

        // Not possible to get removed testcases
        assert!(corpus.get(ids[0]).is_err());
        assert!(corpus.get_from_all(ids[0]).is_err());
        assert!(corpus.get(ids[1]).is_err());
        assert!(corpus.get_from_all(ids[1]).is_err());

        // Only the third testcase should remain
        assert!(corpus.get(ids[2]).is_ok());

        Ok(())
    }
}
