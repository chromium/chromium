/// The inprocess executor singal handling code for unix
#[cfg(unix)]
pub mod unix_signal_handler {
    use alloc::{boxed::Box, string::String, vec::Vec};
    use core::mem::transmute;
    use std::{io::Write, panic};

    use libafl_bolts::os::{
        SIGNAL_RECURSION_EXIT,
        unix_signals::{Signal, SignalHandler, ucontext_t},
    };
    use libc::siginfo_t;

    use crate::{
        events::{EventFirer, EventRestarter},
        executors::{
            Executor, ExitKind, HasObservers, common_signals,
            hooks::inprocess::{GLOBAL_STATE, HasTimeout, InProcessExecutorHandlerData},
            inprocess::{HasInProcessHooks, run_observers_and_save_state},
        },
        feedbacks::Feedback,
        fuzzer::HasObjective,
        inputs::Input,
        observers::ObserversTuple,
        state::{HasCurrentTestcase, HasExecutions, HasSolutions},
    };

    pub(crate) type HandlerFuncPtr = unsafe fn(
        Signal,
        &mut siginfo_t,
        Option<&mut ucontext_t>,
        data: *mut InProcessExecutorHandlerData,
    );

    // A handler that does nothing.
    /*pub fn nop_handler(
        _signal: Signal,
        _info: &mut siginfo_t,
        _context: Option<&mut ucontext_t>,
        _data: &mut InProcessExecutorHandlerData,
    ) {
    }*/

    #[cfg(unix)]
    impl SignalHandler for InProcessExecutorHandlerData {
        /// # Safety
        /// This will access global state.
        unsafe fn handle(
            &mut self,
            signal: Signal,
            info: &mut siginfo_t,
            context: Option<&mut ucontext_t>,
        ) {
            // # Safety
            // This runs in a signal handler, no other threads access these variables/borrows anymore.
            unsafe {
                let data = &raw mut GLOBAL_STATE;
                let (max_depth_reached, signal_depth) = (*data).signal_handler_enter();

                if max_depth_reached {
                    log::error!(
                        "The in process signal handler has been triggered {signal_depth} times recursively, which is not expected. Exiting with error code {SIGNAL_RECURSION_EXIT}..."
                    );
                    libc::exit(SIGNAL_RECURSION_EXIT);
                }

                match signal {
                    Signal::SigUser2 | Signal::SigAlarm => {
                        if !(*data).timeout_handler.is_null() {
                            let func: HandlerFuncPtr = transmute((*data).timeout_handler);
                            (func)(signal, info, context, data);
                        }
                    }
                    _ => {
                        if !(*data).crash_handler.is_null() {
                            let func: HandlerFuncPtr = transmute((*data).crash_handler);
                            func(signal, info, context, data);
                        }
                    }
                }
                (*data).signal_handler_exit();
            }
        }

        fn signals(&self) -> Vec<Signal> {
            common_signals()
        }
    }

    /// invokes the `post_exec` hook on all observer in case of panic
    pub fn setup_panic_hook<E, EM, I, OF, S, Z>()
    where
        E: Executor<EM, I, S, Z> + HasObservers,
        E::Observers: ObserversTuple<I, S>,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, E::Observers, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        Z: HasObjective<Objective = OF>,
        I: Input + Clone,
    {
        let old_hook = panic::take_hook();
        // # Safety
        // The panic handler should only run when all other execution stopped.
        // At this point, accessing the global state should be sound.
        panic::set_hook(Box::new(move |panic_info| unsafe {
            old_hook(panic_info);
            let data = &raw mut GLOBAL_STATE;
            let (max_depth_reached, signal_depth) = (*data).signal_handler_enter();

            if max_depth_reached {
                log::error!(
                    "The in process signal handler has been triggered {signal_depth} times recursively, which is not expected. Exiting with error code {SIGNAL_RECURSION_EXIT}..."
                );
                libc::exit(SIGNAL_RECURSION_EXIT);
            }

            if (*data).is_valid() {
                // We are fuzzing!
                let executor = (*data).executor_mut::<E>();
                let state = (*data).state_mut::<S>();
                let input = (*data).take_current_input::<I>();
                let fuzzer = (*data).fuzzer_mut::<Z>();
                let event_mgr = (*data).event_mgr_mut::<EM>();

                run_observers_and_save_state::<E, EM, I, OF, S, Z>(
                    executor,
                    state,
                    input,
                    fuzzer,
                    event_mgr,
                    ExitKind::Crash,
                );

                libc::_exit(128 + 6); // SIGABRT exit code
            }

            (*data).signal_handler_exit();
        }));
    }

    /// Timeout-Handler for in-process fuzzing.
    /// It will store the current State to shmem, then exit.
    ///
    /// # Safety
    /// Well, signal handling is not safe
    #[cfg(unix)]
    #[allow(clippy::needless_pass_by_value)] // nightly no longer requires this
    pub unsafe fn inproc_timeout_handler<E, EM, I, OF, S, Z>(
        _signal: Signal,
        _info: &mut siginfo_t,
        _context: Option<&mut ucontext_t>,
        data: &mut InProcessExecutorHandlerData,
    ) where
        E: HasInProcessHooks<I, S> + HasObservers,
        E::Observers: ObserversTuple<I, S>,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, E::Observers, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        Z: HasObjective<Objective = OF>,
        I: Input + Clone,
    {
        unsafe {
            // this stuff is for batch timeout
            if !data.executor_ptr.is_null()
                && data
                    .executor_mut::<E>()
                    .inprocess_hooks_mut()
                    .handle_timeout(data)
            {
                return;
            }

            if !data.is_valid() {
                log::warn!("TIMEOUT or SIGUSR2 happened, but currently not fuzzing.");
                return;
            }

            let executor = data.executor_mut::<E>();
            let state = data.state_mut::<S>();
            let event_mgr = data.event_mgr_mut::<EM>();
            let fuzzer = data.fuzzer_mut::<Z>();
            let input = data.take_current_input::<I>();

            log::error!("Timeout in fuzz run.");

            run_observers_and_save_state::<E, EM, I, OF, S, Z>(
                executor,
                state,
                input,
                fuzzer,
                event_mgr,
                ExitKind::Timeout,
            );
            log::info!("Exiting");
            libc::_exit(55);
        }
    }

    /// Crash-Handler for in-process fuzzing.
    /// Will be used for signal handling.
    /// It will store the current State to shmem, then exit.
    ///
    /// # Safety
    /// Well, signal handling is not safe
    #[allow(clippy::needless_pass_by_value)] // nightly no longer requires this
    pub unsafe fn inproc_crash_handler<E, EM, I, OF, S, Z>(
        signal: Signal,
        _info: &mut siginfo_t,
        _context: Option<&mut ucontext_t>,
        data: &mut InProcessExecutorHandlerData,
    ) where
        E: Executor<EM, I, S, Z> + HasObservers,
        E::Observers: ObserversTuple<I, S>,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, E::Observers, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        Z: HasObjective<Objective = OF>,
        I: Input + Clone,
    {
        unsafe {
            #[cfg(all(target_os = "android", target_arch = "aarch64"))]
            let _context = _context.map(|p| {
                &mut *(((core::ptr::from_mut(p) as *mut libc::c_void as usize) + 128)
                    as *mut libc::c_void as *mut ucontext_t)
            });

            log::error!("Crashed with {signal}");
            if data.is_valid() {
                let executor = data.executor_mut::<E>();
                // disarms timeout in case of timeout
                let state = data.state_mut::<S>();
                let event_mgr = data.event_mgr_mut::<EM>();
                let fuzzer = data.fuzzer_mut::<Z>();
                let input = data.take_current_input::<I>();

                log::error!("Child crashed!");

                {
                    let mut bsod = Vec::new();
                    {
                        let mut writer = std::io::BufWriter::new(&mut bsod);
                        let _ = writeln!(writer, "input: {:?}", input.generate_name(None));
                        let bsod = libafl_bolts::minibsod::generate_minibsod(
                            &mut writer,
                            signal,
                            _info,
                            _context.as_deref(),
                        );
                        if bsod.is_err() {
                            log::error!("generate_minibsod failed");
                        }
                        let _ = writer.flush();
                    }
                    if let Ok(r) = core::str::from_utf8(&bsod) {
                        log::error!("{r}");
                    }
                }

                run_observers_and_save_state::<E, EM, I, OF, S, Z>(
                    executor,
                    state,
                    input,
                    fuzzer,
                    event_mgr,
                    ExitKind::Crash,
                );
            } else {
                {
                    log::error!("Double crash\n");
                    #[cfg(target_os = "android")]
                    let si_addr = (_info._pad[0] as i64) | ((_info._pad[1] as i64) << 32);
                    #[cfg(not(target_os = "android"))]
                    let si_addr = { _info.si_addr() as usize };

                    log::error!(
                        "We crashed at addr 0x{si_addr:x}, but are not in the target... Bug in the fuzzer? Exiting."
                    );

                    {
                        let mut bsod = Vec::new();
                        {
                            let mut writer = std::io::BufWriter::new(&mut bsod);
                            let bsod = libafl_bolts::minibsod::generate_minibsod(
                                &mut writer,
                                signal,
                                _info,
                                _context.as_deref(),
                            );
                            if bsod.is_err() {
                                log::error!("generate_minibsod failed");
                            }
                            let _ = writer.flush();
                        }
                        if let Ok(r) = core::str::from_utf8(&bsod) {
                            log::error!("{r}");
                        }
                    }
                }

                {
                    log::error!("Type QUIT to restart the child");
                    let mut line = String::new();
                    while line.trim() != "QUIT" {
                        let _ = std::io::stdin().read_line(&mut line);
                    }
                }

                // TODO tell the parent to not restart
            }

            libc::_exit(128 + (signal as i32));
        }
    }
}
