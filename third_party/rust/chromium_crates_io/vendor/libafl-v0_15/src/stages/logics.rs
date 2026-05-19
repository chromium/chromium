//! Stage wrappers that add logics to stage list

use core::marker::PhantomData;

use crate::{
    Error,
    stages::{Restartable, Stage, StageId, StagesTuple},
    state::HasNestedStage,
};

/// Progress for nested stages. This merely enters/exits the inner stage's scope.
#[derive(Debug)]
pub struct NestedStageRetryCountRestartHelper;

impl NestedStageRetryCountRestartHelper {
    fn should_restart<S, ST>(state: &mut S, _stage: &ST) -> Result<bool, Error>
    where
        S: HasNestedStage,
    {
        state.enter_inner_stage()?;
        Ok(true)
    }

    fn clear_progress<S, ST>(state: &mut S, _stage: &ST) -> Result<(), Error>
    where
        S: HasNestedStage,
    {
        state.exit_inner_stage()?;
        Ok(())
    }
}

#[derive(Debug)]
/// Perform the stage while the closure evaluates to true
pub struct WhileStage<CB, E, EM, ST, S, Z> {
    closure: CB,
    stages: ST,
    phantom: PhantomData<(E, EM, S, Z)>,
}

impl<CB, E, EM, ST, S, Z> Stage<E, EM, S, Z> for WhileStage<CB, E, EM, ST, S, Z>
where
    CB: FnMut(&mut Z, &mut E, &mut S, &mut EM) -> Result<bool, Error>,
    ST: StagesTuple<E, EM, S, Z>,
    S: HasNestedStage,
{
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        while state.current_stage_id()?.is_some()
            || (self.closure)(fuzzer, executor, state, manager)?
        {
            self.stages.perform_all(fuzzer, executor, state, manager)?;
        }

        Ok(())
    }
}

impl<CB, E, EM, ST, S, Z> Restartable<S> for WhileStage<CB, E, EM, ST, S, Z>
where
    S: HasNestedStage,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        NestedStageRetryCountRestartHelper::should_restart(state, self)
    }

    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        NestedStageRetryCountRestartHelper::clear_progress(state, self)
    }
}

impl<CB, E, EM, ST, S, Z> WhileStage<CB, E, EM, ST, S, Z>
where
    CB: FnMut(&mut Z, &mut E, &mut S, &mut EM) -> Result<bool, Error>,
{
    /// Constructor
    pub fn new(closure: CB, stages: ST) -> Self {
        Self {
            closure,
            stages,
            phantom: PhantomData,
        }
    }
}

/// A conditionally enabled stage.
/// If the closure returns true, the wrapped stage will be executed, else it will be skipped.
#[derive(Debug)]
pub struct IfStage<CB, E, EM, ST, S, Z> {
    closure: CB,
    if_stages: ST,
    phantom: PhantomData<(E, EM, S, Z)>,
}

impl<CB, E, EM, ST, S, Z> Stage<E, EM, S, Z> for IfStage<CB, E, EM, ST, S, Z>
where
    CB: FnMut(&mut Z, &mut E, &mut S, &mut EM) -> Result<bool, Error>,
    ST: StagesTuple<E, EM, S, Z>,
    S: HasNestedStage,
{
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        if state.current_stage_id()?.is_some() || (self.closure)(fuzzer, executor, state, manager)?
        {
            self.if_stages
                .perform_all(fuzzer, executor, state, manager)?;
        }
        Ok(())
    }
}

impl<CB, E, EM, ST, S, Z> Restartable<S> for IfStage<CB, E, EM, ST, S, Z>
where
    S: HasNestedStage,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        NestedStageRetryCountRestartHelper::should_restart(state, self)
    }

    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        NestedStageRetryCountRestartHelper::clear_progress(state, self)
    }
}

impl<CB, E, EM, ST, S, Z> IfStage<CB, E, EM, ST, S, Z>
where
    CB: FnMut(&mut Z, &mut E, &mut S, &mut EM) -> Result<bool, Error>,
{
    /// Constructor for this conditionally enabled stage.
    /// If the closure returns true, the wrapped stage will be executed, else it will be skipped.
    pub fn new(closure: CB, if_stages: ST) -> Self {
        Self {
            closure,
            if_stages,
            phantom: PhantomData,
        }
    }
}

/// Perform the stage if closure evaluates to true
#[derive(Debug)]
pub struct IfElseStage<CB, E, EM, ST1, ST2, S, Z> {
    closure: CB,
    if_stages: ST1,
    else_stages: ST2,
    phantom: PhantomData<(E, EM, S, Z)>,
}

impl<CB, E, EM, ST1, ST2, S, Z> Stage<E, EM, S, Z> for IfElseStage<CB, E, EM, ST1, ST2, S, Z>
where
    CB: FnMut(&mut Z, &mut E, &mut S, &mut EM) -> Result<bool, Error>,
    ST1: StagesTuple<E, EM, S, Z>,
    ST2: StagesTuple<E, EM, S, Z>,
    S: HasNestedStage,
{
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        let current = state.current_stage_id()?;

        // this is None if you didn't recover from restart
        // because should_restart() which is called right before this will create a new stage stack
        let fresh = current.is_none();
        let closure_res = fresh && (self.closure)(fuzzer, executor, state, manager)?;

        if current == Some(StageId(0)) || closure_res {
            if fresh {
                state.set_current_stage_id(StageId(0))?;
            }
            state.enter_inner_stage()?;
            self.if_stages
                .perform_all(fuzzer, executor, state, manager)?;
        } else {
            if fresh {
                state.set_current_stage_id(StageId(1))?;
            }
            state.enter_inner_stage()?;
            self.else_stages
                .perform_all(fuzzer, executor, state, manager)?;
        }

        state.exit_inner_stage()?;
        state.clear_stage_id()?;

        Ok(())
    }
}

impl<CB, E, EM, ST1, ST2, S, Z> Restartable<S> for IfElseStage<CB, E, EM, ST1, ST2, S, Z>
where
    S: HasNestedStage,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        NestedStageRetryCountRestartHelper::should_restart(state, self)
    }

    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        NestedStageRetryCountRestartHelper::clear_progress(state, self)
    }
}

impl<CB, E, EM, ST1, ST2, S, Z> IfElseStage<CB, E, EM, ST1, ST2, S, Z>
where
    CB: FnMut(&mut Z, &mut E, &mut S, &mut EM) -> Result<bool, Error>,
{
    /// Constructor
    pub fn new(closure: CB, if_stages: ST1, else_stages: ST2) -> Self {
        Self {
            closure,
            if_stages,
            else_stages,
            phantom: PhantomData,
        }
    }
}

/// A stage wrapper where the stages do not need to be initialized, but can be [`None`].
#[derive(Debug)]
pub struct OptionalStage<E, EM, ST, S, Z> {
    stages: Option<ST>,
    phantom: PhantomData<(E, EM, S, Z)>,
}

impl<E, EM, ST, S, Z> Stage<E, EM, S, Z> for OptionalStage<E, EM, ST, S, Z>
where
    ST: StagesTuple<E, EM, S, Z>,
    S: HasNestedStage,
{
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        match &mut self.stages {
            Some(stages) => stages.perform_all(fuzzer, executor, state, manager),
            _ => Ok(()),
        }
    }
}

impl<E, EM, ST, S, Z> Restartable<S> for OptionalStage<E, EM, ST, S, Z>
where
    S: HasNestedStage,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        NestedStageRetryCountRestartHelper::should_restart(state, self)
    }

    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        NestedStageRetryCountRestartHelper::clear_progress(state, self)
    }
}

impl<E, EM, ST, S, Z> OptionalStage<E, EM, ST, S, Z> {
    /// Constructor for this conditionally enabled stage.
    #[must_use]
    pub fn new(stages: Option<ST>) -> Self {
        Self {
            stages,
            phantom: PhantomData,
        }
    }

    /// Constructor for this conditionally enabled stage with set stages.
    #[must_use]
    pub fn some(stages: ST) -> Self {
        Self {
            stages: Some(stages),
            phantom: PhantomData,
        }
    }

    /// Constructor for this conditionally enabled stage, without stages set.
    #[must_use]
    pub fn none() -> Self {
        Self {
            stages: None,
            phantom: PhantomData,
        }
    }
}

#[cfg(test)]
mod test {
    use alloc::rc::Rc;
    use core::{cell::RefCell, marker::PhantomData};

    use libafl_bolts::{
        Error, impl_serdeany,
        tuples::{tuple_list, tuple_list_type},
    };
    use serde::{Deserialize, Serialize};

    #[cfg(any(not(feature = "serdeany_autoreg"), miri))]
    use crate::stages::RetryCountRestartHelper;
    use crate::{
        HasMetadata, NopFuzzer,
        events::NopEventManager,
        executors::nop::NopExecutor,
        stages::{
            ClosureStage, CorpusId, HasCurrentCorpusId, IfElseStage, IfStage, Restartable, Stage,
            StagesTuple, WhileStage,
        },
        state::{HasCurrentStageId, StdState},
    };

    #[derive(Debug)]
    pub struct ResumeSucceededStage<S> {
        phantom: PhantomData<S>,
    }
    #[derive(Debug)]
    pub struct ResumeFailedStage<S> {
        completed: Rc<RefCell<bool>>,
        phantom: PhantomData<S>,
    }
    #[derive(Serialize, Deserialize, Debug)]
    pub struct TestProgress {
        count: usize,
    }

    impl_serdeany!(TestProgress);

    impl TestProgress {
        #[expect(clippy::unnecessary_wraps)]
        fn should_restart<S, ST>(state: &mut S, _stage: &ST) -> Result<bool, Error>
        where
            S: HasMetadata,
        {
            // check if we're resuming
            let _metadata = state.metadata_or_insert_with(|| Self { count: 0 });
            Ok(true)
        }

        fn clear_progress<S, ST>(state: &mut S, _stage: &ST) -> Result<(), Error>
        where
            S: HasMetadata,
        {
            if state.remove_metadata::<Self>().is_none() {
                return Err(Error::illegal_state(
                    "attempted to clear status metadata when none was present",
                ));
            }
            Ok(())
        }
    }

    impl<E, EM, S, Z> Stage<E, EM, S, Z> for ResumeSucceededStage<S>
    where
        S: HasMetadata,
    {
        fn perform(
            &mut self,
            _fuzzer: &mut Z,
            _executor: &mut E,
            state: &mut S,
            _manager: &mut EM,
        ) -> Result<(), Error> {
            // metadata is attached by the status
            let meta = state.metadata_mut::<TestProgress>().unwrap();
            meta.count += 1;
            assert!(
                meta.count == 1,
                "Test failed; we resumed a succeeded stage!"
            );
            Ok(())
        }
    }

    impl<S> Restartable<S> for ResumeSucceededStage<S>
    where
        S: HasMetadata,
    {
        fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
            TestProgress::should_restart(state, self)
        }

        fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
            TestProgress::clear_progress(state, self)
        }
    }

    impl<E, EM, S, Z> Stage<E, EM, S, Z> for ResumeFailedStage<S>
    where
        S: HasMetadata,
    {
        fn perform(
            &mut self,
            _fuzzer: &mut Z,
            _executor: &mut E,
            state: &mut S,
            _manager: &mut EM,
        ) -> Result<(), Error> {
            // metadata is attached by the status
            let meta = state.metadata_mut::<TestProgress>().unwrap();
            meta.count += 1;
            if meta.count == 1 {
                return Err(Error::shutting_down());
            } else if meta.count > 2 {
                panic!("Resume was somehow corrupted?")
            } else {
                self.completed.replace(true);
            }
            Ok(())
        }
    }

    impl<S> Restartable<S> for ResumeFailedStage<S>
    where
        S: HasMetadata,
    {
        fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
            TestProgress::should_restart(state, self)
        }

        fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
            TestProgress::clear_progress(state, self)
        }
    }

    #[must_use]
    #[allow(clippy::type_complexity)]
    pub fn test_resume_stages<S>() -> (
        Rc<RefCell<bool>>,
        tuple_list_type!(ResumeSucceededStage<S>, ResumeFailedStage<S>),
    ) {
        let completed = Rc::new(RefCell::new(false));
        (
            completed.clone(),
            tuple_list!(
                ResumeSucceededStage {
                    phantom: PhantomData
                },
                ResumeFailedStage {
                    completed,
                    phantom: PhantomData
                },
            ),
        )
    }

    pub fn test_resume<ST, S>(completed: &Rc<RefCell<bool>>, state: &mut S, mut stages: ST)
    where
        ST: StagesTuple<NopExecutor, NopEventManager, S, NopFuzzer>,
        S: HasCurrentStageId + HasCurrentCorpusId,
    {
        #[cfg(any(not(feature = "serdeany_autoreg"), miri))]
        unsafe {
            TestProgress::register();
            RetryCountRestartHelper::register();
        }

        let mut fuzzer = NopFuzzer::new();
        let mut executor = NopExecutor::ok();
        let mut manager = NopEventManager::new();
        for _ in 0..2 {
            completed.replace(false);
            // fake one, just any number so retryhelper won't fail.
            // in reality you always have corpus id set by stdfuzzer
            state.set_corpus_id(CorpusId::from(0_usize)).unwrap();
            let Err(e) = stages.perform_all(&mut fuzzer, &mut executor, state, &mut manager) else {
                panic!("Test failed; stages should fail the first time.")
            };
            assert!(
                matches!(e, Error::ShuttingDown),
                "Unexpected error encountered."
            );
            assert!(!*completed.borrow(), "Unexpectedly complete?");
            state
                .on_restart()
                .expect("Couldn't notify state of restart.");
            assert!(
                stages
                    .perform_all(&mut fuzzer, &mut executor, state, &mut manager)
                    .is_ok(),
                "Test failed; stages should pass the second time."
            );
            assert!(
                *completed.borrow(),
                "Test failed; we did not set completed."
            );
        }
    }

    #[test]
    fn check_resumability_while() {
        let once = RefCell::new(true);

        let (completed, stages) = test_resume_stages();
        let whilestage = WhileStage::new(
            |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(once.replace(false)),
            stages,
        );
        let resetstage = ClosureStage::new(|_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| {
            once.replace(true);
            Ok(())
        });
        let mut state = StdState::nop().unwrap();
        test_resume(&completed, &mut state, tuple_list!(whilestage, resetstage));
    }

    #[test]
    fn check_resumability_if() {
        let once = RefCell::new(true);
        let (completed, stages) = test_resume_stages();
        let ifstage = IfStage::new(
            |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(once.replace(false)),
            stages,
        );
        let resetstage = ClosureStage::new(|_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| {
            once.replace(true);
            Ok(())
        });
        let mut state = StdState::nop().unwrap();
        test_resume(&completed, &mut state, tuple_list!(ifstage, resetstage));
    }

    #[test]
    fn check_resumability_if_deep() {
        let (completed, stages) = test_resume_stages();
        let ifstage = IfStage::new(
            |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(true),
            tuple_list!(IfStage::new(
                |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(true),
                tuple_list!(IfStage::new(
                    |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(true),
                    tuple_list!(IfStage::new(
                        |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(true),
                        tuple_list!(IfStage::new(
                            |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(true),
                            stages
                        ),),
                    ),),
                ))
            )),
        );
        let mut state = StdState::nop().unwrap();
        test_resume(&completed, &mut state, tuple_list!(ifstage));
    }

    #[derive(Debug)]
    pub struct PanicStage<S> {
        phantom: PhantomData<S>,
    }
    impl<S> PanicStage<S> {
        pub fn new() -> Self {
            Self {
                phantom: PhantomData,
            }
        }
    }
    impl<E, EM, S, Z> Stage<E, EM, S, Z> for PanicStage<S> {
        fn perform(
            &mut self,
            _fuzzer: &mut Z,
            _executor: &mut E,
            _state: &mut S,
            _manager: &mut EM,
        ) -> Result<(), Error> {
            panic!("Test failed; panic stage should never be executed.");
        }
    }

    impl<S> Restartable<S> for PanicStage<S> {
        fn should_restart(&mut self, _state: &mut S) -> Result<bool, Error> {
            Ok(true)
        }

        fn clear_progress(&mut self, _state: &mut S) -> Result<(), Error> {
            Ok(())
        }
    }
    #[test]
    fn check_resumability_if_else_if() {
        let once = RefCell::new(true);
        let (completed, stages) = test_resume_stages();
        let ifstage = IfElseStage::new(
            |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(once.replace(false)),
            stages,
            tuple_list!(PanicStage::new()),
        );
        let resetstage = ClosureStage::new(|_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| {
            once.replace(true);
            Ok(())
        });
        let mut state = StdState::nop().unwrap();
        test_resume(&completed, &mut state, tuple_list!(ifstage, resetstage));
    }

    #[test]
    fn check_resumability_if_else_else() {
        let once = RefCell::new(false);
        let (completed, stages) = test_resume_stages();
        let ifstage = IfElseStage::new(
            |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(once.replace(true)),
            tuple_list!(PanicStage::new()),
            stages,
        );
        let resetstage = ClosureStage::new(|_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| {
            once.replace(false);
            Ok(())
        });
        let mut state = StdState::nop().unwrap();
        test_resume(&completed, &mut state, tuple_list!(ifstage, resetstage));
    }
    #[test]
    fn check_resumability_if_else_else_deep() {
        let (completed, stages) = test_resume_stages();
        let ifstage = IfElseStage::new(
            |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(false),
            tuple_list!(PanicStage::new()),
            tuple_list!(IfElseStage::new(
                |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(false),
                tuple_list!(PanicStage::new()),
                tuple_list!(IfElseStage::new(
                    |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(false),
                    tuple_list!(PanicStage::new()),
                    tuple_list!(IfElseStage::new(
                        |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(false),
                        tuple_list!(PanicStage::new()),
                        tuple_list!(IfElseStage::new(
                            |_a: &mut _, _b: &mut _, _c: &mut _, _d: &mut _| Ok(false),
                            tuple_list!(PanicStage::new()),
                            stages,
                        )),
                    )),
                )),
            )),
        );
        let mut state = StdState::nop().unwrap();
        test_resume(&completed, &mut state, tuple_list!(ifstage));
    }
}
