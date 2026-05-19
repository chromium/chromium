//! LLMP-backed event manager for scalable multi-processed fuzzing

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
    Error,
    events::{Event, EventFirer, EventWithStats},
    fuzzer::EvaluatorObservers,
    inputs::{Input, InputConverter, NopInput},
    state::{HasCurrentTestcase, HasSolutions, NopState},
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

/// Specify if the State must be persistent over restarts
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum LlmpShouldSaveState {
    /// Always save and restore the state on restart (not OOM resistant)
    OnRestart,
    /// Never save the state (not OOM resistant)
    Never,
    /// Best-effort save and restore the state on restart (OOM safe)
    /// This adds additional runtime costs when processing events
    OOMSafeOnRestart,
    /// Never save the state (OOM safe)
    /// This adds additional runtime costs when processing events
    OOMSafeNever,
}

impl LlmpShouldSaveState {
    /// Check if the state must be saved `on_restart()`
    #[must_use]
    pub fn on_restart(&self) -> bool {
        matches!(
            self,
            LlmpShouldSaveState::OnRestart | LlmpShouldSaveState::OOMSafeOnRestart
        )
    }

    /// Check if the policy is OOM safe
    #[must_use]
    pub fn oom_safe(&self) -> bool {
        matches!(
            self,
            LlmpShouldSaveState::OOMSafeOnRestart | LlmpShouldSaveState::OOMSafeNever
        )
    }
}

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
        ICB: InputConverter<From = DI, To = I>,
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

                let res = fuzzer.evaluate_input_with_observers(
                    state,
                    executor,
                    manager,
                    &converter.convert(input)?,
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

                let res = fuzzer.evaluate_input_with_observers(
                    state,
                    executor,
                    manager,
                    &converter.convert(unwrapped_input)?,
                    false,
                )?;

                if let Some(item) = res.1 {
                    log::info!("Added received Objective as item #{item}");
                }
                Ok(())
            }
            Event::Stop => Ok(()),
            _ => Err(Error::unknown(format!(
                "Received illegal message that message should not have arrived: {:?}.",
                event.name()
            ))),
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
        ICB: InputConverter<From = DI, To = I>,
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
    IC: InputConverter<From = I>,
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
    fn fire(&mut self, _state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
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
                    input: self.converter.as_mut().unwrap().convert(input)?,
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
    fn fire(&mut self, _state: &mut S, event: EventWithStats<I>) -> Result<(), Error> {
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
                    input: self.converter.as_mut().unwrap().convert(input)?,
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
