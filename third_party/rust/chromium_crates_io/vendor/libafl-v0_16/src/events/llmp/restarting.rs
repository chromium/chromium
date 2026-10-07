//! The `LLMP` restarting manager will
//! forward messages over lockless shared maps.
//! When the target crashes, a watch process (the parent) will
//! restart/refork it.

#[cfg(feature = "std")]
use alloc::string::ToString;
use core::{net::SocketAddr, num::NonZeroUsize, time::Duration};
#[cfg(feature = "std")]
use std::net::TcpStream;

#[cfg(feature = "std")]
use libafl_bolts::llmp::{TcpRequest, TcpResponse, recv_tcp_msg, send_tcp_msg};
use libafl_bolts::{
    core_affinity::CoreId,
    llmp::{Broker, LlmpBroker, LlmpClientDescription, LlmpConnection},
    shmem::{ShMem, ShMemProvider, StdShMem, StdShMemProvider},
    tuples::tuple_list,
};
#[cfg(feature = "std")]
use libafl_core::IP_LOCALHOST;
use serde::{Serialize, de::DeserializeOwned};

use crate::{
    Error, HasMetadata,
    corpus::HasCurrentCorpusId,
    events::{EventConfig, EventManagerHooksTuple, StdLlmpEventHook, launcher::ClientDescription},
    inputs::Input,
    monitors::Monitor,
    state::{
        HasCorpus, HasCurrentStageId, HasCurrentTestcase, HasExecutions, HasImported,
        HasLastReportTime, HasSolutions, MaybeHasClientPerfMonitor, Stoppable,
    },
};
/// A manager that can restart on the fly, storing states in-between (in `on_restart`)
pub type LlmpRestartingEventManager<EMH, I, S, SHM, SP> =
    crate::events::RestartingEventManager<super::LlmpEventManager<EMH, I, S, SHM, SP>, SP>;

impl<EMH, I, S, SHM, SP> LlmpRestartingEventManager<EMH, I, S, SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    /// Calling this function will tell the llmp broker that this client is exiting
    /// This should be called from the restarter not from the actual fuzzer client
    /// This function serves the same roll as the `LlmpClient.send_exiting()`
    /// However, from the the event restarter process it is forbidden to call `send_exiting()`
    /// (You can call it and it compiles but you should never do so)
    /// `send_exiting()` is exclusive to the fuzzer client.
    #[cfg(feature = "std")]
    pub fn detach_from_broker(&self, broker_port: u16) -> Result<(), Error> {
        let client_id = self.inner.llmp.sender().id();
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

use crate::events::ShouldSaveState;

/// The llmp connection from the actual fuzzer to the process supervising it
const _ENV_FUZZER_SENDER: &str = "_AFL_ENV_FUZZER_SENDER";
const _ENV_FUZZER_RECEIVER: &str = "_AFL_ENV_FUZZER_RECEIVER";
/// The llmp (2 way) connection from a fuzzer to the broker (broadcasting all other fuzzer messages)
const _ENV_FUZZER_BROKER_CLIENT_INITIAL: &str = "_AFL_ENV_FUZZER_BROKER_CLIENT";

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
    I: DeserializeOwned + Input,
    MT: Monitor,
    S: Serialize
        + DeserializeOwned
        + HasCurrentStageId
        + HasImported
        + HasCurrentTestcase<I>
        + HasSolutions<I>
        + Stoppable
        + HasMetadata
        + HasExecutions
        + HasLastReportTime
        + MaybeHasClientPerfMonitor
        + HasCurrentCorpusId
        + HasCorpus<I>,
{
    setup_restarting_mgr_llmp(
        StdShMemProvider::new()?,
        configuration,
        Some(monitor),
        broker_port,
        ManagerKind::Any,
        None,
        ShouldSaveState::OnRestart,
        tuple_list!(),
    )
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
    MT: Monitor,
    S: Serialize
        + DeserializeOwned
        + HasCurrentStageId
        + HasImported
        + HasCurrentTestcase<I>
        + HasSolutions<I>
        + Stoppable
        + HasMetadata
        + HasExecutions
        + HasLastReportTime
        + MaybeHasClientPerfMonitor
        + HasCurrentCorpusId
        + HasCorpus<I>,
    I: DeserializeOwned + Input,
{
    setup_restarting_mgr_llmp(
        StdShMemProvider::new()?,
        configuration,
        Some(monitor),
        broker_port,
        ManagerKind::Any,
        None,
        ShouldSaveState::OnRestart,
        tuple_list!(),
    )
}

/// Sets up a restarting fuzzer, using the [`StdShMemProvider`], and standard features.
#[expect(
    clippy::type_complexity,
    clippy::too_many_arguments,
    clippy::needless_pass_by_value
)]
pub fn setup_restarting_mgr_llmp<EMH, I, MT, S, SP>(
    shmem_provider: SP,
    configuration: EventConfig,
    mut monitor: Option<MT>,
    broker_port: u16,
    kind: ManagerKind,
    exit_cleanly_after: Option<NonZeroUsize>,
    serialize_state: ShouldSaveState,
    hooks: EMH,
) -> Result<
    (
        Option<S>,
        LlmpRestartingEventManager<EMH, I, S, SP::ShMem, SP>,
    ),
    Error,
>
where
    EMH: EventManagerHooksTuple<I, S> + Copy + Clone,
    I: DeserializeOwned + Input,
    MT: Monitor,
    S: Serialize
        + DeserializeOwned
        + HasCurrentStageId
        + HasImported
        + HasCurrentTestcase<I>
        + HasSolutions<I>
        + Stoppable
        + HasMetadata
        + HasExecutions
        + HasLastReportTime
        + MaybeHasClientPerfMonitor
        + HasCurrentCorpusId
        + HasCorpus<I>,
    SP: ShMemProvider,
{
    // We start ourselves as child process to actually fuzz
    let restarting_mgr = crate::events::RestartingMgr::new(shmem_provider.clone());
    #[cfg(unix)]
    let mut restarting_mgr = restarting_mgr;
    #[cfg(unix)]
    restarting_mgr.fork(true);

    crate::events::restarting::setup_generic_restarting_mgr(
        restarting_mgr,
        |state: Option<LlmpClientDescription>| {
            let broker_things =
                |mut broker: LlmpBroker<_, SP::ShMem, SP>,
                 remote_broker_addr: Option<SocketAddr>| {
                    if let Some(remote_broker_addr) = remote_broker_addr {
                        log::info!("B2b: Connecting to {remote_broker_addr:?}");
                        broker.inner_mut().connect_b2b(remote_broker_addr)?;
                    }

                    if let Some(exit_cleanly_after) = exit_cleanly_after {
                        broker.set_exit_after(exit_cleanly_after);
                    }

                    broker.loop_with_timeouts(
                        Duration::from_secs(30),
                        Some(Duration::from_millis(5)),
                    );

                    #[cfg(feature = "llmp_debug")]
                    log::info!("The last client quit. Exiting.");

                    Err(Error::shutting_down())
                };

            // We get here if we are on Unix, or we are a broker on Windows (or without forks).
            let (mgr, core_id) = if let Some(desc) = state {
                let mgr = super::LlmpEventManagerBuilder::new()
                    .hooks(hooks)
                    .save_state(serialize_state)
                    .build_existing_client_from_description(
                        shmem_provider.clone(),
                        &desc,
                        configuration,
                    )?;

                (mgr, None)
            } else {
                match &kind {
                    ManagerKind::Any => {
                        let connection =
                            LlmpConnection::on_port(shmem_provider.clone(), broker_port)?;
                        match connection {
                            LlmpConnection::IsBroker { broker } => {
                                let llmp_hook =
                                    StdLlmpEventHook::<I, MT>::new(monitor.take().unwrap())?;

                                // Yep, broker. Just loop here.
                                log::info!(
                                    "Doing broker things. Run this tool again to start fuzzing in a client."
                                );

                                broker_things(
                                    broker.add_hooks(tuple_list!(llmp_hook)),
                                    None, // remote_broker_addr
                                )?;

                                return Err(Error::shutting_down());
                            }
                            LlmpConnection::IsClient { client } => {
                                let mgr: super::LlmpEventManager<EMH, I, S, SP::ShMem, SP> =
                                    super::LlmpEventManagerBuilder::new()
                                        .hooks(hooks)
                                        .save_state(serialize_state)
                                        .build_from_client(client, configuration)?;

                                (mgr, None::<CoreId>)
                            }
                        }
                    }
                    ManagerKind::Broker => {
                        let llmp_hook = StdLlmpEventHook::<I, MT>::new(monitor.take().unwrap())?;

                        let broker = LlmpBroker::create_attach_to_tcp(
                            shmem_provider.clone(),
                            tuple_list!(llmp_hook),
                            broker_port,
                        )?;

                        broker_things(broker, None)?;

                        return Err(Error::shutting_down());
                    }
                    ManagerKind::Client { client_description } => {
                        // We are a client
                        let mgr = super::LlmpEventManagerBuilder::new()
                            .hooks(hooks)
                            .save_state(serialize_state)
                            .build_on_port(shmem_provider.clone(), broker_port, configuration)?;

                        (mgr, Some(client_description.core_id()))
                    }
                }
            };

            if let Some(core_id) = core_id {
                let _ = core_id.set_affinity();
            }

            Ok(mgr)
        },
    )
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
        events::llmp::restarting::_ENV_FUZZER_SENDER,
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

        let mut llmp_mgr = crate::events::llmp::LlmpEventManagerBuilder::new()
            .build_from_client(llmp_client, "fuzzer".into())
            .unwrap();

        let scheduler = RandScheduler::new();

        let feedback = ConstFeedback::new(true);
        let objective = ConstFeedback::new(false);

        let mut fuzzer = StdFuzzer::new(scheduler, feedback, objective);

        let mut harness = |_buf: &BytesInput| ExitKind::Ok;
        let mut executor = InProcessExecutor::builder()
            .harness(&mut harness)
            .observers(tuple_list!(time))
            .fuzzer(&mut fuzzer)
            .state(&mut state)
            .event_mgr(&mut llmp_mgr)
            .build()
            .unwrap();

        let mutator = BitFlipMutator::new();
        let mut stages = tuple_list!(StdMutationalStage::new(mutator));

        // First, create a channel from the current fuzzer to the next to store state between restarts.
        let mut staterestorer = StateRestorer::<StdShMem, StdShMemProvider>::new(
            shmem_provider.new_shmem(256 * 1024 * 1024).unwrap(),
        );

        staterestorer.reset();
        staterestorer
            .save(&(&mut state, &llmp_mgr.describe().unwrap()))
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
        let mut llmp_clone = crate::events::llmp::LlmpEventManagerBuilder::new()
            .build_existing_client_from_description(
                shmem_provider,
                &mgr_description,
                "fuzzer".into(),
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
