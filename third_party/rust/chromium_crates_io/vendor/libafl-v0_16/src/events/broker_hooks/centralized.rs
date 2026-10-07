use alloc::vec::Vec;
use core::{fmt::Debug, marker::PhantomData};

use libafl_bolts::{
    ClientId, Error,
    llmp::{Flags, LlmpBrokerInner, LlmpHook, LlmpMsgHookResult, Tag},
};
#[cfg(feature = "llmp_compression")]
use libafl_bolts::{compress::GzipCompressor, llmp::LLMP_FLAG_COMPRESSED};
use serde::de::DeserializeOwned;

#[cfg(feature = "llmp_compression")]
use crate::events::COMPRESS_THRESHOLD;
use crate::events::{_LLMP_TAG_TO_MAIN, BrokerEventResult, Event, EventWithStats};

/// An LLMP-backed event manager for scalable multi-processed fuzzing
pub struct CentralizedLlmpHook<I> {
    #[cfg(feature = "llmp_compression")]
    compressor: GzipCompressor,
    phantom: PhantomData<I>,
}

impl<I, SHM, SP> LlmpHook<SHM, SP> for CentralizedLlmpHook<I>
where
    I: DeserializeOwned,
{
    fn on_new_message(
        &mut self,
        _broker_inner: &mut LlmpBrokerInner<SHM, SP>,
        client_id: ClientId,
        msg_tag: &mut Tag,
        _msg_flags: &mut Flags,
        msg: &mut [u8],
        _new_msgs: &mut Vec<(Tag, Flags, Vec<u8>)>,
    ) -> Result<LlmpMsgHookResult, Error> {
        if *msg_tag == _LLMP_TAG_TO_MAIN {
            #[cfg(feature = "llmp_compression")]
            let compressor = &self.compressor;
            #[cfg(not(feature = "llmp_compression"))]
            let event_bytes = msg;
            #[cfg(feature = "llmp_compression")]
            let compressed;
            #[cfg(feature = "llmp_compression")]
            let event_bytes = if *_msg_flags & LLMP_FLAG_COMPRESSED == LLMP_FLAG_COMPRESSED {
                compressed = compressor.decompress(msg)?;
                &compressed
            } else {
                &*msg
            };
            let event: EventWithStats<I> = postcard::from_bytes(event_bytes)?;
            match Self::handle_in_broker(client_id, &event)? {
                BrokerEventResult::Forward => Ok(LlmpMsgHookResult::ForwardToClients),
                BrokerEventResult::Handled => Ok(LlmpMsgHookResult::Handled),
            }
        } else {
            Ok(LlmpMsgHookResult::ForwardToClients)
        }
    }
}

impl<I> Debug for CentralizedLlmpHook<I> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let mut debug_struct = f.debug_struct("CentralizedLlmpHook");

        #[cfg(feature = "llmp_compression")]
        let debug_struct = debug_struct.field("compressor", &self.compressor);

        debug_struct
            .field("phantom", &self.phantom)
            .finish_non_exhaustive()
    }
}

impl<I> CentralizedLlmpHook<I> {
    /// Create an event broker from a raw broker.
    pub fn new() -> Result<Self, Error> {
        Ok(Self {
            #[cfg(feature = "llmp_compression")]
            compressor: GzipCompressor::with_threshold(COMPRESS_THRESHOLD),
            phantom: PhantomData,
        })
    }

    /// Handle arriving events in the broker
    #[expect(clippy::unnecessary_wraps)]
    fn handle_in_broker(
        _client_id: ClientId,
        event: &EventWithStats<I>,
    ) -> Result<BrokerEventResult, Error> {
        match event.event() {
            Event::NewTestcase { .. } | Event::Stop => Ok(BrokerEventResult::Forward),
            _ => Ok(BrokerEventResult::Handled),
        }
    }
}
