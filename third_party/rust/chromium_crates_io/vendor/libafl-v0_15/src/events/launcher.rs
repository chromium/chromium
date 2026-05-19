//! The [`Launcher`] launches multiple fuzzer instances in parallel.
//! Thanks to it, we won't need a `for` loop in a shell script...
//!
//! It will hide child output, unless the settings indicate otherwise, or the `LIBAFL_DEBUG_OUTPUT` env variable is set.
//!
//! To use multiple [`Launcher`]`s` for individual configurations,
//! we can set `spawn_broker` to `false` on all but one.
//!
//! To connect multiple nodes together via TCP, we can use the `remote_broker_addr`.
//! (this requires the `llmp_bind_public` compile-time feature for `LibAFL`).
//!
//! On `Unix` systems, the [`Launcher`] will use `fork` if the `fork` feature is used for `LibAFL`.
//! Else, it will start subsequent nodes with the same commandline, and will set special `env` variables accordingly.

use alloc::string::String;
use core::{
    fmt::{self, Debug, Formatter},
    net::SocketAddr,
    num::NonZeroUsize,
    time::Duration,
};

use libafl_bolts::{
    core_affinity::{CoreId, Cores},
    shmem::ShMemProvider,
    tuples::tuple_list,
};
use serde::{Deserialize, Serialize, de::DeserializeOwned};
use typed_builder::TypedBuilder;
#[cfg(all(unix, feature = "fork"))]
use {
    crate::{
        events::{CentralizedLlmpHook, StdLlmpEventHook, centralized::CentralizedEventManager},
        inputs::Input,
    },
    alloc::boxed::Box,
    alloc::string::ToString,
    libafl_bolts::{
        core_affinity::get_core_ids,
        llmp::{Broker, Brokers, LlmpBroker},
        os::{ForkResult, fork},
    },
};
#[cfg(unix)]
use {
    libafl_bolts::os::dup2,
    std::{fs::File, os::unix::io::AsRawFd},
};
#[cfg(any(windows, not(feature = "fork")))]
use {libafl_bolts::os::startable_self, std::process::Stdio};

#[cfg(all(unix, feature = "fork", feature = "multi_machine"))]
use crate::events::multi_machine::{NodeDescriptor, TcpMultiMachineHooks};
use crate::{
    Error,
    events::{
        EventConfig, EventManagerHooksTuple,
        llmp::{LlmpRestartingEventManager, LlmpShouldSaveState, ManagerKind, RestartingMgr},
    },
    monitors::Monitor,
};

/// The (internal) `env` that indicates we're running as client.
const _AFL_LAUNCHER_CLIENT: &str = "AFL_LAUNCHER_CLIENT";

/// The env variable to set in order to enable child output
#[cfg(all(feature = "fork", unix))]
const LIBAFL_DEBUG_OUTPUT: &str = "LIBAFL_DEBUG_OUTPUT";

/// Information about this client from the launcher
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ClientDescription {
    id: usize,
    overcommit_id: usize,
    core_id: CoreId,
}

impl ClientDescription {
    /// Create a [`ClientDescription`]
    #[must_use]
    pub fn new(id: usize, overcommit_id: usize, core_id: CoreId) -> Self {
        Self {
            id,
            overcommit_id,
            core_id,
        }
    }

    /// Id unique to all clients spawned by this launcher
    #[must_use]
    pub fn id(&self) -> usize {
        self.id
    }

    /// [`CoreId`] this client is bound to
    #[must_use]
    pub fn core_id(&self) -> CoreId {
        self.core_id
    }

    /// Incremental id unique for all clients on the same core
    #[must_use]
    pub fn overcommit_id(&self) -> usize {
        self.overcommit_id
    }

    /// Create a string representation safe for environment variables
    #[must_use]
    pub fn to_safe_string(&self) -> String {
        format!("{}_{}_{}", self.id, self.overcommit_id, self.core_id.0)
    }

    /// Parse the string created by [`Self::to_safe_string`].
    #[must_use]
    pub fn from_safe_string(input: &str) -> Self {
        let mut iter = input.split('_');
        let id = iter.next().unwrap().parse().unwrap();
        let overcommit_id = iter.next().unwrap().parse().unwrap();
        let core_id = iter.next().unwrap().parse::<usize>().unwrap().into();
        Self {
            id,
            overcommit_id,
            core_id,
        }
    }
}

/// Provides a [`Launcher`], which can be used to launch a fuzzing run on a specified list of cores
///
/// Will hide child output, unless the settings indicate otherwise, or the `LIBAFL_DEBUG_OUTPUT` env variable is set.
#[derive(TypedBuilder)]
pub struct Launcher<'a, CF, MT, SP> {
    /// The `ShmemProvider` to use
    shmem_provider: SP,
    /// The monitor instance to use
    monitor: MT,
    /// The configuration
    configuration: EventConfig,
    /// The 'main' function to run for each client forked. This probably shouldn't return
    #[builder(default, setter(strip_option))]
    run_client: Option<CF>,
    /// The broker port to use (or to attach to, in case [`Self::spawn_broker`] is `false`)
    #[builder(default = 1337_u16)]
    broker_port: u16,
    /// The list of cores to run on
    cores: &'a Cores,
    /// The number of clients to spawn on each core
    #[builder(default = 1)]
    overcommit: usize,
    /// A file name to write all client output to
    #[cfg(unix)]
    #[builder(default = None)]
    stdout_file: Option<&'a str>,
    /// The time in milliseconds to delay between child launches
    #[builder(default = 10)]
    launch_delay: u64,
    /// The actual, opened, `stdout_file` - so that we keep it open until the end
    #[cfg(all(unix, feature = "fork"))]
    #[builder(setter(skip), default = None)]
    opened_stdout_file: Option<File>,
    /// A file name to write all client stderr output to. If not specified, output is sent to
    /// `stdout_file`.
    #[cfg(unix)]
    #[builder(default = None)]
    stderr_file: Option<&'a str>,
    /// The actual, opened, `stdout_file` - so that we keep it open until the end
    #[cfg(all(unix, feature = "fork"))]
    #[builder(setter(skip), default = None)]
    opened_stderr_file: Option<File>,
    /// The `ip:port` address of another broker to connect our new broker to for multi-machine
    /// clusters.
    #[builder(default = None)]
    remote_broker_addr: Option<SocketAddr>,
    /// If this launcher should spawn a new `broker` on `[Self::broker_port]` (default).
    /// The reason you may not want this is, if you already have a [`Launcher`]
    /// with a different configuration (for the same target) running on this machine.
    /// Then, clients launched by this [`Launcher`] can connect to the original `broker`.
    #[builder(default = true)]
    spawn_broker: bool,
    /// Tell the manager to serialize or not the state on restart
    #[builder(default = LlmpShouldSaveState::OnRestart)]
    serialize_state: LlmpShouldSaveState,
}

impl<CF, MT, SP> Debug for Launcher<'_, CF, MT, SP> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        let mut dbg_struct = f.debug_struct("Launcher");
        dbg_struct
            .field("configuration", &self.configuration)
            .field("broker_port", &self.broker_port)
            .field("core", &self.cores)
            .field("spawn_broker", &self.spawn_broker)
            .field("remote_broker_addr", &self.remote_broker_addr);
        #[cfg(unix)]
        {
            dbg_struct
                .field("stdout_file", &self.stdout_file)
                .field("stderr_file", &self.stderr_file);
        }

        dbg_struct.finish_non_exhaustive()
    }
}

impl<CF, MT, SP> Launcher<'_, CF, MT, SP>
where
    MT: Monitor + Clone,
{
    /// Launch the broker and the clients and fuzz
    #[cfg(any(windows, not(feature = "fork"), all(unix, feature = "fork")))]
    pub fn launch<I, S>(&mut self) -> Result<(), Error>
    where
        CF: FnOnce(
            Option<S>,
            LlmpRestartingEventManager<(), I, S, SP::ShMem, SP>,
            ClientDescription,
        ) -> Result<(), Error>,
        I: DeserializeOwned,
        S: DeserializeOwned + Serialize,
        SP: ShMemProvider,
    {
        Self::launch_with_hooks(self, tuple_list!())
    }
}

impl<CF, MT, SP> Launcher<'_, CF, MT, SP>
where
    MT: Monitor + Clone,
    SP: ShMemProvider,
{
    /// Launch the broker and the clients and fuzz with a user-supplied hook
    #[cfg(all(unix, feature = "fork"))]
    pub fn launch_with_hooks<EMH, I, S>(&mut self, hooks: EMH) -> Result<(), Error>
    where
        S: DeserializeOwned + Serialize,
        I: DeserializeOwned,
        EMH: EventManagerHooksTuple<I, S> + Clone + Copy,
        CF: FnOnce(
            Option<S>,
            LlmpRestartingEventManager<EMH, I, S, SP::ShMem, SP>,
            ClientDescription,
        ) -> Result<(), Error>,
    {
        if self.cores.ids.is_empty() {
            return Err(Error::illegal_argument(
                "No cores to spawn on given, cannot launch anything.",
            ));
        }

        if self.run_client.is_none() {
            return Err(Error::illegal_argument(
                "No client callback provided".to_string(),
            ));
        }

        let core_ids = get_core_ids()?;
        let mut handles = vec![];

        log::info!("spawning on cores: {:?}", self.cores);

        self.opened_stdout_file = self
            .stdout_file
            .map(|filename| File::create(filename).unwrap());
        self.opened_stderr_file = self
            .stderr_file
            .map(|filename| File::create(filename).unwrap());

        let debug_output = std::env::var(LIBAFL_DEBUG_OUTPUT).is_ok();

        // Spawn clients
        let mut index = 0_usize;
        for bind_to in core_ids {
            if self.cores.ids.contains(&bind_to) {
                for overcommit_id in 0..self.overcommit {
                    index += 1;
                    self.shmem_provider.pre_fork()?;
                    // # Safety
                    // Fork is safe in general, apart from potential side effects to the OS and other threads
                    match unsafe { fork() }? {
                        ForkResult::Parent(child) => {
                            self.shmem_provider.post_fork(false)?;
                            handles.push(child.pid);
                            log::info!(
                                "child spawned with id {index} and bound to core {bind_to:?}"
                            );
                        }
                        ForkResult::Child => {
                            // # Safety
                            // A call to `getpid` is safe.
                            log::info!("{:?} PostFork", unsafe { libc::getpid() });
                            self.shmem_provider.post_fork(true)?;

                            std::thread::sleep(Duration::from_millis(
                                index as u64 * self.launch_delay,
                            ));

                            if !debug_output {
                                if let Some(file) = &self.opened_stdout_file {
                                    // # Safety
                                    // We assume the file descriptors are valid here
                                    unsafe {
                                        dup2(file.as_raw_fd(), libc::STDOUT_FILENO)?;
                                        match &self.opened_stderr_file {
                                            Some(stderr) => {
                                                dup2(stderr.as_raw_fd(), libc::STDERR_FILENO)?;
                                            }
                                            _ => {
                                                dup2(file.as_raw_fd(), libc::STDERR_FILENO)?;
                                            }
                                        }
                                    }
                                }
                            }

                            let client_description =
                                ClientDescription::new(index, overcommit_id, bind_to);

                            // Fuzzer client. keeps retrying the connection to broker till the broker starts
                            let builder = RestartingMgr::<EMH, I, MT, S, SP>::builder()
                                .shmem_provider(self.shmem_provider.clone())
                                .broker_port(self.broker_port)
                                .kind(ManagerKind::Client {
                                    client_description: client_description.clone(),
                                })
                                .configuration(self.configuration)
                                .serialize_state(self.serialize_state)
                                .hooks(hooks);
                            let (state, mgr) = builder.build().launch()?;

                            return (self.run_client.take().unwrap())(
                                state,
                                mgr,
                                client_description,
                            );
                        }
                    }
                }
            }
        }

        if self.spawn_broker {
            log::info!("I am broker!!.");

            // TODO we don't want always a broker here, think about using different laucher process to spawn different configurations
            let builder = RestartingMgr::<EMH, I, MT, S, SP>::builder()
                .shmem_provider(self.shmem_provider.clone())
                .monitor(Some(self.monitor.clone()))
                .broker_port(self.broker_port)
                .kind(ManagerKind::Broker)
                .remote_broker_addr(self.remote_broker_addr)
                .exit_cleanly_after(Some(NonZeroUsize::try_from(self.cores.ids.len()).unwrap()))
                .configuration(self.configuration)
                .serialize_state(self.serialize_state)
                .hooks(hooks);

            builder.build().launch()?;

            // Broker exited. kill all clients.
            for handle in &handles {
                // # Safety
                // Normal libc call, no dereferences whatsoever
                unsafe {
                    libc::kill(*handle, libc::SIGINT);
                }
            }
        } else {
            for handle in &handles {
                let mut status = 0;
                log::info!(
                    "Not spawning broker (spawn_broker is false). Waiting for fuzzer children to exit..."
                );
                unsafe {
                    libc::waitpid(*handle, &raw mut status, 0);
                    if status != 0 {
                        log::info!("Client with pid {handle} exited with status {status}");
                    }
                }
            }
        }

        Ok(())
    }

    /// Launch the broker and the clients and fuzz
    #[cfg(any(windows, not(feature = "fork")))]
    #[expect(clippy::too_many_lines, clippy::match_wild_err_arm)]
    pub fn launch_with_hooks<EMH, I, S>(&mut self, hooks: EMH) -> Result<(), Error>
    where
        CF: FnOnce(
            Option<S>,
            LlmpRestartingEventManager<EMH, I, S, SP::ShMem, SP>,
            ClientDescription,
        ) -> Result<(), Error>,
        EMH: EventManagerHooksTuple<I, S> + Clone + Copy,
        I: DeserializeOwned,
        S: DeserializeOwned + Serialize,
    {
        use libafl_bolts::core_affinity::get_core_ids;

        let is_client = std::env::var(_AFL_LAUNCHER_CLIENT);

        let mut handles = match is_client {
            Ok(core_conf) => {
                let client_description = ClientDescription::from_safe_string(&core_conf);
                // the actual client. do the fuzzing

                let builder = RestartingMgr::<EMH, I, MT, S, SP>::builder()
                    .shmem_provider(self.shmem_provider.clone())
                    .broker_port(self.broker_port)
                    .kind(ManagerKind::Client {
                        client_description: client_description.clone(),
                    })
                    .configuration(self.configuration)
                    .serialize_state(self.serialize_state)
                    .hooks(hooks);

                let (state, mgr) = builder.build().launch()?;

                return (self.run_client.take().unwrap())(state, mgr, client_description);
            }
            Err(std::env::VarError::NotPresent) => {
                // I am a broker
                // before going to the broker loop, spawn n clients

                let core_ids = get_core_ids().unwrap();
                let mut handles = vec![];

                log::info!("spawning on cores: {:?}", self.cores);

                let debug_output = std::env::var("LIBAFL_DEBUG_OUTPUT").is_ok();
                #[cfg(unix)]
                {
                    // Set own stdout and stderr as set by the user
                    if !debug_output {
                        let opened_stdout_file = self
                            .stdout_file
                            .map(|filename| File::create(filename).unwrap());
                        let opened_stderr_file = self
                            .stderr_file
                            .map(|filename| File::create(filename).unwrap());
                        if let Some(file) = opened_stdout_file {
                            // # Safety
                            // We assume the file descriptors are valid here
                            unsafe {
                                dup2(file.as_raw_fd(), libc::STDOUT_FILENO)?;
                                if let Some(stderr) = opened_stderr_file {
                                    dup2(stderr.as_raw_fd(), libc::STDERR_FILENO)?;
                                } else {
                                    dup2(file.as_raw_fd(), libc::STDERR_FILENO)?;
                                }
                            }
                        }
                    }
                }
                //spawn clients
                let mut index = 0;
                for core_id in core_ids {
                    if self.cores.ids.contains(&core_id) {
                        for overcommit_i in 0..self.overcommit {
                            index += 1;
                            // Forward own stdio to child processes, if requested by user
                            #[allow(unused_mut)] // mut only on certain cfgs
                            let (mut stdout, mut stderr) = (Stdio::null(), Stdio::null());
                            #[cfg(unix)]
                            {
                                if self.stdout_file.is_some() || self.stderr_file.is_some() {
                                    stdout = Stdio::inherit();
                                    stderr = Stdio::inherit();
                                }
                            }

                            std::thread::sleep(Duration::from_millis(
                                core_id.0 as u64 * self.launch_delay,
                            ));

                            let client_description =
                                ClientDescription::new(index, overcommit_i, core_id);
                            // # Safety
                            // This is set only once, in here, for the child.
                            unsafe {
                                std::env::set_var(
                                    _AFL_LAUNCHER_CLIENT,
                                    client_description.to_safe_string(),
                                );
                            }
                            let mut child = startable_self()?;
                            let child = (if debug_output {
                                &mut child
                            } else {
                                child.stdout(stdout);
                                child.stderr(stderr)
                            })
                            .spawn()?;
                            handles.push(child);
                        }
                    }
                }
                handles
            }
            Err(_) => panic!("Env variables are broken, received non-unicode!"),
        };

        // It's fine to check this after the client spawn loop - since we won't have spawned any clients...
        // Doing it later means one less check in each spawned process.
        if self.cores.ids.is_empty() {
            return Err(Error::illegal_argument(
                "No cores to spawn on given, cannot launch anything.",
            ));
        }

        if self.spawn_broker {
            log::info!("I am broker!!.");

            let builder = RestartingMgr::<EMH, I, MT, S, SP>::builder()
                .shmem_provider(self.shmem_provider.clone())
                .monitor(Some(self.monitor.clone()))
                .broker_port(self.broker_port)
                .kind(ManagerKind::Broker)
                .remote_broker_addr(self.remote_broker_addr)
                .exit_cleanly_after(Some(NonZeroUsize::try_from(self.cores.ids.len()).unwrap()))
                .configuration(self.configuration)
                .serialize_state(self.serialize_state)
                .hooks(hooks);

            builder.build().launch()?;

            //broker exited. kill all clients.
            for handle in &mut handles {
                handle.kill()?;
            }
        } else {
            log::info!(
                "Not spawning broker (spawn_broker is false). Waiting for fuzzer children to exit..."
            );
            for handle in &mut handles {
                let ecode = handle.wait()?;
                if !ecode.success() {
                    log::info!("Client with handle {handle:?} exited with {ecode:?}");
                }
            }
        }

        Ok(())
    }
}

/// A Launcher that minimizes re-execution of shared testcases.
///
/// Provides a Launcher, which can be used to launch a fuzzing run on a specified list of cores with a single main and multiple secondary nodes
/// This is for centralized, the 4th argument of the closure should mean if this is the main node.
#[cfg(all(unix, feature = "fork"))]
#[derive(TypedBuilder)]
pub struct CentralizedLauncher<'a, CF, MF, MT, SP> {
    /// The `ShmemProvider` to use
    shmem_provider: SP,
    /// The monitor instance to use
    monitor: MT,
    /// The configuration
    configuration: EventConfig,
    /// The 'main' function to run for each secondary client forked. This probably shouldn't return
    #[builder(default, setter(strip_option))]
    secondary_run_client: Option<CF>,
    /// The 'main' function to run for the main evaluator node.
    #[builder(default, setter(strip_option))]
    main_run_client: Option<MF>,
    /// The broker port to use (or to attach to, in case [`Self::spawn_broker`] is `false`)
    #[builder(default = 1337_u16)]
    broker_port: u16,
    /// The centralized broker port to use (or to attach to, in case [`Self::spawn_broker`] is `false`)
    #[builder(default = 1338_u16)]
    centralized_broker_port: u16,
    /// The time observer by which to adaptively serialize
    /// The list of cores to run on
    cores: &'a Cores,
    /// The number of clients to spawn on each core
    #[builder(default = 1)]
    overcommit: usize,
    /// A file name to write all client output to
    #[builder(default = None)]
    stdout_file: Option<&'a str>,
    /// The time in milliseconds to delay between child launches
    #[builder(default = 10)]
    launch_delay: u64,
    /// The actual, opened, `stdout_file` - so that we keep it open until the end
    #[cfg(all(unix, feature = "fork"))]
    #[builder(setter(skip), default = None)]
    opened_stdout_file: Option<File>,
    /// A file name to write all client stderr output to. If not specified, output is sent to
    /// `stdout_file`.
    #[builder(default = None)]
    stderr_file: Option<&'a str>,
    /// The actual, opened, `stdout_file` - so that we keep it open until the end
    #[cfg(all(unix, feature = "fork"))]
    #[builder(setter(skip), default = None)]
    opened_stderr_file: Option<File>,
    /// The `ip:port` address of another broker to connect our new broker to for multi-machine
    /// clusters.
    #[builder(default = None)]
    remote_broker_addr: Option<SocketAddr>,
    #[cfg(feature = "multi_machine")]
    multi_machine_node_descriptor: NodeDescriptor<SocketAddr>,
    /// If this launcher should spawn a new `broker` on `[Self::broker_port]` (default).
    /// The reason you may not want this is, if you already have a [`Launcher`]
    /// with a different configuration (for the same target) running on this machine.
    /// Then, clients launched by this [`Launcher`] can connect to the original `broker`.
    #[builder(default = true)]
    spawn_broker: bool,
    /// Tell the manager to serialize or not the state on restart
    #[builder(default = LlmpShouldSaveState::OnRestart)]
    serialize_state: LlmpShouldSaveState,
}

#[cfg(all(unix, feature = "fork"))]
impl<CF, MF, MT, SP> Debug for CentralizedLauncher<'_, CF, MF, MT, SP> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("Launcher")
            .field("configuration", &self.configuration)
            .field("broker_port", &self.broker_port)
            .field("cores", &self.cores)
            .field("overcommit", &self.overcommit)
            .field("spawn_broker", &self.spawn_broker)
            .field("remote_broker_addr", &self.remote_broker_addr)
            .field("stdout_file", &self.stdout_file)
            .field("stderr_file", &self.stderr_file)
            .finish_non_exhaustive()
    }
}

/// The standard inner manager of centralized
pub type StdCentralizedInnerMgr<I, S, SHM, SP> = LlmpRestartingEventManager<(), I, S, SHM, SP>;

#[cfg(all(unix, feature = "fork"))]
impl<CF, MF, MT, SP> CentralizedLauncher<'_, CF, MF, MT, SP>
where
    MT: Monitor + Clone + 'static,
    SP: ShMemProvider + 'static,
{
    /// Launch a standard Centralized-based fuzzer
    pub fn launch<I, S>(&mut self) -> Result<(), Error>
    where
        S: DeserializeOwned + Serialize,
        I: DeserializeOwned + Input + Send + Sync + 'static,
        CF: FnOnce(
            Option<S>,
            CentralizedEventManager<
                StdCentralizedInnerMgr<I, S, SP::ShMem, SP>,
                I,
                S,
                SP::ShMem,
                SP,
            >,
            ClientDescription,
        ) -> Result<(), Error>,
        MF: FnOnce(
            Option<S>,
            CentralizedEventManager<
                StdCentralizedInnerMgr<I, S, SP::ShMem, SP>,
                I,
                S,
                SP::ShMem,
                SP,
            >,
            ClientDescription,
        ) -> Result<(), Error>,
    {
        let restarting_mgr_builder =
            |centralized_launcher: &Self, client_description: ClientDescription| {
                // Fuzzer client. keeps retrying the connection to broker till the broker starts
                let builder = RestartingMgr::<(), I, MT, S, SP>::builder()
                    .shmem_provider(centralized_launcher.shmem_provider.clone())
                    .broker_port(centralized_launcher.broker_port)
                    .kind(ManagerKind::Client { client_description })
                    .configuration(centralized_launcher.configuration)
                    .serialize_state(centralized_launcher.serialize_state)
                    .hooks(tuple_list!());

                builder.build().launch()
            };

        self.launch_generic(restarting_mgr_builder, restarting_mgr_builder)
    }
}

#[cfg(all(unix, feature = "fork"))]
impl<CF, MF, MT, SP> CentralizedLauncher<'_, CF, MF, MT, SP>
where
    MT: Monitor + Clone + 'static,
    SP: ShMemProvider + 'static,
{
    /// Launch a Centralized-based fuzzer.
    /// - `main_inner_mgr_builder` will be called to build the inner manager of the main node.
    /// - `secondary_inner_mgr_builder` will be called to build the inner manager of the secondary nodes.
    pub fn launch_generic<EM, EMB, I, S>(
        &mut self,
        main_inner_mgr_builder: EMB,
        secondary_inner_mgr_builder: EMB,
    ) -> Result<(), Error>
    where
        I: Input + Send + Sync + 'static,
        CF: FnOnce(
            Option<S>,
            CentralizedEventManager<EM, I, S, SP::ShMem, SP>,
            ClientDescription,
        ) -> Result<(), Error>,
        EMB: FnOnce(&Self, ClientDescription) -> Result<(Option<S>, EM), Error>,
        MF: FnOnce(
            Option<S>,
            CentralizedEventManager<EM, I, S, SP::ShMem, SP>, // No broker_hooks for centralized EM
            ClientDescription,
        ) -> Result<(), Error>,
    {
        let mut main_inner_mgr_builder = Some(main_inner_mgr_builder);
        let mut secondary_inner_mgr_builder = Some(secondary_inner_mgr_builder);

        if self.cores.ids.is_empty() {
            return Err(Error::illegal_argument(
                "No cores to spawn on given, cannot launch anything.",
            ));
        }

        if self.secondary_run_client.is_none() {
            return Err(Error::illegal_argument(
                "No client callback provided".to_string(),
            ));
        }

        let core_ids = get_core_ids().unwrap();
        let mut handles = vec![];

        log::debug!("spawning on cores: {:?}", self.cores);

        self.opened_stdout_file = self
            .stdout_file
            .map(|filename| File::create(filename).unwrap());
        self.opened_stderr_file = self
            .stderr_file
            .map(|filename| File::create(filename).unwrap());

        let debug_output = std::env::var(LIBAFL_DEBUG_OUTPUT).is_ok();

        // Spawn clients
        let mut index = 0_usize;
        for bind_to in core_ids {
            if self.cores.ids.contains(&bind_to) {
                for overcommit_id in 0..self.overcommit {
                    index += 1;
                    self.shmem_provider.pre_fork()?;
                    match unsafe { fork() }? {
                        ForkResult::Parent(child) => {
                            self.shmem_provider.post_fork(false)?;
                            handles.push(child.pid);
                            log::info!(
                                "child with client id {index} spawned and bound to core {bind_to:?}"
                            );
                        }
                        ForkResult::Child => {
                            log::info!("{:?} PostFork", unsafe { libc::getpid() });
                            self.shmem_provider.post_fork(true)?;

                            std::thread::sleep(Duration::from_millis(
                                index as u64 * self.launch_delay,
                            ));

                            if !debug_output {
                                if let Some(file) = &self.opened_stdout_file {
                                    // # Safety
                                    // We assume the file descriptors are valid here
                                    unsafe {
                                        dup2(file.as_raw_fd(), libc::STDOUT_FILENO)?;
                                        match &self.opened_stderr_file {
                                            Some(stderr) => {
                                                dup2(stderr.as_raw_fd(), libc::STDERR_FILENO)?;
                                            }
                                            _ => {
                                                dup2(file.as_raw_fd(), libc::STDERR_FILENO)?;
                                            }
                                        }
                                    }
                                }
                            }

                            let client_description =
                                ClientDescription::new(index, overcommit_id, bind_to);

                            if index == 1 {
                                // Main client
                                log::debug!("Running main client on PID {}", std::process::id());
                                let (state, mgr) = main_inner_mgr_builder.take().unwrap()(
                                    self,
                                    client_description.clone(),
                                )?;

                                let mut centralized_event_manager_builder =
                                    CentralizedEventManager::builder();
                                centralized_event_manager_builder =
                                    centralized_event_manager_builder.is_main(true);

                                let c_mgr = centralized_event_manager_builder.build_on_port(
                                    mgr,
                                    // tuple_list!(multi_machine_event_manager_hook.take().unwrap()),
                                    self.shmem_provider.clone(),
                                    self.centralized_broker_port,
                                )?;

                                self.main_run_client.take().unwrap()(
                                    state,
                                    c_mgr,
                                    client_description,
                                )?;
                                Err(Error::shutting_down())
                            } else {
                                // Secondary clients
                                log::debug!(
                                    "Running secondary client on PID {}",
                                    std::process::id()
                                );
                                let (state, mgr) = secondary_inner_mgr_builder.take().unwrap()(
                                    self,
                                    client_description.clone(),
                                )?;

                                let centralized_builder = CentralizedEventManager::builder();

                                let c_mgr = centralized_builder.build_on_port(
                                    mgr,
                                    self.shmem_provider.clone(),
                                    self.centralized_broker_port,
                                )?;

                                self.secondary_run_client.take().unwrap()(
                                    state,
                                    c_mgr,
                                    client_description,
                                )?;
                                Err(Error::shutting_down())
                            }
                        }?,
                    }
                }
            }
        }

        // Create this after forks, to avoid problems with tokio runtime

        // # Safety
        // The `multi_machine_receiver_hook` needs messages to outlive the receiver.
        // The underlying memory region for incoming messages lives longer than the async thread processing them.
        #[cfg(feature = "multi_machine")]
        let TcpMultiMachineHooks {
            sender: multi_machine_sender_hook,
            receiver: multi_machine_receiver_hook,
        } = unsafe {
            TcpMultiMachineHooks::builder()
                .node_descriptor(self.multi_machine_node_descriptor.clone())
                .build::<I>()?
        };

        let mut brokers = Brokers::new();
        let exit_cleanly_after = NonZeroUsize::try_from(self.cores.ids.len()).unwrap();

        // Add centralized broker
        brokers.add(Box::new({
            #[cfg(feature = "multi_machine")]
            let centralized_hooks = tuple_list!(
                CentralizedLlmpHook::<I>::new()?,
                multi_machine_receiver_hook,
            );

            #[cfg(not(feature = "multi_machine"))]
            let centralized_hooks = tuple_list!(CentralizedLlmpHook::<I>::new()?);

            // TODO switch to false after solving the bug
            let mut broker = LlmpBroker::with_keep_pages_attach_to_tcp(
                self.shmem_provider.clone(),
                centralized_hooks,
                self.centralized_broker_port,
                true,
            )?;
            broker.set_exit_after(exit_cleanly_after);
            broker
        }));

        #[cfg(feature = "multi_machine")]
        assert!(
            self.spawn_broker,
            "Multi machine is not compatible with externally spawned brokers for now."
        );

        // If we should add another broker, add it to other brokers.
        if self.spawn_broker {
            log::info!("I am broker!!.");

            #[cfg(not(feature = "multi_machine"))]
            let llmp_hook = tuple_list!(StdLlmpEventHook::<I, MT>::new(self.monitor.clone())?);

            #[cfg(feature = "multi_machine")]
            let llmp_hook = tuple_list!(
                StdLlmpEventHook::<I, MT>::new(self.monitor.clone())?,
                multi_machine_sender_hook,
            );

            let mut broker = LlmpBroker::create_attach_to_tcp(
                self.shmem_provider.clone(),
                llmp_hook,
                self.broker_port,
            )?;

            if let Some(remote_broker_addr) = self.remote_broker_addr {
                log::info!("B2b: Connecting to {:?}", &remote_broker_addr);
                broker.inner_mut().connect_b2b(remote_broker_addr)?;
            }

            broker.set_exit_after(exit_cleanly_after);

            brokers.add(Box::new(broker));
        }
        log::debug!("Broker has been initialized; pid {}.", std::process::id());

        // Loop over all the brokers that should be polled
        brokers.loop_with_timeouts(Duration::from_secs(30), Some(Duration::from_millis(5)));

        #[cfg(feature = "llmp_debug")]
        log::info!("The last client quit. Exiting.");

        // Brokers exited. kill all clients.
        for handle in &handles {
            unsafe {
                libc::kill(*handle, libc::SIGINT);
            }
        }

        Err(Error::shutting_down())
    }
}
