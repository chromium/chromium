//! A very simple event manager, that just supports log outputs, but no multiprocessing

use alloc::vec::Vec;
use core::{fmt::Debug, marker::PhantomData, time::Duration};

#[cfg(feature = "std")]
use hashbrown::HashMap;
use libafl_bolts::ClientId;
#[cfg(feature = "std")]
use libafl_bolts::shmem::ShMemProvider;
#[cfg(feature = "std")]
use serde::Serialize;
#[cfg(feature = "std")]
use serde::de::DeserializeOwned;

use super::{AwaitRestartSafe, EventWithStats, ProgressReporter, std_on_restart};
use crate::{
    Error, HasMetadata,
    events::{
        BrokerEventResult, Event, EventFirer, EventManagerId, EventReceiver, EventRestarter,
        HasEventManagerId, SendExiting, std_maybe_report_progress, std_report_progress,
    },
    monitors::{Monitor, stats::ClientStatsManager},
    state::{
        HasCurrentStageId, HasExecutions, HasLastReportTime, MaybeHasClientPerfMonitor, Stoppable,
    },
};
#[cfg(feature = "std")]
use crate::{
    events::RestartingMgr,
    monitors::{SimplePrintingMonitor, stats::ClientStats},
    state::HasSolutions,
};

/// The llmp connection from the actual fuzzer to the process supervising it
const _ENV_FUZZER_SENDER: &str = "_AFL_ENV_FUZZER_SENDER";
const _ENV_FUZZER_RECEIVER: &str = "_AFL_ENV_FUZZER_RECEIVER";
/// The llmp (2 way) connection from a fuzzer to the broker (broadcasting all other fuzzer messages)
const _ENV_FUZZER_BROKER_CLIENT_INITIAL: &str = "_AFL_ENV_FUZZER_BROKER_CLIENT";

/// A simple, single-threaded event manager that just logs
pub struct SimpleEventManager<I, MT, S> {
    /// The monitor
    monitor: MT,
    /// The events that happened since the last `handle_in_broker`
    events: Vec<EventWithStats<I>>,
    phantom: PhantomData<S>,
    client_stats_manager: ClientStatsManager,
}

impl<I, MT, S> Debug for SimpleEventManager<I, MT, S>
where
    MT: Debug,
    I: Debug,
{
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("SimpleEventManager")
            //.field("custom_buf_handlers", self.custom_buf_handlers)
            .field("monitor", &self.monitor)
            .field("events", &self.events)
            .finish_non_exhaustive()
    }
}

impl<I, MT, S> EventFirer<I, S> for SimpleEventManager<I, MT, S>
where
    I: Debug,
    MT: Monitor,
    S: Stoppable,
{
    fn should_send(&self) -> bool {
        true
    }

    fn fire(&mut self, _state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        match Self::handle_in_broker(&mut self.monitor, &mut self.client_stats_manager, &event)? {
            BrokerEventResult::Forward => self.events.push(event),
            BrokerEventResult::Handled => (),
        }
        Ok(())
    }
}

impl<I, MT, S> SendExiting for SimpleEventManager<I, MT, S> {
    fn send_exiting(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn on_shutdown(&mut self) -> Result<(), Error> {
        self.send_exiting()
    }
}

impl<I, MT, S> AwaitRestartSafe for SimpleEventManager<I, MT, S> {
    fn await_restart_safe(&mut self) {}
}

impl<I, MT, S> EventRestarter<S> for SimpleEventManager<I, MT, S>
where
    S: HasCurrentStageId,
{
    fn on_restart(&mut self, state: &mut S) -> Result<(), Error> {
        std_on_restart(self, state)
    }
}

impl<I, MT, S> EventReceiver<I, S> for SimpleEventManager<I, MT, S>
where
    I: Debug,
    MT: Monitor,
    S: Stoppable,
{
    fn try_receive(&mut self, state: &mut S) -> Result<Option<(EventWithStats<I>, bool)>, Error> {
        while let Some(event) = self.events.pop() {
            match event.event() {
                Event::Stop => {
                    state.request_stop();
                }
                _ => {
                    return Err(Error::unknown(format!(
                        "Received illegal message that message should not have arrived: {event:?}."
                    )));
                }
            }
        }
        Ok(None)
    }
    fn on_interesting(
        &mut self,
        _state: &mut S,
        _event_vec: EventWithStats<I>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

impl<I, MT, S> ProgressReporter<S> for SimpleEventManager<I, MT, S>
where
    I: Debug,
    MT: Monitor,
    S: HasMetadata + HasExecutions + HasLastReportTime + Stoppable + MaybeHasClientPerfMonitor,
{
    fn maybe_report_progress(
        &mut self,
        state: &mut S,
        monitor_timeout: Duration,
    ) -> Result<(), Error> {
        std_maybe_report_progress(self, state, monitor_timeout)
    }

    fn report_progress(&mut self, state: &mut S) -> Result<(), Error> {
        std_report_progress(self, state)
    }
}

impl<I, MT, S> HasEventManagerId for SimpleEventManager<I, MT, S> {
    fn mgr_id(&self) -> EventManagerId {
        EventManagerId(0)
    }
}

#[cfg(feature = "std")]
impl<I, S> SimpleEventManager<I, SimplePrintingMonitor, S>
where
    I: Debug,
    S: Stoppable,
{
    /// Creates a [`SimpleEventManager`] that just prints to `stdout`.
    #[must_use]
    pub fn printing() -> Self {
        Self::new(SimplePrintingMonitor::new())
    }
}

impl<I, MT, S> SimpleEventManager<I, MT, S>
where
    I: Debug,
    MT: Monitor,
    S: Stoppable,
{
    /// Creates a new [`SimpleEventManager`].
    pub fn new(monitor: MT) -> Self {
        Self {
            monitor,
            events: vec![],
            client_stats_manager: ClientStatsManager::default(),
            phantom: PhantomData,
        }
    }

    /// Handle arriving events in the broker
    fn handle_in_broker(
        monitor: &mut MT,
        client_stats_manager: &mut ClientStatsManager,
        event: &EventWithStats<I>,
    ) -> Result<BrokerEventResult, Error> {
        let stats = event.stats();

        client_stats_manager.client_stats_insert(ClientId(0))?;
        client_stats_manager.update_client_stats_for(ClientId(0), |client_stat| {
            client_stat.update_executions(stats.executions, stats.time);
        })?;

        let event = event.event();
        match event {
            Event::NewTestcase { corpus_size, .. } => {
                client_stats_manager.client_stats_insert(ClientId(0))?;
                client_stats_manager.update_client_stats_for(ClientId(0), |client_stat| {
                    client_stat.update_corpus_size(*corpus_size as u64);
                })?;
                monitor.display(client_stats_manager, event.name(), ClientId(0))?;
                Ok(BrokerEventResult::Handled)
            }
            Event::Heartbeat => {
                monitor.display(client_stats_manager, event.name(), ClientId(0))?;
                Ok(BrokerEventResult::Handled)
            }
            Event::UpdateUserStats { name, value, .. } => {
                client_stats_manager.client_stats_insert(ClientId(0))?;
                client_stats_manager.update_client_stats_for(ClientId(0), |client_stat| {
                    client_stat.update_user_stats(name.clone(), value.clone());
                })?;
                client_stats_manager.aggregate(name);
                monitor.display(client_stats_manager, event.name(), ClientId(0))?;
                Ok(BrokerEventResult::Handled)
            }
            Event::UpdateUserStatsMap { stats, .. } => {
                client_stats_manager.client_stats_insert(ClientId(0))?;
                client_stats_manager.update_client_stats_for(ClientId(0), |client_stat| {
                    for (name, value) in stats {
                        client_stat.update_user_stats(name.clone(), value.clone());
                    }
                })?;
                for name in stats.keys() {
                    client_stats_manager.aggregate(name);
                }
                monitor.display(client_stats_manager, event.name(), ClientId(0))?;
                Ok(BrokerEventResult::Handled)
            }
            #[cfg(feature = "introspection")]
            Event::UpdatePerfMonitor {
                introspection_stats,
                ..
            } => {
                // TODO: The monitor buffer should be added on client add.
                client_stats_manager.update_client_stats_for(ClientId(0), |client_stat| {
                    client_stat.update_introspection_stats(introspection_stats);
                })?;
                monitor.display(client_stats_manager, event.name(), ClientId(0))?;
                Ok(BrokerEventResult::Handled)
            }
            Event::Objective { objective_size, .. } => {
                client_stats_manager.client_stats_insert(ClientId(0))?;
                client_stats_manager.update_client_stats_for(ClientId(0), |client_stat| {
                    client_stat.update_objective_size(*objective_size as u64);
                })?;
                monitor.display(client_stats_manager, event.name(), ClientId(0))?;
                Ok(BrokerEventResult::Handled)
            }
            Event::Log {
                severity_level,
                message,
                ..
            } => {
                let (_, _) = (message, severity_level);
                log::log!((*severity_level).into(), "{message}");
                Ok(BrokerEventResult::Handled)
            }
            Event::Stop => Ok(BrokerEventResult::Forward),
        }
    }
}

/// Provides a `builder` which can be used to build a [`SimpleRestartingEventManager`].
///
/// The [`SimpleRestartingEventManager`] is a combination of a
/// `restarter` and `runner`, that can be used on systems both with and without `fork` support. The
/// `restarter` will start a new process each time the child crashes or times out.
#[cfg(feature = "std")]
pub type SimpleRestartingEventManager<I, MT, S, SP> =
    crate::events::RestartingEventManager<SimpleEventManager<I, MT, S>, SP>;

#[cfg(feature = "std")]
impl<I, MT, S, SP> crate::events::Restorable<S, SP> for SimpleEventManager<I, MT, S>
where
    I: Debug,
    MT: Monitor,
    S: HasCurrentStageId + Serialize,
    SP: ShMemProvider,
{
    type RestartState = (Duration, HashMap<ClientId, ClientStats>);

    fn on_restart(&mut self, state: &mut S) -> Result<(bool, Self::RestartState), Error> {
        state.on_restart()?;

        let inner_state = (
            self.client_stats_manager.start_time(),
            self.client_stats_manager.client_stats().clone(),
        );

        Ok((true, inner_state))
    }
}

#[cfg(feature = "std")]
impl<I, MT, S, SP> SimpleRestartingEventManager<I, MT, S, SP>
where
    I: Debug,
    MT: Monitor,
    S: Stoppable,
    SP: ShMemProvider,
{
    /// Launch the simple restarting manager.
    /// This `EventManager` is simple and single threaded,
    /// but can still used shared maps to recover from crashes and timeouts.
    ///
    /// # Arguments
    ///
    /// * `monitor` - The monitor to use for the event manager.
    /// * `shmem_provider` - The shared memory provider to use for the event manager.
    /// * `use_fork` - Whether to use fork to spawn child processes (on Unix only)
    ///   or to spawn the binary again with the same parameters.
    pub fn launch(
        monitor: MT,
        shmem_provider: &mut SP,
        _use_fork: bool,
    ) -> Result<(Option<S>, Self), Error>
    where
        S: DeserializeOwned + Serialize + HasSolutions<I> + HasCurrentStageId,
        MT: Debug,
    {
        // We start ourself as child process to actually fuzz
        let mut restarting_mgr = RestartingMgr::new(shmem_provider.clone());
        #[cfg(unix)]
        restarting_mgr.fork(_use_fork);

        restarting_mgr.launch(|mut staterestorer, _new_shmem_provider, _core_id| {
            // If we're restarting, deserialize the old state.
            let (state, mgr) = match staterestorer
                .restore::<(Option<S>, (Duration, HashMap<ClientId, ClientStats>))>()?
            {
                None => {
                    log::info!("First run. Let's set it all up");
                    // Mgr to send and receive msgs from/to all other fuzzer instances
                    (
                        None::<S>,
                        SimpleRestartingEventManager::new(
                            SimpleEventManager::<I, MT, S>::new(monitor),
                            staterestorer,
                        ),
                    )
                }
                // Restoring from a previous run, deserialize state and corpus.
                Some((state_opt, (start_time, clients_stats))) => {
                    log::info!("Subsequent run. Loaded previous state.");
                    // We reset the staterestorer, the next staterestorer and receiver (after crash) will reuse the page from the initial message.
                    staterestorer.reset();

                    // reload the state of the monitor to display the correct stats after restarts
                    let mut inner = SimpleEventManager::<I, MT, S>::new(monitor);
                    inner.client_stats_manager.set_start_time(start_time);
                    inner
                        .client_stats_manager
                        .update_all_client_stats(clients_stats);

                    (
                        state_opt,
                        SimpleRestartingEventManager::new(inner, staterestorer),
                    )
                }
            };

            Ok((state, mgr))
        })
    }
}
