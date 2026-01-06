use crate::compiler::instructions::Instruction;
use crate::error::{Error, ErrorKind};

use std::sync::atomic::{AtomicIsize, Ordering};
use std::sync::Arc;

/// Helper for tracking fuel consumption
pub struct FuelTracker {
    // The initial fuel level.
    initial: u64,
    // This should be an AtomicI64 but sadly 32bit targets do not necessarily have
    // AtomicI64 available.
    remaining: AtomicIsize,
}

impl FuelTracker {
    /// Creates a new fuel tracker.
    ///
    /// The fuel tracker is always wrapped in an `Arc` so that it can be
    /// shared across nested invocations of the template evaluation.
    pub fn new(fuel: u64) -> Arc<FuelTracker> {
        Arc::new(FuelTracker {
            initial: fuel,
            remaining: AtomicIsize::new(fuel as isize),
        })
    }

    /// Tracks an instruction.  If it runs out of fuel an error is returned.
    pub fn track(&self, instr: &Instruction) -> Result<(), Error> {
        let fuel_to_consume = fuel_for_instruction(instr);
        if fuel_to_consume != 0 {
            let old_fuel = self.remaining.fetch_sub(fuel_to_consume, Ordering::Relaxed);
            if old_fuel - fuel_to_consume <= 0 {
                return Err(Error::from(ErrorKind::OutOfFuel));
            }
        }
        Ok(())
    }

    /// Returns the remaining fuel.
    pub fn remaining(&self) -> u64 {
        self.remaining.load(Ordering::Relaxed) as _
    }

    /// Returns the consumed fuel.
    pub fn consumed(&self) -> u64 {
        self.initial.saturating_sub(self.remaining())
    }
}

/// How much fuel does an instruction consume?
fn fuel_for_instruction(instruction: &Instruction) -> isize {
    match instruction {
        Instruction::BeginCapture(_)
        | Instruction::PushLoop(_)
        | Instruction::PushDidNotIterate
        | Instruction::PushWith
        | Instruction::PopFrame
        | Instruction::PopLoopFrame
        | Instruction::DupTop
        | Instruction::DiscardTop
        | Instruction::PushAutoEscape
        | Instruction::PopAutoEscape => 0,
        #[cfg(feature = "multi_template")]
        Instruction::ExportLocals => 0,
        #[cfg(feature = "macros")]
        Instruction::LoadBlocks | Instruction::BuildMacro(..) | Instruction::Return => 0,
        _ => 1,
    }
}
