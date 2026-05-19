//! The [`NewHashFeedback`] uses the backtrace hash and a hashset to only keep novel cases

use alloc::{borrow::Cow, string::ToString};
use core::fmt::Debug;

use hashbrown::HashSet;
use libafl_bolts::{
    Named,
    tuples::{Handle, Handled, MatchName, MatchNameRef},
};
use serde::{Deserialize, Serialize};

#[cfg(feature = "track_hit_feedbacks")]
use crate::feedbacks::premature_last_result_err;
use crate::{
    Error, HasNamedMetadata,
    executors::ExitKind,
    feedbacks::{Feedback, HasObserverHandle, StateInitializer},
    observers::ObserverWithHashField,
};

/// The prefix of the metadata names
pub const NEWHASHFEEDBACK_PREFIX: &str = "newhashfeedback_metadata_";

/// A state that implements this trait has a hash set
pub trait HashSetState<T> {
    /// creates a new instance with a specific hashset
    fn with_hash_set(hash_set: HashSet<T>) -> Self;
    /// updates the `hash_set` with the given value
    fn update_hash_set(&mut self, value: T) -> Result<bool, Error>;
}

/// The state of [`NewHashFeedback`]
#[derive(Default, Serialize, Deserialize, Debug, Clone)]
#[expect(clippy::unsafe_derive_deserialize)]
pub struct NewHashFeedbackMetadata {
    /// Contains information about untouched entries
    hash_set: HashSet<u64>,
}

#[rustfmt::skip]
libafl_bolts::impl_serdeany!(NewHashFeedbackMetadata);

impl NewHashFeedbackMetadata {
    /// Create a new [`NewHashFeedbackMetadata`]
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Create a new [`NewHashFeedbackMetadata`] with the given initial capacity
    #[must_use]
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            hash_set: HashSet::with_capacity(capacity),
        }
    }

    /// Reset the internal state
    pub fn reset(&mut self) -> Result<(), Error> {
        self.hash_set.clear();
        Ok(())
    }

    /// Gets the associated [`HashSet`] being used to track hashes
    #[must_use]
    pub fn hash_set(&self) -> &HashSet<u64> {
        &self.hash_set
    }

    /// Gets the associated [`HashSet`] being used to track hashes, mutably
    pub fn hash_set_mut(&mut self) -> &mut HashSet<u64> {
        &mut self.hash_set
    }
}

impl HashSetState<u64> for NewHashFeedbackMetadata {
    /// Create new [`NewHashFeedbackMetadata`] using a name and a hash set.
    fn with_hash_set(hash_set: HashSet<u64>) -> Self {
        Self { hash_set }
    }

    fn update_hash_set(&mut self, value: u64) -> Result<bool, Error> {
        let r = self.hash_set.insert(value);
        // log::trace!("Got r={}, the hashset is {:?}", r, &self.hash_set);
        Ok(r)
    }
}

/// A [`NewHashFeedback`] maintains a hashset of already seen stacktraces and considers interesting unseen ones
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct NewHashFeedback<O> {
    name: Cow<'static, str>,
    o_ref: Handle<O>,
    /// Initial capacity of hash set
    capacity: usize,
    #[cfg(feature = "track_hit_feedbacks")]
    // The previous run's result of `Self::is_interesting`
    last_result: Option<bool>,
}

impl<O> NewHashFeedback<O>
where
    O: ObserverWithHashField + Named,
{
    fn has_interesting_backtrace_hash_observation<OT, S: HasNamedMetadata>(
        &mut self,
        state: &mut S,
        observers: &OT,
    ) -> Result<bool, Error>
    where
        OT: MatchName,
    {
        let observer = observers
            .get(&self.o_ref)
            .expect("A NewHashFeedback needs a BacktraceObserver");

        let backtrace_state = state
            .named_metadata_map_mut()
            .get_mut::<NewHashFeedbackMetadata>(&self.name)
            .unwrap();

        let res = match observer.hash() {
            Some(hash) => backtrace_state.update_hash_set(hash)?,
            None => {
                // We get here if the hash was not updated, i.e the first run or if no crash happens
                false
            }
        };
        #[cfg(feature = "track_hit_feedbacks")]
        {
            self.last_result = Some(res);
        }
        Ok(res)
    }
}

impl<O, S> StateInitializer<S> for NewHashFeedback<O>
where
    S: HasNamedMetadata,
{
    fn init_state(&mut self, state: &mut S) -> Result<(), Error> {
        state.add_named_metadata_checked(
            &self.name,
            NewHashFeedbackMetadata::with_capacity(self.capacity),
        )?;
        Ok(())
    }
}

impl<O, EM, I, OT, S> Feedback<EM, I, OT, S> for NewHashFeedback<O>
where
    O: ObserverWithHashField + Named,
    OT: MatchName,
    S: HasNamedMetadata,
{
    fn is_interesting(
        &mut self,
        state: &mut S,
        _manager: &mut EM,
        _input: &I,
        observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        self.has_interesting_backtrace_hash_observation(state, observers)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        self.last_result.ok_or(premature_last_result_err())
    }
}

impl<O> Named for NewHashFeedback<O> {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<O> HasObserverHandle for NewHashFeedback<O> {
    type Observer = O;

    #[inline]
    fn observer_handle(&self) -> &Handle<O> {
        &self.o_ref
    }
}

/// Default capacity for the [`HashSet`] in [`NewHashFeedback`].
///
/// This is reasonably large on the assumption that you expect there to be many
/// runs of the target, producing many different feedbacks.
const DEFAULT_CAPACITY: usize = 4096;

impl<O> NewHashFeedback<O>
where
    O: Named,
{
    /// Returns a new [`NewHashFeedback`].
    #[must_use]
    pub fn new(observer: &O) -> Self {
        Self::with_capacity(observer, DEFAULT_CAPACITY)
    }

    /// Returns a new [`NewHashFeedback`] that will create a hash set with the
    /// given initial capacity.
    #[must_use]
    pub fn with_capacity(observer: &O, capacity: usize) -> Self {
        Self {
            name: Cow::from(NEWHASHFEEDBACK_PREFIX.to_string() + observer.name()),
            o_ref: observer.handle(),
            capacity,
            #[cfg(feature = "track_hit_feedbacks")]
            last_result: None,
        }
    }
}
