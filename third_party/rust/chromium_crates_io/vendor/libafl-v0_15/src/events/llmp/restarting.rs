//! The `LLMP` restarting manager will
//! forward messages over lockless shared maps.
//! When the target crashes, a watch process (the parent) will
//! restart/refork it.

#[cfg(feature = "std")]
use alloc::string::ToString;
use alloc::vec::Vec;
use core::{
    marker::PhantomData,
    net::SocketAddr,
    num::NonZeroUsize,
    sync::atomic::{Ordering, compiler_fence},
    time::Duration,
};
#[cfg(feature = "std")]
use std::net::TcpStream;

#[cfg(any(windows, not(feature = "fork")))]
use libafl_bolts::os::startable_self;
#[cfg(all(unix, not(miri)))]
use libafl_bolts::os::unix_signals::setup_signal_handler;
#[cfg(all(feature = "fork", unix))]
use libafl_bolts::os::{ForkResult, fork};
#[cfg(feature = "std")]
use libafl_bolts::{
    IP_LOCALHOST,
    llmp::{TcpRequest, TcpResponse, recv_tcp_msg, send_tcp_msg},
};
#[cfg(feature = "llmp_compression")]
use libafl_bolts::{
    compress::GzipCompressor,
    llmp::{LLMP_FLAG_COMPRESSED, LLMP_FLAG_INITIALIZED},
};
use libafl_bolts::{
    core_affinity::CoreId,
    current_time,
    llmp::{
        Broker, LLMP_FLAG_FROM_MM, LlmpBroker, LlmpClient, LlmpClientDescription, LlmpConnection,
    },
    os::CTRL_C_EXIT,
    shmem::{ShMem, ShMemProvider, StdShMem, StdShMemProvider},
    staterestore::StateRestorer,
    tuples::tuple_list,
};
use serde::{Serialize, de::DeserializeOwned};
use typed_builder::TypedBuilder;

#[cfg(feature = "llmp_compression")]
use crate::events::COMPRESS_THRESHOLD;
#[cfg(all(unix, not(miri)))]
use crate::events::EVENTMGR_SIGHANDLER_STATE;
use crate::{
    Error,
    common::HasMetadata,
    events::{
        _LLMP_TAG_EVENT_TO_BROKER, AwaitRestartSafe, Event, EventConfig, EventFirer,
        EventManagerHooksTuple, EventManagerId, EventReceiver, EventRestarter, EventWithStats,
        HasEventManagerId, LLMP_TAG_EVENT_TO_BOTH, LlmpShouldSaveState, ProgressReporter,
        SendExiting, StdLlmpEventHook, launcher::ClientDescription, std_maybe_report_progress,
        std_report_progress,
    },
    inputs::Input,
    monitors::Monitor,
    state::{
        HasCurrentStageId, HasCurrentTestcase, HasExecutions, HasImported, HasLastReportTime,
        HasSolutions, MaybeHasClientPerfMonitor, Stoppable,
    },
};

const INITIAL_EVENT_BUFFER_SIZE: usize = 1024 * 4;
/// A manager that can restart on the fly, storing states in-between (in `on_restart`)
#[derive(Debug)]
pub struct LlmpRestartingEventManager<EMH, I, S, SHM, SP> {
    /// We only send 1 testcase for every `throttle` second
    pub(crate) throttle: Option<Duration>,
    /// We sent last message at `last_sent`
    last_sent: Duration,
    hooks: EMH,
    /// The LLMP client for inter process communication
    llmp: LlmpClient<SHM, SP>,
    #[cfg(feature = "llmp_compression")]
    compressor: GzipCompressor,
    /// The configuration defines this specific fuzzer.
    /// A node will not re-use the observer values sent over LLMP
    /// from nodes with other configurations.
    configuration: EventConfig,
    event_buffer: Vec<u8>,
    /// The staterestorer to serialize the state for the next runner
    /// If this is Some, this event manager can restart. Else it does not.
    staterestorer: Option<StateRestorer<SHM, SP>>,
    /// Decide if the state restorer must save the serialized state
    save_state: LlmpShouldSaveState,
    phantom: PhantomData<(I, S)>,
}

impl<EMH, I, S, SHM, SP> ProgressReporter<S> for LlmpRestartingEventManager<EMH, I, S, SHM, SP>
where
    I: Serialize,
    S: HasExecutions + HasLastReportTime + HasMetadata + Serialize + MaybeHasClientPerfMonitor,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
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

impl<EMH, I, S, SHM, SP> EventFirer<I, S> for LlmpRestartingEventManager<EMH, I, S, SHM, SP>
where
    I: Serialize,
    S: Serialize,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn fire(&mut self, _state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        // Check if we are going to crash in the event, in which case we store our current state for the next runner
        #[cfg(feature = "llmp_compression")]
        let flags = LLMP_FLAG_INITIALIZED;

        self.event_buffer.resize(self.event_buffer.capacity(), 0);

        // Serialize the event, reallocating event_buffer if needed
        let written_len = match postcard::to_slice(&event, &mut self.event_buffer) {
            Ok(written) => written.len(),
            Err(postcard::Error::SerializeBufferFull) => {
                let serialized = postcard::to_allocvec(&event)?;
                self.event_buffer = serialized;
                self.event_buffer.len()
            }
            Err(e) => return Err(Error::from(e)),
        };

        #[cfg(feature = "llmp_compression")]
        {
            match self
                .compressor
                .maybe_compress(&self.event_buffer[..written_len])
            {
                Some(comp_buf) => {
                    self.llmp.send_buf_with_flags(
                        LLMP_TAG_EVENT_TO_BOTH,
                        flags | LLMP_FLAG_COMPRESSED,
                        &comp_buf,
                    )?;
                }
                None => {
                    self.llmp
                        .send_buf(LLMP_TAG_EVENT_TO_BOTH, &self.event_buffer[..written_len])?;
                }
            }
        }

        #[cfg(not(feature = "llmp_compression"))]
        {
            self.llmp
                .send_buf(LLMP_TAG_EVENT_TO_BOTH, &self.event_buffer[..written_len])?;
        }

        self.last_sent = current_time();

        if self.staterestorer.is_some() {
            self.intermediate_save()?;
        }
        Ok(())
    }

    fn configuration(&self) -> EventConfig {
        self.configuration
    }

    fn should_send(&self) -> bool {
        if let Some(throttle) = self.throttle {
            current_time()
                .checked_sub(self.last_sent)
                .unwrap_or(throttle)
                >= throttle
        } else {
            true
        }
    }
}

impl<EMH, I, S, SHM, SP> EventRestarter<S> for LlmpRestartingEventManager<EMH, I, S, SHM, SP>
where
    S: Serialize + HasCurrentStageId,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    /// Reset the single page (we reuse it over and over from pos 0), then send the current state to the next runner.
    fn on_restart(&mut self, state: &mut S) -> Result<(), Error> {
        state.on_restart()?;

        if let Some(sr) = &mut self.staterestorer {
            // First, reset the page to 0 so the next iteration can read from the beginning of this page
            sr.reset();
            sr.save(&(
                if self.save_state.on_restart() {
                    Some(state)
                } else {
                    None
                },
                &self.llmp.describe()?,
            ))?;

            log::info!("Waiting for broker...");
        }

        self.await_restart_safe();
        Ok(())
    }
}

impl<EMH, I, S, SHM, SP> SendExiting for LlmpRestartingEventManager<EMH, I, S, SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn send_exiting(&mut self) -> Result<(), Error> {
        if let Some(sr) = &mut self.staterestorer {
            sr.send_exiting();
        }
        // Also inform the broker that we are about to exit.
        // This way, the broker can clean up the pages, and eventually exit.
        self.llmp.sender_mut().send_exiting()
    }

    fn on_shutdown(&mut self) -> Result<(), Error> {
        self.send_exiting()
    }
}

impl<EMH, I, S, SHM, SP> AwaitRestartSafe for LlmpRestartingEventManager<EMH, I, S, SHM, SP>
where
    SHM: ShMem,
{
    /// The llmp client needs to wait until a broker mapped all pages, before shutting down.
    /// Otherwise, the OS may already have removed the shared maps,
    #[inline]
    fn await_restart_safe(&mut self) {
        self.llmp.await_safe_to_unmap_blocking();
    }
}

impl<EMH, I, S, SHM, SP> EventReceiver<I, S> for LlmpRestartingEventManager<EMH, I, S, SHM, SP>
where
    EMH: EventManagerHooksTuple<I, S>,
    I: DeserializeOwned + Input,
    S: HasImported + HasCurrentTestcase<I> + HasSolutions<I> + Stoppable + Serialize,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn try_receive(&mut self, state: &mut S) -> Result<Option<(EventWithStats<I>, bool)>, Error> {
        // TODO: Get around local event copy by moving handle_in_client
        let self_id = self.llmp.sender().id();
        while let Some((client_id, tag, flags, msg)) = self.llmp.recv_buf_with_flags()? {
            assert_ne!(
                tag, _LLMP_TAG_EVENT_TO_BROKER,
                "EVENT_TO_BROKER parcel should not have arrived in the client!"
            );

            if client_id == self_id {
                continue;
            }

            #[cfg(not(feature = "llmp_compression"))]
            let event_bytes = msg;
            #[cfg(feature = "llmp_compression")]
            let compressed;
            #[cfg(feature = "llmp_compression")]
            let event_bytes = if flags & LLMP_FLAG_COMPRESSED == LLMP_FLAG_COMPRESSED {
                compressed = self.compressor.decompress(msg)?;
                &compressed
            } else {
                msg
            };

            let event: EventWithStats<I> = postcard::from_bytes(event_bytes)?;
            log::debug!(
                "Received event in normal llmp {}",
                event.event().name_detailed()
            );

            // If the message comes from another machine, do not
            // consider other events than new testcase.
            if !event.event().is_new_testcase() && (flags & LLMP_FLAG_FROM_MM == LLMP_FLAG_FROM_MM)
            {
                continue;
            }

            log::trace!(
                "Got event in client: {} from {client_id:?}",
                event.event().name()
            );
            if !self.hooks.pre_receive_all(state, client_id, &event)? {
                continue;
            }
            let evt_name = event.event().name_detailed();
            match event.event() {
                Event::NewTestcase {
                    client_config,
                    observers_buf,
                    #[cfg(feature = "std")]
                    forward_id,
                    ..
                } => {
                    #[cfg(feature = "std")]
                    log::debug!(
                        "[{}] Received new Testcase {evt_name} from {client_id:?} ({client_config:?}, forward {forward_id:?})",
                        std::process::id()
                    );

                    if client_config.match_with(&self.configuration) && observers_buf.is_some() {
                        return Ok(Some((event, true)));
                    }

                    return Ok(Some((event, false)));
                }
                Event::Objective { .. } => {
                    #[cfg(feature = "std")]
                    log::debug!("[{}] Received new Objective", std::process::id());

                    return Ok(Some((event, false)));
                }
                Event::Stop => {
                    state.request_stop();
                }
                _ => {
                    return Err(Error::unknown(format!(
                        "Received illegal message that message should not have arrived: {:?}.",
                        event.event().name()
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

impl<EMH, I, S, SHM, SP> HasEventManagerId for LlmpRestartingEventManager<EMH, I, S, SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn mgr_id(&self) -> EventManagerId {
        EventManagerId(self.llmp.sender().id().0 as usize)
    }
}

/// The llmp connection from the actual fuzzer to the process supervising it
const _ENV_FUZZER_SENDER: &str = "_AFL_ENV_FUZZER_SENDER";
const _ENV_FUZZER_RECEIVER: &str = "_AFL_ENV_FUZZER_RECEIVER";
/// The llmp (2 way) connection from a fuzzer to the broker (broadcasting all other fuzzer messages)
const _ENV_FUZZER_BROKER_CLIENT_INITIAL: &str = "_AFL_ENV_FUZZER_BROKER_CLIENT";

/// Builder for `LlmpRestartingEventManager`
#[derive(Debug)]
pub struct LlmpEventManagerBuilder<EMH> {
    throttle: Option<Duration>,
    save_state: LlmpShouldSaveState,
    hooks: EMH,
}

impl Default for LlmpEventManagerBuilder<()> {
    fn default() -> Self {
        Self::builder()
    }
}

impl LlmpEventManagerBuilder<()> {
    /// Create a new `LlmpEventManagerBuilder`
    #[must_use]
    pub fn builder() -> Self {
        Self {
            throttle: None,
            save_state: LlmpShouldSaveState::OnRestart,
            hooks: (),
        }
    }
}

impl LlmpEventManagerBuilder<()> {
    /// Add hooks to it
    pub fn hooks<EMH>(self, hooks: EMH) -> LlmpEventManagerBuilder<EMH> {
        LlmpEventManagerBuilder {
            throttle: self.throttle,
            save_state: self.save_state,
            hooks,
        }
    }
}

impl<EMH> LlmpEventManagerBuilder<EMH> {
    /// Change the sampling rate
    #[must_use]
    pub fn throttle(mut self, throttle: Duration) -> Self {
        self.throttle = Some(throttle);
        self
    }

    /// Change save state policy
    #[must_use]
    pub fn save_state(mut self, save_state: LlmpShouldSaveState) -> Self {
        self.save_state = save_state;
        self
    }

    /// Create a manager from a raw LLMP client
    /// If staterestorer is some then this restarting manager restarts
    /// Otherwise this restarting manager does not restart
    pub fn build_from_client<I, S, SHM, SP>(
        self,
        llmp: LlmpClient<SHM, SP>,
        configuration: EventConfig,
        staterestorer: Option<StateRestorer<SHM, SP>>,
    ) -> Result<LlmpRestartingEventManager<EMH, I, S, SHM, SP>, Error> {
        Ok(LlmpRestartingEventManager {
            throttle: self.throttle,
            last_sent: Duration::from_secs(0),
            hooks: self.hooks,
            llmp,
            #[cfg(feature = "llmp_compression")]
            compressor: GzipCompressor::with_threshold(COMPRESS_THRESHOLD),
            configuration,
            event_buffer: Vec::with_capacity(INITIAL_EVENT_BUFFER_SIZE),
            staterestorer,
            save_state: LlmpShouldSaveState::OnRestart,
            phantom: PhantomData,
        })
    }

    /// Create an LLMP event manager on a port.
    /// It expects a broker to exist on this port.
    #[cfg(feature = "std")]
    pub fn build_on_port<I, S, SHM, SP>(
        self,
        shmem_provider: SP,
        port: u16,
        configuration: EventConfig,
        staterestorer: Option<StateRestorer<SHM, SP>>,
    ) -> Result<LlmpRestartingEventManager<EMH, I, S, SHM, SP>, Error>
    where
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let llmp = LlmpClient::create_attach_to_tcp(shmem_provider, port)?;
        Self::build_from_client(self, llmp, configuration, staterestorer)
    }

    /// If a client respawns, it may reuse the existing connection, previously
    /// stored by [`LlmpClient::to_env()`].
    #[cfg(feature = "std")]
    pub fn build_existing_client_from_env<I, S, SHM, SP>(
        self,
        shmem_provider: SP,
        env_name: &str,
        configuration: EventConfig,
        staterestorer: Option<StateRestorer<SHM, SP>>,
    ) -> Result<LlmpRestartingEventManager<EMH, I, S, SHM, SP>, Error>
    where
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let llmp = LlmpClient::on_existing_from_env(shmem_provider, env_name)?;
        Self::build_from_client(self, llmp, configuration, staterestorer)
    }

    /// Create an existing client from description
    pub fn build_existing_client_from_description<I, S, SHM, SP>(
        self,
        shmem_provider: SP,
        description: &LlmpClientDescription,
        configuration: EventConfig,
        staterestorer: Option<StateRestorer<SHM, SP>>,
    ) -> Result<LlmpRestartingEventManager<EMH, I, S, SHM, SP>, Error>
    where
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let llmp = LlmpClient::existing_client_from_description(shmem_provider, description)?;
        Self::build_from_client(self, llmp, configuration, staterestorer)
    }
}

impl<EMH, I, S, SHM, SP> LlmpRestartingEventManager<EMH, I, S, SHM, SP>
where
    S: Serialize,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    /// Write the config for a client `EventManager` to env vars, a new
    /// client can reattach using [`LlmpEventManagerBuilder::build_existing_client_from_env()`].
    ///
    /// # Safety
    /// This will write to process env. Should only be called from a single thread at a time.
    #[cfg(feature = "std")]
    pub unsafe fn to_env(&self, env_name: &str) {
        unsafe {
            self.llmp.to_env(env_name).unwrap();
        }
    }

    /// Get the staterestorer
    pub fn staterestorer(&self) -> &Option<StateRestorer<SHM, SP>> {
        &self.staterestorer
    }

    /// Get the staterestorer (mutable)
    pub fn staterestorer_mut(&mut self) -> &mut Option<StateRestorer<SHM, SP>> {
        &mut self.staterestorer
    }

    /// Save LLMP state and empty state in staterestorer
    pub fn intermediate_save(&mut self) -> Result<(), Error> {
        // First, reset the page to 0 so the next iteration can read read from the beginning of this page
        if let Some(sr) = &mut self.staterestorer {
            if self.save_state.oom_safe() {
                sr.reset();
                sr.save(&(None::<S>, &self.llmp.describe()?))?;
            }
        }

        Ok(())
    }

    /// Reset the state in state restorer
    pub fn staterestorer_reset(&mut self) -> Result<(), Error> {
        if let Some(sr) = &mut self.staterestorer {
            sr.reset();
        }

        Ok(())
    }

    /// Calling this function will tell the llmp broker that this client is exiting
    /// This should be called from the restarter not from the actual fuzzer client
    /// This function serves the same roll as the `LlmpClient.send_exiting()`
    /// However, from the the event restarter process it is forbidden to call `send_exiting()`
    /// (You can call it and it compiles but you should never do so)
    /// `send_exiting()` is exclusive to the fuzzer client.
    #[cfg(feature = "std")]
    pub fn detach_from_broker(&self, broker_port: u16) -> Result<(), Error> {
        let client_id = self.llmp.sender().id();
        let Ok(mut stream) = TcpStream::connect((IP_LOCALHOST, broker_port)) else {
            log::error!("Connection refused.");
            return Ok(());
        };
        // The broker tells us hello we don't care we just tell it our client died
        let TcpResponse::BrokerConnectHello {
            broker_shmem_description: _,
            hostname: _,
        } = recv_tcp_msg(&mut stream)?.try_into()?
        else {
            return Err(Error::illegal_state(
                "Received unexpected Broker Hello".to_string(),
            ));
        };
        let msg = TcpRequest::ClientQuit { client_id };
        // Send this mesasge off and we are leaving.
        match send_tcp_msg(&mut stream, &msg) {
            Ok(()) => (),
            Err(e) => log::error!("Failed to send tcp message {e:#?}"),
        }
        log::debug!("Asking the broker to be disconnected");
        Ok(())
    }
}

/// The kind of manager we're creating right now
#[derive(Debug, Clone)]
pub enum ManagerKind {
    /// Any kind will do
    Any,
    /// A client, getting messages from a local broker.
    Client {
        /// The client description
        client_description: ClientDescription,
    },
    /// An [`LlmpBroker`], forwarding the packets of local clients.
    Broker,
}

/// Sets up a restarting fuzzer, using the [`StdShMemProvider`], and standard features.
///
/// The restarting mgr is a combination of restarter and runner, that can be used on systems with and without `fork` support.
/// The restarter will spawn a new process each time the child crashes or timeouts.
#[expect(clippy::type_complexity)]
pub fn setup_restarting_mgr_std<I, MT, S>(
    monitor: MT,
    broker_port: u16,
    configuration: EventConfig,
) -> Result<
    (
        Option<S>,
        LlmpRestartingEventManager<(), I, S, StdShMem, StdShMemProvider>,
    ),
    Error,
>
where
    I: DeserializeOwned,
    MT: Monitor + Clone,
    S: Serialize + DeserializeOwned,
{
    RestartingMgr::builder()
        .shmem_provider(StdShMemProvider::new()?)
        .monitor(Some(monitor))
        .broker_port(broker_port)
        .configuration(configuration)
        .hooks(tuple_list!())
        .build()
        .launch()
}

/// Sets up a restarting fuzzer, using the [`StdShMemProvider`], and standard features.
///
/// The restarting mgr is a combination of restarter and runner, that can be used on systems with and without `fork` support.
/// The restarter will spawn a new process each time the child crashes or timeouts.
/// This one, additionally uses the timeobserver for the adaptive serialization
#[expect(clippy::type_complexity)]
pub fn setup_restarting_mgr_std_adaptive<I, MT, S>(
    monitor: MT,
    broker_port: u16,
    configuration: EventConfig,
) -> Result<
    (
        Option<S>,
        LlmpRestartingEventManager<(), I, S, StdShMem, StdShMemProvider>,
    ),
    Error,
>
where
    MT: Monitor + Clone,
    S: Serialize + DeserializeOwned,
    I: DeserializeOwned,
{
    RestartingMgr::builder()
        .shmem_provider(StdShMemProvider::new()?)
        .monitor(Some(monitor))
        .broker_port(broker_port)
        .configuration(configuration)
        .hooks(tuple_list!())
        .build()
        .launch()
}

/// Provides a `builder` which can be used to build a [`RestartingMgr`].
///
/// The [`RestartingMgr`] is is a combination of a
/// `restarter` and `runner`, that can be used on systems both with and without `fork` support. The
/// `restarter` will start a new process each time the child crashes or times out.
#[derive(TypedBuilder, Debug)]
pub struct RestartingMgr<EMH, I, MT, S, SP> {
    /// The shared memory provider to use for the broker or client spawned by the restarting
    /// manager.
    shmem_provider: SP,
    /// The configuration
    configuration: EventConfig,
    /// The monitor to use
    #[builder(default = None)]
    monitor: Option<MT>,
    /// The broker port to use
    #[builder(default = 1337_u16)]
    broker_port: u16,
    /// The address to connect to
    #[builder(default = None)]
    remote_broker_addr: Option<SocketAddr>,
    /// The type of manager to build
    #[builder(default = ManagerKind::Any)]
    kind: ManagerKind,
    /// The amount of external clients that should have connected (not counting our own tcp client)
    /// before this broker quits _after the last client exited_.
    /// If `None`, the broker will never quit when the last client exits, but run forever.
    ///
    /// So, if this value is `Some(2)`, the broker will not exit after client 1 connected and disconnected,
    /// but it will quit after client 2 connected and disconnected.
    #[builder(default = None)]
    exit_cleanly_after: Option<NonZeroUsize>,
    /// Tell the manager to serialize or not the state on restart
    #[builder(default = LlmpShouldSaveState::OnRestart)]
    serialize_state: LlmpShouldSaveState,
    /// The hooks passed to event manager:
    hooks: EMH,
    #[builder(setter(skip), default = PhantomData)]
    phantom_data: PhantomData<(EMH, I, S)>,
}

#[expect(clippy::type_complexity, clippy::too_many_lines)]
impl<EMH, I, MT, S, SP> RestartingMgr<EMH, I, MT, S, SP>
where
    EMH: EventManagerHooksTuple<I, S> + Copy + Clone,
    I: DeserializeOwned,
    MT: Monitor + Clone,
    S: Serialize + DeserializeOwned,
    SP: ShMemProvider,
{
    /// Launch the broker and the clients and fuzz
    pub fn launch(
        &mut self,
    ) -> Result<
        (
            Option<S>,
            LlmpRestartingEventManager<EMH, I, S, SP::ShMem, SP>,
        ),
        Error,
    > {
        // We start ourselves as child process to actually fuzz
        let (staterestorer, new_shmem_provider, core_id) = if std::env::var(_ENV_FUZZER_SENDER)
            .is_err()
        {
            let broker_things = |mut broker: LlmpBroker<_, SP::ShMem, SP>, remote_broker_addr| {
                if let Some(remote_broker_addr) = remote_broker_addr {
                    log::info!("B2b: Connecting to {:?}", &remote_broker_addr);
                    broker.inner_mut().connect_b2b(remote_broker_addr)?;
                }

                if let Some(exit_cleanly_after) = self.exit_cleanly_after {
                    broker.set_exit_after(exit_cleanly_after);
                }

                broker.loop_with_timeouts(Duration::from_secs(30), Some(Duration::from_millis(5)));

                #[cfg(feature = "llmp_debug")]
                log::info!("The last client quit. Exiting.");

                Err(Error::shutting_down())
            };
            // We get here if we are on Unix, or we are a broker on Windows (or without forks).
            let (mgr, core_id) = match &self.kind {
                ManagerKind::Any => {
                    let connection =
                        LlmpConnection::on_port(self.shmem_provider.clone(), self.broker_port)?;
                    match connection {
                        LlmpConnection::IsBroker { broker } => {
                            let llmp_hook =
                                StdLlmpEventHook::<I, MT>::new(self.monitor.take().unwrap())?;

                            // Yep, broker. Just loop here.
                            log::info!(
                                "Doing broker things. Run this tool again to start fuzzing in a client."
                            );

                            broker_things(
                                broker.add_hooks(tuple_list!(llmp_hook)),
                                self.remote_broker_addr,
                            )?;

                            return Err(Error::shutting_down());
                        }
                        LlmpConnection::IsClient { client } => {
                            let mgr: LlmpRestartingEventManager<EMH, I, S, SP::ShMem, SP> =
                                LlmpEventManagerBuilder::builder()
                                    .hooks(self.hooks)
                                    .build_from_client(client, self.configuration, None)?;
                            (mgr, None)
                        }
                    }
                }
                ManagerKind::Broker => {
                    let llmp_hook = StdLlmpEventHook::new(self.monitor.take().unwrap())?;

                    let broker = LlmpBroker::create_attach_to_tcp(
                        self.shmem_provider.clone(),
                        tuple_list!(llmp_hook),
                        self.broker_port,
                    )?;

                    broker_things(broker, self.remote_broker_addr)?;
                    unreachable!(
                        "The broker may never return normally, only on errors or when shutting down."
                    );
                }
                ManagerKind::Client { client_description } => {
                    // We are a client
                    let mgr = LlmpEventManagerBuilder::builder()
                        .hooks(self.hooks)
                        .build_on_port(
                            self.shmem_provider.clone(),
                            self.broker_port,
                            self.configuration,
                            None,
                        )?;

                    (mgr, Some(client_description.core_id()))
                }
            };

            if let Some(core_id) = core_id {
                let core_id: CoreId = core_id;
                log::info!("Setting core affinity to {core_id:?}");
                core_id.set_affinity()?;
            }

            // We are the fuzzer respawner in a llmp client
            // # Safety
            // There should only ever be one launcher thread.
            unsafe {
                mgr.to_env(_ENV_FUZZER_BROKER_CLIENT_INITIAL);
            }

            // First, create a channel from the current fuzzer to the next to store state between restarts.
            #[cfg(unix)]
            let staterestorer: StateRestorer<SP::ShMem, SP> =
                StateRestorer::new(self.shmem_provider.new_shmem(256 * 1024 * 1024)?);

            #[cfg(not(unix))]
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

                // On Unix, we fork (when fork feature is enabled)
                #[cfg(all(unix, feature = "fork"))]
                let child_status = {
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
                            log::debug!(
                                "{} has been forked into {}",
                                std::os::unix::process::parent_id(),
                                std::process::id()
                            );
                            self.shmem_provider.post_fork(true)?;
                            break (staterestorer, self.shmem_provider.clone(), core_id);
                        }
                    }
                };

                // If this guy wants to fork, then ignore sigint
                #[cfg(any(windows, not(feature = "fork")))]
                unsafe {
                    #[cfg(windows)]
                    libafl_bolts::os::windows_exceptions::signal(
                        libafl_bolts::os::windows_exceptions::SIGINT,
                        libafl_bolts::os::windows_exceptions::sig_ign(),
                    );

                    #[cfg(unix)]
                    libc::signal(libc::SIGINT, libc::SIG_IGN);
                }

                // On Windows (or in any case without fork), we spawn ourself again
                #[cfg(any(windows, not(feature = "fork")))]
                let child_status = startable_self()?.status()?;
                #[cfg(any(windows, not(feature = "fork")))]
                let child_status = child_status.code().unwrap_or_default();

                compiler_fence(Ordering::SeqCst); // really useful?

                if child_status == CTRL_C_EXIT || staterestorer.wants_to_exit() {
                    // if ctrl-c is pressed, we end up in this branch
                    if let Err(err) = mgr.detach_from_broker(self.broker_port) {
                        log::error!("Failed to detach from broker: {err}");
                    }
                    return Err(Error::shutting_down());
                }

                if !staterestorer.has_content() && !self.serialize_state.oom_safe() {
                    if let Err(err) = mgr.detach_from_broker(self.broker_port) {
                        log::error!("Failed to detach from broker: {err}");
                    }
                    #[cfg(unix)]
                    assert_ne!(
                        9, child_status,
                        "Target received SIGKILL!. This could indicate the target crashed due to OOM, user sent SIGKILL, or the target was in an unrecoverable situation and could not save state to restart"
                    );
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

        if let Some(core_id) = core_id {
            let core_id: CoreId = core_id;
            core_id.set_affinity()?;
        }

        // If we're restarting, deserialize the old state.
        let (state, mut mgr) =
            if let Some((state_opt, mgr_description)) = staterestorer.restore()? {
                (
                    state_opt,
                    LlmpEventManagerBuilder::builder()
                        .hooks(self.hooks)
                        .save_state(self.serialize_state)
                        .build_existing_client_from_description(
                            new_shmem_provider,
                            &mgr_description,
                            self.configuration,
                            Some(staterestorer),
                        )?,
                )
            } else {
                log::info!("First run. Let's set it all up");
                // Mgr to send and receive msgs from/to all other fuzzer instances
                (
                    None,
                    LlmpEventManagerBuilder::builder()
                        .hooks(self.hooks)
                        .save_state(self.serialize_state)
                        .build_existing_client_from_env(
                            new_shmem_provider,
                            _ENV_FUZZER_BROKER_CLIENT_INITIAL,
                            self.configuration,
                            Some(staterestorer),
                        )?,
                )
            };
        // We reset the staterestorer, the next staterestorer and receiver (after crash) will reuse the page from the initial message.
        if self.serialize_state.oom_safe() {
            mgr.intermediate_save()?;
        } else {
            mgr.staterestorer_reset()?;
        }

        /* TODO: Not sure if this is needed
        // We commit an empty NO_RESTART message to this buf, against infinite loops,
        // in case something crashes in the fuzzer.
        staterestorer.send_buf(_LLMP_TAG_NO_RESTART, []);
        */

        Ok((state, mgr))
    }
}

#[cfg(test)]
mod tests {
    use core::sync::atomic::{Ordering, compiler_fence};

    use libafl_bolts::{
        ClientId,
        llmp::{LlmpClient, LlmpSharedMap},
        rands::StdRand,
        shmem::{ShMemProvider, StdShMem, StdShMemProvider},
        staterestore::StateRestorer,
        tuples::tuple_list,
    };
    use serial_test::serial;

    use crate::{
        StdFuzzer,
        corpus::{Corpus, InMemoryCorpus, Testcase},
        events::llmp::restarting::{_ENV_FUZZER_SENDER, LlmpEventManagerBuilder},
        executors::{ExitKind, InProcessExecutor},
        feedbacks::ConstFeedback,
        fuzzer::Fuzzer,
        inputs::BytesInput,
        mutators::BitFlipMutator,
        observers::TimeObserver,
        schedulers::RandScheduler,
        stages::StdMutationalStage,
        state::StdState,
    };

    #[test]
    #[serial]
    #[cfg_attr(miri, ignore)]
    fn test_mgr_state_restore() {
        // # Safety
        // The same testcase doesn't usually run twice
        #[cfg(any(not(feature = "serdeany_autoreg"), miri))]
        unsafe {
            crate::stages::RetryCountRestartHelper::register();
        }

        let rand = StdRand::with_seed(0);

        let time = TimeObserver::new("time");

        let mut corpus = InMemoryCorpus::<BytesInput>::new();
        let testcase = Testcase::new(vec![0; 4].into());
        corpus.add(testcase).unwrap();

        let solutions = InMemoryCorpus::<BytesInput>::new();

        let mut feedback = ConstFeedback::new(false);
        let mut objective = ConstFeedback::new(false);

        let mut state =
            StdState::new(rand, corpus, solutions, &mut feedback, &mut objective).unwrap();

        let mut shmem_provider = StdShMemProvider::new().unwrap();

        let mut llmp_client = LlmpClient::new(
            shmem_provider.clone(),
            LlmpSharedMap::new(ClientId(0), shmem_provider.new_shmem(1024).unwrap()),
            ClientId(0),
        )
        .unwrap();

        // A little hack for CI. Don't do that in a real-world scenario.
        unsafe {
            llmp_client.mark_safe_to_unmap();
        }

        let mut llmp_mgr = LlmpEventManagerBuilder::builder()
            .build_from_client(llmp_client, "fuzzer".into(), None)
            .unwrap();

        let scheduler = RandScheduler::new();

        let feedback = ConstFeedback::new(true);
        let objective = ConstFeedback::new(false);

        let mut fuzzer = StdFuzzer::new(scheduler, feedback, objective);

        let mut harness = |_buf: &BytesInput| ExitKind::Ok;
        let mut executor = InProcessExecutor::new(
            &mut harness,
            tuple_list!(time),
            &mut fuzzer,
            &mut state,
            &mut llmp_mgr,
        )
        .unwrap();

        let mutator = BitFlipMutator::new();
        let mut stages = tuple_list!(StdMutationalStage::new(mutator));

        // First, create a channel from the current fuzzer to the next to store state between restarts.
        let mut staterestorer = StateRestorer::<StdShMem, StdShMemProvider>::new(
            shmem_provider.new_shmem(256 * 1024 * 1024).unwrap(),
        );

        staterestorer.reset();
        staterestorer
            .save(&(&mut state, &llmp_mgr.llmp.describe().unwrap()))
            .unwrap();
        assert!(staterestorer.has_content());

        // Store the information to a map.
        // # Safety
        // Single-threaded test code
        unsafe {
            staterestorer.write_to_env(_ENV_FUZZER_SENDER).unwrap();
        }

        compiler_fence(Ordering::SeqCst);

        let sc_cpy = StateRestorer::from_env(&mut shmem_provider, _ENV_FUZZER_SENDER).unwrap();
        assert!(sc_cpy.has_content());

        let (mut state_clone, mgr_description) = staterestorer.restore().unwrap().unwrap();
        let mut llmp_clone = LlmpEventManagerBuilder::builder()
            .build_existing_client_from_description(
                shmem_provider,
                &mgr_description,
                "fuzzer".into(),
                None,
            )
            .unwrap();

        fuzzer
            .fuzz_one(
                &mut stages,
                &mut executor,
                &mut state_clone,
                &mut llmp_clone,
            )
            .unwrap();
    }
}
