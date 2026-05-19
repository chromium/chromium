//! Executor for differential fuzzing.
//!
//! It wraps two executors that will be run after each other with the same input.
//! In comparison to the [`crate::executors::CombinedExecutor`] it also runs the secondary executor in `run_target`.
use core::{
    cell::UnsafeCell,
    fmt::Debug,
    marker::PhantomData,
    ops::{Deref, DerefMut},
    ptr,
};

use libafl_bolts::{
    ownedref::OwnedMutPtr,
    tuples::{MatchName, RefIndexable},
};
use serde::{Deserialize, Serialize};

use super::HasTimeout;
use crate::{
    Error,
    executors::{Executor, ExitKind, HasObservers, SetTimeout},
    observers::{DifferentialObserversTuple, ObserversTuple},
};

/// A [`DiffExecutor`] wraps a primary executor, forwarding its methods, and a secondary one
#[derive(Debug)]
pub struct DiffExecutor<A, B, DOT, I, OTA, OTB, S> {
    primary: A,
    secondary: B,
    observers: UnsafeCell<ProxyObserversTuple<OTA, OTB, DOT>>,
    phantom: PhantomData<(I, S)>,
}

impl<A, B, DOT, I, OTA, OTB, S> DiffExecutor<A, B, DOT, I, OTA, OTB, S> {
    /// Create a new `DiffExecutor`, wrapping the given `executor`s.
    pub fn new(primary: A, secondary: B, observers: DOT) -> Self {
        Self {
            primary,
            secondary,
            observers: UnsafeCell::new(ProxyObserversTuple {
                primary: OwnedMutPtr::Ptr(ptr::null_mut()),
                secondary: OwnedMutPtr::Ptr(ptr::null_mut()),
                differential: observers,
            }),
            phantom: PhantomData,
        }
    }

    /// Retrieve the primary `Executor` that is wrapped by this `DiffExecutor`.
    pub fn primary(&mut self) -> &mut A {
        &mut self.primary
    }

    /// Retrieve the secondary `Executor` that is wrapped by this `DiffExecutor`.
    pub fn secondary(&mut self) -> &mut B {
        &mut self.secondary
    }
}

impl<A, B, DOT, EM, I, S, Z> Executor<EM, I, S, Z>
    for DiffExecutor<A, B, DOT, I, A::Observers, B::Observers, S>
where
    A: Executor<EM, I, S, Z> + HasObservers,
    B: Executor<EM, I, S, Z> + HasObservers,
    <A as HasObservers>::Observers: ObserversTuple<I, S>,
    <B as HasObservers>::Observers: ObserversTuple<I, S>,
    DOT: DifferentialObserversTuple<A::Observers, B::Observers, I, S> + MatchName,
{
    fn run_target(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error> {
        self.observers(); // update in advance
        let observers = self.observers.get_mut();
        observers
            .differential
            .pre_observe_first_all(observers.primary.as_mut())?;
        observers.primary.as_mut().pre_exec_all(state, input)?;
        let ret1 = self.primary.run_target(fuzzer, state, mgr, input)?;
        observers
            .primary
            .as_mut()
            .post_exec_all(state, input, &ret1)?;
        observers
            .differential
            .post_observe_first_all(observers.primary.as_mut())?;
        observers
            .differential
            .pre_observe_second_all(observers.secondary.as_mut())?;
        observers.secondary.as_mut().pre_exec_all(state, input)?;
        let ret2 = self.secondary.run_target(fuzzer, state, mgr, input)?;
        observers
            .secondary
            .as_mut()
            .post_exec_all(state, input, &ret2)?;
        observers
            .differential
            .post_observe_second_all(observers.secondary.as_mut())?;
        if ret1 == ret2 {
            Ok(ret1)
        } else {
            // We found a diff in the exit codes!
            Ok(ExitKind::Diff {
                primary: ret1.into(),
                secondary: ret2.into(),
            })
        }
    }
}

impl<A, B, DOT, I, OTA, OTB, S> HasTimeout for DiffExecutor<A, B, DOT, I, OTA, OTB, S>
where
    A: HasTimeout,
    B: HasTimeout,
{
    #[inline]
    fn timeout(&self) -> core::time::Duration {
        assert!(
            self.primary.timeout() == self.secondary.timeout(),
            "Primary and Secondary Executors have different timeouts!"
        );
        self.primary.timeout()
    }
}

impl<A, B, DOT, I, OTA, OTB, S> SetTimeout for DiffExecutor<A, B, DOT, I, OTA, OTB, S>
where
    A: SetTimeout,
    B: SetTimeout,
{
    #[inline]
    fn set_timeout(&mut self, timeout: core::time::Duration) {
        self.primary.set_timeout(timeout);
        self.secondary.set_timeout(timeout);
    }
}

/// Proxy the observers of the inner executors
#[derive(Serialize, Deserialize, Debug)]
#[serde(
    bound = "A: serde::Serialize + serde::de::DeserializeOwned, B: serde::Serialize + serde::de::DeserializeOwned, DOT: serde::Serialize + serde::de::DeserializeOwned"
)]
pub struct ProxyObserversTuple<A, B, DOT> {
    primary: OwnedMutPtr<A>,
    secondary: OwnedMutPtr<B>,
    differential: DOT,
}

impl<A, B, DOT, I, S> ObserversTuple<I, S> for ProxyObserversTuple<A, B, DOT>
where
    A: MatchName,
    B: MatchName,
    DOT: DifferentialObserversTuple<A, B, I, S> + MatchName,
{
    fn pre_exec_all(&mut self, state: &mut S, input: &I) -> Result<(), Error> {
        self.differential.pre_exec_all(state, input)
    }

    fn post_exec_all(
        &mut self,
        state: &mut S,
        input: &I,
        exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        self.differential.post_exec_all(state, input, exit_kind)
    }

    fn pre_exec_child_all(&mut self, state: &mut S, input: &I) -> Result<(), Error> {
        self.differential.pre_exec_child_all(state, input)
    }

    fn post_exec_child_all(
        &mut self,
        state: &mut S,
        input: &I,
        exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        self.differential
            .post_exec_child_all(state, input, exit_kind)
    }
}

impl<A, B, DOT> Deref for ProxyObserversTuple<A, B, DOT> {
    type Target = DOT;

    fn deref(&self) -> &Self::Target {
        &self.differential
    }
}

impl<A, B, DOT> DerefMut for ProxyObserversTuple<A, B, DOT> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.differential
    }
}

impl<A, B, DOT> MatchName for ProxyObserversTuple<A, B, DOT>
where
    A: MatchName,
    B: MatchName,
    DOT: MatchName,
{
    #[expect(deprecated)]
    fn match_name<T>(&self, name: &str) -> Option<&T> {
        match self.primary.as_ref().match_name::<T>(name) {
            Some(t) => Some(t),
            _ => match self.secondary.as_ref().match_name::<T>(name) {
                Some(t) => Some(t),
                _ => self.differential.match_name::<T>(name),
            },
        }
    }

    #[expect(deprecated)]
    fn match_name_mut<T>(&mut self, name: &str) -> Option<&mut T> {
        match self.primary.as_mut().match_name_mut::<T>(name) {
            Some(t) => Some(t),
            _ => match self.secondary.as_mut().match_name_mut::<T>(name) {
                Some(t) => Some(t),
                _ => self.differential.match_name_mut::<T>(name),
            },
        }
    }
}

impl<A, B, DOT> ProxyObserversTuple<A, B, DOT> {
    fn set(&mut self, primary: &A, secondary: &B) {
        self.primary = OwnedMutPtr::Ptr(ptr::from_ref(primary).cast_mut());
        self.secondary = OwnedMutPtr::Ptr(ptr::from_ref(secondary).cast_mut());
    }
}

impl<A, B, DOT, I, OTA, OTB, S> HasObservers for DiffExecutor<A, B, DOT, I, OTA, OTB, S>
where
    A: HasObservers<Observers = OTA>,
    B: HasObservers<Observers = OTB>,
    DOT: DifferentialObserversTuple<OTA, OTB, I, S> + MatchName,
    OTA: ObserversTuple<I, S>,
    OTB: ObserversTuple<I, S>,
{
    type Observers = ProxyObserversTuple<OTA, OTB, DOT>;

    #[inline]
    fn observers(&self) -> RefIndexable<&Self::Observers, Self::Observers> {
        unsafe {
            self.observers
                .get()
                .as_mut()
                .unwrap()
                .set(&*self.primary.observers(), &*self.secondary.observers());
            RefIndexable::from(self.observers.get().as_ref().unwrap())
        }
    }

    #[inline]
    fn observers_mut(&mut self) -> RefIndexable<&mut Self::Observers, Self::Observers> {
        unsafe {
            self.observers.get().as_mut().unwrap().set(
                &*self.primary.observers_mut(),
                &*self.secondary.observers_mut(),
            );
            RefIndexable::from(self.observers.get().as_mut().unwrap())
        }
    }
}
