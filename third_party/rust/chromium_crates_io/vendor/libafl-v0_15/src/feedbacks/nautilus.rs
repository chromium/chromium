//! Nautilus grammar mutator, see <https://github.com/nautilus-fuzz/nautilus>
use alloc::{
    borrow::Cow,
    string::{String, ToString},
};
use core::fmt::Debug;
use std::fs::create_dir_all;

use libafl_bolts::Named;
use serde::{Deserialize, Serialize};

use crate::{
    Error, HasMetadata,
    common::nautilus::grammartec::{chunkstore::ChunkStore, context::Context},
    corpus::{Corpus, Testcase},
    feedbacks::{Feedback, StateInitializer},
    generators::NautilusContext,
    inputs::NautilusInput,
    state::HasCorpus,
};

/// Metadata for Nautilus grammar mutator chunks
#[derive(Serialize, Deserialize)]
pub struct NautilusChunksMetadata {
    /// the chunk store
    pub cks: ChunkStore,
}

impl Debug for NautilusChunksMetadata {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "NautilusChunksMetadata {{ {} }}",
            serde_json::to_string_pretty(self).unwrap(),
        )
    }
}

libafl_bolts::impl_serdeany!(NautilusChunksMetadata);

impl NautilusChunksMetadata {
    /// Creates a new [`NautilusChunksMetadata`]
    #[must_use]
    pub fn new(work_dir: String) -> Self {
        create_dir_all(format!("{}/outputs/chunks", &work_dir))
            .expect("Could not create folder in workdir");
        Self {
            cks: ChunkStore::new(work_dir),
        }
    }
}

/// A nautilus feedback for grammar fuzzing
#[derive(Debug)]
pub struct NautilusFeedback<'a> {
    ctx: &'a Context,
}

impl<'a> NautilusFeedback<'a> {
    /// Create a new [`NautilusFeedback`]
    #[must_use]
    pub fn new(context: &'a NautilusContext) -> Self {
        Self { ctx: &context.ctx }
    }

    fn append_nautilus_metadata_to_state<S>(
        &mut self,
        state: &mut S,
        testcase: &mut Testcase<NautilusInput>,
    ) -> Result<(), Error>
    where
        S: HasCorpus<NautilusInput> + HasMetadata,
    {
        state.corpus().load_input_into(testcase)?;
        let input = testcase.input().as_ref().unwrap().clone();
        let meta = state
            .metadata_map_mut()
            .get_mut::<NautilusChunksMetadata>()
            .expect("NautilusChunksMetadata not in the state");
        meta.cks.add_tree(input.tree, self.ctx);

        Ok(())
    }
}

impl Named for NautilusFeedback<'_> {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("NautilusFeedback");
        &NAME
    }
}

impl<S> StateInitializer<S> for NautilusFeedback<'_> {}

impl<EM, OT, S> Feedback<EM, NautilusInput, OT, S> for NautilusFeedback<'_>
where
    S: HasMetadata + HasCorpus<NautilusInput>,
{
    fn append_metadata(
        &mut self,
        state: &mut S,
        _manager: &mut EM,
        _observers: &OT,
        testcase: &mut Testcase<NautilusInput>,
    ) -> Result<(), Error> {
        self.append_nautilus_metadata_to_state(state, testcase)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(false)
    }
}

#[derive(Serialize, Deserialize, Clone, Debug)]
struct NautilusUnparseMetadata {
    unparsed: String,
}

libafl_bolts::impl_serdeany!(NautilusUnparseMetadata);

/// Add the unparsed input to the testcase metadata. Useful e.g. if you want to have the input be unparsed on any solution automatically.
///
/// Returns a constant `false` for `is_interesting`.
#[derive(Debug)]
pub struct NautilusUnparseToMetadataFeedback<'a> {
    context: &'a NautilusContext,
}

impl<'a> NautilusUnparseToMetadataFeedback<'a> {
    /// Create a new [`NautilusUnparseToMetadataFeedback`]
    #[must_use]
    pub fn new(context: &'a NautilusContext) -> Self {
        Self { context }
    }
}
impl<S> StateInitializer<S> for NautilusUnparseToMetadataFeedback<'_> {}

impl<EM, OT, S> Feedback<EM, NautilusInput, OT, S> for NautilusUnparseToMetadataFeedback<'_> {
    fn append_metadata(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        _observers: &OT,
        testcase: &mut Testcase<NautilusInput>,
    ) -> Result<(), Error> {
        let input = testcase.input().as_ref().unwrap().clone();
        let mut unparse_target = vec![];
        input.unparse(self.context, &mut unparse_target);
        let unparsed = String::from_utf8_lossy(&unparse_target).to_string();
        testcase.metadata_or_insert_with(|| NautilusUnparseMetadata { unparsed });

        Ok(())
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(false)
    }
}

impl Named for NautilusUnparseToMetadataFeedback<'_> {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("NautilusUnparseToMetadataFeedback");
        &NAME
    }
}
