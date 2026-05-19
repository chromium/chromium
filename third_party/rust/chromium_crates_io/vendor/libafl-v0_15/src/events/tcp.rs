//! TCP-backed event manager for scalable multi-processed fuzzing

use alloc::{sync::Arc, vec::Vec};
use core::{
    marker::PhantomData,
    net::SocketAddr,
    num::NonZeroUsize,
    sync::atomic::{Ordering, compiler_fence},
    time::Duration,
};
use std::{
    env,
    io::{ErrorKind, Read, Write},
    net::{TcpListener, TcpStream, ToSocketAddrs},
};

#[cfg(feature = "tcp_compression")]
use libafl_bolts::compress::GzipCompressor;
#[cfg(any(windows, not(feature = "fork")))]
use libafl_bolts::os::startable_self;
#[cfg(all(unix, not(miri)))]
use libafl_bolts::os::unix_signals::setup_signal_handler;
#[cfg(all(feature = "fork", unix))]
use libafl_bolts::os::{ForkResult, fork};
use libafl_bolts::{
    ClientId,
    core_affinity::CoreId,
    os::CTRL_C_EXIT,
    shmem::{ShMem, ShMemProvider, StdShMem, StdShMemProvider},
    staterestore::StateRestorer,
    tuples::tuple_list,
};
use serde::{Serialize, de::DeserializeOwned};
use tokio::{
    io::{AsyncReadExt, AsyncWriteExt},
    sync::{broadcast, broadcast::error::RecvError, mpsc},
    task::{JoinHandle, spawn},
};
use typed_builder::TypedBuilder;

use super::{AwaitRestartSafe, SendExiting, std_maybe_report_progress, std_report_progress};
#[cfg(all(unix, not(miri)))]
use crate::events::EVENTMGR_SIGHANDLER_STATE;
use crate::{
    Error, HasMetadata,
    events::{
        BrokerEventResult, Event, EventConfig, EventFirer, EventManagerHooksTuple, EventManagerId,
        EventReceiver, EventRestarter, EventWithStats, HasEventManagerId, ProgressReporter,
        std_on_restart,
    },
    inputs::Input,
    monitors::{Monitor, stats::ClientStatsManager},
    state::{
        HasCurrentStageId, HasCurrentTestcase, HasExecutions, HasImported, HasLastReportTime,
        HasSolutions, MaybeHasClientPerfMonitor, Stoppable,
    },
};

/// Tries to create (synchronously) a [`TcpListener`] that is `nonblocking` (for later use in tokio).
/// Will error if the port is already in use (or other errors occur)
fn create_nonblocking_listener<A: ToSocketAddrs>(addr: A) -> Result<TcpListener, Error> {
    let listener = TcpListener::bind(addr)?;
    listener.set_nonblocking(true)?;
    Ok(listener)
}

/// An TCP-backed event manager for simple multi-processed fuzzing
#[derive(Debug)]
pub struct TcpEventBroker<I, MT>
where
    I: Input,
    MT: Monitor,
    //CE: CustomEvent<I>,
{
    monitor: MT,
    /// A `nonblocking` [`TcpListener`] that we will `take` and convert to a Tokio listener in [`Self::broker_loop()`].
    listener: Option<TcpListener>,
    /// Amount of all clients ever, after which (when all are disconnected) this broker should quit.
    exit_cleanly_after: Option<NonZeroUsize>,
    client_stats_manager: ClientStatsManager,
    phantom: PhantomData<I>,
}

const UNDEFINED_CLIENT_ID: ClientId = ClientId(0xffffffff);

impl<I, MT> TcpEventBroker<I, MT>
where
    I: Input,
    MT: Monitor,
{
    /// Create a TCP broker, listening on the given address.
    pub fn new<A: ToSocketAddrs>(addr: A, monitor: MT) -> Result<Self, Error> {
        Ok(Self::with_listener(
            create_nonblocking_listener(addr)?,
            monitor,
        ))
    }

    /// Create a TCP broker, with a listener that needs to already be bound to an address.
    pub fn with_listener(listener: TcpListener, monitor: MT) -> Self {
        Self {
            listener: Some(listener),
            monitor,
            client_stats_manager: ClientStatsManager::default(),
            phantom: PhantomData,
            exit_cleanly_after: None,
        }
    }

    /// Exit the broker process cleanly after at least `n` clients attached and all of them disconnected again
    pub fn set_exit_cleanly_after(&mut self, n_clients: NonZeroUsize) {
        self.exit_cleanly_after = Some(n_clients);
    }

    /// Run in the broker until all clients exit
    // TODO: remove expect(clippy::needless_return) when clippy is fixed
    #[tokio::main(flavor = "current_thread")]
    #[expect(clippy::too_many_lines)]
    pub async fn broker_loop(&mut self) -> Result<(), Error> {
        let (tx_bc, rx) = broadcast::channel(65536);
        let (tx, mut rx_mpsc) = mpsc::channel(65536);

        let exit_cleanly_after = self.exit_cleanly_after;

        let listener = self
            .listener
            .take()
            .ok_or_else(|| Error::illegal_state("Listener has already been used / was none"))?;
        let listener = tokio::net::TcpListener::from_std(listener)?;

        let tokio_broker = spawn(async move {
            let mut recv_handles: Vec<JoinHandle<_>> = vec![];
            let mut receivers: Vec<Arc<tokio::sync::Mutex<broadcast::Receiver<_>>>> = vec![];

            loop {
                let mut reached_max = false;
                if let Some(max_clients) = exit_cleanly_after {
                    if max_clients.get() <= recv_handles.len() {
                        // we waited for all the clients we wanted to see attached. Now wait for them to close their tcp connections.
                        reached_max = true;
                    }
                }

                // Asynchronously wait for an inbound socket.
                let (socket, _) = listener.accept().await.expect("Accept failed");
                let (mut read, mut write) = tokio::io::split(socket);

                // Protocol: the new client communicate its old ClientId or -1 if new
                let mut this_client_id = [0; 4];
                read.read_exact(&mut this_client_id)
                    .await
                    .expect("Socket closed?");
                let this_client_id = ClientId(u32::from_le_bytes(this_client_id));

                let (this_client_id, is_old) = if this_client_id == UNDEFINED_CLIENT_ID {
                    if reached_max {
                        (UNDEFINED_CLIENT_ID, false) // Dumb id
                    } else {
                        // ClientIds for this broker start at 0.
                        (ClientId(recv_handles.len().try_into().unwrap()), false)
                    }
                } else {
                    (this_client_id, true)
                };

                let this_client_id_bytes = this_client_id.0.to_le_bytes();

                // Protocol: Send the client id for this node;
                write.write_all(&this_client_id_bytes).await.unwrap();

                if !is_old && reached_max {
                    continue;
                }

                let tx_inner = tx.clone();

                let handle = async move {
                    // In a loop, read data from the socket and write the data back.
                    loop {
                        let mut len_buf = [0; 4];

                        if read.read_exact(&mut len_buf).await.is_err() {
                            // The socket is closed, the client is restarting
                            log::info!("Socket closed, client restarting");
                            return;
                        }

                        let mut len = u32::from_le_bytes(len_buf);
                        // we forward the sender id as well, so we add 4 bytes to the message length
                        len += 4;

                        log::debug!("TCP Manager - len +4 = {len:?}");

                        let mut buf = vec![0; len as usize];

                        if read
                            .read_exact(&mut buf)
                            .await
                            // .expect("Failed to read data from socket"); // TODO verify if we have to handle this error
                            .is_err()
                        {
                            // The socket is closed, the client is restarting
                            log::info!("Socket closed, client restarting");
                            return;
                        }

                        log::debug!("TCP Manager - len: {len:?} - {buf:?}");
                        tx_inner.send(buf).await.expect("Could not send");
                    }
                };

                let client_idx = this_client_id.0 as usize;

                // Keep all handles around.
                if is_old {
                    recv_handles[client_idx].abort();
                    recv_handles[client_idx] = spawn(handle);
                } else {
                    recv_handles.push(spawn(handle));
                    // Get old messages only if new
                    let rx_inner = Arc::new(tokio::sync::Mutex::new(rx.resubscribe()));
                    receivers.push(rx_inner.clone());
                }

                let rx_inner = receivers[client_idx].clone();

                // The forwarding end. No need to keep a handle to this (TODO: unless they don't quit/get stuck?)
                spawn(async move {
                    // In a loop, read data from the socket and write the data back.
                    loop {
                        let buf: Vec<u8> = match rx_inner.lock().await.recv().await {
                            Ok(buf) => buf,
                            Err(RecvError::Lagged(num)) => {
                                log::error!("Receiver lagged, skipping {num} messages");
                                continue;
                            }
                            _ => panic!("Could not receive"),
                        };

                        log::debug!("TCP Manager - {buf:?}");

                        if buf.len() <= 4 {
                            log::warn!("We got no contents (or only the length) in a broadcast");
                            continue;
                        }

                        if buf[..4] == this_client_id_bytes {
                            log::debug!(
                                "TCP Manager - Not forwarding message from this very client ({this_client_id:?})."
                            );
                            continue;
                        }

                        // subtract 4 since the client_id isn't part of the actual message.
                        let len = u32::try_from(buf.len() - 4).unwrap();
                        let len_buf: [u8; 4] = len.to_le_bytes();

                        // Write message length
                        if write.write_all(&len_buf).await.is_err() {
                            // The socket is closed, the client is restarting
                            log::info!("Socket closed, client restarting");
                            return;
                        }
                        // Write the rest
                        if write.write_all(&buf).await.is_err() {
                            // The socket is closed, the client is restarting
                            log::info!("Socket closed, client restarting");
                            return;
                        }
                    }
                });
            }

            /*log::info!("Joining handles..");
            // wait for all clients to exit/error out
            for recv_handle in recv_handles {
                drop(recv_handle.await);
            }*/
        });

        loop {
            let buf = rx_mpsc.recv().await.expect("Could not receive");

            // read client ID.
            let mut client_id_buf = [0_u8; 4];
            client_id_buf.copy_from_slice(&buf[..4]);

            let client_id = ClientId(u32::from_le_bytes(client_id_buf));

            // cut off the ID.
            let event_bytes = &buf[4..];

            #[cfg(feature = "tcp_compression")]
            let event_bytes = &GzipCompressor::new().decompress(event_bytes)?;

            let event: EventWithStats<I> = postcard::from_bytes(event_bytes)?;
            match Self::handle_in_broker(
                &mut self.monitor,
                &mut self.client_stats_manager,
                client_id,
                &event,
            )? {
                BrokerEventResult::Forward => {
                    tx_bc.send(buf).expect("Could not send");
                }
                BrokerEventResult::Handled => (),
            }

            if tokio_broker.is_finished() {
                tokio_broker.await.unwrap();
                break;
            }
        }
        log::info!("TCP Manager - The last client quit. Exiting.");

        Err(Error::shutting_down())
    }

    /// Handle arriving events in the broker
    fn handle_in_broker(
        monitor: &mut MT,
        client_stats_manager: &mut ClientStatsManager,
        client_id: ClientId,
        event: &EventWithStats<I>,
    ) -> Result<BrokerEventResult, Error> {
        let stats = event.stats();

        client_stats_manager.client_stats_insert(client_id)?;
        client_stats_manager.update_client_stats_for(client_id, |client_stat| {
            client_stat.update_executions(stats.executions, stats.time);
        })?;

        let event = event.event();
        match event {
            Event::NewTestcase {
                corpus_size,
                forward_id,
                ..
            } => {
                let id = if let Some(id) = *forward_id {
                    id
                } else {
                    client_id
                };
                client_stats_manager.client_stats_insert(id)?;
                client_stats_manager.update_client_stats_for(id, |client| {
                    client.update_corpus_size(*corpus_size as u64);
                })?;
                monitor.display(client_stats_manager, event.name(), id)?;
                Ok(BrokerEventResult::Forward)
            }
            Event::Heartbeat => Ok(BrokerEventResult::Handled),
            Event::UpdateUserStats {
                name,
                value,
                phantom: _,
            } => {
                client_stats_manager.client_stats_insert(client_id)?;
                client_stats_manager.update_client_stats_for(client_id, |client| {
                    client.update_user_stats(name.clone(), value.clone());
                })?;
                client_stats_manager.aggregate(name);
                monitor.display(client_stats_manager, event.name(), client_id)?;
                Ok(BrokerEventResult::Handled)
            }
            #[cfg(feature = "introspection")]
            Event::UpdatePerfMonitor {
                introspection_stats,
                phantom: _,
            } => {
                // TODO: The monitor buffer should be added on client add.

                // Get the client for the staterestorer ID
                client_stats_manager.client_stats_insert(client_id)?;
                client_stats_manager.update_client_stats_for(client_id, |client| {
                    // Update the performance monitor for this client
                    client.update_introspection_stats((**introspection_stats).clone());
                })?;

                // Display the monitor via `.display` only on core #1
                monitor.display(client_stats_manager, event.name(), client_id)?;

                // Correctly handled the event
                Ok(BrokerEventResult::Handled)
            }
            Event::Objective { objective_size, .. } => {
                client_stats_manager.client_stats_insert(client_id)?;
                client_stats_manager.update_client_stats_for(client_id, |client| {
                    client.update_objective_size(*objective_size as u64);
                })?;
                monitor.display(client_stats_manager, event.name(), client_id)?;
                Ok(BrokerEventResult::Handled)
            }
            Event::Log {
                severity_level,
                message,
                phantom: _,
            } => {
                let (_, _) = (severity_level, message);
                // TODO rely on Monitor
                log::log!((*severity_level).into(), "{message}");
                Ok(BrokerEventResult::Handled)
            }
            Event::Stop => Ok(BrokerEventResult::Forward),
            //_ => Ok(BrokerEventResult::Forward),
        }
    }
}

/// An `EventManager` that forwards all events to other attached via tcp.
pub struct TcpEventManager<EMH, I, S> {
    /// We send message every `throttle` second
    throttle: Option<Duration>,
    /// When we sent the last message
    last_sent: Duration,
    hooks: EMH,
    /// The TCP stream for inter process communication
    tcp: TcpStream,
    /// Our `CientId`
    client_id: ClientId,
    #[cfg(feature = "tcp_compression")]
    compressor: GzipCompressor,
    /// The configuration defines this specific fuzzer.
    /// A node will not re-use the observer values sent over TCP
    /// from nodes with other configurations.
    configuration: EventConfig,
    phantom: PhantomData<(I, S)>,
}

impl<I, S> TcpEventManager<(), I, S> {
    /// Create a builder for [`TcpEventManager`]
    #[must_use]
    pub fn builder() -> TcpEventManagerBuilder<(), I, S> {
        TcpEventManagerBuilder::new()
    }
}

/// Builder for `TcpEventManager`
#[derive(Debug, Copy, Clone)]
pub struct TcpEventManagerBuilder<EMH, I, S> {
    throttle: Option<Duration>,
    hooks: EMH,
    phantom: PhantomData<(I, S)>,
}

impl<I, S> Default for TcpEventManagerBuilder<(), I, S> {
    fn default() -> Self {
        Self::new()
    }
}

impl<I, S> TcpEventManagerBuilder<(), I, S> {
    /// Set the constructor
    #[must_use]
    pub fn new() -> Self {
        Self {
            throttle: None,
            hooks: (),
            phantom: PhantomData,
        }
    }

    /// Set the hooks
    #[must_use]
    pub fn hooks<EMH>(self, hooks: EMH) -> TcpEventManagerBuilder<EMH, I, S> {
        TcpEventManagerBuilder {
            throttle: self.throttle,
            hooks,
            phantom: PhantomData,
        }
    }
}

impl<EMH, I, S> TcpEventManagerBuilder<EMH, I, S> {
    /// Set the throttle
    #[must_use]
    pub fn throttle(mut self, throttle: Duration) -> Self {
        self.throttle = Some(throttle);
        self
    }

    /// Create a manager from a raw TCP client with hooks
    pub fn build_from_client<A: ToSocketAddrs>(
        self,
        addr: &A,
        client_id: ClientId,
        configuration: EventConfig,
    ) -> Result<TcpEventManager<EMH, I, S>, Error> {
        let mut tcp = TcpStream::connect(addr)?;

        let mut our_client_id_buf = client_id.0.to_le_bytes();
        tcp.write_all(&our_client_id_buf)
            .expect("Cannot write to the broker");

        tcp.read_exact(&mut our_client_id_buf)
            .expect("Cannot read from the broker");
        let client_id = ClientId(u32::from_le_bytes(our_client_id_buf));

        log::info!("Our client id: {client_id:?}");

        Ok(TcpEventManager {
            throttle: self.throttle,
            last_sent: Duration::from_secs(0),
            hooks: self.hooks,
            tcp,
            client_id,
            #[cfg(feature = "tcp_compression")]
            compressor: GzipCompressor::new(),
            configuration,
            phantom: PhantomData,
        })
    }

    /// Create an TCP event manager on a port specifying the client id with hooks
    ///
    /// If the port is not yet bound, it will act as a broker; otherwise, it
    /// will act as a client.
    pub fn build_on_port(
        self,
        port: u16,
        client_id: ClientId,
        configuration: EventConfig,
    ) -> Result<TcpEventManager<EMH, I, S>, Error> {
        Self::build_from_client(self, &("127.0.0.1", port), client_id, configuration)
    }

    /// Create an TCP event manager on a port specifying the client id from env with hooks
    ///
    /// If the port is not yet bound, it will act as a broker; otherwise, it
    /// will act as a client.
    pub fn build_existing_from_env<A: ToSocketAddrs>(
        self,
        addr: &A,
        env_name: &str,
        configuration: EventConfig,
    ) -> Result<TcpEventManager<EMH, I, S>, Error> {
        let this_id = ClientId(str::parse::<u32>(&env::var(env_name)?)?);
        Self::build_from_client(self, addr, this_id, configuration)
    }
}

impl<EMH, I, S> core::fmt::Debug for TcpEventManager<EMH, I, S> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let mut debug_struct = f.debug_struct("TcpEventManager");
        let debug = debug_struct.field("tcp", &self.tcp);
        //.field("custom_buf_handlers", &self.custom_buf_handlers)
        #[cfg(feature = "tcp_compression")]
        let debug = debug.field("compressor", &self.compressor);
        debug
            .field("configuration", &self.configuration)
            .field("phantom", &self.phantom)
            .finish_non_exhaustive()
    }
}

impl<EMH, I, S> Drop for TcpEventManager<EMH, I, S> {
    /// TCP clients will have to wait until their pages are mapped by somebody.
    fn drop(&mut self) {
        self.await_restart_safe();
    }
}

impl<EMH, I, S> TcpEventManager<EMH, I, S>
where
    EMH: EventManagerHooksTuple<I, S>,
    S: HasExecutions + HasMetadata + HasImported + Stoppable,
{
    /// Write the client id for a client `EventManager` to env vars
    ///
    /// # Safety
    /// This writes to env variables, and may only be done single-threaded
    pub unsafe fn to_env(&self, env_name: &str) {
        unsafe {
            env::set_var(env_name, format!("{}", self.client_id.0));
        }
    }
}

impl<EMH, I, S> TcpEventManager<EMH, I, S> {
    /// Send information that this client is exiting.
    /// The other side may free up all allocated memory.
    /// We are no longer allowed to send anything afterwards.
    pub fn send_exiting(&mut self) -> Result<(), Error> {
        //TODO: Should not be needed since TCP does that for us
        //self.tcp.sender.send_exiting()
        Ok(())
    }
}

impl<EMH, I, S> EventFirer<I, S> for TcpEventManager<EMH, I, S>
where
    EMH: EventManagerHooksTuple<I, S>,
    I: Serialize,
{
    fn should_send(&self) -> bool {
        if let Some(throttle) = self.throttle {
            libafl_bolts::current_time()
                .checked_sub(self.last_sent)
                .unwrap_or(throttle)
                >= throttle
        } else {
            true
        }
    }

    fn fire(&mut self, _state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        let serialized = postcard::to_allocvec(&event)?;

        #[cfg(feature = "tcp_compression")]
        let serialized = self.compressor.compress(&serialized);

        let size = u32::try_from(serialized.len())?;
        self.tcp.write_all(&size.to_le_bytes())?;
        self.tcp.write_all(&self.client_id.0.to_le_bytes())?;
        self.tcp.write_all(&serialized)?;

        self.last_sent = libafl_bolts::current_time();
        Ok(())
    }

    fn configuration(&self) -> EventConfig {
        self.configuration
    }
}

impl<EMH, I, S> EventRestarter<S> for TcpEventManager<EMH, I, S>
where
    S: HasCurrentStageId,
{
    fn on_restart(&mut self, state: &mut S) -> Result<(), Error> {
        std_on_restart(self, state)
    }
}

impl<EMH, I, S> EventReceiver<I, S> for TcpEventManager<EMH, I, S>
where
    EMH: EventManagerHooksTuple<I, S>,
    S: HasExecutions
        + HasMetadata
        + HasImported
        + HasSolutions<I>
        + HasCurrentTestcase<I>
        + Stoppable,
    I: DeserializeOwned,
{
    fn try_receive(&mut self, state: &mut S) -> Result<Option<(EventWithStats<I>, bool)>, Error> {
        // TODO: Get around local event copy by moving handle_in_client
        let self_id = self.client_id;
        let mut len_buf = [0_u8; 4];
        self.tcp.set_nonblocking(true).expect("set to non-blocking");
        // read all pending messages
        loop {
            match self.tcp.read_exact(&mut len_buf) {
                Ok(()) => {
                    self.tcp.set_nonblocking(false).expect("set to blocking");
                    let len = u32::from_le_bytes(len_buf);
                    let mut buf = vec![0_u8; 4_usize + len as usize];
                    self.tcp.read_exact(&mut buf)?;

                    let mut client_id_buf = [0_u8; 4];
                    client_id_buf.copy_from_slice(&buf[..4]);

                    let other_client_id = ClientId(u32::from_le_bytes(client_id_buf));

                    self.tcp.set_nonblocking(true).expect("set to non-blocking");
                    if self_id == other_client_id {
                        panic!("Own ID should never have been sent by the broker");
                    } else {
                        log::info!("{self_id:?} (from {other_client_id:?}) Received: {buf:?}");

                        let buf = &buf[4..];
                        #[cfg(feature = "tcp_compression")]
                        let buf = &self.compressor.decompress(buf)?;

                        // make decompressed vec and slice compatible
                        let event: EventWithStats<I> = postcard::from_bytes(buf)?;

                        if !self.hooks.pre_receive_all(state, other_client_id, &event)? {
                            continue;
                        }
                        match event.event() {
                            Event::NewTestcase {
                                client_config,
                                observers_buf,
                                forward_id,
                                ..
                            } => {
                                log::info!(
                                    "Received new Testcase from {other_client_id:?} ({client_config:?}, forward {forward_id:?})"
                                );
                                if client_config.match_with(&self.configuration)
                                    && observers_buf.is_some()
                                {
                                    return Ok(Some((event, true)));
                                }
                                return Ok(Some((event, false)));
                            }
                            Event::Objective { .. } => {
                                log::info!("Received new Objective");
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
                }
                Err(e) if e.kind() == ErrorKind::WouldBlock => {
                    // no new data on the socket
                    break;
                }
                Err(e) => {
                    panic!("Unexpected error {e:?}");
                }
            }
        }
        self.tcp.set_nonblocking(false).expect("set to blocking");
        Ok(None)
    }

    fn on_interesting(&mut self, _state: &mut S, _event: EventWithStats<I>) -> Result<(), Error> {
        Ok(())
    }
}

impl<EMH, I, S> AwaitRestartSafe for TcpEventManager<EMH, I, S> {
    /// The TCP client needs to wait until a broker has mapped all pages before shutting down.
    /// Otherwise, the OS may already have removed the shared maps.
    fn await_restart_safe(&mut self) {
        // wait until we can drop the message safely.
        //self.tcp.await_safe_to_unmap_blocking();
    }
}

impl<EMH, I, S> SendExiting for TcpEventManager<EMH, I, S> {
    fn send_exiting(&mut self) -> Result<(), Error> {
        //TODO: Should not be needed since TCP does that for us
        //self.tcp.sender.send_exiting()
        Ok(())
    }

    fn on_shutdown(&mut self) -> Result<(), Error> {
        self.send_exiting()
    }
}

impl<EMH, I, S> ProgressReporter<S> for TcpEventManager<EMH, I, S>
where
    EMH: EventManagerHooksTuple<I, S>,
    I: Serialize,
    S: HasExecutions + HasMetadata + HasLastReportTime + MaybeHasClientPerfMonitor,
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

impl<EMH, I, S> HasEventManagerId for TcpEventManager<EMH, I, S> {
    /// Gets the id assigned to this staterestorer.
    fn mgr_id(&self) -> EventManagerId {
        EventManagerId(self.client_id.0 as usize)
    }
}

/// A manager that can restart on the fly, storing states in-between (in `on_restart`)
#[derive(Debug)]
pub struct TcpRestartingEventManager<EMH, I, S, SHM, SP> {
    /// The embedded TCP event manager
    tcp_mgr: TcpEventManager<EMH, I, S>,
    /// The staterestorer to serialize the state for the next runner
    staterestorer: StateRestorer<SHM, SP>,
    /// Decide if the state restorer must save the serialized state
    save_state: bool,
}

impl<EMH, I, S, SHM, SP> ProgressReporter<S> for TcpRestartingEventManager<EMH, I, S, SHM, SP>
where
    EMH: EventManagerHooksTuple<I, S>,
    S: HasMetadata + HasExecutions + HasLastReportTime + MaybeHasClientPerfMonitor,
    I: Serialize,
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

impl<EMH, I, S, SHM, SP> EventFirer<I, S> for TcpRestartingEventManager<EMH, I, S, SHM, SP>
where
    EMH: EventManagerHooksTuple<I, S>,
    I: Serialize,
{
    fn should_send(&self) -> bool {
        self.tcp_mgr.should_send()
    }

    fn fire(&mut self, state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        // Check if we are going to crash in the event, in which case we store our current state for the next runner
        self.tcp_mgr.fire(state, event)
    }

    fn configuration(&self) -> EventConfig {
        self.tcp_mgr.configuration()
    }
}

impl<EMH, I, S, SHM, SP> SendExiting for TcpRestartingEventManager<EMH, I, S, SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn send_exiting(&mut self) -> Result<(), Error> {
        self.staterestorer.send_exiting();
        // Also inform the broker that we are about to exit.
        // This way, the broker can clean up the pages, and eventually exit.
        self.tcp_mgr.send_exiting()
    }

    fn on_shutdown(&mut self) -> Result<(), Error> {
        self.send_exiting()
    }
}

impl<EMH, I, S, SHM, SP> AwaitRestartSafe for TcpRestartingEventManager<EMH, I, S, SHM, SP>
where
    SHM: ShMem,
{
    /// The tcp client needs to wait until a broker mapped all pages, before shutting down.
    /// Otherwise, the OS may already have removed the shared maps,
    #[inline]
    fn await_restart_safe(&mut self) {
        self.tcp_mgr.await_restart_safe();
    }
}

impl<EMH, I, S, SHM, SP> EventRestarter<S> for TcpRestartingEventManager<EMH, I, S, SHM, SP>
where
    EMH: EventManagerHooksTuple<I, S>,
    S: HasExecutions + HasCurrentStageId + Serialize,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    /// Reset the single page (we reuse it over and over from pos 0), then send the current state to the next runner.
    fn on_restart(&mut self, state: &mut S) -> Result<(), Error> {
        state.on_restart()?;

        // First, reset the page to 0 so the next iteration can read read from the beginning of this page
        self.staterestorer.reset();
        self.staterestorer.save(&if self.save_state {
            Some((state, self.tcp_mgr.client_id))
        } else {
            None
        })?;

        self.await_restart_safe();
        Ok(())
    }
}

impl<EMH, I, S, SHM, SP> EventReceiver<I, S> for TcpRestartingEventManager<EMH, I, S, SHM, SP>
where
    EMH: EventManagerHooksTuple<I, S>,
    I: DeserializeOwned,
    S: HasExecutions
        + HasMetadata
        + HasImported
        + HasSolutions<I>
        + HasCurrentTestcase<I>
        + Stoppable,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn try_receive(&mut self, state: &mut S) -> Result<Option<(EventWithStats<I>, bool)>, Error> {
        self.tcp_mgr.try_receive(state)
    }

    fn on_interesting(&mut self, state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        self.tcp_mgr.on_interesting(state, event)
    }
}

impl<EMH, I, S, SHM, SP> HasEventManagerId for TcpRestartingEventManager<EMH, I, S, SHM, SP> {
    fn mgr_id(&self) -> EventManagerId {
        self.tcp_mgr.mgr_id()
    }
}

/// The tcp connection from the actual fuzzer to the process supervising it
const _ENV_FUZZER_SENDER: &str = "_AFL_ENV_FUZZER_SENDER";
const _ENV_FUZZER_RECEIVER: &str = "_AFL_ENV_FUZZER_RECEIVER";
/// The tcp (2 way) connection from a fuzzer to the broker (broadcasting all other fuzzer messages)
const _ENV_FUZZER_BROKER_CLIENT_INITIAL: &str = "_AFL_ENV_FUZZER_BROKER_CLIENT";

impl<EMH, I, S, SHM, SP> TcpRestartingEventManager<EMH, I, S, SHM, SP>
where
    EMH: EventManagerHooksTuple<I, S>,
{
    /// Create a new runner, the executed child doing the actual fuzzing.
    pub fn new(tcp_mgr: TcpEventManager<EMH, I, S>, staterestorer: StateRestorer<SHM, SP>) -> Self {
        Self {
            tcp_mgr,
            staterestorer,
            save_state: true,
        }
    }

    /// Create a new runner specifying if it must save the serialized state on restart.
    pub fn with_save_state(
        tcp_mgr: TcpEventManager<EMH, I, S>,
        staterestorer: StateRestorer<SHM, SP>,
        save_state: bool,
    ) -> Self {
        Self {
            tcp_mgr,
            staterestorer,
            save_state,
        }
    }

    /// Get the staterestorer
    pub fn staterestorer(&self) -> &StateRestorer<SHM, SP> {
        &self.staterestorer
    }

    /// Get the staterestorer (mutable)
    pub fn staterestorer_mut(&mut self) -> &mut StateRestorer<SHM, SP> {
        &mut self.staterestorer
    }
}

/// The kind of manager we're creating right now
#[derive(Debug, Copy, Clone)]
pub enum TcpManagerKind {
    /// Any kind will do
    Any,
    /// A client, getting messages from a local broker.
    Client {
        /// The CPU core ID of this client
        cpu_core: Option<CoreId>,
    },
    /// A broker, forwarding all packets of local clients via TCP.
    Broker,
}

/// Sets up a restarting fuzzer, using the [`StdShMemProvider`], and standard features.
///
/// The [`TcpRestartingEventManager`] is a combination of restarter and runner, that can be used on systems with and without `fork` support.
/// The restarter will spawn a new process each time the child crashes or timeouts.
#[expect(clippy::type_complexity)]
pub fn setup_restarting_mgr_tcp<I, MT, S>(
    monitor: MT,
    broker_port: u16,
    configuration: EventConfig,
) -> Result<
    (
        Option<S>,
        TcpRestartingEventManager<(), I, S, StdShMem, StdShMemProvider>,
    ),
    Error,
>
where
    MT: Monitor + Clone,
    S: HasExecutions
        + HasMetadata
        + HasImported
        + HasSolutions<I>
        + HasCurrentTestcase<I>
        + DeserializeOwned
        + Stoppable,
    I: Input,
{
    TcpRestartingMgr::builder()
        .shmem_provider(StdShMemProvider::new()?)
        .monitor(Some(monitor))
        .broker_port(broker_port)
        .configuration(configuration)
        .hooks(tuple_list!())
        .build()
        .launch()
}

/// Provides a `builder` which can be used to build a [`TcpRestartingMgr`].
///
/// The [`TcpRestartingMgr`] is a combination of a
/// `restarter` and `runner`, that can be used on systems both with and without `fork` support. The
/// `restarter` will start a new process each time the child crashes or times out.
#[derive(TypedBuilder, Debug)]
pub struct TcpRestartingMgr<EMH, I, MT, S, SP> {
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
    #[builder(default = TcpManagerKind::Any)]
    kind: TcpManagerKind,
    /// The amount of external clients that should have connected (not counting our own tcp client)
    /// before this broker quits _after the last client exited_.
    /// If `None`, the broker will never quit when the last client exits, but run forever.
    ///
    /// So, if this value is `Some(2)`, the broker will not exit after client 1 connected and disconnected,
    /// but it will quit after client 2 connected and disconnected.
    #[builder(default = None)]
    exit_cleanly_after: Option<NonZeroUsize>,
    /// Tell the manager to serialize or not the state on restart
    #[builder(default = true)]
    serialize_state: bool,
    /// The hooks for `handle_in_client`
    hooks: EMH,
    #[builder(setter(skip), default = PhantomData)]
    phantom_data: PhantomData<(I, S)>,
}

#[expect(clippy::type_complexity, clippy::too_many_lines)]
impl<EMH, I, MT, S, SP> TcpRestartingMgr<EMH, I, MT, S, SP>
where
    EMH: EventManagerHooksTuple<I, S> + Copy + Clone,
    I: Input,
    MT: Monitor + Clone,
    S: HasExecutions
        + HasMetadata
        + HasImported
        + HasSolutions<I>
        + HasCurrentTestcase<I>
        + DeserializeOwned
        + Stoppable,
    SP: ShMemProvider,
{
    /// Launch the restarting manager
    pub fn launch(
        &mut self,
    ) -> Result<
        (
            Option<S>,
            TcpRestartingEventManager<EMH, I, S, SP::ShMem, SP>,
        ),
        Error,
    > {
        // We start ourself as child process to actually fuzz
        let (staterestorer, _new_shmem_provider, core_id) = if env::var(_ENV_FUZZER_SENDER).is_err()
        {
            let broker_things = |mut broker: TcpEventBroker<I, MT>, _remote_broker_addr| {
                if let Some(exit_cleanly_after) = self.exit_cleanly_after {
                    broker.set_exit_cleanly_after(exit_cleanly_after);
                }

                broker.broker_loop()
            };

            // We get here if we are on Unix, or we are a broker on Windows (or without forks).
            let (mgr, core_id) = match self.kind {
                TcpManagerKind::Any => {
                    let connection = create_nonblocking_listener(("127.0.0.1", self.broker_port));
                    match connection {
                        Ok(listener) => {
                            let event_broker = TcpEventBroker::<I, MT>::with_listener(
                                listener,
                                self.monitor.take().unwrap(),
                            );

                            // Yep, broker. Just loop here.
                            log::info!(
                                "Doing broker things. Run this tool again to start fuzzing in a client."
                            );

                            broker_things(event_broker, self.remote_broker_addr)?;

                            return Err(Error::shutting_down());
                        }
                        Err(Error::OsError(..)) => {
                            // port was likely already bound
                            let mgr = TcpEventManagerBuilder::new()
                                .hooks(self.hooks)
                                .build_from_client(
                                    &("127.0.0.1", self.broker_port),
                                    UNDEFINED_CLIENT_ID,
                                    self.configuration,
                                )?;
                            (mgr, None)
                        }
                        Err(e) => {
                            return Err(e);
                        }
                    }
                }
                TcpManagerKind::Broker => {
                    let event_broker = TcpEventBroker::<I, MT>::new(
                        format!("127.0.0.1:{}", self.broker_port),
                        self.monitor.take().unwrap(),
                    )?;

                    broker_things(event_broker, self.remote_broker_addr)?;
                    unreachable!(
                        "The broker may never return normally, only on errors or when shutting down."
                    );
                }
                TcpManagerKind::Client { cpu_core } => {
                    // We are a client
                    let mgr = TcpEventManagerBuilder::new()
                        .hooks(self.hooks)
                        .build_on_port(self.broker_port, UNDEFINED_CLIENT_ID, self.configuration)?;

                    (mgr, cpu_core)
                }
            };

            if let Some(core_id) = core_id {
                let core_id: CoreId = core_id;
                log::info!("Setting core affinity to {core_id:?}");
                core_id.set_affinity()?;
            }

            // We are the fuzzer respawner in a tcp client
            // # Safety
            // There should only ever be one thread doing launcher things.
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
            // It's reasonable to assume launcher only gets called on a single thread.
            // If not, nothing too bad will happen.
            unsafe {
                staterestorer.write_to_env(_ENV_FUZZER_SENDER)?;
            }

            let mut ctr: u64 = 0;
            // Client->parent loop
            loop {
                log::info!("Spawning next client (id {ctr})");
                println!("Spawning next client (id {ctr}) {core_id:?}");

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
                            self.shmem_provider.post_fork(true)?;
                            break (staterestorer, self.shmem_provider.clone(), core_id);
                        }
                    }
                };

                // If this guy wants to fork, then ignore sigit
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

                compiler_fence(Ordering::SeqCst);

                if child_status == CTRL_C_EXIT || staterestorer.wants_to_exit() {
                    return Err(Error::shutting_down());
                }

                #[expect(clippy::manual_assert)]
                if !staterestorer.has_content() && self.serialize_state {
                    #[cfg(unix)]
                    if child_status == 137 {
                        // Out of Memory, see https://tldp.org/LDP/abs/html/exitcodes.html
                        // and https://github.com/AFLplusplus/LibAFL/issues/32 for discussion.
                        panic!(
                            "Fuzzer-respawner: The fuzzed target crashed with an out of memory error! Fix your harness, or switch to another executor (for example, a forkserver)."
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

        if let Some(core_id) = core_id {
            let core_id: CoreId = core_id;
            core_id.set_affinity()?;
        }

        // If we're restarting, deserialize the old state.
        let (state, mut mgr) = if let Some((state_opt, this_id)) = staterestorer.restore()? {
            (
                state_opt,
                TcpRestartingEventManager::with_save_state(
                    TcpEventManagerBuilder::new()
                        .hooks(self.hooks)
                        .build_on_port(self.broker_port, this_id, self.configuration)?,
                    staterestorer,
                    self.serialize_state,
                ),
            )
        } else {
            log::info!("First run. Let's set it all up");
            // Mgr to send and receive msgs from/to all other fuzzer instances
            let mgr = TcpEventManagerBuilder::new()
                .hooks(self.hooks)
                .build_existing_from_env(
                    &("127.0.0.1", self.broker_port),
                    _ENV_FUZZER_BROKER_CLIENT_INITIAL,
                    self.configuration,
                )?;

            (
                None,
                TcpRestartingEventManager::with_save_state(
                    mgr,
                    staterestorer,
                    self.serialize_state,
                ),
            )
        };
        // We reset the staterestorer, the next staterestorer and receiver (after crash) will reuse the page from the initial message.
        mgr.staterestorer.reset();

        /* TODO: Not sure if this is needed
        // We commit an empty NO_RESTART message to this buf, against infinite loops,
        // in case something crashes in the fuzzer.
        staterestorer.send_buf(_TCP_TAG_NO_RESTART, []);
        */

        Ok((state, mgr))
    }
}
