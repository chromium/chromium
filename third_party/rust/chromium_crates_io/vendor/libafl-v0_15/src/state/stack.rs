use alloc::vec::Vec;

use libafl_bolts::Error;
use serde::{Deserialize, Serialize};

use crate::{
    stages::StageId,
    state::{HasCurrentStageId, HasNestedStage},
};

/// A stack to keep track of which stage is executing
#[derive(Serialize, Deserialize, Debug, Clone, Default)]
pub struct StageStack {
    /// The stage indexes for each nesting of stages
    stage_idx_stack: Vec<StageId>,
    /// The current stage depth
    stage_depth: usize,
}

impl HasCurrentStageId for StageStack {
    fn set_current_stage_id(&mut self, idx: StageId) -> Result<(), Error> {
        // ensure we are in the right frame
        if self.stage_depth != self.stage_idx_stack.len() {
            return Err(Error::illegal_state(
                "stage not resumed before setting stage",
            ));
        }
        self.stage_idx_stack.push(idx);
        Ok(())
    }

    fn clear_stage_id(&mut self) -> Result<(), Error> {
        self.stage_idx_stack.truncate(self.stage_depth);
        Ok(())
    }

    fn current_stage_id(&self) -> Result<Option<StageId>, Error> {
        Ok(self.stage_idx_stack.get(self.stage_depth).copied())
    }

    fn on_restart(&mut self) -> Result<(), Error> {
        self.stage_depth = 0; // reset the stage depth so that we may resume inward
        Ok(())
    }
}

impl HasNestedStage for StageStack {
    fn enter_inner_stage(&mut self) -> Result<(), Error> {
        self.stage_depth += 1;
        Ok(())
    }

    fn exit_inner_stage(&mut self) -> Result<(), Error> {
        self.stage_depth -= 1;
        Ok(())
    }
}
