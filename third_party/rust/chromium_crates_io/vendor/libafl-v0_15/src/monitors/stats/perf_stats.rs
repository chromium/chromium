//! Statistics related to introspection

use alloc::{string::String, vec::Vec};
use core::fmt;

use hashbrown::HashMap;
use serde::{Deserialize, Serialize};

/// Client performance statistics
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ClientPerfStats {
    /// Starting counter (in clock cycles from `read_time_counter`)
    start_time: u64,

    /// Current counter in the fuzzer (in clock cycles from `read_time_counter`
    current_time: u64,

    /// Clock cycles spent in the scheduler
    scheduler: u64,

    /// Clock cycles spent in the manager
    manager: u64,

    /// Current stage index to write the next stage benchmark time
    curr_stage: u8,

    /// Flag to dictate this stage is in use. Used during printing to not print the empty
    /// stages if they are not in use.
    stages_used: Vec<bool>,

    /// Clock cycles spent in the the various features of each stage
    stages: Vec<[u64; PerfFeature::Count as usize]>,

    /// Clock cycles spent in each feedback mechanism of the fuzzer.
    feedbacks: HashMap<String, u64>,

    /// Current time set by `start_timer`
    timer_start: Option<u64>,
}

/// Various features that are measured for performance
#[derive(Serialize, Deserialize, Debug, Clone)]
#[repr(u8)]
pub enum PerfFeature {
    /// Getting an input from the corpus
    GetInputFromCorpus = 0,

    /// Mutating the input
    Mutate = 1,

    /// Post-Exec Mutator callback
    MutatePostExec = 2,

    /// Actual time spent executing the target
    TargetExecution = 3,

    /// Time spent in `pre_exec`
    PreExec = 4,

    /// Time spent in `post_exec`
    PostExec = 5,

    /// Time spent in `observer` `pre_exec_all`
    PreExecObservers = 6,

    /// Time spent in `executor.observers_mut().post_exec_all`
    PostExecObservers = 7,

    /// Time spent getting the feedback from `is_interesting` from all feedbacks
    GetFeedbackInterestingAll = 8,

    /// Time spent getting the feedback from `is_interesting` from all objectives
    GetObjectivesInterestingAll = 9,

    /// Used as a counter to know how many elements are in [`PerfFeature`]. Must be the
    /// last value in the enum.
    Count, // !! No more values here since Count is last! !!
           // !! No more values here since Count is last! !!
}

// TryFromPrimitive requires `std` so these are implemented manually
impl From<PerfFeature> for usize {
    fn from(val: PerfFeature) -> usize {
        match val {
            PerfFeature::GetInputFromCorpus => PerfFeature::GetInputFromCorpus as usize,
            PerfFeature::Mutate => PerfFeature::Mutate as usize,
            PerfFeature::MutatePostExec => PerfFeature::MutatePostExec as usize,
            PerfFeature::TargetExecution => PerfFeature::TargetExecution as usize,
            PerfFeature::PreExec => PerfFeature::PreExec as usize,
            PerfFeature::PostExec => PerfFeature::PostExec as usize,
            PerfFeature::PreExecObservers => PerfFeature::PreExecObservers as usize,
            PerfFeature::PostExecObservers => PerfFeature::PostExecObservers as usize,
            PerfFeature::GetFeedbackInterestingAll => {
                PerfFeature::GetFeedbackInterestingAll as usize
            }
            PerfFeature::GetObjectivesInterestingAll => {
                PerfFeature::GetObjectivesInterestingAll as usize
            }
            PerfFeature::Count => PerfFeature::Count as usize,
        }
    }
}

// TryFromPrimitive requires `std` so these are implemented manually
impl From<usize> for PerfFeature {
    fn from(val: usize) -> PerfFeature {
        match val {
            0 => PerfFeature::GetInputFromCorpus,
            1 => PerfFeature::Mutate,
            2 => PerfFeature::MutatePostExec,
            3 => PerfFeature::TargetExecution,
            4 => PerfFeature::PreExec,
            5 => PerfFeature::PostExec,
            6 => PerfFeature::PreExecObservers,
            7 => PerfFeature::PostExecObservers,
            8 => PerfFeature::GetFeedbackInterestingAll,
            9 => PerfFeature::GetObjectivesInterestingAll,
            _ => panic!("Unknown PerfFeature: {val}"),
        }
    }
}

/// Number of features we can measure for performance
pub const NUM_PERF_FEATURES: usize = PerfFeature::Count as usize;

impl ClientPerfStats {
    /// Create a blank [`ClientPerfStats`] with the `start_time` and `current_time` with
    /// the current clock counter
    #[must_use]
    pub fn new() -> Self {
        let start_time = libafl_bolts::cpu::read_time_counter();

        Self {
            start_time,
            current_time: start_time,
            scheduler: 0,
            manager: 0,
            curr_stage: 0,
            stages: vec![],
            stages_used: vec![],
            feedbacks: HashMap::new(),
            timer_start: None,
        }
    }

    /// Set the current time with the given time
    #[inline]
    pub fn set_current_time(&mut self, time: u64) {
        self.current_time = time;
    }

    /// Start a timer with the current time counter
    #[inline]
    pub fn start_timer(&mut self) {
        self.timer_start = Some(libafl_bolts::cpu::read_time_counter());
    }

    /// Update the current [`ClientPerfStats`] with the given [`ClientPerfStats`]
    pub fn update(&mut self, monitor: &ClientPerfStats) {
        self.set_current_time(monitor.current_time);
        self.update_scheduler(monitor.scheduler);
        self.update_manager(monitor.manager);
        self.update_stages(&monitor.stages);
        self.update_feedbacks(&monitor.feedbacks);
    }

    /// Gets the elapsed time since the internal timer started. Resets the timer when
    /// finished execution.
    #[inline]
    fn mark_time(&mut self) -> u64 {
        match self.timer_start {
            None => {
                // Warning message if marking time without starting the timer first
                log::warn!("Attempted to `mark_time` without starting timer first.");

                // Return 0 for no time marked
                0
            }
            Some(timer_start) => {
                // Calculate the elapsed time
                let elapsed = libafl_bolts::cpu::read_time_counter() - timer_start;

                // Reset the timer
                self.timer_start = None;

                // Return the elapsed time
                elapsed
            }
        }
    }

    /// Update the time spent in the scheduler with the elapsed time that we have seen
    #[inline]
    pub fn mark_scheduler_time(&mut self) {
        // Get the current elapsed time
        let elapsed = self.mark_time();

        // Add the time to the scheduler stat
        self.update_scheduler(elapsed);
    }

    /// Update the time spent in the scheduler with the elapsed time that we have seen
    #[inline]
    pub fn mark_manager_time(&mut self) {
        // Get the current elapsed time
        let elapsed = self.mark_time();

        // Add the time the manager stat
        self.update_manager(elapsed);
    }

    /// Update the time spent in the given [`PerfFeature`] with the elapsed time that we have seen
    #[inline]
    pub fn mark_feature_time(&mut self, feature: PerfFeature) {
        // Get the current elapsed time
        let elapsed = self.mark_time();

        // Add the time the the given feature
        self.update_feature(feature, elapsed);
    }

    /// Add the given `time` to the `scheduler` monitor
    #[inline]
    pub fn update_scheduler(&mut self, time: u64) {
        self.scheduler = self
            .scheduler
            .checked_add(time)
            .expect("update_scheduler overflow");
    }

    /// Add the given `time` to the `manager` monitor
    #[inline]
    pub fn update_manager(&mut self, time: u64) {
        self.manager = self
            .manager
            .checked_add(time)
            .expect("update_manager overflow");
    }

    /// Update the total stage counter and increment the stage counter for the next stage
    #[inline]
    pub fn finish_stage(&mut self) {
        // Increment the stage to the next index. The check is only done if this were to
        // be used past the length of the `self.stages` buffer
        self.curr_stage += 1;
    }

    /// Reset the stage index counter to zero
    #[inline]
    pub fn reset_stage_index(&mut self) {
        self.curr_stage = 0;
    }

    /// Update the time spent in the feedback
    pub fn update_feedback(&mut self, name: &str, time: u64) {
        self.feedbacks.insert(
            name.into(),
            self.feedbacks
                .get(name)
                .unwrap_or(&0)
                .checked_add(time)
                .expect("update_feedback overflow"),
        );
    }

    /// Update the time spent in all the feedbacks
    pub fn update_feedbacks(&mut self, feedbacks: &HashMap<String, u64>) {
        for (key, value) in feedbacks {
            self.update_feedback(key, *value);
        }
    }

    /// Update the time spent in the stages
    pub fn update_stages(&mut self, stages: &[[u64; PerfFeature::Count as usize]]) {
        if self.stages.len() < stages.len() {
            self.stages
                .resize(stages.len(), [0; PerfFeature::Count as usize]);
            self.stages_used.resize(stages.len(), false);
        }
        for (stage_index, features) in stages.iter().enumerate() {
            for (feature_index, feature) in features.iter().enumerate() {
                self.stages[stage_index][feature_index] = self.stages[stage_index][feature_index]
                    .checked_add(*feature)
                    .expect("Stage overflow");
            }
        }
    }

    /// Update the given [`PerfFeature`] with the given `time`
    pub fn update_feature(&mut self, feature: PerfFeature, time: u64) {
        // Get the current stage index as `usize`
        let stage_index: usize = self.curr_stage.into();

        // Get the index of the given feature
        let feature_index: usize = feature.into();

        if stage_index >= self.stages.len() {
            self.stages
                .resize(stage_index + 1, [0; PerfFeature::Count as usize]);
            self.stages_used.resize(stage_index + 1, false);
        }

        // Update the given feature
        self.stages[stage_index][feature_index] = self.stages[stage_index][feature_index]
            .checked_add(time)
            .expect("Stage overflow");

        // Set that the current stage is being used
        self.stages_used[stage_index] = true;
    }

    /// The elapsed cycles (or time)
    #[must_use]
    pub fn elapsed_cycles(&self) -> u64 {
        self.current_time - self.start_time
    }

    /// The amount of cycles the `manager` did
    #[must_use]
    pub fn manager_cycles(&self) -> u64 {
        self.manager
    }

    /// The amount of cycles the `scheduler` did
    #[must_use]
    pub fn scheduler_cycles(&self) -> u64 {
        self.scheduler
    }

    /// Iterator over all used stages
    pub fn used_stages(
        &self,
    ) -> impl Iterator<Item = (usize, &[u64; PerfFeature::Count as usize])> {
        let used = self.stages_used.clone();
        self.stages
            .iter()
            .enumerate()
            .filter(move |(stage_index, _)| used[*stage_index])
    }

    /// A map of all `feedbacks`
    #[must_use]
    pub fn feedbacks(&self) -> &HashMap<String, u64> {
        &self.feedbacks
    }
}

impl fmt::Display for ClientPerfStats {
    #[expect(clippy::cast_precision_loss)]
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        // Calculate the elapsed time from the monitor
        let elapsed: f64 = self.elapsed_cycles() as f64;

        // Calculate the percentages for each benchmark
        let scheduler_percent = self.scheduler as f64 / elapsed;
        let manager_percent = self.manager as f64 / elapsed;

        // Calculate the remaining percentage that has not been benchmarked
        let mut other_percent = 1.0;
        other_percent -= scheduler_percent;
        other_percent -= manager_percent;

        // Create the formatted string
        writeln!(
            f,
            "  {scheduler_percent:6.4}: Scheduler\n  {manager_percent:6.4}: Manager"
        )?;

        // Calculate each stage
        // Make sure we only iterate over used stages
        for (stage_index, features) in self.used_stages() {
            // Write the stage header
            writeln!(f, "  Stage {stage_index}:")?;

            for (feature_index, feature) in features.iter().enumerate() {
                // Calculate this current stage's percentage
                let feature_percent = *feature as f64 / elapsed;

                // Ignore this feature if it isn't used
                if feature_percent == 0.0 {
                    continue;
                }

                // Update the other percent by removing this current percent
                other_percent -= feature_percent;

                // Get the actual feature from the feature index for printing its name
                let feature: PerfFeature = feature_index.into();

                // Write the percentage for this feature
                writeln!(f, "    {feature_percent:6.4}: {feature:?}")?;
            }
        }

        writeln!(f, "  Feedbacks:")?;

        for (feedback_name, feedback_time) in self.feedbacks() {
            // Calculate this current stage's percentage
            let feedback_percent = *feedback_time as f64 / elapsed;

            // Ignore this feedback if it isn't used
            if feedback_percent == 0.0 {
                continue;
            }

            // Update the other percent by removing this current percent
            other_percent -= feedback_percent;

            // Write the percentage for this feedback
            writeln!(f, "    {feedback_percent:6.4}: {feedback_name}")?;
        }

        write!(f, "  {other_percent:6.4}: Not Measured")?;

        Ok(())
    }
}

impl Default for ClientPerfStats {
    fn default() -> Self {
        Self::new()
    }
}
