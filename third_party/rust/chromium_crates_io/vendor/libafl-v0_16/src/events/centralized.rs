//! Centralized event manager is a special event manager that will be used to achieve a more efficient message passing architecture.

// Some technical details..
// A very standard multi-process fuzzing using centralized event manager will consist of 4 components
// 1. The "fuzzer clients", the fuzzer that will do the "normal" fuzzing
// 2. The "centralized broker, the broker that gathers all the testcases from all the fuzzer clients
// 3. The "main evaluator", the evaluator node that will evaluate all the testcases pass by the centralized event manager to see if the testcases are worth propagating
// 4. The "main broker", the gathers the stats from the fuzzer clients and broadcast the newly found testcases from the main evaluator.

use alloc::string::String;
use core::{fmt::Debug, marker::PhantomData, time::Duration};
use std::process;

use libafl_bolts::{
    ClientId,
    llmp::{LlmpClient, LlmpClientDescription, Tag},
    shmem::{ShMem, ShMemProvider},
};
#[cfg(feature = "llmp_compression")]
use libafl_bolts::{
    compress::GzipCompressor,
    llmp::{LLMP_FLAG_COMPRESSED, LLMP_FLAG_INITIALIZED},
};

use super::{AwaitRestartSafe, EventWithStats};
#[cfg(feature = "llmp_compression")]
use crate::events::llmp::COMPRESS_THRESHOLD;
use crate::{
    Error,
    common::HasMetadata,
    events::{
        Event, EventConfig, EventFirer, EventManagerId, EventReceiver, EventRestarter,
        HasEventManagerId, LogSeverity, ProgressReporter, SendExiting, std_maybe_report_progress,
        std_report_progress,
    },
    inputs::Input,
    state::{HasExecutions, HasLastReportTime, MaybeHasClientPerfMonitor, Stoppable},
};

pub(crate) const _LLMP_TAG_TO_MAIN: Tag = Tag(0x3453453);

/// A wrapper manager to implement a main-secondary architecture with another broker
#[derive(Debug)]
pub struct CentralizedEventManager<EM, I, S, SHM, SP> {
    inner: EM,
    /// The centralized LLMP client for inter process communication
    client: LlmpClient<SHM, SP>,
    #[cfg(feature = "llmp_compression")]
    compressor: GzipCompressor,
    is_main: bool,
    phantom: PhantomData<(I, S)>,
}

impl CentralizedEventManager<(), (), (), (), ()> {
    /// Creates a builder for [`CentralizedEventManager`]
    #[must_use]
    pub fn builder() -> CentralizedEventManagerBuilder {
        CentralizedEventManagerBuilder::new()
    }
}

/// The builder or `CentralizedEventManager`
#[derive(Debug)]
pub struct CentralizedEventManagerBuilder {
    is_main: bool,
}

impl Default for CentralizedEventManagerBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl CentralizedEventManagerBuilder {
    /// The constructor
    #[must_use]
    pub fn new() -> Self {
        Self { is_main: false }
    }

    /// Make this a main evaluator node
    #[must_use]
    pub fn is_main(self, is_main: bool) -> Self {
        Self { is_main }
    }

    /// Creates a new [`CentralizedEventManager`].
    pub fn build_from_client<EM, I, S, SP>(
        self,
        inner: EM,
        client: LlmpClient<SP::ShMem, SP>,
    ) -> Result<CentralizedEventManager<EM, I, S, SP::ShMem, SP>, Error>
    where
        SP: ShMemProvider,
    {
        Ok(CentralizedEventManager {
            inner,
            client,
            #[cfg(feature = "llmp_compression")]
            compressor: GzipCompressor::with_threshold(COMPRESS_THRESHOLD),
            is_main: self.is_main,
            phantom: PhantomData,
        })
    }

    /// Create a centralized event manager on a port
    ///
    /// If the port is not yet bound, it will act as a broker; otherwise, it
    /// will act as a client.
    pub fn build_on_port<EM, I, S, SHM, SP>(
        self,
        inner: EM,
        shmem_provider: SP,
        port: u16,
    ) -> Result<CentralizedEventManager<EM, I, S, SHM, SP>, Error>
    where
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let client = LlmpClient::create_attach_to_tcp(shmem_provider, port)?;
        Self::build_from_client(self, inner, client)
    }

    /// If a client respawns, it may reuse the existing connection, previously
    /// stored by [`LlmpClient::to_env()`].
    pub fn build_existing_client_from_env<EM, I, S, SHM, SP>(
        self,
        inner: EM,
        shmem_provider: SP,
        env_name: &str,
    ) -> Result<CentralizedEventManager<EM, I, S, SHM, SP>, Error>
    where
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let client = LlmpClient::on_existing_from_env(shmem_provider, env_name)?;
        Self::build_from_client(self, inner, client)
    }

    /// Create an existing client from description
    pub fn existing_client_from_description<EM, I, S, SHM, SP>(
        self,
        inner: EM,
        shmem_provider: SP,
        description: &LlmpClientDescription,
    ) -> Result<CentralizedEventManager<EM, I, S, SHM, SP>, Error>
    where
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let client = LlmpClient::existing_client_from_description(shmem_provider, description)?;
        Self::build_from_client(self, inner, client)
    }
}

impl<EM, I, S, SHM, SP> EventFirer<I, S> for CentralizedEventManager<EM, I, S, SHM, SP>
where
    EM: HasEventManagerId + EventFirer<I, S>,
    S: Stoppable,
    I: Input,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn should_send(&self) -> bool {
        self.inner.should_send()
    }

    #[expect(clippy::match_same_arms)]
    fn fire(&mut self, state: &mut S, mut event: EventWithStats<I>) -> Result<(), Error> {
        if !self.is_main {
            // secondary node
            let mut is_tc = false;
            // Forward to main only if new tc, heartbeat, or optionally, a new objective
            let should_be_forwarded = match event.event_mut() {
                Event::NewTestcase { forward_id, .. } => {
                    *forward_id = Some(ClientId(self.inner.mgr_id().0 as u32));
                    is_tc = true;
                    true
                }
                Event::Heartbeat => true, // the only purpose is to keep this client alive else the broker thinks it is dead and will dc it
                Event::Objective { .. } => true,
                Event::Stop => true,
                _ => false,
            };

            if should_be_forwarded {
                self.forward_to_main(&event)?;
                if is_tc {
                    // early return here because we only send it to centralized not main broker.
                    return Ok(());
                }
            }
        }

        // now inner llmp manager will process it if it's not a new testcase from a secondary node.
        self.inner.fire(state, event)
    }

    fn log(
        &mut self,
        state: &mut S,
        severity_level: LogSeverity,
        message: String,
    ) -> Result<(), Error>
    where
        S: HasExecutions,
    {
        self.inner.log(state, severity_level, message)
    }

    fn configuration(&self) -> EventConfig {
        self.inner.configuration()
    }
}

impl<EM, I, S, SHM, SP> EventRestarter<S> for CentralizedEventManager<EM, I, S, SHM, SP>
where
    EM: EventRestarter<S>,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    #[inline]
    fn on_restart(&mut self, state: &mut S) -> Result<(), Error> {
        self.client.await_safe_to_unmap_blocking();
        self.inner.on_restart(state)?;
        Ok(())
    }
}

impl<EM, I, S, SHM, SP> SendExiting for CentralizedEventManager<EM, I, S, SHM, SP>
where
    EM: SendExiting,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn send_exiting(&mut self) -> Result<(), Error> {
        self.client.sender_mut().send_exiting()?;
        self.inner.send_exiting()
    }

    fn on_shutdown(&mut self) -> Result<(), Error> {
        self.inner.on_shutdown()?;
        self.client.sender_mut().send_exiting()
    }
}

impl<EM, I, S, SHM, SP> AwaitRestartSafe for CentralizedEventManager<EM, I, S, SHM, SP>
where
    SHM: ShMem,
    EM: AwaitRestartSafe,
{
    #[inline]
    fn await_restart_safe(&mut self) {
        self.client.await_safe_to_unmap_blocking();
        self.inner.await_restart_safe();
    }
}

impl<EM, I, S, SHM, SP> EventReceiver<I, S> for CentralizedEventManager<EM, I, S, SHM, SP>
where
    EM: EventReceiver<I, S> + HasEventManagerId + EventFirer<I, S>,
    I: Input,
    S: Stoppable,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn try_receive(&mut self, state: &mut S) -> Result<Option<(EventWithStats<I>, bool)>, Error> {
        if self.is_main {
            // main node
            self.receive_from_secondary(state)
            // self.inner.process(fuzzer, state, executor)
        } else {
            // The main node does not process incoming events from the broker ATM
            self.inner.try_receive(state)
        }
    }

    fn on_interesting(&mut self, state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        self.inner.fire(state, event)
    }
}

impl<EM, I, S, SHM, SP> ProgressReporter<S> for CentralizedEventManager<EM, I, S, SHM, SP>
where
    EM: EventFirer<I, S> + HasEventManagerId,
    I: Input,
    S: HasExecutions + HasMetadata + HasLastReportTime + Stoppable + MaybeHasClientPerfMonitor,
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

impl<EM, I, S, SHM, SP> HasEventManagerId for CentralizedEventManager<EM, I, S, SHM, SP>
where
    EM: HasEventManagerId,
{
    fn mgr_id(&self) -> EventManagerId {
        self.inner.mgr_id()
    }
}

impl<EM, I, S, SHM, SP> CentralizedEventManager<EM, I, S, SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    /// Describe the client event manager's LLMP parts in a restorable fashion
    pub fn describe(&self) -> Result<LlmpClientDescription, Error> {
        self.client.describe()
    }

    /// Write the config for a client `EventManager` to env vars, a new
    /// client can reattach using [`CentralizedEventManagerBuilder::build_existing_client_from_env()`].
    ///
    /// # Safety
    /// Writes to env variables and may only be done single-threaded.
    pub unsafe fn to_env(&self, env_name: &str) {
        unsafe {
            self.client.to_env(env_name).unwrap();
        }
    }

    /// Know if this instance is main or secondary
    pub fn is_main(&self) -> bool {
        self.is_main
    }
}

impl<EM, I, S, SHM, SP> CentralizedEventManager<EM, I, S, SHM, SP>
where
    EM: HasEventManagerId + EventFirer<I, S>,
    I: Input,
    S: Stoppable,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    #[cfg(feature = "llmp_compression")]
    fn forward_to_main(&mut self, event: &EventWithStats<I>) -> Result<(), Error> {
        let serialized = postcard::to_allocvec(event)?;
        let flags = LLMP_FLAG_INITIALIZED;

        match self.compressor.maybe_compress(&serialized) {
            Some(comp_buf) => {
                self.client.send_buf_with_flags(
                    _LLMP_TAG_TO_MAIN,
                    flags | LLMP_FLAG_COMPRESSED,
                    &comp_buf,
                )?;
            }
            None => {
                self.client.send_buf(_LLMP_TAG_TO_MAIN, &serialized)?;
            }
        }
        Ok(())
    }

    #[cfg(not(feature = "llmp_compression"))]
    fn forward_to_main(&mut self, event: &EventWithStats<I>) -> Result<(), Error> {
        let serialized = postcard::to_allocvec(event)?;
        self.client.send_buf(_LLMP_TAG_TO_MAIN, &serialized)?;
        Ok(())
    }

    fn receive_from_secondary(
        &mut self,
        state: &mut S,
    ) -> Result<Option<(EventWithStats<I>, bool)>, Error> {
        // TODO: Get around local event copy by moving handle_in_client
        let self_id = self.client.sender().id();
        while let Some((client_id, tag, _flags, msg)) = self.client.recv_buf_with_flags()? {
            assert!(
                tag == _LLMP_TAG_TO_MAIN,
                "Only _LLMP_TAG_TO_MAIN parcel should have arrived in the main node!"
            );

            if client_id == self_id {
                continue;
            }
            #[cfg(not(feature = "llmp_compression"))]
            let event_bytes = msg;
            #[cfg(feature = "llmp_compression")]
            let compressed;
            #[cfg(feature = "llmp_compression")]
            let event_bytes = if _flags & LLMP_FLAG_COMPRESSED == LLMP_FLAG_COMPRESSED {
                compressed = self.compressor.decompress(msg)?;
                &compressed
            } else {
                msg
            };
            let event: EventWithStats<I> = postcard::from_bytes(event_bytes)?;
            log::debug!(
                "Processor received message {}",
                event.event().name_detailed()
            );

            let event_name = event.event().name_detailed();

            match event.event() {
                Event::NewTestcase {
                    client_config,
                    observers_buf,
                    forward_id,
                    ..
                } => {
                    log::debug!(
                        "Received {event_name} from {client_id:?} ({client_config:?}, forward {forward_id:?})"
                    );

                    log::debug!(
                        "[{}] Running fuzzer with event {}",
                        process::id(),
                        event_name
                    );

                    if client_config.match_with(&self.configuration()) && observers_buf.is_some() {
                        return Ok(Some((event, true)));
                    }
                    return Ok(Some((event, false)));
                }
                Event::Stop => {
                    state.request_stop();
                }
                _ => {
                    return Err(Error::illegal_state(format!(
                        "Received illegal message that message should not have arrived: {:?}.",
                        event.event().name()
                    )));
                }
            }
        }
        Ok(None)
    }
}
