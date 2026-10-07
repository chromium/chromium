//! TCP-backed event manager for scalable multi-processed fuzzing

use alloc::{sync::Arc, vec::Vec};
use core::{marker::PhantomData, net::SocketAddr, num::NonZeroUsize, time::Duration};
use std::{
    env,
    io::{Read, Write},
    net::{TcpListener, TcpStream, ToSocketAddrs},
};

#[cfg(feature = "tcp_compression")]
use libafl_bolts::compress::GzipCompressor;
use libafl_bolts::{
    ClientId,
    core_affinity::CoreId,
    shmem::{ShMemProvider, StdShMemProvider},
    tuples::tuple_list,
};
use serde::{Serialize, de::DeserializeOwned};
use tokio::{
    io::{AsyncReadExt, AsyncWriteExt},
    sync::{broadcast, broadcast::error::RecvError, mpsc},
    task::{JoinHandle, spawn},
};

use super::{AwaitRestartSafe, SendExiting, std_maybe_report_progress, std_report_progress};

/// The env var that tells the client that it is the initial client
pub const _ENV_FUZZER_BROKER_CLIENT_INITIAL: &str = "_AFL_ENV_FUZZER_BROKER_CLIENT_INITIAL";
/// The env var that tells the client that it is a sender
pub const _ENV_FUZZER_SENDER: &str = "_AFL_ENV_FUZZER_SENDER";

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
        HasCorpus, HasCurrentStageId, HasCurrentTestcase, HasExecutions, HasImported,
        HasLastReportTime, HasSolutions, MaybeHasClientPerfMonitor, Stoppable,
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
                if let Some(max_clients) = exit_cleanly_after
                    && max_clients.get() <= recv_handles.len()
                {
                    // we waited for all the clients we wanted to see attached. Now wait for them to close their tcp connections.
                    reached_max = true;
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
                        log::debug!("TCP Manager - len = {len:?}");

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
                        log::info!(
                            "TCP Manager - Forwarding message from {this_client_id:?} (buf starts with {:?})",
                            &buf[..4]
                        );

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
                event,
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
        event: EventWithStats<I>,
    ) -> Result<BrokerEventResult, Error> {
        let (event, stats) = event.into_parts();

        client_stats_manager.client_stats_insert(client_id)?;
        client_stats_manager.update_client_stats_for(client_id, |client_stat| {
            client_stat.update_executions(stats.executions, stats.time);
        })?;

        match event {
            Event::NewTestcase {
                corpus_size,
                forward_id,
                ..
            } => {
                let id = if let Some(id) = forward_id {
                    id
                } else {
                    client_id
                };
                client_stats_manager.client_stats_insert(id)?;
                client_stats_manager.update_client_stats_for(id, |client| {
                    client.update_corpus_size(corpus_size as u64);
                })?;
                monitor.display(client_stats_manager, event.name(), id)?;
                Ok(BrokerEventResult::Forward)
            }
            Event::Heartbeat => Ok(BrokerEventResult::Handled),
            Event::UpdateUserStats {
                ref name,
                ref value,
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
                ref introspection_stats,
                phantom: _,
            } => {
                // TODO: The monitor buffer should be added on client add.

                // Get the client for the staterestorer ID
                client_stats_manager.client_stats_insert(client_id)?;
                client_stats_manager.update_client_stats_for(client_id, |client| {
                    // Update the performance monitor for this client
                    client.update_introspection_stats(introspection_stats);
                })?;

                // Display the monitor via `.display` only on core #1
                monitor.display(client_stats_manager, event.name(), client_id)?;

                // Correctly handled the event
                Ok(BrokerEventResult::Handled)
            }
            Event::Objective { objective_size, .. } => {
                client_stats_manager.client_stats_insert(client_id)?;
                client_stats_manager.update_client_stats_for(client_id, |client_stat| {
                    client_stat.update_objective_size(objective_size as u64);
                })?;
                monitor.display(client_stats_manager, event.name(), client_id)?;
                Ok(BrokerEventResult::Handled)
            }
            Event::Log {
                severity_level,
                message,
                phantom: _,
            } => {
                // TODO rely on Monitor
                log::log!(severity_level.into(), "{message}");
                Ok(BrokerEventResult::Handled)
            }
            Event::UpdateUserStatsMap {
                ref stats,
                phantom: _,
            } => {
                client_stats_manager.client_stats_insert(client_id)?;
                // Collect keys to avoid borrowing stats while updating (if we needed to, but here we clone keys anyway)
                let keys: Vec<_> = stats.keys().cloned().collect();

                client_stats_manager.update_client_stats_for(client_id, |client| {
                    for (name, value) in stats {
                        client.update_user_stats(name.clone(), value.clone());
                    }
                })?;
                for name in keys {
                    client_stats_manager.aggregate(&name);
                }
                monitor.display(client_stats_manager, event.name(), client_id)?;
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
    /// If the state should be saved on restart
    save_state: bool,
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
    save_state: bool,
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
            save_state: false,
            phantom: PhantomData,
        }
    }

    /// Set the hooks
    #[must_use]
    pub fn hooks<EMH>(self, hooks: EMH) -> TcpEventManagerBuilder<EMH, I, S> {
        TcpEventManagerBuilder {
            throttle: self.throttle,
            hooks,
            save_state: self.save_state,
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

    /// Set if the state must be saved on restart
    #[must_use]
    pub fn save_state(mut self, save_state: bool) -> Self {
        self.save_state = save_state;
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

        if client_id.0 == 0xffffffff {
            log::error!("Client ID is UNDEFINED in build_from_client!");
        }

        let mgr = TcpEventManager {
            throttle: self.throttle,
            last_sent: Duration::from_secs(0),
            hooks: self.hooks,
            tcp,
            client_id,
            #[cfg(feature = "tcp_compression")]
            compressor: GzipCompressor::new(),
            configuration,
            save_state: self.save_state,
            phantom: PhantomData,
        };

        Ok(mgr)
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
    /// Writes to env variables and may only be done single-threaded.
    pub unsafe fn to_env(&self, env_name: &str) {
        unsafe {
            env::set_var(env_name, format!("{}", self.client_id.0));
        }
    }
}

impl<EMH, I, S, SP> crate::events::Restorable<S, SP> for TcpEventManager<EMH, I, S>
where
    EMH: EventManagerHooksTuple<I, S>,
    S: HasExecutions + HasCurrentStageId + Serialize,
    SP: ShMemProvider,
{
    type RestartState = Option<ClientId>;

    fn on_restart(&mut self, _state: &mut S) -> Result<(bool, Self::RestartState), Error> {
        let should_save = self.save_state;
        let inner_state = Some(self.client_id);

        Ok((should_save, inner_state))
    }
}

impl<EMH, I, S> TcpEventManager<EMH, I, S> {
    /// Send information that this client is exiting.
    /// The other side may free up all allocated memory.
    /// We are no longer allowed to send anything afterwards.
    pub fn send_exiting(&mut self) -> Result<(), Error> {
        Ok(())
    }

    /// Restore the state from the restart state
    pub fn on_restore(
        &mut self,
        _state: &mut S,
        restart_state: &Option<ClientId>,
    ) -> Result<(), Error> {
        // TODO: Don't ignore state
        if let Some(client_id) = restart_state {
            self.client_id = *client_id;
        }
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

                    assert_ne!(
                        self_id, other_client_id,
                        "Own ID should never have been sent by the broker"
                    );

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
                #[allow(clippy::std_instead_of_core)]
                Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {
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
pub type TcpRestartingEventManager<EMH, I, S, SP> =
    crate::events::RestartingEventManager<TcpEventManager<EMH, I, S>, SP>;

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
#[allow(clippy::type_complexity)]
pub fn setup_restarting_mgr_tcp<I, MT, S>(
    monitor: MT,
    broker_port: u16,
    configuration: EventConfig,
) -> Result<
    (
        Option<S>,
        TcpRestartingEventManager<(), I, S, StdShMemProvider>,
    ),
    Error,
>
where
    MT: Monitor,
    S: HasExecutions
        + HasMetadata
        + HasImported
        + HasSolutions<I>
        + HasCurrentTestcase<I>
        + DeserializeOwned
        + Serialize
        + Stoppable
        + HasLastReportTime
        + MaybeHasClientPerfMonitor
        + HasCorpus<I>
        + HasCurrentStageId,
    I: Input,
{
    let shmem_provider = StdShMemProvider::new().expect("Failed to init shared memory");

    setup_restarting_mgr_tcp_internal(
        shmem_provider,
        configuration,
        Some(monitor),
        broker_port,
        TcpManagerKind::Any,
        None,
        true,
        tuple_list!(),
    )
}

/// Sets up a restarting fuzzer, using the [`StdShMemProvider`], and standard features.
#[allow(clippy::too_many_arguments)]
#[allow(clippy::type_complexity)]
#[allow(clippy::needless_pass_by_value)]
pub fn setup_restarting_mgr_tcp_internal<EMH, I, MT, S, SP>(
    shmem_provider: SP,
    configuration: EventConfig,
    mut monitor: Option<MT>,
    broker_port: u16,
    kind: TcpManagerKind,
    exit_cleanly_after: Option<NonZeroUsize>,
    serialize_state: bool,
    hooks: EMH,
) -> Result<(Option<S>, TcpRestartingEventManager<EMH, I, S, SP>), Error>
where
    EMH: EventManagerHooksTuple<I, S> + Copy + Clone,
    I: Input,
    MT: Monitor,
    S: HasExecutions
        + HasMetadata
        + HasImported
        + HasSolutions<I>
        + HasCurrentTestcase<I>
        + DeserializeOwned
        + Serialize
        + Stoppable
        + HasLastReportTime
        + MaybeHasClientPerfMonitor
        + HasCorpus<I>
        + HasCurrentStageId,
    SP: ShMemProvider,
{
    // We start ourself as child process to actually fuzz
    let (_mgr, _core_id) = if env::var(_ENV_FUZZER_SENDER).is_err() {
        let broker_things = |mut broker: TcpEventBroker<I, MT>,
                             _remote_broker_addr: Option<SocketAddr>| {
            if let Some(exit_cleanly_after) = exit_cleanly_after {
                broker.set_exit_cleanly_after(exit_cleanly_after);
            }
            broker.broker_loop()
        };

        // We get here if we are on Unix, or we are a broker on Windows (or without forks).
        let (_mgr, _core_id) = match kind {
            TcpManagerKind::Any => {
                let connection = create_nonblocking_listener(("127.0.0.1", broker_port));
                match connection {
                    Ok(listener) => {
                        let event_broker = TcpEventBroker::<I, MT>::with_listener(
                            listener,
                            monitor.take().unwrap(),
                        );

                        // Yep, broker. Just loop here.
                        log::info!(
                            "Doing broker things. Run this tool again to start fuzzing in a client."
                        );

                        broker_things(event_broker, None)?;

                        return Err(Error::shutting_down());
                    }
                    Err(Error::OsError(..)) => {
                        // port was likely already bound
                        let mgr = TcpEventManagerBuilder::new()
                            .hooks(hooks)
                            .build_from_client(
                                &("127.0.0.1", broker_port),
                                UNDEFINED_CLIENT_ID,
                                configuration,
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
                    format!("127.0.0.1:{broker_port}"),
                    monitor.take().unwrap(),
                )?;

                broker_things(event_broker, None)?;
                unreachable!(
                    "The broker may never return normally, only on errors or when shutting down."
                );
            }
            TcpManagerKind::Client { cpu_core } => {
                // We are a client
                let mgr = TcpEventManagerBuilder::new().hooks(hooks).build_on_port(
                    broker_port,
                    UNDEFINED_CLIENT_ID,
                    configuration,
                )?;

                (mgr, cpu_core)
            }
        };

        if let Some(core_id) = _core_id {
            let core_id: CoreId = core_id;
            log::info!("Setting core affinity to {core_id:?}");
            core_id.set_affinity()?;
        }

        // We are the fuzzer respawner in a tcp client
        // # Safety
        // There should only ever be one thread doing launcher things.
        unsafe {
            _mgr.to_env(_ENV_FUZZER_BROKER_CLIENT_INITIAL);
        }

        (_mgr, _core_id)
    } else {
        // We are the newly started fuzzing instance (i.e. on Windows), first, connect to our own restore map.
        // We get here *only on Windows*, if we were started by a restarting fuzzer.
        // A staterestorer and a receiver for single communication
        let _mgr = TcpEventManagerBuilder::new()
            .hooks(hooks)
            .build_existing_from_env(
                &("127.0.0.1", broker_port),
                _ENV_FUZZER_BROKER_CLIENT_INITIAL,
                configuration,
            )?;

        (_mgr, None)
    };

    // We start ourself as child process to actually fuzz
    let restarting_mgr = crate::events::RestartingMgr::new(shmem_provider.clone());
    #[cfg(unix)]
    let mut restarting_mgr = restarting_mgr;
    #[cfg(unix)]
    restarting_mgr.fork(true);

    crate::events::restarting::setup_generic_restarting_mgr(
        restarting_mgr,
        |state: Option<Option<ClientId>>| {
            let this_id = state.flatten().unwrap_or(UNDEFINED_CLIENT_ID);

            let (_mgr, _core_id) = if env::var(_ENV_FUZZER_SENDER).is_err() {
                let broker_things =
                    |mut broker: TcpEventBroker<I, MT>, _remote_broker_addr: Option<SocketAddr>| {
                        if let Some(exit_cleanly_after) = exit_cleanly_after {
                            broker.set_exit_cleanly_after(exit_cleanly_after);
                        }
                        broker.broker_loop()
                    };

                // We get here if we are on Unix, or we are a broker on Windows (or without forks).
                let (_mgr, _core_id) = match kind {
                    TcpManagerKind::Any => {
                        let connection = create_nonblocking_listener(("127.0.0.1", broker_port));
                        match connection {
                            Ok(listener) => {
                                let event_broker = TcpEventBroker::<I, MT>::with_listener(
                                    listener,
                                    monitor.take().unwrap(),
                                );

                                // Yep, broker. Just loop here.
                                log::info!(
                                    "Doing broker things. Run this tool again to start fuzzing in a client."
                                );

                                broker_things(event_broker, None)?;

                                return Err(Error::shutting_down());
                            }
                            Err(Error::OsError(..)) => {
                                // port was likely already bound
                                let mgr = TcpEventManagerBuilder::new()
                                    .hooks(hooks)
                                    .save_state(serialize_state)
                                    .build_from_client(
                                        &("127.0.0.1", broker_port),
                                        this_id,
                                        configuration,
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
                            format!("127.0.0.1:{broker_port}"),
                            monitor.take().unwrap(),
                        )?;

                        broker_things(event_broker, None)?;
                        unreachable!(
                            "The broker may never return normally, only on errors or when shutting down."
                        );
                    }
                    TcpManagerKind::Client { cpu_core } => {
                        // We are a client
                        let mgr = TcpEventManagerBuilder::new()
                            .hooks(hooks)
                            .save_state(serialize_state)
                            .build_on_port(broker_port, this_id, configuration)?;

                        (mgr, cpu_core)
                    }
                };

                if let Some(core_id) = _core_id {
                    let core_id: CoreId = core_id;
                    log::info!("Setting core affinity to {core_id:?}");
                    core_id.set_affinity()?;
                }

                // We are the fuzzer respawner in a tcp client
                // # Safety
                // There should only ever be one thread doing launcher things.
                unsafe {
                    _mgr.to_env(_ENV_FUZZER_BROKER_CLIENT_INITIAL);
                }

                (_mgr, _core_id)
            } else {
                // We are the newly started fuzzing instance (i.e. on Windows), first, connect to our own restore map.
                // We get here *only on Windows*, if we were started by a restarting fuzzer.
                // A staterestorer and a receiver for single communication
                let mut _mgr = TcpEventManagerBuilder::new()
                    .hooks(hooks)
                    .save_state(serialize_state)
                    .build_existing_from_env(
                        &("127.0.0.1", broker_port),
                        _ENV_FUZZER_BROKER_CLIENT_INITIAL,
                        configuration,
                    )?;

                if this_id.0 != 0 {
                    _mgr.client_id = ClientId(this_id.0);
                }

                (_mgr, None)
            };

            Ok(_mgr)
        },
    )
}
