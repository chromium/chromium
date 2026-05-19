//! Feedback and metatadata for stderr and stdout.

use alloc::{borrow::Cow, string::String};

use libafl_bolts::{
    Named, impl_serdeany,
    tuples::{Handle, Handled, MatchName, MatchNameRef},
};
use serde::{Deserialize, Serialize};

use crate::{
    Error, HasMetadata,
    corpus::Testcase,
    feedbacks::{Feedback, StateInitializer},
    observers::{StdErrObserver, StdOutObserver},
};

/// Metadata for [`StdOutToMetadataFeedback`].
#[derive(Debug, Serialize, Deserialize)]
pub struct StdOutMetadata {
    stdout: String,
}

impl_serdeany!(StdOutMetadata);

/// Nop feedback that annotates stdout in the new testcase. The testcase
/// is never interesting (use with an OR).
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct StdOutToMetadataFeedback {
    o_ref: Handle<StdOutObserver>,
}

impl StdOutToMetadataFeedback {
    /// Append to the testcase the generated metadata in case of a new corpus item.
    #[inline]
    fn append_stdout_observation_to_testcase<I, OT>(
        &mut self,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error>
    where
        OT: MatchName,
    {
        let observer = observers
            .get(&self.o_ref)
            .ok_or(Error::illegal_state("StdOutObserver is missing"))?;
        let buffer = observer
            .output
            .as_ref()
            .ok_or(Error::illegal_state("StdOutObserver has no stdout"))?;
        let stdout = String::from_utf8_lossy(buffer).into_owned();

        testcase
            .metadata_map_mut()
            .insert(StdOutMetadata { stdout });

        Ok(())
    }
}

impl<S> StateInitializer<S> for StdOutToMetadataFeedback {}

impl<EM, I, OT, S> Feedback<EM, I, OT, S> for StdOutToMetadataFeedback
where
    OT: MatchName,
{
    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(false)
    }

    /// Append to the testcase the generated metadata in case of a new corpus item.
    #[inline]
    fn append_metadata(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        self.append_stdout_observation_to_testcase(observers, testcase)
    }
}

impl Named for StdOutToMetadataFeedback {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        self.o_ref.name()
    }
}

impl StdOutToMetadataFeedback {
    /// Creates a new [`StdOutToMetadataFeedback`].
    #[must_use]
    pub fn new(observer: &StdOutObserver) -> Self {
        Self {
            o_ref: observer.handle(),
        }
    }
}

/// Metadata for [`StdErrToMetadataFeedback`].
#[derive(Debug, Serialize, Deserialize)]
pub struct StdErrMetadata {
    stderr: String,
}

impl_serdeany!(StdErrMetadata);

/// Nop feedback that annotates stderr in the new testcase. The testcase
/// is never interesting (use with an OR).
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct StdErrToMetadataFeedback {
    o_ref: Handle<StdErrObserver>,
}

impl<S> StateInitializer<S> for StdErrToMetadataFeedback {}

impl<EM, I, OT, S> Feedback<EM, I, OT, S> for StdErrToMetadataFeedback
where
    OT: MatchName,
{
    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(false)
    }

    /// Append to the testcase the generated metadata in case of a new corpus item.
    #[inline]
    fn append_metadata(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        let observer = observers
            .get(&self.o_ref)
            .ok_or(Error::illegal_state("StdErrObserver is missing"))?;
        let buffer = observer
            .output
            .as_ref()
            .ok_or(Error::illegal_state("StdErrObserver has no stderr"))?;
        let stderr = String::from_utf8_lossy(buffer).into_owned();

        testcase
            .metadata_map_mut()
            .insert(StdErrMetadata { stderr });

        Ok(())
    }
}

impl Named for StdErrToMetadataFeedback {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        self.o_ref.name()
    }
}

impl StdErrToMetadataFeedback {
    /// Creates a new [`StdErrToMetadataFeedback`].
    #[must_use]
    pub fn new(observer: &StdErrObserver) -> Self {
        Self {
            o_ref: observer.handle(),
        }
    }
}
