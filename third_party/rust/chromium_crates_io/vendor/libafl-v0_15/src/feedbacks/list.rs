use alloc::borrow::Cow;
use core::{
    fmt::{Debug, LowerHex},
    hash::Hash,
};
#[cfg(feature = "std")]
use std::{fs::File, io::Write, path::Path};

use hashbrown::HashSet;
use libafl_bolts::{
    Error, HasRefCnt, Named,
    tuples::{Handle, Handled, MatchName, MatchNameRef},
};
use serde::{Deserialize, Serialize, de::DeserializeOwned};

use crate::{
    HasNamedMetadata,
    executors::ExitKind,
    feedbacks::{Feedback, StateInitializer},
    observers::ListObserver,
};

/// The metadata to remember past observed value
#[derive(Debug, Serialize, Deserialize)]
#[serde(bound = "T: Eq + Hash + for<'a> Deserialize<'a> + Serialize")]
pub struct ListFeedbackMetadata<T> {
    /// Contains the information of past observed set of values.
    pub set: HashSet<T>,
    /// A refcount used to know when we can remove this metadata
    pub tcref: isize,
}

impl<T> ListFeedbackMetadata<T> {
    /// The constructor
    #[must_use]
    pub fn new() -> Self {
        Self {
            set: HashSet::new(),
            tcref: 0,
        }
    }

    /// Reset the inner hashset
    pub fn reset(&mut self) -> Result<(), Error> {
        self.set.clear();
        Ok(())
    }
}

impl<T> Default for ListFeedbackMetadata<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T> HasRefCnt for ListFeedbackMetadata<T> {
    fn refcnt(&self) -> isize {
        self.tcref
    }

    fn refcnt_mut(&mut self) -> &mut isize {
        &mut self.tcref
    }
}

/// Consider interesting a testcase if the list in `ListObserver` is not empty.
#[derive(Debug)]
pub struct ListFeedback<T> {
    observer_handle: Handle<ListObserver<T>>,
    novelty: HashSet<T>,
    #[cfg(feature = "std")]
    file: Option<File>,
}

libafl_bolts::impl_serdeany!(
    ListFeedbackMetadata<T: Debug + 'static + Serialize + DeserializeOwned + Eq + Hash>,
    <u8>,<u16>,<u32>,<u64>,<i8>,<i16>,<i32>,<i64>,<bool>,<char>,<usize>
);

impl<T> ListFeedback<T>
where
    T: Debug + Eq + Hash + for<'a> Deserialize<'a> + Serialize + 'static + Copy + LowerHex,
{
    fn has_interesting_list_observer_feedback<OT, S>(
        &mut self,
        state: &mut S,
        observers: &OT,
    ) -> bool
    where
        OT: MatchName,
        S: HasNamedMetadata,
    {
        let observer = observers.get(&self.observer_handle).unwrap();
        // TODO register the list content in a testcase metadata
        self.novelty.clear();
        // can't fail
        let history_set = state
            .named_metadata_map_mut()
            .get_mut::<ListFeedbackMetadata<T>>(self.name())
            .unwrap();
        for v in observer.list() {
            if !history_set.set.contains(v) {
                self.novelty.insert(*v);
            }
        }
        !self.novelty.is_empty()
    }

    #[cfg(feature = "std")]
    fn dump_coverage(&mut self) {
        if let Some(mut file) = self.file.as_ref() {
            for line in &self.novelty {
                file.write_all(format!("0x{line:x}\n").as_bytes()).unwrap();
            }
        }
    }

    fn append_list_observer_metadata<S: HasNamedMetadata>(&mut self, state: &mut S) {
        let history_set = state
            .named_metadata_map_mut()
            .get_mut::<ListFeedbackMetadata<T>>(self.name())
            .unwrap();

        for v in &self.novelty {
            history_set.set.insert(*v);
        }

        #[cfg(feature = "std")]
        self.dump_coverage();
    }
}

impl<S, T> StateInitializer<S> for ListFeedback<T>
where
    S: HasNamedMetadata,
    T: Debug + Eq + Hash + for<'a> Deserialize<'a> + Serialize + Default + Copy + 'static,
{
    fn init_state(&mut self, state: &mut S) -> Result<(), Error> {
        state.add_named_metadata_checked(self.name(), ListFeedbackMetadata::<T>::default())?;
        Ok(())
    }
}

impl<EM, I, OT, S, T> Feedback<EM, I, OT, S> for ListFeedback<T>
where
    OT: MatchName,
    S: HasNamedMetadata,
    T: Debug
        + Eq
        + Hash
        + for<'a> Deserialize<'a>
        + Serialize
        + Default
        + Copy
        + 'static
        + LowerHex,
{
    fn is_interesting(
        &mut self,
        state: &mut S,
        _manager: &mut EM,
        _input: &I,
        observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        Ok(self.has_interesting_list_observer_feedback(state, observers))
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(!self.novelty.is_empty())
    }

    fn append_metadata(
        &mut self,
        state: &mut S,
        _manager: &mut EM,
        _observers: &OT,
        _testcase: &mut crate::corpus::Testcase<I>,
    ) -> Result<(), Error> {
        self.append_list_observer_metadata(state);
        Ok(())
    }
}

impl<T> Named for ListFeedback<T> {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        self.observer_handle.name()
    }
}

impl<T> ListFeedback<T> {
    /// Creates a new [`ListFeedback`], deciding if the given [`ListObserver`] value of a run is interesting.
    #[must_use]
    pub fn new(observer: &ListObserver<T>) -> Self {
        Self {
            observer_handle: observer.handle(),
            novelty: HashSet::new(),
            #[cfg(feature = "std")]
            file: None,
        }
    }

    /// Creates a new [`ListFeedback`], deciding if the given [`ListObserver`] value of a run is interesting.
    /// Will dump newly observed addresses to `path`. If `path` exists, the file will be truncated.
    #[cfg(feature = "std")]
    pub fn with_coverage_dump<P: AsRef<Path>>(observer: &ListObserver<T>, path: P) -> Self {
        let file = Some(File::create(path).unwrap());

        Self {
            observer_handle: observer.handle(),
            novelty: HashSet::new(),
            file,
        }
    }
}
