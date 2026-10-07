//! Whole corpus minimizers, for reducing the number of samples/the total size/the average runtime
//! of your corpus.

use alloc::{borrow::Cow, string::ToString, vec::Vec};
use core::{hash::Hash, marker::PhantomData, time::Duration};

use hashbrown::{HashMap, HashSet};
use libafl_bolts::{
    AsIter, Named,
    tuples::{Handle, Handled},
};
use num_traits::ToPrimitive;
use z3::{Optimize, ast::Bool};

use crate::{
    Error, HasMetadata, HasScheduler,
    corpus::Corpus,
    events::{Event, EventFirer, EventWithStats, LogSeverity},
    executors::{Executor, ExitKind, HasObservers},
    inputs::Input,
    monitors::stats::{AggregatorOps, UserStats, UserStatsValue},
    observers::{MapObserver, ObserversTuple},
    schedulers::{LenTimeMulTestcasePenalty, RemovableScheduler, Scheduler, TestcasePenalty},
    stages::run_target_with_timing,
    state::{HasCorpus, HasExecutions},
};

/// Minimizes a corpus according to coverage maps, weighting by the specified `TestcasePenalty`.
///
/// Algorithm based on WMOPT: <https://hexhive.epfl.ch/publications/files/21ISSTA2.pdf>
#[derive(Debug)]
pub struct MapCorpusMinimizer<C, E, I, O, S, T, TP> {
    observer_handle: Handle<C>,
    phantom: PhantomData<(E, I, O, S, T, TP)>,
}

/// Standard corpus minimizer, which weights inputs by length and time.
pub type StdCorpusMinimizer<C, E, I, O, S, T> =
    MapCorpusMinimizer<C, E, I, O, S, T, LenTimeMulTestcasePenalty>;

impl<C, E, I, O, S, T, TP> MapCorpusMinimizer<C, E, I, O, S, T, TP>
where
    C: Named,
{
    /// Constructs a new `MapCorpusMinimizer` from a provided observer. This observer will be used
    /// in the future to get observed maps from an executed input.
    pub fn new(obs: &C) -> Self {
        Self {
            observer_handle: obs.handle(),
            phantom: PhantomData,
        }
    }
}

impl<C, E, I, O, S, T, TP> MapCorpusMinimizer<C, E, I, O, S, T, TP>
where
    for<'a> O: MapObserver<Entry = T> + AsIter<'a, Item = T>,
    C: AsRef<O>,
    I: Input,
    S: HasMetadata + HasCorpus<I> + HasExecutions,
    T: Copy + Hash + Eq,
    TP: TestcasePenalty<I, S>,
{
    /// Do the minimization
    #[expect(clippy::too_many_lines)]
    pub fn minimize<CS, EM, Z>(
        &self,
        fuzzer: &mut Z,
        executor: &mut E,
        mgr: &mut EM,
        state: &mut S,
    ) -> Result<(), Error>
    where
        E: Executor<EM, I, S, Z> + HasObservers,
        E::Observers: ObserversTuple<I, S>,
        CS: Scheduler<I, S> + RemovableScheduler<I, S>,
        EM: EventFirer<I, S>,
        Z: HasScheduler<I, S, Scheduler = CS>,
    {
        // don't delete this else it won't work after restart
        let current = *state.corpus().current();

        let opt = Optimize::new();

        let mut seed_exprs = HashMap::new();
        let mut cov_map = HashMap::new();

        let mut cur_id = state.corpus().first();

        mgr.log(
            state,
            LogSeverity::Info,
            "Executing each input...".to_string(),
        )?;

        let total = state.corpus().count() as u64;
        let mut curr = 0;
        while let Some(id) = cur_id {
            let (weight, executions) = {
                if state.corpus().get(id)?.borrow().scheduled_count() == 0 {
                    // Execute the input; we cannot rely on the metadata already being present.

                    let input = state
                        .corpus()
                        .get(id)?
                        .borrow_mut()
                        .load_input(state.corpus())?
                        .clone();

                    let (exit_kind, mut total_time, _) =
                        run_target_with_timing(fuzzer, executor, state, mgr, &input, false)?;
                    if exit_kind != ExitKind::Ok {
                        total_time = Duration::from_secs(1);
                    }
                    state
                        .corpus()
                        .get(id)?
                        .borrow_mut()
                        .set_exec_time(total_time);
                }

                let mut testcase = state.corpus().get(id)?.borrow_mut();
                (
                    TP::compute(state, &mut *testcase)?
                        .to_u64()
                        .expect("Weight must be computable."),
                    *state.executions(),
                )
            };

            curr += 1;

            mgr.fire(
                state,
                EventWithStats::with_current_time(
                    Event::UpdateUserStats {
                        name: Cow::from("minimisation exec pass"),
                        value: UserStats::new(
                            UserStatsValue::Ratio(curr, total),
                            AggregatorOps::None,
                        ),
                        phantom: PhantomData,
                    },
                    executions,
                ),
            )?;

            let seed_expr = Bool::fresh_const("seed");
            let observers = executor.observers();
            let obs = observers[&self.observer_handle].as_ref();

            // Store coverage, mapping coverage map indices to hit counts (if present) and the
            // associated seeds for the map indices with those hit counts.
            for (i, e) in obs.as_iter().map(|x| *x).enumerate() {
                if e != obs.initial() {
                    cov_map
                        .entry(i)
                        .or_insert_with(HashMap::new)
                        .entry(e)
                        .or_insert_with(HashSet::new)
                        .insert(seed_expr.clone());
                }
            }

            // Keep track of that seed's index and weight
            seed_exprs.insert(seed_expr, (id, weight));

            cur_id = state.corpus().next(id);
        }

        mgr.log(
            state,
            LogSeverity::Info,
            "Preparing Z3 assertions...".to_string(),
        )?;

        for (_, cov) in cov_map {
            for (_, seeds) in cov {
                // At least one seed for each hit count of each coverage map index
                if let Some(reduced) = seeds.into_iter().reduce(|s1, s2| s1 | s2) {
                    opt.assert(&reduced);
                }
            }
        }
        for (seed, (_, weight)) in &seed_exprs {
            // opt will attempt to minimise the number of violated assertions.
            //
            // To tell opt to minimize the number of seeds, we tell opt to maximize the number of
            // not seeds.
            //
            // Additionally, each seed has a weight associated with them; the higher, the more z3
            // doesn't want to violate the assertion. Thus, inputs which have higher weights will be
            // less likely to appear in the final corpus -- provided all their coverage points are
            // hit by at least one other input.
            opt.assert_soft(&!seed, *weight, None);
        }

        mgr.log(state, LogSeverity::Info, "Performing MaxSAT...".to_string())?;
        // Perform the optimization!
        opt.check(&[]);

        if let Some(model) = opt.get_model() {
            let mut removed = Vec::with_capacity(state.corpus().count());
            for (seed, (id, _)) in seed_exprs {
                // if the model says the seed isn't there, mark it for deletion
                if !model.eval(&seed, true).unwrap().as_bool().unwrap() {
                    removed.push(id);
                }
            }
            // reverse order; if indexes are stored in a vec, we need to remove from back to front
            removed.sort_unstable_by(|id1, id2| id2.cmp(id1));
            for id in removed {
                if let Some(_cur) = current {
                    continue;
                }

                let removed = state.corpus_mut().remove(id)?;
                // scheduler needs to know we've removed the input, or it will continue to try
                // to use now-missing inputs
                fuzzer
                    .scheduler_mut()
                    .on_remove(state, id, &Some(removed))?;
            }

            *state.corpus_mut().current_mut() = None; //we may have removed the current ID from the corpus
            return Ok(());
        }
        Err(Error::unknown("Corpus minimization failed; unsat."))
    }
}
