//! A restarting event manager is for fuzzers that restart their process.
//! Keeps the fuzzing state alive across restarts using a persistent broker.
use core::{fmt::Debug, sync::atomic::Ordering, time::Duration};

#[cfg(feature = "std")]
use libafl_bolts::core_affinity::CoreId;
#[cfg(all(unix, not(miri)))]
use libafl_bolts::os::unix_signals::setup_signal_handler;
#[cfg(unix)]
use libafl_bolts::os::{ForkResult, fork};
use libafl_bolts::{
    os::{CTRL_C_EXIT, startable_self},
    shmem::ShMemProvider,
    staterestore::StateRestorer,
};
use serde::{Serialize, de::DeserializeOwned};

#[cfg(all(unix, not(miri)))]
use crate::events::EVENTMGR_SIGHANDLER_STATE;
use crate::{
    Error,
    events::{
        AwaitRestartSafe, EventFirer, EventManagerId, EventReceiver, EventRestarter,
        EventWithStats, HasEventManagerId, ProgressReporter, SendExiting,
    },
};

/// The llmp connection from the actual fuzzer to the process supervising it
pub const _ENV_FUZZER_SENDER: &str = "_AFL_ENV_FUZZER_SENDER";

/// A trait for event managers that can be restored from a state restorer
pub trait Restorable<S, SP>
where
    SP: ShMemProvider,
{
    /// The state to be saved
    type RestartState: Serialize + DeserializeOwned;

    /// Called when the event manager is restarted
    /// Returns a tuple: (`should_save_state`, `inner_state`)
    fn on_restart(&mut self, state: &mut S) -> Result<(bool, Self::RestartState), Error>;

    /// Called when the event manager fires an event
    fn on_fire(&mut self, staterestorer: &mut StateRestorer<SP::ShMem, SP>) -> Result<(), Error> {
        let _ = staterestorer;
        Ok(())
    }
}

/// The generic restarting event manager
#[derive(Debug)]
pub struct RestartingEventManager<EM, SP>
where
    SP: ShMemProvider,
{
    /// The actual event manager
    pub inner: EM,
    /// The state restorer
    pub staterestorer: StateRestorer<SP::ShMem, SP>,
}

impl<EM, SP> RestartingEventManager<EM, SP>
where
    SP: ShMemProvider,
{
    /// Creates a new [`RestartingEventManager`]
    pub fn new(inner: EM, staterestorer: StateRestorer<SP::ShMem, SP>) -> Self {
        Self {
            inner,
            staterestorer,
        }
    }
}

impl<EM, I, S, SP> EventFirer<I, S> for RestartingEventManager<EM, SP>
where
    EM: EventFirer<I, S> + Restorable<S, SP>,
    SP: ShMemProvider,
{
    fn should_send(&self) -> bool {
        self.inner.should_send()
    }

    fn fire(&mut self, state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        self.inner.fire(state, event)?;
        self.inner.on_fire(&mut self.staterestorer)?;
        Ok(())
    }
}

impl<EM, S, SP> EventRestarter<S> for RestartingEventManager<EM, SP>
where
    EM: Restorable<S, SP> + AwaitRestartSafe,
    SP: ShMemProvider,
    S: Serialize,
{
    fn on_restart(&mut self, state: &mut S) -> Result<(), Error> {
        let (should_save, inner_state) = self.inner.on_restart(state)?;

        // First, reset the page to 0 so the next iteration can read from the beginning of this page
        self.staterestorer.reset();
        self.staterestorer
            .save(&(if should_save { Some(state) } else { None }, inner_state))?;

        self.inner.await_restart_safe();
        Ok(())
    }
}

impl<EM, I, S, SP> EventReceiver<I, S> for RestartingEventManager<EM, SP>
where
    EM: EventReceiver<I, S>,
    SP: ShMemProvider,
{
    fn try_receive(&mut self, state: &mut S) -> Result<Option<(EventWithStats<I>, bool)>, Error> {
        // log::info!("RestartingEventManager::try_receive inner addr: {:p}", &self.inner);
        self.inner.try_receive(state)
    }

    fn on_interesting(&mut self, state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        self.inner.on_interesting(state, event)
    }
}

impl<EM, S, SP> ProgressReporter<S> for RestartingEventManager<EM, SP>
where
    EM: ProgressReporter<S>,
    SP: ShMemProvider,
{
    fn maybe_report_progress(
        &mut self,
        state: &mut S,
        monitor_timeout: Duration,
    ) -> Result<(), Error> {
        self.inner.maybe_report_progress(state, monitor_timeout)
    }

    fn report_progress(&mut self, state: &mut S) -> Result<(), Error> {
        self.inner.report_progress(state)
    }
}

impl<EM, SP> HasEventManagerId for RestartingEventManager<EM, SP>
where
    EM: HasEventManagerId,
    SP: ShMemProvider,
{
    fn mgr_id(&self) -> EventManagerId {
        self.inner.mgr_id()
    }
}

impl<EM, SP> AwaitRestartSafe for RestartingEventManager<EM, SP>
where
    EM: AwaitRestartSafe,
    SP: ShMemProvider,
{
    fn await_restart_safe(&mut self) {
        self.inner.await_restart_safe();
    }
}

impl<EM, SP> SendExiting for RestartingEventManager<EM, SP>
where
    EM: SendExiting,
    SP: ShMemProvider,
{
    fn send_exiting(&mut self) -> Result<(), Error> {
        self.inner.send_exiting()?;
        self.staterestorer.send_exiting();
        Ok(())
    }

    fn on_shutdown(&mut self) -> Result<(), Error> {
        self.inner.on_shutdown()?;
        self.staterestorer.send_exiting();
        Ok(())
    }
}

/// The manager that handles restarting logic
#[derive(Debug)]
pub struct RestartingMgr<SP> {
    shmem_provider: SP,
    #[cfg(unix)]
    fork: bool,
}

impl<SP> RestartingMgr<SP>
where
    SP: ShMemProvider,
{
    /// Creates a new [`RestartingMgr`]
    pub fn new(shmem_provider: SP) -> Self {
        Self {
            shmem_provider,
            #[cfg(unix)]
            fork: true,
        }
    }

    /// Sets if we should use fork or not (on Unix)
    #[cfg(unix)]
    pub fn fork(&mut self, fork: bool) -> &mut Self {
        self.fork = fork;
        self
    }

    /// Launch the restarting manager
    pub fn launch<F, R>(&mut self, do_in_child: F) -> Result<R, Error>
    where
        F: FnOnce(StateRestorer<SP::ShMem, SP>, SP, Option<CoreId>) -> Result<R, Error>,
    {
        // We start ourselves as child process to actually fuzz
        let (staterestorer, new_shmem_provider, core_id) = if std::env::var(_ENV_FUZZER_SENDER)
            .is_err()
        {
            // First, create a channel from the current fuzzer to the next to store state between restarts.
            let staterestorer: StateRestorer<SP::ShMem, SP> =
                StateRestorer::new(self.shmem_provider.new_shmem(256 * 1024 * 1024)?);

            // Store the information to a map.
            // # Safety
            // Very likely single threaded here.
            unsafe {
                staterestorer.write_to_env(_ENV_FUZZER_SENDER)?;
            }

            let mut ctr: u64 = 0;
            // Client->parent loop
            loop {
                log::info!("Spawning next client (id {ctr})");
                // On Unix, we fork
                #[cfg(unix)]
                let child_status = if self.fork {
                    self.shmem_provider.pre_fork()?;
                    match unsafe { fork() }? {
                        ForkResult::Parent(handle) => {
                            unsafe {
                                libc::signal(libc::SIGINT, libc::SIG_IGN);
                            }
                            self.shmem_provider.post_fork(false)?;
                            handle.status()
                        }
                        ForkResult::Child => {
                            self.shmem_provider.post_fork(true)?;
                            // We need to return the staterestorer from the child
                            // But we don't have it in a variable here (it's in mgr)
                            // Actually we can just break and let the code below handle it
                            // But we need to make sure we don't drop mgr or staterestorer incorrectly
                            break (staterestorer, self.shmem_provider.clone(), None::<CoreId>);
                        }
                    }
                } else {
                    unsafe {
                        libc::signal(libc::SIGINT, libc::SIG_IGN);
                    }
                    {
                        let mut cmd = startable_self()?;
                        let status = cmd.status()?;
                        status.code().unwrap_or_default()
                    }
                };

                #[cfg(not(unix))]
                let child_status = startable_self()?.status()?.code().unwrap_or_default();

                core::sync::atomic::compiler_fence(Ordering::SeqCst);

                if child_status == CTRL_C_EXIT || staterestorer.wants_to_exit() {
                    return Err(Error::shutting_down());
                }

                #[cfg(all(unix, feature = "std", not(miri)))]
                if child_status == libafl_bolts::os::SIGNAL_RECURSION_EXIT {
                    return Err(Error::illegal_state(
                        "The fuzzer crashed inside a crash handler, this is likely a bug in fuzzer or libafl.",
                    ));
                }

                #[expect(clippy::manual_assert)]
                if !staterestorer.has_content() {
                    #[cfg(unix)]
                    if child_status == 9 {
                        panic!(
                            "Target received SIGKILL!. This could indicate the target crashed due to OOM, user sent SIGKILL, or the target was in an unrecoverable situation and could not save state to restart"
                        );
                    }
                    // Storing state in the last round did not work
                    panic!(
                        "Fuzzer-respawner: Storing state in crashed fuzzer instance did not work, no point to spawn the next client! This can happen if the child calls `exit()`, in that case make sure it uses `abort()`, if it got killed unrecoverable (OOM), or if there is a bug in the fuzzer itself. (Child exited with: {child_status})"
                    );
                }

                ctr = ctr.wrapping_add(1);
            }
        } else {
            // We are the newly started fuzzing instance (i.e. on Windows), first, connect to our own restore map.
            // We get here *only on Windows*, if we were started by a restarting fuzzer.
            // A staterestorer and a receiver for single communication
            (
                StateRestorer::from_env(&mut self.shmem_provider, _ENV_FUZZER_SENDER)?,
                self.shmem_provider.clone(),
                None,
            )
        };

        // At this point we are the fuzzer *NOT* the restarter.
        // We setup signal handlers to clean up shmem segments used by state restorer
        #[cfg(all(unix, not(miri)))]
        if let Err(_e) = unsafe { setup_signal_handler(&raw mut EVENTMGR_SIGHANDLER_STATE) } {
            // We can live without a proper ctrl+c signal handler. Print and ignore.
            log::error!("Failed to setup signal handlers: {_e}");
        }

        do_in_child(staterestorer, new_shmem_provider, core_id)
    }
}

/// Sets up a restarting fuzzer, using the [`ShMemProvider`], and standard features.
///
/// The [`RestartingEventManager`] is a combination of restarter and runner, that can be used on systems with and without `fork` support.
/// The restarter will spawn a new process each time the child crashes or timeouts.
pub fn setup_generic_restarting_mgr<EM, F, I, S, SP>(
    mut restarting_mgr: RestartingMgr<SP>,
    mgr_constructor: F,
) -> Result<(Option<S>, RestartingEventManager<EM, SP>), Error>
where
    EM: Restorable<S, SP> + EventFirer<I, S> + EventReceiver<I, S> + ProgressReporter<S>,
    F: FnOnce(Option<EM::RestartState>) -> Result<EM, Error>,
    S: Serialize + DeserializeOwned,
    SP: ShMemProvider,
{
    restarting_mgr.launch(
        |mut staterestorer: StateRestorer<SP::ShMem, SP>, _new_shmem_provider, _core_id| {
            // If we're restarting, deserialize the old state.
            let (state, mgr) = match staterestorer.restore::<(Option<S>, EM::RestartState)>()? {
                None => {
                    log::info!("First run. Let's set it all up");
                    // Mgr to send and receive msgs from/to all other fuzzer instances
                    match mgr_constructor(None) {
                        Ok(mgr) => (None::<S>, mgr),
                        Err(Error::ShuttingDown) => {
                            staterestorer.send_exiting();
                            return Err(Error::shutting_down());
                        }
                        Err(e) => return Err(e),
                    }
                }
                // Restoring from a previous run, deserialize state and corpus.
                Some((state, inner_state)) => {
                    log::info!("Subsequent run. Loaded previous state.");
                    // We reset the staterestorer, the next staterestorer and receiver (after crash) will reuse the page from the initial message.

                    match mgr_constructor(Some(inner_state)) {
                        Ok(mgr) => (state, mgr),
                        Err(Error::ShuttingDown) => {
                            staterestorer.send_exiting();
                            return Err(Error::shutting_down());
                        }
                        Err(e) => return Err(e),
                    }
                }
            };

            staterestorer.reset();

            Ok((state, RestartingEventManager::new(mgr, staterestorer)))
        },
    )
}
