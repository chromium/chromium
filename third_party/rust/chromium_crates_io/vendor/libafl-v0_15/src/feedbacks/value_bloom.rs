//! The [`ValueBloomFeedback`] checks if a value has already been observed in a [`BloomFilter`] and returns `true` if the value is new, adding it to the bloom filter.

use alloc::borrow::Cow;
use core::hash::Hash;

use fastbloom::BloomFilter;
use libafl_bolts::{
    Error, Named, impl_serdeany,
    tuples::{Handle, MatchNameRef},
};
use serde::{Deserialize, Serialize};

use crate::{
    HasNamedMetadata,
    executors::ExitKind,
    feedbacks::{Feedback, StateInitializer},
    observers::{ObserversTuple, ValueObserver},
};

impl_serdeany!(ValueBloomFeedbackMetadata);

#[derive(Debug, Serialize, Deserialize)]
struct ValueBloomFeedbackMetadata {
    bloom: BloomFilter,
}

/// A Feedback that returns `true` for `is_interesting` for new values it found in a [`ValueObserver`].
/// It keeps track of the previously seen values in a [`BloomFilter`].
#[derive(Debug)]
pub struct ValueBloomFeedback<'a, T> {
    name: Cow<'static, str>,
    observer_hnd: Handle<ValueObserver<'a, T>>,
    #[cfg(feature = "track_hit_feedbacks")]
    last_result: Option<bool>,
}

impl<'a, T> ValueBloomFeedback<'a, T> {
    /// Create a new [`ValueBloomFeedback`]
    #[must_use]
    pub fn new(observer_hnd: &Handle<ValueObserver<'a, T>>) -> Self {
        Self::with_name(observer_hnd.name().clone(), observer_hnd)
    }

    /// Create a new [`ValueBloomFeedback`] with a given name
    #[must_use]
    pub fn with_name(name: Cow<'static, str>, observer_hnd: &Handle<ValueObserver<'a, T>>) -> Self {
        Self {
            name,
            observer_hnd: observer_hnd.clone(),
            #[cfg(feature = "track_hit_feedbacks")]
            last_result: None,
        }
    }
}

impl<T> Named for ValueBloomFeedback<'_, T> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<S: HasNamedMetadata, T> StateInitializer<S> for ValueBloomFeedback<'_, T> {
    fn init_state(&mut self, state: &mut S) -> Result<(), Error> {
        let _ =
            state.named_metadata_or_insert_with::<ValueBloomFeedbackMetadata>(&self.name, || {
                ValueBloomFeedbackMetadata {
                    bloom: BloomFilter::with_false_pos(0.001).expected_items(1024),
                }
            });
        Ok(())
    }
}

impl<EM, I, OT: ObserversTuple<I, S>, S: HasNamedMetadata, T: Hash> Feedback<EM, I, OT, S>
    for ValueBloomFeedback<'_, T>
{
    fn is_interesting(
        &mut self,
        state: &mut S,
        _manager: &mut EM,
        _input: &I,
        observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        let Some(observer) = observers.get(&self.observer_hnd) else {
            return Err(Error::illegal_state(format!(
                "Observer {:?} not found",
                self.observer_hnd
            )));
        };
        let val = observer.value.as_ref();

        let metadata = state.named_metadata_mut::<ValueBloomFeedbackMetadata>(&self.name)?;

        let res = if metadata.bloom.contains(val) {
            false
        } else {
            metadata.bloom.insert(val);
            true
        };

        #[cfg(feature = "track_hit_feedbacks")]
        {
            self.last_result = Some(true);
        }

        Ok(res)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        self.last_result.ok_or_else(|| Error::illegal_state("No last result set in `ValueBloomFeedback`. Either `is_interesting` has never been called or the fuzzer restarted in the meantime."))
    }
}

#[cfg(test)]
mod test {
    use core::{cell::UnsafeCell, ptr::write_volatile};

    use libafl_bolts::{ownedref::OwnedRef, tuples::Handled};
    use tuple_list::tuple_list;

    use super::ValueBloomFeedback;
    use crate::{
        executors::ExitKind,
        feedbacks::{Feedback, StateInitializer},
        observers::ValueObserver,
        state::NopState,
    };

    #[test]
    fn test_value_bloom_feedback() {
        let value: UnsafeCell<u32> = 0_u32.into();

        // # Safety
        // The same testcase doesn't usually run twice
        #[cfg(any(not(feature = "serdeany_autoreg"), miri))]
        unsafe {
            super::ValueBloomFeedbackMetadata::register();
        }

        // # Safety
        // The value is only read from in the feedback, not while we change the value.
        let value_ptr = unsafe { OwnedRef::from_ptr(value.get()) };

        let observer = ValueObserver::new("test_value", value_ptr);
        let mut vbf = ValueBloomFeedback::new(&observer.handle());

        let observers = tuple_list!(observer);

        let mut state: NopState<()> = NopState::new();
        let mut mgr = ();
        let input = ();
        let exit_ok = ExitKind::Ok;

        vbf.init_state(&mut state).unwrap();

        let first_eval = vbf
            .is_interesting(&mut state, &mut mgr, &input, &observers, &exit_ok)
            .unwrap();
        assert!(first_eval);

        let second_eval = vbf
            .is_interesting(&mut state, &mut mgr, &input, &observers, &exit_ok)
            .unwrap();

        assert!(!second_eval);

        // # Safety
        // The feedback is not keeping a borrow around, only the pointer.
        unsafe {
            write_volatile(value.get(), 1234_u32);
        }

        let next_eval = vbf
            .is_interesting(&mut state, &mut mgr, &input, &observers, &exit_ok)
            .unwrap();
        assert!(next_eval);
    }
}
