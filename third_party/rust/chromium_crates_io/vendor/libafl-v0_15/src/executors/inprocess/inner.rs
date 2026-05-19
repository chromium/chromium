use core::{
    ffi::c_void,
    fmt::{self, Debug, Formatter},
    marker::PhantomData,
    ptr::{self, null, write_volatile},
    sync::atomic::{Ordering, compiler_fence},
    time::Duration,
};

use libafl_bolts::tuples::{Merge, RefIndexable, tuple_list};
#[cfg(windows)]
use windows::Win32::System::Threading::SetThreadStackGuarantee;

#[cfg(all(windows, feature = "std"))]
use crate::executors::hooks::inprocess::HasTimeout;
use crate::{
    Error,
    events::{EventFirer, EventRestarter},
    executors::{
        Executor, HasObservers,
        hooks::{
            ExecutorHooksTuple,
            inprocess::{GLOBAL_STATE, InProcessHooks},
        },
        inprocess::HasInProcessHooks,
    },
    feedbacks::Feedback,
    fuzzer::HasObjective,
    inputs::Input,
    observers::ObserversTuple,
    state::{HasCurrentTestcase, HasExecutions, HasSolutions},
};

/// The internal state of `GenericInProcessExecutor`.
pub struct GenericInProcessExecutorInner<EM, HT, I, OT, S, Z> {
    /// The observers, observing each run
    pub(super) observers: OT,
    /// Crash and timeout hooks
    pub(super) hooks: (InProcessHooks<I, S>, HT),
    /// `EM` and `Z` need to be tracked here to remain stable,
    /// else we can run into type confusions between [`Self::enter_target`] and [`Self::leave_target`].
    phantom: PhantomData<(EM, Z)>,
}

impl<EM, HT, I, OT, S, Z> Debug for GenericInProcessExecutorInner<EM, HT, I, OT, S, Z>
where
    OT: Debug,
{
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("GenericInProcessExecutorState")
            .field("observers", &self.observers)
            .finish_non_exhaustive()
    }
}

impl<EM, HT, I, OT, S, Z> HasObservers for GenericInProcessExecutorInner<EM, HT, I, OT, S, Z> {
    type Observers = OT;

    #[inline]
    fn observers(&self) -> RefIndexable<&Self::Observers, Self::Observers> {
        RefIndexable::from(&self.observers)
    }

    #[inline]
    fn observers_mut(&mut self) -> RefIndexable<&mut Self::Observers, Self::Observers> {
        RefIndexable::from(&mut self.observers)
    }
}

impl<EM, HT, I, OT, S, Z> GenericInProcessExecutorInner<EM, HT, I, OT, S, Z>
where
    OT: ObserversTuple<I, S>,
{
    /// This function marks the boundary between the fuzzer and the target
    ///
    /// # Safety
    /// This function sets a bunch of raw pointers in global variables, reused in other parts of
    /// the code.
    // TODO: Remove EM and Z from function bound and add it to struct instead to avoid possible type confusion
    #[inline]
    pub unsafe fn enter_target(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
        executor_ptr: *const c_void,
    ) {
        // # Safety
        // This writes pointers to global state. Only unsafe if the state is they are accessed incorrectly.
        unsafe {
            let data = &raw mut GLOBAL_STATE;
            write_volatile(
                &raw mut (*data).current_input_ptr,
                ptr::from_ref(input) as *const c_void,
            );
            write_volatile(&raw mut (*data).executor_ptr, executor_ptr);
            // Direct raw pointers access /aliasing is pretty undefined behavior.
            // Since the state and event may have moved in memory, refresh them right before the signal may happen
            write_volatile(
                &raw mut ((*data).state_ptr),
                ptr::from_mut(state) as *mut c_void,
            );
            write_volatile(
                &raw mut (*data).event_mgr_ptr,
                ptr::from_mut(mgr) as *mut c_void,
            );
            write_volatile(
                &raw mut (*data).fuzzer_ptr,
                ptr::from_mut(fuzzer) as *mut c_void,
            );
            compiler_fence(Ordering::SeqCst);
        }
    }

    /// This function marks the boundary between the fuzzer and the target
    #[inline]
    pub fn leave_target(&mut self, _fuzzer: &mut Z, _state: &mut S, _mgr: &mut EM, _input: &I) {
        // # Safety
        // We set the global pointer to null, no direct safety concerns arise.
        unsafe {
            let data = &raw mut GLOBAL_STATE;

            write_volatile(&raw mut (*data).current_input_ptr, null());
            compiler_fence(Ordering::SeqCst);
        }
    }
}

impl<EM, HT, I, OT, S, Z> GenericInProcessExecutorInner<EM, HT, I, OT, S, Z>
where
    HT: ExecutorHooksTuple<I, S>,
    OT: ObserversTuple<I, S>,
    S: HasExecutions + HasSolutions<I>,
{
    /// Create a new in mem executor with the default timeout (5 sec)
    pub fn generic<E, OF>(
        user_hooks: HT,
        observers: OT,
        fuzzer: &mut Z,
        state: &mut S,
        event_mgr: &mut EM,
    ) -> Result<Self, Error>
    where
        E: Executor<EM, I, S, Z> + HasObservers + HasInProcessHooks<I, S>,
        E::Observers: ObserversTuple<I, S>,
        EM: EventFirer<I, S> + EventRestarter<S>,
        I: Input + Clone,
        OF: Feedback<EM, I, E::Observers, S>,
        S: HasCurrentTestcase<I> + HasSolutions<I>,
        Z: HasObjective<Objective = OF>,
    {
        Self::with_timeout_generic::<E, OF>(
            user_hooks,
            observers,
            fuzzer,
            state,
            event_mgr,
            Duration::from_millis(5000),
        )
    }

    /// Create a new in mem executor.
    /// Caution: crash and restart in one of them will lead to odd behavior if multiple are used,
    /// depending on different corpus or state.
    /// * `user_hooks` - the hooks run before and after the harness's execution
    /// * `harness_fn` - the harness, executing the function
    /// * `observers` - the observers observing the target during execution
    ///
    /// This may return an error on unix, if signal handler setup fails
    pub fn with_timeout_generic<E, OF>(
        user_hooks: HT,
        observers: OT,
        _fuzzer: &mut Z,
        state: &mut S,
        _event_mgr: &mut EM,
        timeout: Duration,
    ) -> Result<Self, Error>
    where
        E: Executor<EM, I, S, Z> + HasObservers + HasInProcessHooks<I, S>,
        E::Observers: ObserversTuple<I, S>,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, E::Observers, S>,
        S: HasCurrentTestcase<I> + HasSolutions<I>,
        Z: HasObjective<Objective = OF>,
        I: Input + Clone,
    {
        let default = InProcessHooks::new::<E, EM, OF, Z>(timeout)?;
        let mut hooks = tuple_list!(default).merge(user_hooks);
        hooks.init_all(state);

        #[cfg(windows)]
        // Some initialization necessary for windows.
        unsafe {
            /*
                See https://github.com/AFLplusplus/LibAFL/pull/403
                This one reserves certain amount of memory for the stack.
                If stack overflow happens during fuzzing on windows, the program is transferred to our exception handler for windows.
                However, if we run out of the stack memory again in this exception handler, we'll crash with STATUS_ACCESS_VIOLATION.
                We need this API call because with the llmp_compression
                feature enabled, the exception handler uses a lot of stack memory (in the compression lib code) on release build.
                As far as I have observed, the compression uses around 0x10000 bytes, but for safety let's just reserve 0x20000 bytes for our exception handlers.
                This number 0x20000 could vary depending on the compilers optimization for future compression library changes.
            */
            let mut stack_reserved = 0x20000;
            SetThreadStackGuarantee(&raw mut stack_reserved)?;
        }

        #[cfg(all(feature = "std", windows))]
        {
            // set timeout for the handler
            *hooks.0.millis_sec_mut() = timeout.as_millis() as i64;
        }

        Ok(Self {
            observers,
            hooks,
            phantom: PhantomData,
        })
    }

    /// The inprocess handlers
    #[inline]
    pub fn hooks(&self) -> &(InProcessHooks<I, S>, HT) {
        &self.hooks
    }

    /// The inprocess handlers (mutable)
    #[inline]
    pub fn hooks_mut(&mut self) -> &mut (InProcessHooks<I, S>, HT) {
        &mut self.hooks
    }
}

impl<EM, HT, I, OT, S, Z> HasInProcessHooks<I, S>
    for GenericInProcessExecutorInner<EM, HT, I, OT, S, Z>
{
    /// the timeout handler
    #[inline]
    fn inprocess_hooks(&self) -> &InProcessHooks<I, S> {
        &self.hooks.0
    }

    /// the timeout handler
    #[inline]
    fn inprocess_hooks_mut(&mut self) -> &mut InProcessHooks<I, S> {
        &mut self.hooks.0
    }
}
