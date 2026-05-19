//! Implementation for "SAND: Decoupling Sanitization from Fuzzing for Low Overhead"
//! Reference Implementation: <https://github.com/wtdcode/sand-aflpp>
//! Detailed docs: <https://github.com/AFLplusplus/AFLplusplus/blob/stable/docs/SAND.md>
//! Maintainer: Ziqiao Kong (<https://github.com/wtdcode>)
//! Preprint: <https://arxiv.org/abs/2402.16497> accepted by ICSE'25

use alloc::vec::Vec;
use core::marker::PhantomData;

use libafl_bolts::{
    AsIter, Error, Named, hash_std,
    simd::std_simplify_map,
    tuples::{Handle, MatchName, MatchNameRef},
};

use super::{Executor, ExecutorsTuple, ExitKind, HasObservers, HasTimeout};
use crate::{
    HasNamedMetadata,
    executors::SetTimeout,
    observers::{MapObserver, classify_counts},
};

/// The execution pattern of the [`SANDExecutor`]. The default value used in our paper is
/// [`SANDExecutionPattern::SimplifiedTrace`] and we by design don't include coverage
/// increasing pattern here as it will miss at least 25% bugs and easy enough to implement
/// by iterating the crash corpus.
#[derive(Debug, Clone, Default, Copy)]
pub enum SANDExecutionPattern {
    /// The simplified trace, captures ~92% bug triggering inputs with ~20% overhead
    /// on overage (less than 5% overhead on most targets during evaluation)
    #[default]
    SimplifiedTrace,
    /// The unique trace, captures ~99.9% bug-triggering inputs with more than >50% overhead.
    /// Only use this pattern if you are really scared of missing any bugs =).
    UniqueTrace,
    /// The unclassified unique trace, captures even more bug-triggering inputs compared to
    /// unique trace. Not discussed in the paper but internally evaluated. Not adopted because
    /// incurring tooooo much overhead
    UnclassifiedTrace,
}

/// The core executor implementation. It wraps another executor and a list of extra executors.
/// Please refer to [SAND.md](https://github.com/AFLplusplus/AFLplusplus/blob/stable/docs/SAND.md) for
/// how to build `sand_executors`.
#[derive(Debug, Clone)]
pub struct SANDExecutor<E, ET, C, O> {
    executor: E,
    sand_executors: ET,
    bitmap: Vec<u8>,
    ob_ref: Handle<C>,
    pattern: SANDExecutionPattern,
    ph: PhantomData<O>,
}

impl<E, ET, C, O> SANDExecutor<E, ET, C, O>
where
    C: Named,
{
    fn bitmap_set(&mut self, idx: usize) {
        let bidx = idx % 8;
        let idx = (idx / 8) % self.bitmap.len();
        *self.bitmap.get_mut(idx).unwrap() |= 1u8 << bidx;
    }

    fn bitmap_read(&mut self, idx: usize) -> u8 {
        let bidx = idx % 8;
        let idx = (idx / 8) % self.bitmap.len();
        (self.bitmap[idx] >> bidx) & 1
    }

    /// Create a new [`SANDExecutor`], the observer handle is supposed to be _raw_ edge observer.
    pub fn new(
        executor: E,
        sand_extra_executors: ET,
        observer_handle: Handle<C>,
        bitmap_size: usize,
        pattern: SANDExecutionPattern,
    ) -> Self {
        Self {
            executor,
            sand_executors: sand_extra_executors,
            bitmap: vec![0; bitmap_size],
            ob_ref: observer_handle,
            pattern,
            ph: PhantomData,
        }
    }

    /// Create a new [`SANDExecutor`] using paper setup, the observer handle is supposed to be
    /// _raw_ edge observer.
    pub fn new_paper(executor: E, sand_extra_executors: ET, observer_handle: Handle<C>) -> Self {
        Self::new(
            executor,
            sand_extra_executors,
            observer_handle,
            1 << 29,
            SANDExecutionPattern::SimplifiedTrace,
        )
    }
}

impl<E, ET, C, O> HasTimeout for SANDExecutor<E, ET, C, O>
where
    E: HasTimeout,
{
    fn timeout(&self) -> core::time::Duration {
        self.executor.timeout()
    }
}

impl<E, ET, C, O> SetTimeout for SANDExecutor<E, ET, C, O>
where
    E: SetTimeout,
{
    fn set_timeout(&mut self, timeout: core::time::Duration) {
        self.executor.set_timeout(timeout);
    }
}

impl<E, ET, C, O> HasObservers for SANDExecutor<E, ET, C, O>
where
    E: HasObservers,
{
    type Observers = E::Observers;
    fn observers(&self) -> libafl_bolts::tuples::RefIndexable<&Self::Observers, Self::Observers> {
        self.executor.observers()
    }

    fn observers_mut(
        &mut self,
    ) -> libafl_bolts::tuples::RefIndexable<&mut Self::Observers, Self::Observers> {
        self.executor.observers_mut()
    }
}

impl<E, ET, C, O, EM, I, S, Z, OT> Executor<EM, I, S, Z> for SANDExecutor<E, ET, C, O>
where
    ET: ExecutorsTuple<EM, I, S, Z>,
    E: Executor<EM, I, S, Z> + HasObservers<Observers = OT>,
    OT: MatchName,
    O: MapObserver<Entry = u8> + for<'it> AsIter<'it, Item = u8>,
    C: AsRef<O> + Named,
    S: HasNamedMetadata,
{
    fn run_target(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error> {
        let kind = self.executor.run_target(fuzzer, state, mgr, input)?;
        let ot = self.executor.observers();
        let ob = ot.get(&self.ob_ref).unwrap().as_ref();
        let mut covs = ob.to_vec();
        match self.pattern {
            SANDExecutionPattern::SimplifiedTrace => {
                std_simplify_map(&mut covs);
            }
            SANDExecutionPattern::UniqueTrace => {
                classify_counts(covs.as_mut_slice());
            }
            SANDExecutionPattern::UnclassifiedTrace => {}
        }

        // Our paper uses xxh32 but it shouldn't have significant collision for most hashing algorithms.
        let pattern_hash = hash_std(&covs) as usize;

        let ret = if kind == ExitKind::Ok {
            if self.bitmap_read(pattern_hash) == 0 {
                let sand_kind = self
                    .sand_executors
                    .run_target_all(fuzzer, state, mgr, input)?;
                if sand_kind == ExitKind::Crash {
                    Ok(sand_kind)
                } else {
                    Ok(kind)
                }
            } else {
                Ok(kind)
            }
        } else {
            Ok(kind)
        };

        self.bitmap_set(pattern_hash);
        ret
    }
}
