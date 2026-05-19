//! Hooks called on broker side
use alloc::vec::Vec;
use core::marker::PhantomData;

use libafl_bolts::{
    ClientId,
    llmp::{Flags, LlmpBrokerInner, LlmpHook, LlmpMsgHookResult, Tag},
};
#[cfg(feature = "llmp_compression")]
use libafl_bolts::{compress::GzipCompressor, llmp::LLMP_FLAG_COMPRESSED};
use serde::de::DeserializeOwned;

#[cfg(feature = "llmp_compression")]
use crate::events::llmp::COMPRESS_THRESHOLD;
use crate::{
    Error,
    events::{BrokerEventResult, Event, llmp::LLMP_TAG_EVENT_TO_BOTH},
    monitors::{Monitor, stats::ClientStatsManager},
};

/// centralized hook
#[cfg(all(unix, feature = "std"))]
pub mod centralized;
#[cfg(all(unix, feature = "std"))]
pub use centralized::*;

/// Multi-machine hook
#[cfg(all(unix, feature = "multi_machine"))]
pub mod centralized_multi_machine;
#[cfg(all(unix, feature = "multi_machine"))]
pub use centralized_multi_machine::*;

use super::EventWithStats;

/// An LLMP-backed event hook for scalable multi-processed fuzzing
#[derive(Debug)]
pub struct StdLlmpEventHook<I, MT> {
    monitor: MT,
    #[cfg(feature = "llmp_compression")]
    compressor: GzipCompressor,
    phantom: PhantomData<I>,
    client_stats_manager: ClientStatsManager,
}

impl<I, MT, SHM, SP> LlmpHook<SHM, SP> for StdLlmpEventHook<I, MT>
where
    I: DeserializeOwned,
    MT: Monitor,
{
    fn on_new_message(
        &mut self,
        _broker_inner: &mut LlmpBrokerInner<SHM, SP>,
        client_id: ClientId,
        msg_tag: &mut Tag,
        #[cfg(feature = "llmp_compression")] msg_flags: &mut Flags,
        #[cfg(not(feature = "llmp_compression"))] _msg_flags: &mut Flags,
        msg: &mut [u8],
        _new_msgs: &mut Vec<(Tag, Flags, Vec<u8>)>,
    ) -> Result<LlmpMsgHookResult, Error> {
        let monitor = &mut self.monitor;
        #[cfg(feature = "llmp_compression")]
        let compressor = &self.compressor;

        if *msg_tag == LLMP_TAG_EVENT_TO_BOTH {
            #[cfg(not(feature = "llmp_compression"))]
            let event_bytes = msg;
            #[cfg(feature = "llmp_compression")]
            let compressed;
            #[cfg(feature = "llmp_compression")]
            let event_bytes = if *msg_flags & LLMP_FLAG_COMPRESSED == LLMP_FLAG_COMPRESSED {
                compressed = compressor.decompress(msg)?;
                &compressed
            } else {
                &*msg
            };
            let event: EventWithStats<I> = postcard::from_bytes(event_bytes)?;
            match Self::handle_in_broker(
                monitor,
                &mut self.client_stats_manager,
                client_id,
                &event,
            )? {
                BrokerEventResult::Forward => Ok(LlmpMsgHookResult::ForwardToClients),
                BrokerEventResult::Handled => Ok(LlmpMsgHookResult::Handled),
            }
        } else {
            Ok(LlmpMsgHookResult::ForwardToClients)
        }
    }

    fn on_timeout(&mut self) -> Result<(), Error> {
        self.monitor.display(
            &mut self.client_stats_manager,
            "Broker Heartbeat",
            ClientId(0),
        )?;
        Ok(())
    }
}

impl<I, MT> StdLlmpEventHook<I, MT>
where
    MT: Monitor,
{
    /// Create an event broker from a raw broker.
    pub fn new(monitor: MT) -> Result<Self, Error> {
        Ok(Self {
            monitor,
            #[cfg(feature = "llmp_compression")]
            compressor: GzipCompressor::with_threshold(COMPRESS_THRESHOLD),
            client_stats_manager: ClientStatsManager::default(),
            phantom: PhantomData,
        })
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
        match &event {
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
                client_stats_manager.update_client_stats_for(id, |client_stat| {
                    client_stat.update_corpus_size(*corpus_size as u64);
                })?;
                monitor.display(client_stats_manager, event.name(), id)?;
                Ok(BrokerEventResult::Forward)
            }
            Event::Heartbeat => {
                monitor.display(client_stats_manager, event.name(), client_id)?;
                Ok(BrokerEventResult::Handled)
            }
            Event::UpdateUserStats { name, value, .. } => {
                client_stats_manager.client_stats_insert(client_id)?;
                client_stats_manager.update_client_stats_for(client_id, |client_stat| {
                    client_stat.update_user_stats(name.clone(), value.clone());
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
                client_stats_manager.update_client_stats_for(client_id, |client_stat| {
                    // Update the performance monitor for this client
                    client_stat.update_introspection_stats((**introspection_stats).clone());
                })?;

                // Display the monitor via `.display` only on core #1
                monitor.display(client_stats_manager, event.name(), client_id)?;

                // Correctly handled the event
                Ok(BrokerEventResult::Handled)
            }
            Event::Objective { objective_size, .. } => {
                client_stats_manager.client_stats_insert(client_id)?;
                client_stats_manager.update_client_stats_for(client_id, |client_stat| {
                    client_stat.update_objective_size(*objective_size as u64);
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
