//! LLMP-backed event manager for scalable multi-processed fuzzing

use alloc::vec::Vec;
use core::{fmt::Debug, marker::PhantomData, time::Duration};

use libafl_bolts::{
    ClientId,
    llmp::{LlmpClient, LlmpClientDescription, Tag},
    shmem::{NopShMem, NopShMemProvider, ShMem, ShMemProvider},
};
#[cfg(feature = "llmp_compression")]
use libafl_bolts::{
    compress::GzipCompressor,
    llmp::{LLMP_FLAG_COMPRESSED, LLMP_FLAG_INITIALIZED},
};
use serde::{Serialize, de::DeserializeOwned};

use crate::{
    Error, HasMetadata,
    events::{
        AwaitRestartSafe, Event, EventConfig, EventFirer, EventManagerHooksTuple, EventRestarter,
        EventWithStats, ProgressReporter, SendExiting,
    },
    fuzzer::EvaluatorObservers,
    inputs::{Input, InputConverter, NopInput},
    state::{
        HasCurrentStageId, HasCurrentTestcase, HasExecutions, HasImported, HasLastReportTime,
        HasSolutions, MaybeHasClientPerfMonitor, NopState, Stoppable,
    },
};

/// The llmp restarting manager
#[cfg(feature = "std")]
pub mod restarting;
#[cfg(feature = "std")]
pub use restarting::*;

/// Forward this to the client
pub(crate) const _LLMP_TAG_EVENT_TO_CLIENT: Tag = Tag(0x2C11E471);
/// Only handle this in the broker
pub(crate) const _LLMP_TAG_EVENT_TO_BROKER: Tag = Tag(0x2B80438);
/// Handle in both
pub(crate) const LLMP_TAG_EVENT_TO_BOTH: Tag = Tag(0x2B0741);
pub(crate) const _LLMP_TAG_RESTART: Tag = Tag(0x8357A87);
pub(crate) const _LLMP_TAG_NO_RESTART: Tag = Tag(0x57A7EE71);

/// The minimum buffer size at which to compress LLMP IPC messages.
#[cfg(feature = "llmp_compression")]
pub const COMPRESS_THRESHOLD: usize = 1024;
use crate::events::ShouldSaveState;

/// A manager-like llmp client that converts between input types
pub struct LlmpEventConverter<I, IC, ICB, S, SHM, SP> {
    throttle: Option<Duration>,
    llmp: LlmpClient<SHM, SP>,
    last_sent: Duration,
    #[cfg(feature = "llmp_compression")]
    compressor: GzipCompressor,
    converter: Option<IC>,
    converter_back: Option<ICB>,
    phantom: PhantomData<(I, S)>,
}

impl LlmpEventConverter<NopInput, (), (), NopState<NopInput>, NopShMem, NopShMemProvider> {
    /// Create a builder for [`LlmpEventConverter`]
    #[must_use]
    pub fn builder() -> LlmpEventConverterBuilder {
        LlmpEventConverterBuilder::new()
    }
}

/// Build `LlmpEventConverter`
#[derive(Debug, Clone, Default)]
pub struct LlmpEventConverterBuilder {
    throttle: Option<Duration>,
}

impl LlmpEventConverterBuilder {
    #[must_use]
    /// Constructor
    pub fn new() -> Self {
        Self { throttle: None }
    }

    #[must_use]
    /// Sets the `throttle`
    pub fn throttle(self, throttle: Duration) -> Self {
        Self {
            throttle: Some(throttle),
        }
    }

    /// Create a event converter from a raw llmp client
    pub fn build_from_client<I, IC, ICB, S, SHM, SP>(
        self,
        llmp: LlmpClient<SHM, SP>,
        converter: Option<IC>,
        converter_back: Option<ICB>,
    ) -> Result<LlmpEventConverter<I, IC, ICB, S, SHM, SP>, Error> {
        Ok(LlmpEventConverter {
            throttle: self.throttle,
            last_sent: Duration::from_secs(0),
            llmp,
            #[cfg(feature = "llmp_compression")]
            compressor: GzipCompressor::with_threshold(COMPRESS_THRESHOLD),
            converter,
            converter_back,
            phantom: PhantomData,
        })
    }

    /// Create a client from port and the input converters
    #[cfg(feature = "std")]
    pub fn build_on_port<I, IC, ICB, S, SHM, SP>(
        self,
        shmem_provider: SP,
        port: u16,
        converter: Option<IC>,
        converter_back: Option<ICB>,
    ) -> Result<LlmpEventConverter<I, IC, ICB, S, SHM, SP>, Error>
    where
        I: Input,
        S: HasExecutions + HasMetadata + HasImported + HasSolutions<I> + HasCurrentTestcase<I>,
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let llmp = LlmpClient::create_attach_to_tcp(shmem_provider, port)?;
        Ok(LlmpEventConverter {
            throttle: self.throttle,
            last_sent: Duration::from_secs(0),
            llmp,
            #[cfg(feature = "llmp_compression")]
            compressor: GzipCompressor::with_threshold(COMPRESS_THRESHOLD),
            converter,
            converter_back,
            phantom: PhantomData,
        })
    }

    /// If a client respawns, it may reuse the existing connection, previously stored by [`LlmpClient::to_env()`].
    #[cfg(feature = "std")]
    pub fn build_existing_client_from_env<I, IC, ICB, S, SHM, SP>(
        self,
        shmem_provider: SP,
        env_name: &str,
        converter: Option<IC>,
        converter_back: Option<ICB>,
    ) -> Result<LlmpEventConverter<I, IC, ICB, S, SHM, SP>, Error>
    where
        I: Input,
        S: HasExecutions + HasMetadata + HasImported + HasSolutions<I> + HasCurrentTestcase<I>,
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let llmp = LlmpClient::on_existing_from_env(shmem_provider, env_name)?;
        Ok(LlmpEventConverter {
            throttle: self.throttle,
            last_sent: Duration::from_secs(0),
            llmp,
            #[cfg(feature = "llmp_compression")]
            compressor: GzipCompressor::with_threshold(COMPRESS_THRESHOLD),
            converter,
            converter_back,
            phantom: PhantomData,
        })
    }
}

impl<I, IC, ICB, S, SHM, SP> Debug for LlmpEventConverter<I, IC, ICB, S, SHM, SP>
where
    IC: Debug,
    ICB: Debug,
    SHM: Debug,
    SP: Debug,
{
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let mut debug_struct = f.debug_struct("LlmpEventConverter");
        let debug = debug_struct.field("llmp", &self.llmp);
        //.field("custom_buf_handlers", &self.custom_buf_handlers)
        #[cfg(feature = "llmp_compression")]
        let debug = debug.field("compressor", &self.compressor);
        debug
            .field("converter", &self.converter)
            .field("converter_back", &self.converter_back)
            .field("phantom", &self.phantom)
            .finish_non_exhaustive()
    }
}

impl<I, IC, ICB, S, SHM, SP> LlmpEventConverter<I, IC, ICB, S, SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    // TODO other new_* routines

    /// Check if it can convert the input
    pub fn can_convert(&self) -> bool {
        self.converter.is_some()
    }

    /// Check if it can convert the input back
    pub fn can_convert_back(&self) -> bool {
        self.converter_back.is_some()
    }

    /// Describe the client event mgr's llmp parts in a restorable fashion
    pub fn describe(&self) -> Result<LlmpClientDescription, Error> {
        self.llmp.describe()
    }

    /// Write the config for a client `EventManager` to env vars, a new client can reattach using [`LlmpEventConverterBuilder::build_existing_client_from_env()`].
    ///
    /// # Safety
    /// Writes to env variables and may only be done single-threaded.
    #[cfg(feature = "std")]
    pub unsafe fn to_env(&self, env_name: &str) {
        unsafe {
            self.llmp.to_env(env_name).unwrap();
        }
    }
}

/// LLMP event manager for scalable multi-processed fuzzing
#[derive(Debug)]
pub struct LlmpEventManager<EMH, I, S, SHM, SP> {
    pub(crate) throttle: Option<Duration>,
    /// We sent last message at `last_sent`
    last_sent: Duration,
    hooks: EMH,
    /// The LLMP client for inter process communication
    pub llmp: LlmpClient<SHM, SP>,
    #[cfg(feature = "llmp_compression")]
    compressor: GzipCompressor,
    /// The configuration defines this specific fuzzer.
    /// A node will not re-use the observer values sent over LLMP
    /// from nodes with other configurations.
    configuration: EventConfig,
    event_buffer: Vec<u8>,
    /// Decide if the state restorer must save the serialized state
    save_state: ShouldSaveState,
    phantom: PhantomData<(I, S)>,
}

impl<EMH, I, S, SHM, SP> LlmpEventManager<EMH, I, S, SHM, SP>
where
    I: Input,
    EMH: EventManagerHooksTuple<I, S>,
    S: HasExecutions + HasMetadata + HasImported + HasSolutions<I> + HasCurrentTestcase<I>,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    /// Create a new [`LlmpEventManager`]
    pub fn new(
        llmp: LlmpClient<SHM, SP>,
        hooks: EMH,
        configuration: EventConfig,
        save_state: ShouldSaveState,
    ) -> Result<Self, Error> {
        Ok(Self {
            throttle: None,
            last_sent: Duration::from_secs(0),
            hooks,
            llmp,
            #[cfg(feature = "llmp_compression")]
            compressor: GzipCompressor::with_threshold(COMPRESS_THRESHOLD),
            configuration,
            event_buffer: Vec::with_capacity(1024),
            save_state,
            phantom: PhantomData,
        })
    }

    /// Create a new [`LlmpEventManager`] from a `LlmpClient`
    pub fn with_client(
        llmp: LlmpClient<SHM, SP>,
        hooks: EMH,
        configuration: EventConfig,
        save_state: ShouldSaveState,
    ) -> Result<Self, Error> {
        Ok(Self {
            throttle: None,
            last_sent: Duration::from_secs(0),
            hooks,
            llmp,
            #[cfg(feature = "llmp_compression")]
            compressor: GzipCompressor::with_threshold(COMPRESS_THRESHOLD),
            configuration,
            event_buffer: Vec::with_capacity(1024),
            save_state,
            phantom: PhantomData,
        })
    }

    /// Check if we should save the state
    pub fn save_state(&self) -> ShouldSaveState {
        self.save_state
    }
    /// Describe the client event mgr's llmp parts in a restorable fashion
    pub fn describe(&self) -> Result<LlmpClientDescription, Error> {
        self.llmp.describe()
    }

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
}

impl<EMH, I, S, SHM, SP> ProgressReporter<S> for LlmpEventManager<EMH, I, S, SHM, SP>
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
        crate::events::std_maybe_report_progress(self, state, monitor_timeout)
    }

    fn report_progress(&mut self, state: &mut S) -> Result<(), Error> {
        crate::events::std_report_progress(self, state)
    }
}

impl<EMH, I, S, SHM, SP> EventFirer<I, S> for LlmpEventManager<EMH, I, S, SHM, SP>
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

        self.last_sent = libafl_bolts::current_time();
        Ok(())
    }

    fn configuration(&self) -> EventConfig {
        self.configuration
    }

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
}

#[cfg(feature = "std")]
impl<EMH, I, S, SHM, SP> crate::events::Restorable<S, SP> for LlmpEventManager<EMH, I, S, SHM, SP>
where
    S: Serialize + HasCurrentStageId,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    type RestartState = LlmpClientDescription;

    fn on_restart(&mut self, state: &mut S) -> Result<(bool, Self::RestartState), Error> {
        state.on_restart()?;

        let should_save = self.save_state.on_restart();
        let desc = self.llmp.describe()?;

        log::info!("Waiting for broker...");

        Ok((should_save, desc))
    }

    fn on_fire(
        &mut self,
        staterestorer: &mut libafl_bolts::staterestore::StateRestorer<SHM, SP>,
    ) -> Result<(), Error> {
        if self.save_state.oom_safe() {
            staterestorer.reset();
            staterestorer.save(&(None::<S>, &self.llmp.describe()?))?;
        }
        Ok(())
    }
}

impl<EMH, I, S, SHM, SP> AwaitRestartSafe for LlmpEventManager<EMH, I, S, SHM, SP>
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

impl<EMH, I, S, SHM, SP> SendExiting for LlmpEventManager<EMH, I, S, SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn send_exiting(&mut self) -> Result<(), Error> {
        self.llmp.sender_mut().send_exiting()
    }

    fn on_shutdown(&mut self) -> Result<(), Error> {
        self.send_exiting()
    }
}

impl<EMH, I, S, SHM, SP> EventRestarter<S> for LlmpEventManager<EMH, I, S, SHM, SP>
where
    S: HasCurrentStageId,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn on_restart(&mut self, state: &mut S) -> Result<(), Error> {
        state.on_restart()?;
        Ok(())
    }
}

impl<EMH, I, S, SHM, SP> crate::events::EventReceiver<I, S> for LlmpEventManager<EMH, I, S, SHM, SP>
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
            if !event.event().is_new_testcase()
                && (flags & libafl_bolts::llmp::LLMP_FLAG_FROM_MM
                    == libafl_bolts::llmp::LLMP_FLAG_FROM_MM)
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

            let has_observers = match event.event() {
                Event::NewTestcase { observers_buf, .. } => observers_buf.is_some(),
                _ => false,
            };

            return Ok(Some((event, has_observers)));
        }
        Ok(None)
    }

    fn on_interesting(&mut self, _state: &mut S, _event: EventWithStats<I>) -> Result<(), Error> {
        Ok(())
    }
}

impl<EMH, I, S, SHM, SP> crate::events::HasEventManagerId for LlmpEventManager<EMH, I, S, SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn mgr_id(&self) -> crate::events::EventManagerId {
        crate::events::EventManagerId(self.llmp.sender().id().0 as usize)
    }
}

/// Builder for `LlmpEventManager`
#[derive(Debug)]
pub struct LlmpEventManagerBuilder<EMH> {
    throttle: Option<Duration>,
    save_state: ShouldSaveState,
    hooks: EMH,
}

impl Default for LlmpEventManagerBuilder<()> {
    fn default() -> Self {
        Self::new()
    }
}

impl LlmpEventManagerBuilder<()> {
    /// Create a new `LlmpEventManagerBuilder`
    #[must_use]
    pub fn new() -> Self {
        Self {
            throttle: None,
            save_state: ShouldSaveState::OnRestart,
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
    pub fn save_state(mut self, save_state: ShouldSaveState) -> Self {
        self.save_state = save_state;
        self
    }

    /// Create a manager from a raw LLMP client
    pub fn build_from_client<I, S, SHM, SP>(
        self,
        llmp: LlmpClient<SHM, SP>,
        configuration: EventConfig,
    ) -> Result<LlmpEventManager<EMH, I, S, SHM, SP>, Error>
    where
        I: Input,
        EMH: EventManagerHooksTuple<I, S>,
        S: HasExecutions + HasMetadata + HasImported + HasSolutions<I> + HasCurrentTestcase<I>,
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        LlmpEventManager::new(llmp, self.hooks, configuration, self.save_state)
    }

    /// Create an LLMP event manager on a port.
    /// It expects a broker to exist on this port.
    #[cfg(feature = "std")]
    pub fn build_on_port<I, S, SHM, SP>(
        self,
        shmem_provider: SP,
        port: u16,
        configuration: EventConfig,
    ) -> Result<LlmpEventManager<EMH, I, S, SHM, SP>, Error>
    where
        I: Input,
        EMH: EventManagerHooksTuple<I, S>,
        S: HasExecutions + HasMetadata + HasImported + HasSolutions<I> + HasCurrentTestcase<I>,
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let llmp = LlmpClient::create_attach_to_tcp(shmem_provider, port)?;
        Self::build_from_client(self, llmp, configuration)
    }

    /// If a client respawns, it may reuse the existing connection, previously
    /// stored by [`LlmpClient::to_env()`].
    #[cfg(feature = "std")]
    pub fn build_existing_client_from_env<I, S, SHM, SP>(
        self,
        shmem_provider: SP,
        env_name: &str,
        configuration: EventConfig,
    ) -> Result<LlmpEventManager<EMH, I, S, SHM, SP>, Error>
    where
        I: Input,
        EMH: EventManagerHooksTuple<I, S>,
        S: HasExecutions + HasMetadata + HasImported + HasSolutions<I> + HasCurrentTestcase<I>,
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let llmp = LlmpClient::on_existing_from_env(shmem_provider, env_name)?;
        Self::build_from_client(self, llmp, configuration)
    }

    /// Create an existing client from description
    pub fn build_existing_client_from_description<I, S, SHM, SP>(
        self,
        shmem_provider: SP,
        description: &LlmpClientDescription,
        configuration: EventConfig,
    ) -> Result<LlmpEventManager<EMH, I, S, SHM, SP>, Error>
    where
        I: Input,
        EMH: EventManagerHooksTuple<I, S>,
        S: HasExecutions + HasMetadata + HasImported + HasSolutions<I> + HasCurrentTestcase<I>,
        SHM: ShMem,
        SP: ShMemProvider<ShMem = SHM>,
    {
        let llmp = LlmpClient::existing_client_from_description(shmem_provider, description)?;
        Self::build_from_client(self, llmp, configuration)
    }
}

impl<I, IC, ICB, S, SHM, SP> LlmpEventConverter<I, IC, ICB, S, SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    // Handle arriving events in the client
    fn handle_in_client<DI, E, EM, Z>(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
        client_id: ClientId,
        event: Event<DI>,
    ) -> Result<(), Error>
    where
        ICB: InputConverter<S, From = DI, To = I>,
        Z: EvaluatorObservers<E, EM, I, S>,
    {
        match event {
            Event::NewTestcase {
                input, forward_id, ..
            } => {
                log::debug!(
                    "Received new Testcase to convert from {client_id:?} (forward {forward_id:?}, forward {forward_id:?})"
                );

                let Some(converter) = self.converter_back.as_mut() else {
                    return Ok(());
                };

                let converted_input = converter.convert(state, input)?;
                let res = fuzzer.evaluate_input_with_observers(
                    state,
                    executor,
                    manager,
                    &converted_input,
                    false,
                )?;

                if let Some(item) = res.1 {
                    log::info!("Added received Testcase as item #{item}");
                }
                Ok(())
            }
            Event::Objective {
                input: Some(unwrapped_input),
                ..
            } => {
                log::debug!("Received new Objective");

                let Some(converter) = self.converter_back.as_mut() else {
                    return Ok(());
                };

                let converted_input = converter.convert(state, unwrapped_input)?;
                let res = fuzzer.evaluate_input_with_observers(
                    state,
                    executor,
                    manager,
                    &converted_input,
                    false,
                )?;

                if let Some(item) = res.1 {
                    log::info!("Added received Objective as item #{item}");
                }
                Ok(())
            }
            _ => Ok(()),
        }
    }

    /// Handle arriving events in the client
    pub fn process<DI, E, EM, Z>(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        executor: &mut E,
        manager: &mut EM,
    ) -> Result<usize, Error>
    where
        ICB: InputConverter<S, From = DI, To = I>,
        DI: DeserializeOwned + Input,
        S: HasCurrentTestcase<I> + HasSolutions<I>,
        Z: EvaluatorObservers<E, EM, I, S>,
    {
        // TODO: Get around local event copy by moving handle_in_client
        let self_id = self.llmp.sender().id();
        let mut count = 0;
        while let Some((client_id, tag, _flags, msg)) = self.llmp.recv_buf_with_flags()? {
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
            let event_bytes = if _flags & LLMP_FLAG_COMPRESSED == LLMP_FLAG_COMPRESSED {
                compressed = self.compressor.decompress(msg)?;
                &compressed
            } else {
                msg
            };

            let event: Event<DI> = postcard::from_bytes(event_bytes)?;
            log::debug!("Processor received message {}", event.name_detailed());
            self.handle_in_client(fuzzer, executor, state, manager, client_id, event)?;
            count += 1;
        }
        Ok(count)
    }
}

impl<I, IC, ICB, S, SHM, SP> EventFirer<I, S> for LlmpEventConverter<I, IC, ICB, S, SHM, SP>
where
    IC: InputConverter<S, From = I>,
    IC::To: Serialize,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
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

    #[cfg(feature = "llmp_compression")]
    fn fire(&mut self, state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        if self.converter.is_none() {
            return Ok(());
        }

        // Filter out non interestign events and convert `NewTestcase`
        let converted_event = EventWithStats::new(
            match event.event {
                Event::NewTestcase {
                    input,
                    client_config,
                    exit_kind,
                    corpus_size,
                    observers_buf,
                    forward_id,
                    #[cfg(all(unix, feature = "std", feature = "multi_machine"))]
                    node_id,
                } => Event::NewTestcase {
                    input: self.converter.as_mut().unwrap().convert(state, input)?,
                    client_config,
                    exit_kind,
                    corpus_size,
                    observers_buf,
                    forward_id,
                    #[cfg(all(unix, feature = "std", feature = "multi_machine"))]
                    node_id,
                },
                _ => {
                    return Ok(());
                }
            },
            event.stats,
        );

        let serialized = postcard::to_allocvec(&converted_event)?;
        let flags = LLMP_FLAG_INITIALIZED;

        match self.compressor.maybe_compress(&serialized) {
            Some(comp_buf) => {
                self.llmp.send_buf_with_flags(
                    LLMP_TAG_EVENT_TO_BOTH,
                    flags | LLMP_FLAG_COMPRESSED,
                    &comp_buf,
                )?;
            }
            None => {
                self.llmp.send_buf(LLMP_TAG_EVENT_TO_BOTH, &serialized)?;
            }
        }
        self.last_sent = libafl_bolts::current_time();
        Ok(())
    }

    #[cfg(not(feature = "llmp_compression"))]
    fn fire(&mut self, state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
        if self.converter.is_none() {
            return Ok(());
        }

        // Filter out non interestign events and convert `NewTestcase`
        let converted_event = EventWithStats::new(
            match event.event {
                Event::NewTestcase {
                    input,
                    client_config,
                    exit_kind,
                    corpus_size,
                    observers_buf,
                    forward_id,
                    #[cfg(all(unix, feature = "std", feature = "multi_machine"))]
                    node_id,
                } => Event::NewTestcase {
                    input: self.converter.as_mut().unwrap().convert(state, input)?,
                    client_config,
                    exit_kind,
                    corpus_size,
                    observers_buf,
                    forward_id,
                    #[cfg(all(unix, feature = "std", feature = "multi_machine"))]
                    node_id,
                },
                _ => {
                    return Ok(());
                }
            },
            event.stats,
        );

        let serialized = postcard::to_allocvec(&converted_event)?;
        self.llmp.send_buf(LLMP_TAG_EVENT_TO_BOTH, &serialized)?;
        Ok(())
    }
}
