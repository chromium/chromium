/// In-Process crash handling for `Windows`
pub mod windows_asan_handler {
    use alloc::string::String;
    use core::sync::atomic::{Ordering, compiler_fence};

    use libafl_bolts::os::SIGNAL_RECURSION_EXIT;
    use windows::Win32::System::Threading::{
        CRITICAL_SECTION, EnterCriticalSection, ExitProcess, LeaveCriticalSection,
    };

    use crate::{
        events::{EventFirer, EventRestarter},
        executors::{
            Executor, ExitKind, HasObservers, hooks::inprocess::GLOBAL_STATE,
            inprocess::run_observers_and_save_state,
        },
        feedbacks::Feedback,
        fuzzer::HasObjective,
        inputs::Input,
        observers::ObserversTuple,
        state::{HasCurrentTestcase, HasExecutions, HasSolutions},
    };

    /// # Safety
    /// ASAN deatch handler
    pub unsafe extern "C" fn asan_death_handler<E, EM, I, OF, S, Z>()
    where
        E: Executor<EM, I, S, Z> + HasObservers,
        E::Observers: ObserversTuple<I, S>,
        EM: EventFirer<I, S> + EventRestarter<S>,
        I: Input + Clone,
        OF: Feedback<EM, I, E::Observers, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        Z: HasObjective<Objective = OF>,
    {
        unsafe {
            let data = &raw mut GLOBAL_STATE;
            let (max_depth_reached, _signal_depth) = (*data).signal_handler_enter();

            if max_depth_reached {
                log::error!(
                    "We crashed inside a asan death handler, but this should never happen!"
                );
                ExitProcess(SIGNAL_RECURSION_EXIT as u32);
            }

            // Have we set a timer_before?
            if (*data).ptp_timer.is_some() {
                /*
                    We want to prevent the timeout handler being run while the main thread is executing the crash handler
                    Timeout handler runs if it has access to the critical section or data.in_target == 0
                    Writing 0 to the data.in_target makes the timeout handler makes the timeout handler invalid.
                */
                compiler_fence(Ordering::SeqCst);
                EnterCriticalSection((*data).critical as *mut CRITICAL_SECTION);
                compiler_fence(Ordering::SeqCst);
                (*data).in_target = 0;
                compiler_fence(Ordering::SeqCst);
                LeaveCriticalSection((*data).critical as *mut CRITICAL_SECTION);
                compiler_fence(Ordering::SeqCst);
            }

            log::error!("ASAN detected crash!");
            if (*data).current_input_ptr.is_null() {
                {
                    log::error!("Double crash\n");
                    log::error!(
                        "ASAN detected crash but we're not in the target... Bug in the fuzzer? Exiting.",
                    );
                }
                #[cfg(feature = "std")]
                {
                    log::error!("Type QUIT to restart the child");
                    let mut line = String::new();
                    while line.trim() != "QUIT" {
                        let _ = std::io::stdin().read_line(&mut line);
                    }
                }

                // TODO tell the parent to not restart
            } else {
                let executor = (*data).executor_mut::<E>();
                // reset timer
                if (*data).ptp_timer.is_some() {
                    (*data).ptp_timer = None;
                }

                let state = (*data).state_mut::<S>();
                let fuzzer = (*data).fuzzer_mut::<Z>();
                let event_mgr = (*data).event_mgr_mut::<EM>();

                log::error!("Child crashed!");

                // Make sure we don't crash in the crash handler forever.
                let input = (*data).take_current_input::<I>();

                run_observers_and_save_state::<E, EM, I, OF, S, Z>(
                    executor,
                    state,
                    input,
                    fuzzer,
                    event_mgr,
                    ExitKind::Crash,
                );
            }
            // Don't need to exit, Asan will exit for us
            // ExitProcess(1);
        }
    }
}

/// The module to take care of windows crash or timeouts
pub mod windows_exception_handler {
    #[cfg(feature = "std")]
    use alloc::boxed::Box;
    use alloc::{string::String, vec::Vec};
    use core::{
        ffi::c_void,
        mem::transmute,
        ptr,
        sync::atomic::{Ordering, compiler_fence},
    };
    #[cfg(feature = "std")]
    use std::io::Write;
    #[cfg(feature = "std")]
    use std::panic;

    use libafl_bolts::os::{
        SIGNAL_RECURSION_EXIT,
        windows_exceptions::{
            CRASH_EXCEPTIONS, EXCEPTION_HANDLERS_SIZE, EXCEPTION_POINTERS, ExceptionCode,
            ExceptionHandler,
        },
    };
    use windows::Win32::System::Threading::{
        CRITICAL_SECTION, EnterCriticalSection, ExitProcess, LeaveCriticalSection,
    };

    use crate::{
        events::{EventFirer, EventRestarter},
        executors::{
            Executor, ExitKind, HasObservers,
            hooks::inprocess::{GLOBAL_STATE, HasTimeout, InProcessExecutorHandlerData},
            inprocess::{HasInProcessHooks, run_observers_and_save_state},
        },
        feedbacks::Feedback,
        fuzzer::HasObjective,
        inputs::Input,
        observers::ObserversTuple,
        state::{HasCurrentTestcase, HasExecutions, HasSolutions},
    };

    pub(crate) type HandlerFuncPtr =
        unsafe fn(*mut EXCEPTION_POINTERS, *mut InProcessExecutorHandlerData);

    /*pub unsafe fn nop_handler(
        _code: ExceptionCode,
        _exception_pointers: *mut EXCEPTION_POINTERS,
        _data: &mut InProcessExecutorHandlerData,
    ) {
    }*/

    impl ExceptionHandler for InProcessExecutorHandlerData {
        /// # Safety
        /// Will dereference `EXCEPTION_POINTERS` and access `GLOBAL_STATE`.
        unsafe fn handle(
            &mut self,
            _code: ExceptionCode,
            exception_pointers: *mut EXCEPTION_POINTERS,
        ) {
            unsafe {
                let data = &raw mut GLOBAL_STATE;
                let (max_depth_reached, _signal_depth) = (*data).signal_handler_enter();

                if max_depth_reached {
                    log::error!("We crashed inside a crash handler, but this should never happen!");
                    ExitProcess(SIGNAL_RECURSION_EXIT as u32);
                }

                if !(*data).crash_handler.is_null() {
                    let func: HandlerFuncPtr = transmute((*data).crash_handler);
                    (func)(exception_pointers, data);
                }
                (*data).signal_handler_exit();
            }
        }

        fn exceptions(&self) -> Vec<ExceptionCode> {
            let crash_list = CRASH_EXCEPTIONS.to_vec();
            assert!(crash_list.len() < EXCEPTION_HANDLERS_SIZE - 1);
            crash_list
        }
    }

    /// invokes the `post_exec` hook on all observer in case of panic
    ///
    /// # Safety
    /// Well, exception handling is not safe
    #[cfg(feature = "std")]
    pub fn setup_panic_hook<E, EM, I, OF, S, Z>()
    where
        E: Executor<EM, I, S, Z> + HasObservers,
        E::Observers: ObserversTuple<I, S>,
        EM: EventFirer<I, S> + EventRestarter<S>,
        I: Input + Clone,
        OF: Feedback<EM, I, E::Observers, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        Z: HasObjective<Objective = OF>,
    {
        let old_hook = panic::take_hook();
        panic::set_hook(Box::new(move |panic_info| unsafe {
            let data = &raw mut GLOBAL_STATE;
            let (max_depth_reached, _signal_depth) = (*data).signal_handler_enter();

            if max_depth_reached {
                log::error!("We crashed inside a crash handler, but this should never happen!");
                ExitProcess(SIGNAL_RECURSION_EXIT as u32);
            }

            // Have we set a timer_before?
            if (*data).ptp_timer.is_some() {
                /*
                    We want to prevent the timeout handler being run while the main thread is executing the crash handler
                    Timeout handler runs if it has access to the critical section or data.in_target == 0
                    Writing 0 to the data.in_target makes the timeout handler makes the timeout handler invalid.
                */
                compiler_fence(Ordering::SeqCst);
                EnterCriticalSection((*data).critical as *mut CRITICAL_SECTION);
                compiler_fence(Ordering::SeqCst);
                (*data).in_target = 0;
                compiler_fence(Ordering::SeqCst);
                LeaveCriticalSection((*data).critical as *mut CRITICAL_SECTION);
                compiler_fence(Ordering::SeqCst);
            }

            if (*data).is_valid() {
                // We are fuzzing!
                let executor = (*data).executor_mut::<E>();
                let state = (*data).state_mut::<S>();
                let fuzzer = (*data).fuzzer_mut::<Z>();
                let event_mgr = (*data).event_mgr_mut::<EM>();

                let input = (*data).take_current_input::<I>();

                run_observers_and_save_state::<E, EM, I, OF, S, Z>(
                    executor,
                    state,
                    input,
                    fuzzer,
                    event_mgr,
                    ExitKind::Crash,
                );

                ExitProcess(1);
            }
            old_hook(panic_info);
            (*data).signal_handler_exit();
        }));
    }

    /// Timeout handler for windows
    ///
    /// # Safety
    /// Well, exception handling is not safe
    pub unsafe extern "system" fn inproc_timeout_handler<E, EM, I, OF, S, Z>(
        _p0: *mut u8,
        global_state: *mut c_void,
        _p1: *mut u8,
    ) where
        E: Executor<EM, I, S, Z> + HasInProcessHooks<I, S> + HasObservers,
        E::Observers: ObserversTuple<I, S>,
        EM: EventFirer<I, S> + EventRestarter<S>,
        I: Input + Clone,
        OF: Feedback<EM, I, E::Observers, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        Z: HasObjective<Objective = OF>,
    {
        let data: &mut InProcessExecutorHandlerData =
            unsafe { &mut *(global_state as *mut InProcessExecutorHandlerData) };
        compiler_fence(Ordering::SeqCst);
        unsafe {
            EnterCriticalSection((data.critical as *mut CRITICAL_SECTION).as_mut().unwrap());
        }
        compiler_fence(Ordering::SeqCst);

        if !data.executor_ptr.is_null()
            && unsafe {
                data.executor_mut::<E>()
                    .inprocess_hooks_mut()
                    .handle_timeout()
            }
        {
            compiler_fence(Ordering::SeqCst);
            unsafe {
                LeaveCriticalSection((data.critical as *mut CRITICAL_SECTION).as_mut().unwrap());
            }
            compiler_fence(Ordering::SeqCst);

            return;
        }

        if data.in_target == 1 {
            let executor = unsafe { data.executor_mut::<E>() };
            let state = unsafe { data.state_mut::<S>() };
            let fuzzer = unsafe { data.fuzzer_mut::<Z>() };
            let event_mgr = unsafe { data.event_mgr_mut::<EM>() };

            if data.current_input_ptr.is_null() {
                log::error!("TIMEOUT or SIGUSR2 happened, but currently not fuzzing. Exiting");
            } else {
                log::error!("Timeout in fuzz run.");

                let input = unsafe { (data.current_input_ptr as *const I).as_ref().unwrap() };
                data.current_input_ptr = ptr::null_mut();

                run_observers_and_save_state::<E, EM, I, OF, S, Z>(
                    executor,
                    state,
                    input,
                    fuzzer,
                    event_mgr,
                    ExitKind::Timeout,
                );

                compiler_fence(Ordering::SeqCst);

                unsafe {
                    ExitProcess(1);
                }
            }
        }
        compiler_fence(Ordering::SeqCst);
        unsafe {
            LeaveCriticalSection((data.critical as *mut CRITICAL_SECTION).as_mut().unwrap());
        }
        compiler_fence(Ordering::SeqCst);
        // log::info!("TIMER INVOKED!");
    }

    /// Crash handler for windows
    ///
    /// # Safety
    /// Well, exception handling is not safe
    pub unsafe fn inproc_crash_handler<E, EM, I, OF, S, Z>(
        exception_pointers: *mut EXCEPTION_POINTERS,
        data: &mut InProcessExecutorHandlerData,
    ) where
        E: Executor<EM, I, S, Z> + HasObservers,
        E::Observers: ObserversTuple<I, S>,
        EM: EventFirer<I, S> + EventRestarter<S>,
        I: Input + Clone,
        OF: Feedback<EM, I, E::Observers, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        Z: HasObjective<Objective = OF>,
    {
        // Have we set a timer_before?
        if data.ptp_timer.is_some() {
            /*
                We want to prevent the timeout handler being run while the main thread is executing the crash handler
                Timeout handler runs if it has access to the critical section or data.in_target == 0
                Writing 0 to the data.in_target makes the timeout handler makes the timeout handler invalid.
            */
            compiler_fence(Ordering::SeqCst);
            unsafe {
                EnterCriticalSection(data.critical as *mut CRITICAL_SECTION);
            }
            compiler_fence(Ordering::SeqCst);
            data.in_target = 0;
            compiler_fence(Ordering::SeqCst);
            unsafe {
                LeaveCriticalSection(data.critical as *mut CRITICAL_SECTION);
            }
            compiler_fence(Ordering::SeqCst);
        }

        // Is this really crash?
        let mut is_crash = true;
        #[cfg(feature = "std")]
        if let Some(exception_pointers) = unsafe { exception_pointers.as_mut() } {
            let code: ExceptionCode = ExceptionCode::from(unsafe {
                exception_pointers
                    .ExceptionRecord
                    .as_mut()
                    .unwrap()
                    .ExceptionCode
                    .0
            });

            let exception_list = data.exceptions();
            if exception_list.contains(&code) {
                log::error!(
                    "Crashed with {code} at {:?} in thread {:?}",
                    unsafe {
                        exception_pointers
                            .ExceptionRecord
                            .as_mut()
                            .unwrap()
                            .ExceptionAddress
                    },
                    unsafe { winapi::um::processthreadsapi::GetCurrentThreadId() }
                );
            } else {
                // log::trace!("Exception code received, but {code} is not in CRASH_EXCEPTIONS");
                is_crash = false;
            }
        } else {
            log::error!("Crashed without exception (probably due to SIGABRT)");
        }

        if data.current_input_ptr.is_null() {
            {
                log::error!("Double crash\n");
                let crash_addr = unsafe {
                    exception_pointers
                        .as_mut()
                        .unwrap()
                        .ExceptionRecord
                        .as_mut()
                        .unwrap()
                        .ExceptionAddress as usize
                };

                log::error!(
                    "We crashed at addr 0x{crash_addr:x}, but are not in the target... Bug in the fuzzer? Exiting."
                );
            }
            #[cfg(feature = "std")]
            {
                log::error!("Type QUIT to restart the child");
                let mut line = String::new();
                while line.trim() != "QUIT" {
                    let _ = std::io::stdin().read_line(&mut line);
                }
            }

            // TODO tell the parent to not restart
        } else {
            let executor = unsafe { data.executor_mut::<E>() };
            // reset timer
            if data.ptp_timer.is_some() {
                data.ptp_timer = None;
            }

            let state = unsafe { data.state_mut::<S>() };
            let fuzzer = unsafe { data.fuzzer_mut::<Z>() };
            let event_mgr = unsafe { data.event_mgr_mut::<EM>() };

            if is_crash {
                log::error!("Child crashed!");
            } else {
                // log::info!("Exception received!");
            }

            // Make sure we don't crash in the crash handler forever.
            if is_crash {
                log::warn!("Running observers and exiting!");
                // // I want to disable the hooks before doing anything, especially before taking a stack dump
                let input = unsafe { data.take_current_input::<I>() };
                run_observers_and_save_state::<E, EM, I, OF, S, Z>(
                    executor,
                    state,
                    input,
                    fuzzer,
                    event_mgr,
                    ExitKind::Crash,
                );
                {
                    let mut bsod = Vec::new();
                    {
                        let mut writer = std::io::BufWriter::new(&mut bsod);
                        writeln!(writer, "input: {:?}", input.generate_name(None)).unwrap();
                        libafl_bolts::minibsod::generate_minibsod(&mut writer, exception_pointers)
                            .unwrap();
                        writer.flush().unwrap();
                    }
                    log::error!("{}", core::str::from_utf8(&bsod).unwrap());
                }
            } else {
                // This is not worth saving
            }
        }

        if is_crash {
            log::info!("Exiting!");
            unsafe {
                ExitProcess(1);
            }
        }
        // log::info!("Not Exiting!");
    }
}
