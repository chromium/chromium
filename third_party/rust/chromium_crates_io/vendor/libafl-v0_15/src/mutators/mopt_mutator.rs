//! The `MOpt` mutation scheduler used in AFL++.
//!
//! It uses a modified Particle Swarm Optimization algorithm to determine an optimal distribution of mutators.
//! See <https://github.com/puppet-meteor/MOpt-AFL> and <https://www.usenix.org/conference/usenixsecurity19/presentation/lyu>
use alloc::{borrow::Cow, string::ToString, vec::Vec};
use core::fmt::{self, Debug};

use libafl_bolts::{
    Named,
    rands::{Rand, StdRand},
    tuples::{HasConstLen, NamedTuple},
};
use serde::{Deserialize, Serialize};

use super::MutationId;
use crate::{
    Error, HasMetadata,
    corpus::{Corpus, CorpusId},
    mutators::{ComposedByMutations, MutationResult, Mutator, MutatorsTuple, ScheduledMutator},
    state::{HasCorpus, HasRand, HasSolutions},
};

/// A Struct for managing MOpt-mutator parameters.
///
/// There are 2 modes for `MOpt` scheduler, the core fuzzing mode and the pilot fuzzing mode.
/// In short, in the pilot fuzzing mode, the fuzzer employs several `swarms` to compute the probability to choose the mutation operator.
/// On the other hand, in the core fuzzing mode, the fuzzer chooses the best `swarms`, which was determined during the pilot fuzzing mode, to compute the probability to choose the mutation operator.
/// With the current implementation we are always in the pacemaker fuzzing mode.
#[derive(Serialize, Deserialize, Clone)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct MOpt {
    /// Random number generator
    pub rand: StdRand,
    /// The number of total findings (unique crashes and unique interesting paths). This is equivalent to `state.corpus().count() + state.solutions().count()`;
    pub total_finds: usize,
    /// The number of finds before until last swarm.
    pub finds_until_last_swarm: usize,
    /// These w_* and g_* values are the coefficients for updating variables according to the PSO algorithms
    pub w_init: f64,
    /// These w_* and g_* values are the coefficients for updating variables according to the PSO algorithms
    pub w_end: f64,
    /// These w_* and g_* values are the coefficients for updating variables according to the PSO algorithms
    pub w_now: f64,
    /// These w_* and g_* values are the coefficients for updating variables according to the PSO algorithms
    pub g_now: f64,
    /// These w_* and g_* values are the coefficients for updating variables according to the PSO algorithms
    pub g_max: f64,
    /// The number of mutation operators
    pub operator_num: usize,
    /// The number of swarms that we want to employ during the pilot fuzzing mode
    pub swarm_num: usize,
    /// We'll generate inputs for `period_pilot` times before we call `pso_update` in pilot fuzzing module
    pub period_pilot: usize,
    /// We'll generate inputs for `period_core` times before we call `pso_update` in core fuzzing module
    pub period_core: usize,
    /// The number of testcases generated during this pilot fuzzing mode
    pub pilot_time: usize,
    /// The number of testcases generated during this core fuzzing mode
    pub core_time: usize,
    /// The swarm identifier that we are currently using in the pilot fuzzing mode
    pub swarm_now: usize,
    /// A parameter for the PSO algorithm
    x_now: Vec<Vec<f64>>,
    /// A parameter for the PSO algorithm
    l_best: Vec<Vec<f64>>,
    /// A parameter for the PSO algorithm
    eff_best: Vec<Vec<f64>>,
    /// A parameter for the PSO algorithm
    g_best: Vec<f64>,
    /// A parameter for the PSO algorithm
    v_now: Vec<Vec<f64>>,
    /// The probability that we want to use to choose the mutation operator.
    probability_now: Vec<Vec<f64>>,
    /// The fitness for each swarm, we'll calculate the fitness in the pilot fuzzing mode and use the best one in the core fuzzing mode
    pub swarm_fitness: Vec<f64>,
    /// (Pilot Mode) Finds by each operators. This vector is used in `pso_update`
    pub pilot_operator_finds: Vec<Vec<u64>>,
    /// (Pilot Mode) Finds by each operator till now.
    pub pilot_operator_finds_v2: Vec<Vec<u64>>,
    /// (Pilot Mode) The number of mutation operator used. This vector is used in `pso_update`
    pub pilot_operator_cycles: Vec<Vec<u64>>,
    /// (Pilot Mode) The number of mutation operator used till now
    pub pilot_operator_cycles_v2: Vec<Vec<u64>>,
    /// (Pilot Mode) The number of mutation operator used till last execution
    pub pilot_operator_cycles_v3: Vec<Vec<u64>>,
    /// Vector used in `pso_update`
    pub operator_finds_puppet: Vec<u64>,
    /// (Core Mode) Finds by each operators. This vector is used in `pso_update`
    pub core_operator_finds: Vec<u64>,
    /// (Core Mode) Finds by each operator till now.
    pub core_operator_finds_v2: Vec<u64>,
    /// (Core Mode) The number of mutation operator used. This vector is used in `pso_update`
    pub core_operator_cycles: Vec<u64>,
    /// (Core Mode) The number of mutation operator used till now
    pub core_operator_cycles_v2: Vec<u64>,
    /// (Core Mode) The number of mutation operator used till last execution
    pub core_operator_cycles_v3: Vec<u64>,
}

libafl_bolts::impl_serdeany!(MOpt);

impl Debug for MOpt {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("MOpt")
            .field("\ntotal_finds", &self.total_finds)
            .field("\nfinds_until_last_swarm", &self.finds_until_last_swarm)
            .field("\nw_init", &self.w_init)
            .field("\nw_end", &self.w_end)
            .field("\nw_now", &self.g_now)
            .field("\ng_now", &self.g_max)
            .field("\npilot_time", &self.pilot_time)
            .field("\ncore_time", &self.core_time)
            .field("\n\nx_now", &self.x_now)
            .field("\n\nl_best", &self.l_best)
            .field("\n\neff_best", &self.eff_best)
            .field("\n\ng_best", &self.g_best)
            .field("\n\nv_now", &self.v_now)
            .field("\n\nprobability_now", &self.probability_now)
            .field("\n\nswarm_fitness", &self.swarm_fitness)
            .field("\n\npilot_operator_finds", &self.pilot_operator_finds)
            .field(
                "\n\npilot_operator_finds_this",
                &self.pilot_operator_finds_v2,
            )
            .field("\n\npilot_operator_cycles", &self.pilot_operator_cycles)
            .field(
                "\n\npilot_operator_cycles_v2",
                &self.pilot_operator_cycles_v2,
            )
            .field(
                "\n\npilot_operator_cycles_v3",
                &self.pilot_operator_cycles_v3,
            )
            .field("\n\noperator_finds_puppuet", &self.operator_finds_puppet)
            .field("\n\ncore_operator_finds", &self.core_operator_finds)
            .field("\n\ncore_operator_finds_v2", &self.core_operator_finds_v2)
            .field("\n\ncore_operator_cycles", &self.core_operator_cycles)
            .field("\n\ncore_operator_cycles_v2", &self.core_operator_cycles_v2)
            .field("\n\ncore_operator_cycles_v3", &self.core_operator_cycles_v3)
            .finish_non_exhaustive()
    }
}

const PERIOD_PILOT_COEF: f64 = 5000.0;

impl MOpt {
    /// Creates a new [`struct@MOpt`] instance.
    pub fn new(operator_num: usize, swarm_num: usize, rand_seed: u64) -> Result<Self, Error> {
        let mut mopt = Self {
            rand: StdRand::with_seed(rand_seed),
            total_finds: 0,
            finds_until_last_swarm: 0,
            w_init: 0.9,
            w_end: 0.3,
            w_now: 0.0,
            g_now: 0.0,
            g_max: 5000.0,
            operator_num,
            swarm_num,
            period_pilot: 50000,
            period_core: 500000,
            pilot_time: 0,
            core_time: 0,
            swarm_now: 0,
            x_now: vec![vec![0.0; operator_num]; swarm_num],
            l_best: vec![vec![0.0; operator_num]; swarm_num],
            eff_best: vec![vec![0.0; operator_num]; swarm_num],
            g_best: vec![0.0; operator_num],
            v_now: vec![vec![0.0; operator_num]; swarm_num],
            probability_now: vec![vec![0.0; operator_num]; swarm_num],
            swarm_fitness: vec![0.0; swarm_num],
            pilot_operator_finds: vec![vec![0; operator_num]; swarm_num],
            pilot_operator_finds_v2: vec![vec![0; operator_num]; swarm_num],
            pilot_operator_cycles: vec![vec![0; operator_num]; swarm_num],
            pilot_operator_cycles_v2: vec![vec![0; operator_num]; swarm_num],
            pilot_operator_cycles_v3: vec![vec![0; operator_num]; swarm_num],
            operator_finds_puppet: vec![0; operator_num],
            core_operator_finds: vec![0; operator_num],
            core_operator_finds_v2: vec![0; operator_num],
            core_operator_cycles: vec![0; operator_num],
            core_operator_cycles_v2: vec![0; operator_num],
            core_operator_cycles_v3: vec![0; operator_num],
        };
        mopt.pso_initialize()?;
        Ok(mopt)
    }

    /// initialize pso
    pub fn pso_initialize(&mut self) -> Result<(), Error> {
        if self.g_now > self.g_max {
            self.g_now = 0.0;
        }
        self.w_now =
            (self.w_init - self.w_end) * (self.g_max - self.g_now) / self.g_max + self.w_end;

        for swarm in 0..self.swarm_num {
            let mut total_x_now = 0.0;
            let mut x_sum = 0.0;
            for i in 0..self.operator_num {
                self.x_now[swarm][i] = 0.7 * self.rand.next_float() + 0.1;
                total_x_now += self.x_now[swarm][i];
                self.v_now[swarm][i] = 0.1;
                self.l_best[swarm][i] = 0.5;
                self.g_best[i] = 0.5;
            }

            for i in 0..self.operator_num {
                self.x_now[swarm][i] /= total_x_now;
            }

            for i in 0..self.operator_num {
                self.v_now[swarm][i] = self.w_now * self.v_now[swarm][i]
                    + self.rand.next_float() * (self.l_best[swarm][i] - self.x_now[swarm][i])
                    + self.rand.next_float() * (self.g_best[i] - self.x_now[swarm][i]);
                self.x_now[swarm][i] += self.v_now[swarm][i];

                self.x_now[swarm][i] = self.x_now[swarm][i].clamp(V_MIN, V_MAX);

                x_sum += self.x_now[swarm][i];
            }

            for i in 0..self.operator_num {
                self.x_now[swarm][i] /= x_sum;
                if i == 0 {
                    self.probability_now[swarm][i] = self.x_now[swarm][i];
                } else {
                    self.probability_now[swarm][i] =
                        self.probability_now[swarm][i - 1] + self.x_now[swarm][i];
                }
            }
            if self.probability_now[swarm][self.operator_num - 1] < 0.99
                || self.probability_now[swarm][self.operator_num - 1] > 1.01
            {
                return Err(Error::illegal_state(
                    "MOpt: Error in pso_update".to_string(),
                ));
            }
        }
        Ok(())
    }

    /// Update the `PSO` algorithm parameters
    /// See <https://github.com/puppet-meteor/MOpt-AFL/blob/master/MOpt/afl-fuzz.c#L10623>
    #[expect(clippy::cast_precision_loss)]
    pub fn pso_update(&mut self) -> Result<(), Error> {
        self.g_now += 1.0;
        if self.g_now > self.g_max {
            self.g_now = 0.0;
        }
        self.w_now =
            ((self.w_init - self.w_end) * (self.g_max - self.g_now) / self.g_max) + self.w_end;

        let mut operator_finds_sum = 0;

        for i in 0..self.operator_num {
            self.operator_finds_puppet[i] = self.core_operator_finds[i];

            for j in 0..self.swarm_num {
                self.operator_finds_puppet[i] += self.pilot_operator_finds[j][i];
            }
            operator_finds_sum += self.operator_finds_puppet[i];
        }

        for i in 0..self.operator_num {
            if self.operator_finds_puppet[i] > 0 {
                self.g_best[i] =
                    (self.operator_finds_puppet[i] as f64) / (operator_finds_sum as f64);
            }
        }

        for swarm in 0..self.swarm_num {
            let mut x_sum = 0.0;
            for i in 0..self.operator_num {
                self.probability_now[swarm][i] = 0.0;
                self.v_now[swarm][i] = self.w_now * self.v_now[swarm][i]
                    + self.rand.next_float() * (self.l_best[swarm][i] - self.x_now[swarm][i])
                    + self.rand.next_float() * (self.g_best[i] - self.x_now[swarm][i]);
                self.x_now[swarm][i] += self.v_now[swarm][i];

                self.x_now[swarm][i] = self.x_now[swarm][i].clamp(V_MIN, V_MAX);

                x_sum += self.x_now[swarm][i];
            }

            for i in 0..self.operator_num {
                self.x_now[swarm][i] /= x_sum;
                if i == 0 {
                    self.probability_now[swarm][i] = self.x_now[swarm][i];
                } else {
                    self.probability_now[swarm][i] =
                        self.probability_now[swarm][i - 1] + self.x_now[swarm][i];
                }
            }
            if self.probability_now[swarm][self.operator_num - 1] < 0.99
                || self.probability_now[swarm][self.operator_num - 1] > 1.01
            {
                return Err(Error::illegal_state(
                    "MOpt: Error in pso_update".to_string(),
                ));
            }
        }
        self.swarm_now = 0;

        // After pso_update, go back to pilot-fuzzing module
        Ok(())
    }

    /// This function is used to decide the operator that we want to apply next
    /// see <https://github.com/puppet-meteor/MOpt-AFL/blob/master/MOpt/afl-fuzz.c#L397>
    pub fn select_algorithm(&mut self) -> Result<MutationId, Error> {
        let mut res = 0;
        let mut sentry = 0;

        let operator_num = self.operator_num;

        // Fetch a random sele value
        let select_prob: f64 =
            self.probability_now[self.swarm_now][operator_num - 1] * self.rand.next_float();

        for i in 0..operator_num {
            if i == 0 {
                if select_prob < self.probability_now[self.swarm_now][i] {
                    res = i;
                    break;
                }
            } else if select_prob < self.probability_now[self.swarm_now][i] {
                res = i;
                sentry = 1;
                break;
            }
        }

        if (sentry == 1 && select_prob < self.probability_now[self.swarm_now][res - 1])
            || (res + 1 < operator_num
                && select_prob > self.probability_now[self.swarm_now][res + 1])
        {
            return Err(Error::illegal_state(
                "MOpt: Error in select_algorithm".to_string(),
            ));
        }
        Ok(res.into())
    }
}

const V_MAX: f64 = 1.0;
const V_MIN: f64 = 0.05;

/// The `MOpt` mode to use
#[derive(Serialize, Deserialize, Debug, Copy, Clone)]
pub enum MOptMode {
    /// Pilot fuzzing mode
    Pilotfuzzing,
    /// Core fuzzing mode
    Corefuzzing,
}

/// This is the main struct of `MOpt`, an `AFL` mutator.
/// See the original `MOpt` implementation in <https://github.com/puppet-meteor/MOpt-AFL>
#[derive(Debug)]
pub struct StdMOptMutator<MT> {
    name: Cow<'static, str>,
    mode: MOptMode,
    finds_before: usize,
    mutations: MT,
    max_stack_pow: usize,
}

impl<I, MT, S> Mutator<I, S> for StdMOptMutator<MT>
where
    MT: MutatorsTuple<I, S>,
    S: HasRand + HasMetadata + HasCorpus<I> + HasSolutions<I>,
{
    #[inline]
    fn mutate(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        self.finds_before = state.corpus().count() + state.solutions().count();
        self.scheduled_mutate(state, input)
    }

    #[expect(clippy::cast_precision_loss)]
    fn post_exec(&mut self, state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        let before = self.finds_before;
        let after = state.corpus().count() + state.solutions().count();

        let mopt = state.metadata_map_mut().get_mut::<MOpt>().unwrap();
        let key_module = self.mode;
        match key_module {
            MOptMode::Corefuzzing => {
                mopt.core_time += 1;

                if after > before {
                    let diff = after - before;
                    mopt.total_finds += diff;
                    for i in 0..mopt.operator_num {
                        if mopt.core_operator_cycles_v2[i] > mopt.core_operator_cycles_v3[i] {
                            mopt.core_operator_finds_v2[i] += diff as u64;
                        }
                    }
                }

                if mopt.core_time > mopt.period_core {
                    mopt.core_time = 0;
                    let total_finds = mopt.total_finds;
                    mopt.finds_until_last_swarm = total_finds;
                    for i in 0..mopt.operator_num {
                        mopt.core_operator_finds[i] = mopt.core_operator_finds_v2[i];
                        mopt.core_operator_cycles[i] = mopt.core_operator_cycles_v2[i];
                    }
                    mopt.pso_update()?;
                    self.mode = MOptMode::Pilotfuzzing;
                }
            }
            MOptMode::Pilotfuzzing => {
                mopt.pilot_time += 1;
                let swarm_now = mopt.swarm_now;

                if after > before {
                    let diff = after - before;
                    mopt.total_finds += diff;
                    for i in 0..mopt.operator_num {
                        if mopt.pilot_operator_cycles_v2[swarm_now][i]
                            > mopt.pilot_operator_cycles_v3[swarm_now][i]
                        {
                            mopt.pilot_operator_finds_v2[swarm_now][i] += diff as u64;
                        }
                    }
                }

                if mopt.pilot_time > mopt.period_pilot {
                    let new_finds = mopt.total_finds - mopt.finds_until_last_swarm;
                    let f = (new_finds as f64) / ((mopt.pilot_time as f64) / (PERIOD_PILOT_COEF));
                    mopt.swarm_fitness[swarm_now] = f;
                    mopt.pilot_time = 0;
                    let total_finds = mopt.total_finds;
                    mopt.finds_until_last_swarm = total_finds;

                    for i in 0..mopt.operator_num {
                        let mut eff = 0.0;
                        if mopt.pilot_operator_cycles_v2[swarm_now][i]
                            > mopt.pilot_operator_cycles[swarm_now][i]
                        {
                            eff = ((mopt.pilot_operator_finds_v2[swarm_now][i]
                                - mopt.pilot_operator_finds[swarm_now][i])
                                as f64)
                                / ((mopt.pilot_operator_cycles_v2[swarm_now][i]
                                    - mopt.pilot_operator_cycles[swarm_now][i])
                                    as f64);
                        }

                        if mopt.eff_best[swarm_now][i] < eff {
                            mopt.eff_best[swarm_now][i] = eff;
                            mopt.l_best[swarm_now][i] = mopt.x_now[swarm_now][i];
                        }

                        mopt.pilot_operator_finds[swarm_now][i] =
                            mopt.pilot_operator_finds_v2[swarm_now][i];
                        mopt.pilot_operator_cycles[swarm_now][i] =
                            mopt.pilot_operator_cycles_v2[swarm_now][i];
                    }

                    mopt.swarm_now += 1;

                    if mopt.swarm_num == 1 {
                        // If there's only 1 swarm, then no core_fuzzing mode.
                        mopt.pso_update()?;
                    } else if mopt.swarm_now == mopt.swarm_num {
                        self.mode = MOptMode::Corefuzzing;

                        for i in 0..mopt.operator_num {
                            mopt.core_operator_cycles_v2[i] = mopt.core_operator_cycles[i];
                            mopt.core_operator_cycles_v3[i] = mopt.core_operator_cycles[i];
                            mopt.core_operator_finds_v2[i] = mopt.core_operator_finds[i];
                        }

                        let mut swarm_eff = 0.0;
                        let mut best_swarm = 0;
                        for i in 0..mopt.swarm_num {
                            if mopt.swarm_fitness[i] > swarm_eff {
                                swarm_eff = mopt.swarm_fitness[i];
                                best_swarm = i;
                            }
                        }

                        mopt.swarm_now = best_swarm;
                    }
                }
            }
        }
        Ok(())
    }
}

impl<MT> StdMOptMutator<MT> {
    /// Create a new [`StdMOptMutator`].
    pub fn new<S>(
        state: &mut S,
        mutations: MT,
        max_stack_pow: usize,
        swarm_num: usize,
    ) -> Result<Self, Error>
    where
        S: HasMetadata + HasRand,
        MT: NamedTuple + HasConstLen,
    {
        if !state.has_metadata::<MOpt>() {
            let rand_seed = state.rand_mut().next();
            state.add_metadata::<MOpt>(MOpt::new(MT::LEN, swarm_num, rand_seed)?);
        }

        Ok(Self {
            name: Cow::from(format!("StdMOptMutator[{}]", mutations.names().join(","))),
            mode: MOptMode::Pilotfuzzing,
            finds_before: 0,
            mutations,
            max_stack_pow,
        })
    }
    fn core_mutate<I, S>(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error>
    where
        S: HasMetadata + HasRand + HasSolutions<I> + HasCorpus<I>,
        MT: MutatorsTuple<I, S>,
    {
        let mut r = MutationResult::Skipped;
        let mopt = state.metadata_map_mut().get_mut::<MOpt>().unwrap();
        for i in 0..mopt.operator_num {
            mopt.core_operator_cycles_v3[i] = mopt.core_operator_cycles_v2[i];
        }

        for _i in 0..self.iterations(state, input) {
            let idx = self.schedule(state, input);
            let outcome = self.mutations_mut().get_and_mutate(idx, state, input)?;
            if outcome == MutationResult::Mutated {
                r = MutationResult::Mutated;
            }

            state
                .metadata_map_mut()
                .get_mut::<MOpt>()
                .unwrap()
                .core_operator_cycles_v2[idx.0] += 1;
        }
        Ok(r)
    }

    fn pilot_mutate<I, S>(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error>
    where
        S: HasMetadata + HasRand + HasSolutions<I> + HasCorpus<I>,
        MT: MutatorsTuple<I, S>,
    {
        let mut r = MutationResult::Skipped;
        let swarm_now;
        {
            let mopt = state.metadata_map_mut().get_mut::<MOpt>().unwrap();
            swarm_now = mopt.swarm_now;

            for i in 0..mopt.operator_num {
                mopt.pilot_operator_cycles_v3[swarm_now][i] =
                    mopt.pilot_operator_cycles_v2[swarm_now][i];
            }
        }

        for _i in 0..self.iterations(state, input) {
            let idx = self.schedule(state, input);
            let outcome = self.mutations_mut().get_and_mutate(idx, state, input)?;
            if outcome == MutationResult::Mutated {
                r = MutationResult::Mutated;
            }

            state
                .metadata_map_mut()
                .get_mut::<MOpt>()
                .unwrap()
                .pilot_operator_cycles_v2[swarm_now][idx.0] += 1;
        }

        Ok(r)
    }
}

impl<MT> ComposedByMutations for StdMOptMutator<MT> {
    type Mutations = MT;

    /// Get the mutations
    #[inline]
    fn mutations(&self) -> &MT {
        &self.mutations
    }

    // Get the mutations (mutable)
    #[inline]
    fn mutations_mut(&mut self) -> &mut MT {
        &mut self.mutations
    }
}

impl<MT> Named for StdMOptMutator<MT> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<I, MT, S> ScheduledMutator<I, S> for StdMOptMutator<MT>
where
    MT: MutatorsTuple<I, S>,
    S: HasRand + HasMetadata + HasCorpus<I> + HasSolutions<I>,
{
    /// Compute the number of iterations used to apply stacked mutations
    fn iterations(&self, state: &mut S, _: &I) -> u64 {
        1 << (1 + state.rand_mut().below_or_zero(self.max_stack_pow))
    }

    /// Get the next mutation to apply
    #[inline]
    fn schedule(&self, state: &mut S, _: &I) -> MutationId {
        state
            .metadata_map_mut()
            .get_mut::<MOpt>()
            .unwrap()
            .select_algorithm()
            .unwrap()
    }

    fn scheduled_mutate(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        let mode = self.mode;
        match mode {
            MOptMode::Corefuzzing => self.core_mutate(state, input),
            MOptMode::Pilotfuzzing => self.pilot_mutate(state, input),
        }
    }
}
