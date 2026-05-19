use core::{
    ffi::c_void,
    fmt::{self, Debug, Formatter},
    marker::PhantomData,
    ptr::{self, null_mut, write_volatile},
    sync::atomic::{Ordering, compiler_fence},
    time::Duration,
};

use libafl_bolts::{
    os::unix_signals::Signal,
    shmem::ShMemProvider,
    tuples::{Merge, RefIndexable, tuple_list},
};
use nix::{
    sys::wait::{WaitStatus, waitpid},
    unistd::Pid,
};

#[cfg(all(unix, not(target_os = "linux")))]
use crate::executors::hooks::timer::{ITIMER_REAL, Itimerval, Timeval, setitimer};
use crate::{
    Error,
    executors::{
        ExitKind, HasObservers,
        hooks::{
            ExecutorHooksTuple,
            inprocess_fork::{FORK_EXECUTOR_GLOBAL_DATA, InChildProcessHooks},
        },
    },
    observers::ObserversTuple,
};

/// Inner state of GenericInProcessExecutor-like structures.
pub struct GenericInProcessForkExecutorInner<EM, HT, I, OT, S, SP, Z> {
    pub(super) hooks: (InChildProcessHooks<I, S>, HT),
    pub(super) shmem_provider: SP,
    pub(super) observers: OT,
    #[cfg(target_os = "linux")]
    pub(super) itimerspec: libc::itimerspec,
    #[cfg(all(unix, not(target_os = "linux")))]
    pub(super) itimerval: Itimerval,
    pub(super) phantom: PhantomData<(EM, I, S, Z)>,
}

impl<EM, HT, I, OT, S, SP, Z> Debug for GenericInProcessForkExecutorInner<EM, HT, I, OT, S, SP, Z>
where
    HT: Debug,
    OT: Debug,
    SP: Debug,
{
    #[cfg(target_os = "linux")]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("GenericInProcessForkExecutorInner")
            .field("observers", &self.observers)
            .field("shmem_provider", &self.shmem_provider)
            .field("itimerspec", &self.itimerspec)
            .finish_non_exhaustive()
    }

    #[cfg(not(target_os = "linux"))]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        #[cfg(not(target_os = "linux"))]
        return f
            .debug_struct("GenericInProcessForkExecutorInner")
            .field("observers", &self.observers)
            .field("shmem_provider", &self.shmem_provider)
            .field("itimerval", &self.itimerval)
            .finish_non_exhaustive();
    }
}

#[cfg(target_os = "linux")]
fn parse_itimerspec(timeout: Duration) -> libc::itimerspec {
    let milli_sec = timeout.as_millis();
    let it_value = libc::timespec {
        tv_sec: (milli_sec / 1000) as _,
        tv_nsec: ((milli_sec % 1000) * 1000 * 1000) as _,
    };
    let it_interval = libc::timespec {
        tv_sec: 0,
        tv_nsec: 0,
    };
    libc::itimerspec {
        it_interval,
        it_value,
    }
}

#[cfg(not(target_os = "linux"))]
fn parse_itimerval(timeout: Duration) -> Itimerval {
    let milli_sec = timeout.as_millis();
    let it_value = Timeval {
        tv_sec: (milli_sec / 1000) as i64,
        tv_usec: (milli_sec % 1000) as i64,
    };
    let it_interval = Timeval {
        tv_sec: 0,
        tv_usec: 0,
    };
    Itimerval {
        it_interval,
        it_value,
    }
}

impl<EM, HT, I, OT, S, SP, Z> GenericInProcessForkExecutorInner<EM, HT, I, OT, S, SP, Z>
where
    HT: ExecutorHooksTuple<I, S>,
    OT: ObserversTuple<I, S>,
    SP: ShMemProvider,
{
    pub(super) unsafe fn pre_run_target_child(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<(), Error> {
        unsafe {
            self.shmem_provider.post_fork(true)?;

            self.enter_target(fuzzer, state, mgr, input);
            self.hooks.pre_exec_all(state, input);

            self.observers
                .pre_exec_child_all(state, input)
                .expect("Failed to run post_exec on observers");

            #[cfg(target_os = "linux")]
            {
                let mut timerid: libc::timer_t = null_mut();
                // creates a new per-process interval timer
                // we can't do this from the parent, timerid is unique to each process.
                libc::timer_create(libc::CLOCK_MONOTONIC, null_mut(), &raw mut timerid);

                // log::info!("Set timer! {:#?} {timerid:#?}", self.itimerspec);
                let _: i32 = libc::timer_settime(timerid, 0, &raw mut self.itimerspec, null_mut());
            }
            #[cfg(not(target_os = "linux"))]
            {
                setitimer(ITIMER_REAL, &raw mut self.itimerval, null_mut());
            }
            // log::trace!("{v:#?} {}", nix::errno::errno());

            Ok(())
        }
    }

    pub(super) unsafe fn post_run_target_child(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) {
        unsafe {
            self.observers
                .post_exec_child_all(state, input, &ExitKind::Ok)
                .expect("Failed to run post_exec on observers");

            self.hooks.post_exec_all(state, input);
            self.leave_target(fuzzer, state, mgr, input);

            libc::_exit(0);
        }
    }

    pub(super) fn parent(&mut self, child: Pid) -> Result<ExitKind, Error> {
        // log::trace!("from parent {} child is {}", std::process::id(), child);
        self.shmem_provider.post_fork(false)?;

        let res = waitpid(child, None)?;
        log::trace!("{res:#?}");
        match res {
            WaitStatus::Signaled(_, signal, _) => match signal {
                nix::sys::signal::Signal::SIGALRM | nix::sys::signal::Signal::SIGUSR2 => {
                    Ok(ExitKind::Timeout)
                }
                _ => Ok(ExitKind::Crash),
            },
            WaitStatus::Exited(_, code) => {
                if code > 128 && code < 160 {
                    // Signal exit codes
                    let signal = code - 128;
                    if signal == Signal::SigAlarm as libc::c_int
                        || signal == Signal::SigUser2 as libc::c_int
                    {
                        Ok(ExitKind::Timeout)
                    } else {
                        Ok(ExitKind::Crash)
                    }
                } else {
                    Ok(ExitKind::Ok)
                }
            }
            _ => panic!("Unexpected waitpid exit: {res:?}"),
        }
    }
}

impl<EM, HT, I, OT, S, SP, Z> GenericInProcessForkExecutorInner<EM, HT, I, OT, S, SP, Z>
where
    HT: ExecutorHooksTuple<I, S>,
    OT: ObserversTuple<I, S>,
{
    #[inline]
    /// This function marks the boundary between the fuzzer and the target.
    pub fn enter_target(&mut self, _fuzzer: &mut Z, state: &mut S, _event_mgr: &mut EM, input: &I) {
        unsafe {
            let data = &raw mut FORK_EXECUTOR_GLOBAL_DATA;
            write_volatile(
                &raw mut (*data).executor_ptr,
                ptr::from_ref(self) as *const c_void,
            );
            write_volatile(
                &raw mut (*data).current_input_ptr,
                ptr::from_ref(input) as *const c_void,
            );
            write_volatile(
                &raw mut ((*data).state_ptr),
                ptr::from_mut(state) as *mut c_void,
            );
            compiler_fence(Ordering::SeqCst);
        }
    }

    #[inline]
    /// This function marks the boundary between the fuzzer and the target.
    pub fn leave_target(
        &mut self,
        _fuzzer: &mut Z,
        _state: &mut S,
        _event_mgr: &mut EM,
        _input: &I,
    ) {
        // do nothing
    }

    /// Creates a new [`GenericInProcessForkExecutorInner`] with custom hooks
    #[cfg(target_os = "linux")]
    pub fn with_hooks(
        userhooks: HT,
        observers: OT,
        _fuzzer: &mut Z,
        state: &mut S,
        _event_mgr: &mut EM,
        timeout: Duration,
        shmem_provider: SP,
    ) -> Result<Self, Error> {
        let default_hooks = InChildProcessHooks::new::<Self>()?;
        let mut hooks = tuple_list!(default_hooks).merge(userhooks);
        hooks.init_all(state);
        let itimerspec = parse_itimerspec(timeout);
        Ok(Self {
            shmem_provider,
            observers,
            hooks,
            itimerspec,
            phantom: PhantomData,
        })
    }

    /// Creates a new [`GenericInProcessForkExecutorInner`], non linux
    #[cfg(not(target_os = "linux"))]
    pub fn with_hooks(
        userhooks: HT,
        observers: OT,
        _fuzzer: &mut Z,
        state: &mut S,
        _event_mgr: &mut EM,
        timeout: Duration,
        shmem_provider: SP,
    ) -> Result<Self, Error> {
        let default_hooks = InChildProcessHooks::new::<Self>()?;
        let mut hooks = tuple_list!(default_hooks).merge(userhooks);
        hooks.init_all(state);

        let itimerval = parse_itimerval(timeout);

        Ok(Self {
            shmem_provider,
            observers,
            hooks,
            itimerval,
            phantom: PhantomData,
        })
    }
}

impl<EM, HT, I, OT, S, SP, Z> HasObservers
    for GenericInProcessForkExecutorInner<EM, HT, I, OT, S, SP, Z>
{
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
