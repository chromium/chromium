use alloc::{boxed::Box, sync::Arc, vec::Vec};
use core::{
    fmt::Display,
    sync::atomic::{AtomicU64, Ordering},
    time::Duration,
};
use std::{collections::HashMap, io::ErrorKind, process, sync::OnceLock};

use enumflags2::{BitFlags, bitflags};
#[cfg(feature = "llmp_compression")]
use libafl_bolts::compress::GzipCompressor;
use libafl_bolts::{Error, current_time, ownedref::OwnedRef};
use serde::{Deserialize, Serialize};
use tokio::{
    io::{AsyncReadExt, AsyncWriteExt},
    net::{TcpListener, TcpStream, ToSocketAddrs},
    runtime::Runtime,
    sync::RwLock,
    task::JoinHandle,
    time,
};
use typed_builder::TypedBuilder;

use crate::{
    events::{EventWithStats, TcpMultiMachineLlmpReceiverHook, TcpMultiMachineLlmpSenderHook},
    inputs::{Input, NopInput},
};

// const MAX_NB_RECEIVED_AT_ONCE: usize = 100;

#[bitflags(default = SendToParent | SendToChildren)]
#[repr(u8)]
#[derive(Debug, Copy, Clone, PartialEq)]
/// The node policy. It represents flags that can be applied to the node to change how it behaves.
pub enum NodePolicy {
    /// Send current node's interesting inputs to parent.
    SendToParent,
    /// Send current node's interesting inputs to children.
    SendToChildren,
}

const DUMMY_BYTE: u8 = 0x14;

/// Use `OwnedRef` as much as possible here to avoid useless copies.
/// An owned TCP message for multi machine
#[derive(Debug, Clone)]
// #[serde(bound = "I: serde::de::DeserializeOwned")]
pub enum MultiMachineMsg<'a, I> {
    /// A raw llmp message (not deserialized)
    LlmpMsg(OwnedRef<'a, [u8]>),

    /// A `LibAFL` Event (already deserialized)
    Event(OwnedRef<'a, EventWithStats<I>>),
}

/// We do not use raw pointers, so no problem with thead-safety
unsafe impl<I: Input> Send for MultiMachineMsg<'_, I> {}
unsafe impl<I: Input> Sync for MultiMachineMsg<'_, I> {}

impl<'a, I> MultiMachineMsg<'a, I> {
    /// Create a new [`MultiMachineMsg`] as event.
    ///
    /// # Safety
    ///
    /// `OwnedRef` should **never** be a raw pointer for thread-safety reasons.
    /// We check this for debug builds, but not for release.
    #[must_use]
    pub unsafe fn event(event: OwnedRef<'a, EventWithStats<I>>) -> Self {
        debug_assert!(!event.is_raw());

        MultiMachineMsg::Event(event)
    }

    /// Create a new [`MultiMachineMsg`] from an llmp msg.
    #[must_use]
    pub fn llmp_msg(msg: OwnedRef<'a, [u8]>) -> Self {
        MultiMachineMsg::LlmpMsg(msg)
    }

    /// Get the message
    #[must_use]
    pub fn serialize_as_ref(&self) -> &[u8] {
        match self {
            MultiMachineMsg::LlmpMsg(msg) => msg.as_ref(),
            MultiMachineMsg::Event(_) => {
                panic!("Not supported")
            }
        }
    }

    /// To owned message
    #[must_use]
    pub fn from_llmp_msg(msg: Box<[u8]>) -> MultiMachineMsg<'a, I> {
        MultiMachineMsg::LlmpMsg(OwnedRef::Owned(msg))
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, Default, Serialize, Deserialize)]
/// A `NodeId` (unused for now)
pub struct NodeId(pub u64);

impl NodeId {
    /// Generate a unique [`NodeId`].
    pub fn new() -> Self {
        static CTR: OnceLock<AtomicU64> = OnceLock::new();
        let ctr = CTR.get_or_init(|| AtomicU64::new(0));
        NodeId(ctr.fetch_add(1, Ordering::Relaxed))
    }
}

/// The state of the hook shared between the background threads and the main thread.
#[derive(Debug)]
pub struct TcpMultiMachineState<A> {
    node_descriptor: NodeDescriptor<A>,
    /// the parent to which the testcases should be forwarded when deemed interesting
    parent: Option<TcpStream>,
    /// The children who connected during the fuzzing session.
    children: HashMap<NodeId, TcpStream>, // The children who connected during the fuzzing session.
    old_msgs: Vec<Vec<u8>>,
    #[cfg(feature = "llmp_compression")]
    compressor: GzipCompressor,
}

/// The tree descriptor for the
#[derive(Debug, Clone, TypedBuilder)]
pub struct NodeDescriptor<A> {
    /// The parent address, if there is one.
    pub parent_addr: Option<A>,

    /// The node listening port. Defaults to 50000
    #[builder(default = Some(50000))]
    pub node_listening_port: Option<u16>,

    #[builder(default = Duration::from_secs(60))]
    /// The timeout for connecting to parent
    pub timeout: Duration,

    /// Node flags
    #[builder(default_code = "BitFlags::default()")]
    pub flags: BitFlags<NodePolicy>, // The policy for shared messages between nodes.
}

/// A set of multi-machine `broker_hooks`.
///
/// Beware, the hooks should run in the same process as the one this function is called.
/// This is because we spawn a tokio runtime underneath.
/// Check `<https://github.com/tokio-rs/tokio/issues/4301>` for more details.
///
/// Use `TcpMultiMachineHooks::builder()` to initialize the hooks.
///
/// # Safety
/// The [`TcpMultiMachineLlmpReceiverHook`] assumes that the `msg` parameter
/// passed to the `on_new_message` method (or rather, the memory it points to),
/// lives sufficiently long for an async background task to process it.
#[derive(Debug)]
pub struct TcpMultiMachineHooks<A, I> {
    /// The sender hooks
    pub sender: TcpMultiMachineLlmpSenderHook<A, I>,
    /// The hooks
    pub receiver: TcpMultiMachineLlmpReceiverHook<A, I>,
}

impl TcpMultiMachineHooks<(), NopInput> {
    /// Create the builder to build a new [`TcpMultiMachineHooks`]
    /// containing a sender and a receiver from a [`NodeDescriptor`].
    #[must_use]
    pub fn builder() -> TcpMultiMachineHooksBuilder<()> {
        TcpMultiMachineHooksBuilder::<()> {
            node_descriptor: None,
        }
    }
}

/// A Multi-machine `broker_hooks` builder.
#[derive(Debug)]
pub struct TcpMultiMachineHooksBuilder<A> {
    node_descriptor: Option<NodeDescriptor<A>>,
}

impl<A> TcpMultiMachineHooksBuilder<A> {
    /// Set the multi machine [`NodeDescriptor`] used by the resulting [`TcpMultiMachineHooks`].
    pub fn node_descriptor<A2>(
        self,
        node_descriptor: NodeDescriptor<A2>,
    ) -> TcpMultiMachineHooksBuilder<A2>
    where
        A2: Clone + Display + ToSocketAddrs + Send + Sync + 'static,
    {
        TcpMultiMachineHooksBuilder::<A2> {
            node_descriptor: Some(node_descriptor),
        }
    }
}

impl<A> TcpMultiMachineHooksBuilder<A>
where
    A: Clone + Display + ToSocketAddrs + Send + Sync + 'static,
{
    /// Build a new [`TcpMultiMachineHooks`] containing a sender and a receiver from a [`NodeDescriptor`].
    /// Everything is initialized and ready to be used.
    /// Beware, the hooks should run in the same process as the one this function is called.
    /// This is because we spawn a tokio runtime underneath.
    /// Check `<https://github.com/tokio-rs/tokio/issues/4301>` for more details.
    ///
    /// # Safety
    /// The returned [`TcpMultiMachineLlmpReceiverHook`] assumes that the `msg` parameter
    /// passed to the `on_new_message` method (or rather, the memory it points to),
    /// lives sufficiently long for an async background task to process it.
    pub unsafe fn build<I>(mut self) -> Result<TcpMultiMachineHooks<A, I>, Error>
    where
        I: Input + Send + Sync + 'static,
    {
        unsafe {
            let node_descriptor = self.node_descriptor.take().ok_or_else(|| {
                Error::illegal_state(
                    "The node descriptor can never be `None` at this point in the code",
                )
            })?;

            // Create the state of the hook. This will be shared with the background server, so we wrap
            // it with concurrent-safe objects
            let state = Arc::new(RwLock::new(TcpMultiMachineState {
                node_descriptor,
                parent: None,
                children: HashMap::default(),
                old_msgs: Vec::new(),
                #[cfg(feature = "llmp_compression")]
                compressor: GzipCompressor::new(),
            }));

            let rt = Arc::new(
                Runtime::new().map_err(|_| Error::unknown("Tokio runtime spawning failed"))?,
            );

            TcpMultiMachineState::init::<I>(&state.clone(), &rt.clone())?;

            Ok(TcpMultiMachineHooks {
                sender: TcpMultiMachineLlmpSenderHook::new(state.clone(), rt.clone()),
                receiver: TcpMultiMachineLlmpReceiverHook::new(state, rt),
            })
        }
    }
}

impl<A> TcpMultiMachineState<A>
where
    A: Clone + Display + ToSocketAddrs + Send + Sync + 'static,
{
    /// Initializes the Multi-Machine state.
    ///
    /// # Safety
    ///
    /// This should be run **only once**, in the same process as the llmp hooks, and before the hooks
    /// are effectively used.
    unsafe fn init<I: Input>(
        self_mutex: &Arc<RwLock<Self>>,
        rt: &Arc<Runtime>,
    ) -> Result<(), Error> {
        let node_descriptor =
            rt.block_on(async { self_mutex.read().await.node_descriptor.clone() });

        // Try to connect to the parent if we should
        rt.block_on(async {
            let parent_mutex = self_mutex.clone();
            let mut parent_lock = parent_mutex.write().await;

            if let Some(parent_addr) = &parent_lock.node_descriptor.parent_addr {
                let timeout = current_time() + parent_lock.node_descriptor.timeout;

                parent_lock.parent = loop {
                    log::debug!("Trying to connect to parent @ {parent_addr}..");
                    match TcpStream::connect(parent_addr).await {
                        Ok(stream) => {
                            log::debug!("Connected to parent @ {parent_addr}");

                            break Some(stream);
                        }
                        Err(e) => {
                            if current_time() > timeout {
                                return Err(Error::os_error(e, "Unable to connect to parent"));
                            }
                        }
                    }

                    time::sleep(Duration::from_secs(1)).await;
                };
            }

            Ok(())
        })?;

        // Now, setup the background tasks for the children to connect to
        if let Some(listening_port) = node_descriptor.node_listening_port {
            let bg_state = self_mutex.clone();
            let _handle: JoinHandle<Result<(), Error>> = rt.spawn(async move {
                let addr = format!("0.0.0.0:{listening_port}");
                log::debug!("Starting background child task on {addr}...");
                let listener = TcpListener::bind(addr).await.map_err(|e| {
                    Error::os_error(e, format!("Error while binding to port {listening_port}"))
                })?;
                let state = bg_state;

                // The main listening loop. Should never fail.
                'listening: loop {
                    log::debug!("listening for children on {listener:?}...");
                    match listener.accept().await {
                        Ok((mut stream, addr)) => {
                            log::debug!("{addr} joined the children.");
                            let mut state_guard = state.write().await;

                            if let Err(e) = state_guard
                                .send_old_events_to_stream::<I>(&mut stream)
                                .await
                            {
                                log::error!("Error while send old messages: {e:?}.");
                                log::error!("The loop will resume");
                                continue 'listening;
                            }

                            state_guard.children.insert(NodeId::new(), stream);
                            log::debug!(
                                "[pid {}]{addr} added the child. nb children: {}",
                                process::id(),
                                state_guard.children.len()
                            );
                        }
                        Err(e) => {
                            log::error!("Error while accepting child {e:?}.");
                        }
                    }
                }
            });
        }

        Ok(())
    }

    /// Add an event as past event.
    pub fn add_past_msg(&mut self, msg: &[u8]) {
        self.old_msgs.push(msg.to_vec());
    }

    /// The compressor
    #[cfg(feature = "llmp_compression")]
    pub fn compressor(&mut self) -> &GzipCompressor {
        &self.compressor
    }

    /// Read a [`TcpMultiMachineMsg`] from a stream.
    /// Expects a message written by [`TcpMultiMachineState::write_msg`].
    /// If there is nothing to read from the stream, return asap with Ok(None).
    #[expect(clippy::uninit_vec)]
    async fn read_msg<'a, I: Input + 'a>(
        stream: &mut TcpStream,
    ) -> Result<Option<MultiMachineMsg<'a, I>>, Error> {
        // 0. Check if we should try to fetch something from the stream
        let mut dummy_byte: [u8; 1] = [0u8];
        log::debug!("Starting read msg...");

        let n_read = match stream.try_read(&mut dummy_byte) {
            Ok(n) => n,
            Err(e) if e.kind() == ErrorKind::WouldBlock => {
                return Ok(None);
            }
            Err(e) => return Err(Error::os_error(e, "try read failed")),
        };

        log::debug!("msg read.");

        if n_read == 0 {
            log::debug!("No dummy byte received...");
            return Ok(None); // Nothing to read from this stream
        }

        log::debug!("Received dummy byte!");

        // we should always read the dummy byte at this point.
        assert_eq!(u8::from_le_bytes(dummy_byte), DUMMY_BYTE);

        // 1. Read msg size
        let mut node_msg_len: [u8; 4] = [0; 4];
        log::debug!("Receiving msg len...");
        stream.read_exact(&mut node_msg_len).await?;
        log::debug!("msg len received.");
        let node_msg_len = u32::from_le_bytes(node_msg_len) as usize;

        // 2. Read msg
        // do not store msg on the stack to avoid overflow issues
        // TODO: optimize with less allocations...
        let mut node_msg: Vec<u8> = Vec::with_capacity(node_msg_len);
        unsafe {
            node_msg.set_len(node_msg_len);
        }
        log::debug!("Receiving msg...");
        stream.read_exact(node_msg.as_mut_slice()).await?;
        log::debug!("msg received.");
        let node_msg = node_msg.into_boxed_slice();

        Ok(Some(MultiMachineMsg::from_llmp_msg(node_msg)))
    }

    /// Write an [`OwnedTcpMultiMachineMsg`] to a stream.
    /// Can be read back using [`TcpMultiMachineState::read_msg`].
    async fn write_msg<I: Input>(
        stream: &mut TcpStream,
        msg: &MultiMachineMsg<'_, I>,
    ) -> Result<(), Error> {
        let serialized_msg = msg.serialize_as_ref();
        let msg_len = u32::to_le_bytes(serialized_msg.len() as u32);

        // 0. Write the dummy byte
        log::debug!("Sending dummy byte...");
        stream.write_all(&[DUMMY_BYTE]).await?;
        log::debug!("dummy byte sent.");

        // 1. Write msg size
        log::debug!("Sending msg len...");
        stream.write_all(&msg_len).await?;
        log::debug!("msg len sent.");

        // 2. Write msg
        log::debug!("Sending msg...");
        stream.write_all(serialized_msg).await?;
        log::debug!("msg sent.");

        Ok(())
    }

    pub(crate) async fn send_old_events_to_stream<I: Input>(
        &mut self,
        stream: &mut TcpStream,
    ) -> Result<(), Error> {
        log::debug!("Send old events to new child...");

        for old_msg in &self.old_msgs {
            let event_ref: MultiMachineMsg<I> =
                MultiMachineMsg::llmp_msg(OwnedRef::Ref(old_msg.as_slice()));
            log::debug!("Sending an old message...");
            Self::write_msg(stream, &event_ref).await?;
            log::debug!("Old message sent.");
        }

        log::debug!("Sent {} old messages.", self.old_msgs.len());

        Ok(())
    }

    pub(crate) async fn send_interesting_event_to_nodes<I: Input>(
        &mut self,
        msg: &MultiMachineMsg<'_, I>,
    ) -> Result<(), Error> {
        log::debug!("Sending interesting events to nodes...");

        if self
            .node_descriptor
            .flags
            .intersects(NodePolicy::SendToParent)
        {
            if let Some(parent) = &mut self.parent {
                log::debug!("Sending to parent...");
                if let Err(e) = Self::write_msg(parent, msg).await {
                    log::error!(
                        "The parent disconnected. We won't try to communicate with it again."
                    );
                    log::error!("Error: {e:?}");
                    self.parent.take();
                }
            }
        }

        if self
            .node_descriptor
            .flags
            .intersects(NodePolicy::SendToChildren)
        {
            let mut ids_to_remove: Vec<NodeId> = Vec::new();
            for (child_id, child_stream) in &mut self.children {
                log::debug!("Sending to child {child_id:?}...");
                if let Err(err) = Self::write_msg(child_stream, msg).await {
                    // most likely the child disconnected. drop the connection later on and continue.
                    log::debug!(
                        "The child disconnected. We won't try to communicate with it again. Error: {err:?}"
                    );
                    ids_to_remove.push(*child_id);
                }
            }

            // Garbage collect disconnected children
            for id_to_remove in &ids_to_remove {
                log::debug!("Child {id_to_remove:?} has been garbage collected.");
                self.children.remove(id_to_remove);
            }
        }

        Ok(())
    }

    /// Flush the message queue from other nodes and add incoming events to the
    /// centralized event manager queue.
    pub(crate) async fn receive_new_messages_from_nodes<I: Input>(
        &mut self,
        msgs: &mut Vec<MultiMachineMsg<'_, I>>,
    ) -> Result<(), Error> {
        log::debug!("Checking for new events from other nodes...");
        // let mut nb_received = 0usize;

        // Our (potential) parent could have something for us
        if let Some(parent) = &mut self.parent {
            loop {
                // Exit if received a lot of inputs at once.
                // TODO: this causes problems in some cases, it could freeze all fuzzer instances.
                // if nb_received > MAX_NB_RECEIVED_AT_ONCE {
                //     log::debug!("hitting MAX_NB_RECEIVED_AT_ONCE limit...");
                //     return Ok(());
                // }

                log::debug!("Receiving from parent...");
                match Self::read_msg(parent).await {
                    Ok(Some(msg)) => {
                        log::debug!("Received event from parent");
                        // The parent has something for us, we store it
                        msgs.push(msg);
                        // nb_received += 1;
                    }

                    Ok(None) => {
                        // nothing from the parent, we continue
                        log::debug!("Nothing from parent");
                        break;
                    }

                    Err(Error::OsError(_, _, _)) => {
                        // most likely the parent disconnected. drop the connection
                        log::debug!(
                            "The parent disconnected. We won't try to communicate with it again."
                        );
                        self.parent.take();
                        break;
                    }

                    Err(e) => {
                        log::debug!("An error occurred and was not expected.");
                        return Err(e);
                    }
                }
            }
        }

        // What about the (potential) children?
        let mut ids_to_remove: Vec<NodeId> = Vec::new();
        log::debug!(
            "[pid {}] Nb children: {}",
            process::id(),
            self.children.len()
        );
        for (child_id, child_stream) in &mut self.children {
            loop {
                // Exit if received a lot of inputs at once.
                // if nb_received > MAX_NB_RECEIVED_AT_ONCE {
                //    return Ok(());
                //}

                log::debug!("Receiving from child {child_id:?}...");
                match Self::read_msg(child_stream).await {
                    Ok(Some(msg)) => {
                        // The parent has something for us, we store it
                        log::debug!("Received event from child!");
                        msgs.push(msg);
                        // nb_received += 1;
                    }

                    Ok(None) => {
                        // nothing from the parent, we continue
                        log::debug!("Nothing from child");
                        break;
                    }

                    Err(Error::OsError(e, _, _)) => {
                        // most likely the parent disconnected. drop the connection
                        log::error!(
                            "The child disconnected. We won't try to communicate with it again."
                        );
                        log::error!("Error: {e:?}");
                        ids_to_remove.push(*child_id);
                        break;
                    }

                    Err(e) => {
                        // Other error
                        log::debug!("An error occurred and was not expected.");
                        return Err(e);
                    }
                }
            }
        }

        // Garbage collect disconnected children
        for id_to_remove in &ids_to_remove {
            log::debug!("Child {id_to_remove:?} has been garbage collected.");
            self.children.remove(id_to_remove);
        }

        Ok(())
    }
}
