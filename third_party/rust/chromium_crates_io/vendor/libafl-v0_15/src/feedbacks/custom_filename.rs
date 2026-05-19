use alloc::{borrow::Cow, string::String};
use core::fmt::{self, Debug, Formatter};

use libafl_bolts::Named;
use serde::{Deserialize, Serialize};

use crate::{
    Error,
    corpus::Testcase,
    feedbacks::{Feedback, FeedbackFactory, StateInitializer},
};

/// Type which can generate a custom filename for a given input/state pair
pub trait CustomFilenameGenerator<I, S> {
    /// Sets the name of the provided [`Testcase`] based on the state and input
    fn set_name(&mut self, state: &mut S, testcase: &mut Testcase<I>) -> Result<String, Error>;
}

// maintain compatibility with old impls
impl<I, F, S> CustomFilenameGenerator<I, S> for F
where
    F: FnMut(&mut S, &mut Testcase<I>) -> Result<String, Error>,
{
    fn set_name(&mut self, state: &mut S, testcase: &mut Testcase<I>) -> Result<String, Error> {
        self(state, testcase)
    }
}

/// A [`CustomFilenameToTestcaseFeedback`] takes a [`CustomFilenameGenerator`] which returns a
/// filename for the testcase.
/// Is never interesting (use with an Eager OR).
///
/// Note: Use only in conjunction with a `Corpus` type that writes to disk.
/// Note: If used as part of the `Objective` chain, then it will only apply to testcases which are
/// `Objectives`, vice versa for `Feedback`.
#[derive(Serialize, Deserialize)]
pub struct CustomFilenameToTestcaseFeedback<N> {
    /// Generator that returns the filename.
    generator: N,
}

impl<N> CustomFilenameToTestcaseFeedback<N> {
    /// Create a new [`CustomFilenameToTestcaseFeedback`].
    pub fn new(generator: N) -> Self {
        Self { generator }
    }
}

impl<N, T> FeedbackFactory<CustomFilenameToTestcaseFeedback<N>, T>
    for CustomFilenameToTestcaseFeedback<N>
where
    N: Clone,
{
    fn create_feedback(&self, _ctx: &T) -> CustomFilenameToTestcaseFeedback<N> {
        Self {
            generator: self.generator.clone(),
        }
    }
}

impl<N> Named for CustomFilenameToTestcaseFeedback<N> {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("CustomFilenameToTestcaseFeedback");
        &NAME
    }
}

impl<N> Debug for CustomFilenameToTestcaseFeedback<N> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("CustomFilenameToTestcaseFeedback")
            .finish_non_exhaustive()
    }
}

impl<N, S> StateInitializer<S> for CustomFilenameToTestcaseFeedback<N> {}

impl<EM, I, OT, N, S> Feedback<EM, I, OT, S> for CustomFilenameToTestcaseFeedback<N>
where
    N: CustomFilenameGenerator<I, S>,
{
    #[cfg(feature = "track_hit_feedbacks")]
    #[inline]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(false)
    }

    fn append_metadata(
        &mut self,
        state: &mut S,
        _manager: &mut EM,
        _observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        *testcase.filename_mut() = Some(self.generator.set_name(state, testcase)?);
        Ok(())
    }
}
