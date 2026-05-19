//! An `EventManager` manages all events that go to other instances of the fuzzer.
//! The messages are commonly information about new Testcases as well as stats and other [`Event`]s.

pub mod events_hooks;
pub use events_hooks::*;

pub mod simple;
pub use simple::*;
#[cfg(all(unix, feature = "std"))]
pub mod centralized;
#[cfg(all(unix, feature = "std"))]
pub use centralized::*;
#[cfg(feature = "std")]
pub mod launcher;

pub mod llmp;
pub use llmp::*;
#[cfg(feature = "tcp_manager")]
pub mod tcp;

pub mod broker_hooks;
#[cfg(feature = "introspection")]
use alloc::boxed::Box;
use alloc::{borrow::Cow, string::String, vec::Vec};
use core::{
    fmt,
    hash::{BuildHasher, Hasher},
    marker::PhantomData,
    time::Duration,
};

use ahash::RandomState;
pub use broker_hooks::*;
#[cfg(feature = "std")]
pub use launcher::*;
use libafl_bolts::current_time;
#[cfg(all(unix, feature = "std"))]
use libafl_bolts::os::CTRL_C_EXIT;
#[cfg(all(unix, feature = "std"))]
use libafl_bolts::os::unix_signals::{Signal, SignalHandler, siginfo_t, ucontext_t};
use serde::{Deserialize, Serialize};
#[cfg(feature = "std")]
use uuid::Uuid;

use crate::{
    Error, HasMetadata,
    executors::ExitKind,
    inputs::Input,
    monitors::stats::UserStats,
    state::{HasExecutions, HasLastReportTime, MaybeHasClientPerfMonitor},
};

/// Multi-machine mode
#[cfg(all(unix, feature = "std", feature = "multi_machine"))]
pub mod multi_machine;

/// Check if ctrl-c is sent with this struct
#[cfg(all(unix, feature = "std"))]
pub static mut EVENTMGR_SIGHANDLER_STATE: ShutdownSignalData = ShutdownSignalData {};

/// A signal handler for catching `ctrl-c`.
///
/// The purpose of this signal handler is solely for calling `exit()` with a specific exit code 100
/// In this way, the restarting manager can tell that we really want to exit
#[cfg(all(unix, feature = "std"))]
#[derive(Debug, Clone)]
pub struct ShutdownSignalData {}

/// Shutdown handler. `SigTerm`, `SigInterrupt`, `SigQuit` call this
/// We can't handle SIGKILL in the signal handler, this means that you shouldn't kill your fuzzer with `kill -9` because then the shmem segments are never freed
///
/// # Safety
/// This will exit the program
#[cfg(all(unix, feature = "std"))]
impl SignalHandler for ShutdownSignalData {
    unsafe fn handle(
        &mut self,
        _signal: Signal,
        _info: &mut siginfo_t,
        _context: Option<&mut ucontext_t>,
    ) {
        unsafe {
            #[cfg(unix)]
            libc::_exit(CTRL_C_EXIT);

            #[cfg(windows)]
            windows::Win32::System::Threading::ExitProcess(100);
        }
    }

    fn signals(&self) -> Vec<Signal> {
        vec![Signal::SigTerm, Signal::SigInterrupt, Signal::SigQuit]
    }
}

/// A per-fuzzer unique `ID`, usually starting with `0` and increasing
/// by `1` in multiprocessed `EventManagers`, such as [`LlmpRestartingEventManager`].
#[derive(Debug, Copy, Clone, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(transparent)]
pub struct EventManagerId(
    /// The id
    pub usize,
);

#[cfg(all(unix, feature = "std", feature = "multi_machine"))]
use crate::events::multi_machine::NodeId;
#[cfg(feature = "introspection")]
use crate::monitors::stats::ClientPerfStats;
use crate::state::HasCurrentStageId;

/// The log event severity
#[derive(Serialize, Deserialize, Debug, Copy, Clone)]
pub enum LogSeverity {
    /// Debug severity
    Debug,
    /// Information
    Info,
    /// Warning
    Warn,
    /// Error
    Error,
}

impl From<LogSeverity> for log::Level {
    fn from(value: LogSeverity) -> Self {
        match value {
            LogSeverity::Debug => log::Level::Debug,
            LogSeverity::Info => log::Level::Info,
            LogSeverity::Warn => log::Level::Trace,
            LogSeverity::Error => log::Level::Error,
        }
    }
}

impl fmt::Display for LogSeverity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LogSeverity::Debug => write!(f, "Debug"),
            LogSeverity::Info => write!(f, "Info"),
            LogSeverity::Warn => write!(f, "Warn"),
            LogSeverity::Error => write!(f, "Error"),
        }
    }
}

/// Indicate if an event worked or not
#[derive(Serialize, Deserialize, Debug, Copy, Clone)]
pub enum BrokerEventResult {
    /// The broker handled this. No need to pass it on.
    Handled,
    /// Pass this message along to the clients.
    Forward,
}

/// Distinguish a fuzzer by its config
#[derive(Serialize, Deserialize, Debug, Copy, Clone, PartialEq, Eq)]
pub enum EventConfig {
    /// Always assume unique setups for fuzzer configs
    AlwaysUnique,
    /// Create a fuzzer config from a name hash
    FromName {
        /// The name hash
        name_hash: u64,
    },
    /// Create a fuzzer config from a build-time [`Uuid`]
    #[cfg(feature = "std")]
    BuildID {
        /// The build-time [`Uuid`]
        id: Uuid,
    },
}

impl EventConfig {
    /// Create a new [`EventConfig`] from a name hash
    #[must_use]
    pub fn from_name(name: &str) -> Self {
        let mut hasher = RandomState::with_seeds(0, 0, 0, 0).build_hasher(); //AHasher::new_with_keys(0, 0);
        hasher.write(name.as_bytes());
        EventConfig::FromName {
            name_hash: hasher.finish(),
        }
    }

    /// Create a new [`EventConfig`] from a build-time [`Uuid`]
    #[cfg(feature = "std")]
    #[must_use]
    pub fn from_build_id() -> Self {
        EventConfig::BuildID {
            id: libafl_bolts::build_id::get(),
        }
    }

    /// Match if the current [`EventConfig`] matches another given config
    #[must_use]
    pub fn match_with(&self, other: &EventConfig) -> bool {
        match self {
            EventConfig::AlwaysUnique => false,
            EventConfig::FromName { name_hash: a } => match other {
                #[cfg(not(feature = "std"))]
                EventConfig::AlwaysUnique => false,
                EventConfig::FromName { name_hash: b } => a == b,
                #[cfg(feature = "std")]
                EventConfig::AlwaysUnique | EventConfig::BuildID { id: _ } => false,
            },
            #[cfg(feature = "std")]
            EventConfig::BuildID { id: a } => match other {
                EventConfig::AlwaysUnique | EventConfig::FromName { name_hash: _ } => false,
                EventConfig::BuildID { id: b } => a == b,
            },
        }
    }
}

impl From<&str> for EventConfig {
    fn from(name: &str) -> Self {
        Self::from_name(name)
    }
}

impl From<String> for EventConfig {
    fn from(name: String) -> Self {
        Self::from_name(&name)
    }
}

/*
/// A custom event, for own messages, with own handler.
pub trait CustomEvent<I>: SerdeAny
where
    I: Input,
{
    /// Returns the name of this event
    fn name(&self) -> &str;
    /// This method will be called in the broker
    fn handle_in_broker(&self) -> Result<BrokerEventResult, Error>;
    /// This method will be called in the clients after handle_in_broker (unless BrokerEventResult::Handled) was returned in handle_in_broker
    fn handle_in_client(&self) -> Result<(), Error>;
}
*/

/// Basic statistics
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ExecStats {
    /// The time of generation of the [`Event`]
    time: Duration,
    /// The executions of this client
    executions: u64,
}

impl ExecStats {
    /// Create an new [`ExecStats`].
    #[must_use]
    pub fn new(time: Duration, executions: u64) -> Self {
        Self { time, executions }
    }
}

/// Event with associated stats
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct EventWithStats<I> {
    /// The event
    event: Event<I>,
    /// Statistics on new event
    stats: ExecStats,
}

impl<I> EventWithStats<I> {
    /// Create a new [`EventWithStats`].
    pub fn new(event: Event<I>, stats: ExecStats) -> Self {
        Self { event, stats }
    }

    /// Create a new [`EventWithStats`], with the current time.
    pub fn with_current_time(event: Event<I>, executions: u64) -> Self {
        let time = current_time();

        Self {
            event,
            stats: ExecStats { time, executions },
        }
    }

    /// Get the inner ref to the [`Event`] in [`EventWithStats`].
    pub fn event(&self) -> &Event<I> {
        &self.event
    }

    /// Get the inner mutable ref to the [`Event`] in [`EventWithStats`].
    pub fn event_mut(&mut self) -> &mut Event<I> {
        &mut self.event
    }

    /// Get the inner ref to the [`ExecStats`] in [`EventWithStats`].
    pub fn stats(&self) -> &ExecStats {
        &self.stats
    }
}

// TODO remove forward_id as not anymore needed for centralized
/// Events sent around in the library
#[derive(Serialize, Deserialize, Debug, Clone)]
pub enum Event<I> {
    // TODO use an ID to keep track of the original index in the sender Corpus
    // The sender can then use it to send Testcase metadata with CustomEvent
    /// A fuzzer found a new testcase. Rejoice!
    NewTestcase {
        /// The input for the new testcase
        input: I,
        /// The state of the observers when this testcase was found
        observers_buf: Option<Vec<u8>>,
        /// The exit kind
        exit_kind: ExitKind,
        /// The new corpus size of this client
        corpus_size: usize,
        /// The client config for this observers/testcase combination
        client_config: EventConfig,
        /// The original sender if, if forwarded
        forward_id: Option<libafl_bolts::ClientId>,
        /// The (multi-machine) node from which the tc is from, if any
        #[cfg(all(unix, feature = "std", feature = "multi_machine"))]
        node_id: Option<NodeId>,
    },
    /// A hearbeat, to notice a fuzzer is still alive.
    Heartbeat,
    /// New user stats event to monitor.
    UpdateUserStats {
        /// Custom user monitor name
        name: Cow<'static, str>,
        /// Custom user monitor value
        value: UserStats,
        /// [`PhantomData`]
        phantom: PhantomData<I>,
    },
    /// New monitor with performance monitor.
    #[cfg(feature = "introspection")]
    UpdatePerfMonitor {
        /// Current performance statistics
        introspection_stats: Box<ClientPerfStats>,

        /// phantomm data
        phantom: PhantomData<I>,
    },
    /// A new objective was found
    Objective {
        /// Input of newly found Objective
        input: Option<I>,
        /// Objective corpus size
        objective_size: usize,
    },
    /// Write a new log
    Log {
        /// the severity level
        severity_level: LogSeverity,
        /// The message
        message: String,
        /// `PhantomData`
        phantom: PhantomData<I>,
    },
    /// Exit gracefully
    Stop,
    /*/// A custom type
    Custom {
        // TODO: Allow custom events
        // custom_event: Box<dyn CustomEvent<I, OT>>,
    },*/
}

impl<I> Event<I> {
    /// Event's corresponding name
    pub fn name(&self) -> &str {
        match self {
            Event::NewTestcase { .. } => "Testcase",
            Event::Heartbeat => "Client Heartbeat",
            Event::UpdateUserStats { .. } => "UserStats",
            #[cfg(feature = "introspection")]
            Event::UpdatePerfMonitor { .. } => "PerfMonitor",
            Event::Objective { .. } => "Objective",
            Event::Log { .. } => "Log",
            /*Event::Custom {
                sender_id: _, /*custom_event} => custom_event.name()*/
            } => "todo",*/
            Event::Stop => "Stop",
        }
    }

    /// Event's corresponding name with additional info
    fn name_detailed(&self) -> Cow<'static, str>
    where
        I: Input,
    {
        match self {
            Event::NewTestcase { input, .. } => {
                Cow::Owned(format!("Testcase {}", input.generate_name(None)))
            }
            Event::Heartbeat => Cow::Borrowed("Client Heartbeat"),
            Event::UpdateUserStats { .. } => Cow::Borrowed("UserStats"),
            #[cfg(feature = "introspection")]
            Event::UpdatePerfMonitor { .. } => Cow::Borrowed("PerfMonitor"),
            Event::Objective { .. } => Cow::Borrowed("Objective"),
            Event::Log { .. } => Cow::Borrowed("Log"),
            Event::Stop => Cow::Borrowed("Stop"),
            /*Event::Custom {
                sender_id: _, /*custom_event} => custom_event.name()*/
            } => "todo",*/
        }
    }

    /// Returns true if self is a new testcase, false otherwise.
    pub fn is_new_testcase(&self) -> bool {
        matches!(self, Event::NewTestcase { .. })
    }
}

/// [`EventFirer`] fires an event.
pub trait EventFirer<I, S> {
    /// Send off an [`Event`] to the broker
    ///
    /// For multi-processed managers, such as [`LlmpRestartingEventManager`],
    /// this serializes the [`Event`] and commits it to the [`llmp`] page.
    /// In this case, if you `fire` faster than the broker can consume
    /// (for example for each [`Input`], on multiple cores)
    /// the [`llmp`] shared map may fill up and the client will eventually OOM or [`panic`].
    /// This should not happen for a normal use-case.
    fn fire(&mut self, state: &mut S, event: EventWithStats<I>) -> Result<(), Error>;

    /// Send off an [`Event::Log`] event to the broker.
    /// This is a shortcut for [`EventFirer::fire`] with [`Event::Log`] as argument.
    fn log(
        &mut self,
        state: &mut S,
        severity_level: LogSeverity,
        message: String,
    ) -> Result<(), Error>
    where
        S: HasExecutions,
    {
        let executions = *state.executions();
        let cur = current_time();

        let stats = ExecStats {
            executions,
            time: cur,
        };

        self.fire(
            state,
            EventWithStats {
                event: Event::Log {
                    severity_level,
                    message,
                    phantom: PhantomData,
                },
                stats,
            },
        )
    }

    /// Get the configuration
    fn configuration(&self) -> EventConfig {
        EventConfig::AlwaysUnique
    }

    /// Return if we really send this event or not
    fn should_send(&self) -> bool;
}

/// Default implementation of [`ProgressReporter::maybe_report_progress`] for implementors with the
/// given constraints
pub fn std_maybe_report_progress<PR, S>(
    reporter: &mut PR,
    state: &mut S,
    monitor_timeout: Duration,
) -> Result<(), Error>
where
    PR: ProgressReporter<S>,
    S: HasMetadata + HasExecutions + HasLastReportTime,
{
    let Some(last_report_time) = state.last_report_time() else {
        // this is the first time we execute, no need to report progress just yet.
        *state.last_report_time_mut() = Some(current_time());
        return Ok(());
    };
    let cur = current_time();
    // default to 0 here to avoid crashes on clock skew
    if cur.checked_sub(*last_report_time).unwrap_or_default() > monitor_timeout {
        // report_progress sets a new `last_report_time` internally.
        reporter.report_progress(state)?;
    }
    Ok(())
}

/// Default implementation of [`ProgressReporter::report_progress`] for implementors with the
/// given constraints
pub fn std_report_progress<EM, I, S>(reporter: &mut EM, state: &mut S) -> Result<(), Error>
where
    EM: EventFirer<I, S>,
    S: HasExecutions + HasLastReportTime + MaybeHasClientPerfMonitor,
{
    let executions = *state.executions();
    let cur = current_time();

    let stats = ExecStats {
        executions,
        time: cur,
    };

    // Default no introspection implmentation
    #[cfg(not(feature = "introspection"))]
    reporter.fire(
        state,
        EventWithStats {
            event: Event::Heartbeat,
            stats,
        },
    )?;

    // If performance monitor are requested, fire the `UpdatePerfMonitor` event
    #[cfg(feature = "introspection")]
    {
        state
            .introspection_stats_mut()
            .set_current_time(libafl_bolts::cpu::read_time_counter());

        // Send the current monitor over to the manager. This `.clone` shouldn't be
        // costly as `ClientPerfStats` impls `Copy` since it only contains `u64`s
        reporter.fire(
            state,
            EventWithStats::new(
                Event::UpdatePerfMonitor {
                    introspection_stats: Box::new(state.introspection_stats().clone()),
                    phantom: PhantomData,
                },
                stats,
            ),
        )?;
    }

    *state.last_report_time_mut() = Some(cur);

    Ok(())
}

/// [`ProgressReporter`] report progress to the broker.
pub trait ProgressReporter<S> {
    /// Given the last time, if `monitor_timeout` seconds passed, send off an info/monitor/heartbeat message to the broker.
    /// Returns the new `last` time (so the old one, unless `monitor_timeout` time has passed and monitor have been sent)
    /// Will return an [`Error`], if the stats could not be sent.
    /// [`std_maybe_report_progress`] is the standard implementation that you can call.
    fn maybe_report_progress(
        &mut self,
        state: &mut S,
        monitor_timeout: Duration,
    ) -> Result<(), Error>;

    /// Send off an info/monitor/heartbeat message to the broker.
    /// Will return an [`Error`], if the stats could not be sent.
    /// [`std_report_progress`] is the standard implementation that you can call.
    fn report_progress(&mut self, state: &mut S) -> Result<(), Error>;
}

/// Restartable trait
pub trait EventRestarter<S> {
    /// For restarting event managers, implement a way to forward state to their next peers.
    /// You *must* ensure that [`HasCurrentStageId::on_restart`] will be invoked in this method, by you
    /// or an internal [`EventRestarter`], before the state is saved for recovery.
    /// [`std_on_restart`] is the standard implementation that you can call.
    fn on_restart(&mut self, state: &mut S) -> Result<(), Error>;
}

/// Default implementation of [`EventRestarter::on_restart`] for implementors with the given
/// constraints
pub fn std_on_restart<EM, S>(restarter: &mut EM, state: &mut S) -> Result<(), Error>
where
    EM: EventRestarter<S> + AwaitRestartSafe,
    S: HasCurrentStageId,
{
    state.on_restart()?;
    restarter.await_restart_safe();
    Ok(())
}

/// Send that we're about to exit
pub trait SendExiting {
    /// Send information that this client is exiting.
    /// No need to restart us any longer, and no need to print an error, either.
    fn send_exiting(&mut self) -> Result<(), Error>;

    /// Shutdown gracefully; typically without saving state.
    /// This is usually called from `fuzz_loop`.
    fn on_shutdown(&mut self) -> Result<(), Error>;
}

/// Wait until it's safe to restart
pub trait AwaitRestartSafe {
    /// Block until we are safe to exit, usually called inside `on_restart`.
    fn await_restart_safe(&mut self);
}

/// [`EventReceiver`] process all the incoming messages
pub trait EventReceiver<I, S> {
    /// Lookup for incoming events and process them.
    /// Return the event, if any, that needs to be evaluated
    fn try_receive(&mut self, state: &mut S) -> Result<Option<(EventWithStats<I>, bool)>, Error>;

    /// Run the post processing routine after the fuzzer deemed this event as interesting
    /// For example, in centralized manager you wanna send this an event.
    fn on_interesting(&mut self, state: &mut S, event: EventWithStats<I>) -> Result<(), Error>;
}
/// The id of this `EventManager`.
/// For multi processed `EventManagers`,
/// each connected client should have a unique ids.
pub trait HasEventManagerId {
    /// The id of this manager. For Multiprocessed `EventManagers`,
    /// each client should have a unique ids.
    fn mgr_id(&self) -> EventManagerId;
}

/// An eventmgr for tests, and as placeholder if you really don't need an event manager.
#[derive(Debug, Copy, Clone, Default)]
pub struct NopEventManager {}

impl NopEventManager {
    /// Creates a new [`NopEventManager`]
    #[must_use]
    pub fn new() -> Self {
        NopEventManager {}
    }
}

impl<I, S> EventFirer<I, S> for NopEventManager {
    fn should_send(&self) -> bool {
        true
    }

    fn fire(&mut self, _state: &mut S, _event: EventWithStats<I>) -> Result<(), Error> {
        Ok(())
    }
}

impl<S> EventRestarter<S> for NopEventManager
where
    S: HasCurrentStageId,
{
    fn on_restart(&mut self, state: &mut S) -> Result<(), Error> {
        std_on_restart(self, state)
    }
}

impl SendExiting for NopEventManager {
    /// Send information that this client is exiting.
    /// No need to restart us any longer, and no need to print an error, either.
    fn send_exiting(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn on_shutdown(&mut self) -> Result<(), Error> {
        Ok(())
    }
}

impl AwaitRestartSafe for NopEventManager {
    /// Block until we are safe to exit, usually called inside `on_restart`.
    fn await_restart_safe(&mut self) {}
}

impl<I, S> EventReceiver<I, S> for NopEventManager {
    fn try_receive(&mut self, _state: &mut S) -> Result<Option<(EventWithStats<I>, bool)>, Error> {
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

impl<S> ProgressReporter<S> for NopEventManager {
    fn maybe_report_progress(
        &mut self,
        _state: &mut S,
        _monitor_timeout: Duration,
    ) -> Result<(), Error> {
        Ok(())
    }

    fn report_progress(&mut self, _state: &mut S) -> Result<(), Error> {
        Ok(())
    }
}

impl HasEventManagerId for NopEventManager {
    fn mgr_id(&self) -> EventManagerId {
        EventManagerId(0)
    }
}

#[cfg(test)]
mod tests {

    use libafl_bolts::{Named, tuples::tuple_list};
    use tuple_list::tuple_list_type;

    use crate::{
        events::{Event, EventConfig},
        executors::ExitKind,
        inputs::bytes::BytesInput,
        observers::StdMapObserver,
    };

    static mut MAP: [u32; 4] = [0; 4];

    #[test]
    fn test_event_serde() {
        let map_ptr = &raw const MAP;
        let obv = unsafe {
            let len = (*map_ptr).len();
            StdMapObserver::from_mut_ptr("test", &raw mut MAP as *mut u32, len)
        };
        let map = tuple_list!(obv);
        let observers_buf = postcard::to_allocvec(&map).unwrap();

        let i = BytesInput::new(vec![0]);
        let e = Event::NewTestcase {
            input: i,
            observers_buf: Some(observers_buf),
            exit_kind: ExitKind::Ok,
            corpus_size: 123,
            client_config: EventConfig::AlwaysUnique,
            forward_id: None,
            #[cfg(all(unix, feature = "std", feature = "multi_machine"))]
            node_id: None,
        };

        let serialized = postcard::to_allocvec(&e).unwrap();

        let d = postcard::from_bytes::<Event<BytesInput>>(&serialized).unwrap();
        match d {
            Event::NewTestcase { observers_buf, .. } => {
                let o: tuple_list_type!(StdMapObserver::<u32, false>) =
                    postcard::from_bytes(observers_buf.as_ref().unwrap()).unwrap();
                assert_eq!("test", o.0.name());
            }
            _ => panic!("mistmatch"),
        }
    }
}
