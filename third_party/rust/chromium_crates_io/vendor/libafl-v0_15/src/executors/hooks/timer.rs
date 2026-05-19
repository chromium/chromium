//! The struct `TimerStruct` will absorb all the difference in timeout implementation in various system.
use core::time::Duration;
#[cfg(target_os = "linux")]
use core::{mem::zeroed, ptr::null_mut};

#[cfg(all(unix, not(target_os = "linux")))]
pub(crate) const ITIMER_REAL: core::ffi::c_int = 0;

#[cfg(windows)]
use core::{
    ffi::c_void,
    ptr::write_volatile,
    sync::atomic::{Ordering, compiler_fence},
};

#[cfg(target_os = "linux")]
use libafl_bolts::current_time;
#[cfg(windows)]
use windows::Win32::{
    Foundation::FILETIME,
    System::Threading::{
        CRITICAL_SECTION, CreateThreadpoolTimer, EnterCriticalSection, InitializeCriticalSection,
        LeaveCriticalSection, PTP_CALLBACK_INSTANCE, PTP_TIMER, SetThreadpoolTimer,
        TP_CALLBACK_ENVIRON_V3,
    },
};

#[cfg(windows)]
use crate::executors::hooks::inprocess::GLOBAL_STATE;

#[repr(C)]
#[cfg(all(unix, not(target_os = "linux")))]
#[derive(Copy, Clone)]
pub(crate) struct Timeval {
    pub tv_sec: i64,
    pub tv_usec: i64,
}

#[cfg(all(unix, not(target_os = "linux")))]
impl core::fmt::Debug for Timeval {
    #[expect(clippy::cast_sign_loss)]
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "Timeval {{ tv_sec: {:?}, tv_usec: {:?} (tv: {:?}) }}",
            self.tv_sec,
            self.tv_usec,
            Duration::new(self.tv_sec as _, (self.tv_usec * 1000) as _)
        )
    }
}

#[repr(C)]
#[cfg(all(unix, not(target_os = "linux")))]
#[derive(Debug, Copy, Clone)]
pub(crate) struct Itimerval {
    pub it_interval: Timeval,
    pub it_value: Timeval,
}

#[cfg(all(unix, not(target_os = "linux")))]
unsafe extern "C" {
    pub(crate) fn setitimer(
        which: libc::c_int,
        new_value: *mut Itimerval,
        old_value: *mut Itimerval,
    ) -> libc::c_int;
}

/// The strcut about all the internals of the timer.
/// This struct absorb all platform specific differences about timer.
#[expect(missing_debug_implementations)]
pub struct TimerStruct {
    // timeout time (windows)
    #[cfg(windows)]
    milli_sec: i64,
    #[cfg(windows)]
    ptp_timer: PTP_TIMER,
    #[cfg(windows)]
    critical: CRITICAL_SECTION,
    #[cfg(target_os = "linux")]
    pub(crate) batch_mode: bool,
    #[cfg(target_os = "linux")]
    pub(crate) exec_tmout: Duration,
    #[cfg(all(unix, not(target_os = "linux")))]
    itimerval: Itimerval,
    #[cfg(target_os = "linux")]
    pub(crate) timerid: libc::timer_t,
    #[cfg(target_os = "linux")]
    pub(crate) itimerspec: libc::itimerspec,
    #[cfg(target_os = "linux")]
    pub(crate) executions: u32,
    #[cfg(target_os = "linux")]
    pub(crate) avg_mul_k: u32,
    #[cfg(target_os = "linux")]
    pub(crate) last_signal_time: Duration,
    #[cfg(target_os = "linux")]
    pub(crate) avg_exec_time: Duration,
    #[cfg(target_os = "linux")]
    pub(crate) start_time: Duration,
    #[cfg(target_os = "linux")]
    pub(crate) tmout_start_time: Duration,
}

#[cfg(windows)]
#[expect(non_camel_case_types)]
type PTP_TIMER_CALLBACK = unsafe extern "system" fn(
    param0: PTP_CALLBACK_INSTANCE,
    param1: *mut c_void,
    param2: PTP_TIMER,
);

impl TimerStruct {
    /// Timeout value in milli seconds
    #[cfg(windows)]
    #[must_use]
    pub fn milli_sec(&self) -> i64 {
        self.milli_sec
    }

    #[cfg(windows)]
    /// Timeout value in milli seconds (mut ref)
    pub fn milli_sec_mut(&mut self) -> &mut i64 {
        &mut self.milli_sec
    }

    /// The timer object for windows
    #[cfg(windows)]
    #[must_use]
    pub fn ptp_timer(&self) -> &PTP_TIMER {
        &self.ptp_timer
    }

    #[cfg(windows)]
    /// The timer object for windows
    pub fn ptp_timer_mut(&mut self) -> &mut PTP_TIMER {
        &mut self.ptp_timer
    }

    /// The critical section, we need to use critical section to access the globals
    #[cfg(windows)]
    #[must_use]
    pub fn critical(&self) -> &CRITICAL_SECTION {
        &self.critical
    }

    #[cfg(windows)]
    /// The critical section (mut ref), we need to use critical section to access the globals
    pub fn critical_mut(&mut self) -> &mut CRITICAL_SECTION {
        &mut self.critical
    }

    /// Create a `TimerStruct` with the specified timeout
    #[cfg(all(unix, not(target_os = "linux")))]
    #[must_use]
    pub fn new(exec_tmout: Duration) -> Self {
        let milli_sec = exec_tmout.as_millis();
        let it_value = Timeval {
            tv_sec: (milli_sec / 1000) as i64,
            tv_usec: (milli_sec % 1000) as i64,
        };
        let it_interval = Timeval {
            tv_sec: 0,
            tv_usec: 0,
        };
        let itimerval = Itimerval {
            it_interval,
            it_value,
        };
        Self { itimerval }
    }

    /// Constructor
    /// # Safety
    /// This function calls transmute to setup the timeout handler for windows
    #[cfg(windows)]
    #[must_use]
    pub unsafe fn new(exec_tmout: Duration, timeout_handler: *const c_void) -> Self {
        let milli_sec = exec_tmout.as_millis() as i64;

        let timeout_handler: PTP_TIMER_CALLBACK = unsafe { core::mem::transmute(timeout_handler) };
        let ptp_timer = unsafe {
            CreateThreadpoolTimer(
                Some(timeout_handler),
                Some(&raw mut GLOBAL_STATE as *mut c_void),
                Some(&TP_CALLBACK_ENVIRON_V3::default()),
            )
        }
        .expect("CreateThreadpoolTimer failed!");

        let mut critical = CRITICAL_SECTION::default();
        unsafe {
            InitializeCriticalSection(&raw mut critical);
        }
        Self {
            milli_sec,
            ptp_timer,
            critical,
        }
    }

    #[cfg(target_os = "linux")]
    #[must_use]
    /// Create a `TimerStruct` with the specified timeout
    pub fn new(exec_tmout: Duration) -> Self {
        let milli_sec = exec_tmout.as_millis();
        let it_value = libc::timespec {
            tv_sec: (milli_sec / 1000) as _,
            tv_nsec: ((milli_sec % 1000) * 1000 * 1000) as _,
        };
        let it_interval = libc::timespec {
            tv_sec: 0,
            tv_nsec: 0,
        };
        let itimerspec = libc::itimerspec {
            it_interval,
            it_value,
        };
        #[allow(unused_mut)] // miri doesn't mutate this
        let mut timerid: libc::timer_t = null_mut();
        #[cfg(not(miri))]
        unsafe {
            // creates a new per-process interval timer
            libc::timer_create(libc::CLOCK_MONOTONIC, null_mut(), &raw mut timerid);
        }

        Self {
            batch_mode: false,
            itimerspec,
            timerid,
            exec_tmout,
            executions: 0,
            avg_mul_k: 1,
            last_signal_time: Duration::ZERO,
            avg_exec_time: Duration::ZERO,
            start_time: Duration::ZERO,
            tmout_start_time: Duration::ZERO,
        }
    }

    #[cfg(target_os = "linux")]
    #[must_use]
    /// Constructor but use batch mode
    /// More efficient timeout mechanism with imprecise timing.
    ///
    /// The timeout will trigger after t seconds and at most within 2*t seconds.
    /// This means the actual timeout may occur anywhere in the range [t, 2*t],
    /// providing a flexible but bounded execution time limit.
    pub fn batch_mode(exec_tmout: Duration) -> Self {
        let mut me = Self::new(exec_tmout);
        me.batch_mode = true;
        me
    }

    #[cfg(all(unix, not(target_os = "linux")))]
    /// Set up timer
    pub fn set_timer(&mut self) {
        // # Safety
        // Safe because the variables are all alive at this time and don't contain pointers.
        unsafe {
            setitimer(ITIMER_REAL, &raw mut self.itimerval, core::ptr::null_mut());
        }
    }

    #[cfg(windows)]
    #[expect(clippy::cast_sign_loss)]
    /// Set timer
    pub fn set_timer(&mut self) {
        unsafe {
            let data = &raw mut GLOBAL_STATE;

            write_volatile(&raw mut (*data).ptp_timer, Some(*self.ptp_timer()));
            write_volatile(
                &raw mut (*data).critical,
                &raw mut (*self.critical_mut()) as *mut c_void,
            );
            let tm: i64 = -self.milli_sec() * 10 * 1000;
            let ft = FILETIME {
                dwLowDateTime: (tm & 0xffffffff) as u32,
                dwHighDateTime: (tm >> 32) as u32,
            };

            // enter critical section then set timer
            compiler_fence(Ordering::SeqCst);
            EnterCriticalSection(self.critical_mut());
            compiler_fence(Ordering::SeqCst);
            (*data).in_target = 1;
            compiler_fence(Ordering::SeqCst);
            LeaveCriticalSection(self.critical_mut());
            compiler_fence(Ordering::SeqCst);

            SetThreadpoolTimer(*self.ptp_timer(), Some(&raw const ft), 0, None);
        }
    }

    /// Set up timer
    #[cfg(target_os = "linux")]
    pub fn set_timer(&mut self) {
        unsafe {
            if self.batch_mode {
                if self.executions == 0 {
                    libc::timer_settime(self.timerid, 0, &raw mut self.itimerspec, null_mut());
                    self.tmout_start_time = current_time();
                }
                self.start_time = current_time();
            } else {
                #[cfg(not(miri))]
                libc::timer_settime(self.timerid, 0, &raw mut self.itimerspec, null_mut());
            }
        }
    }

    #[cfg(all(unix, not(target_os = "linux")))]
    /// Disable the timer
    pub fn unset_timer(&mut self) {
        // # Safety
        // No user-provided values.
        unsafe {
            let mut itimerval_zero: Itimerval = core::mem::zeroed();
            setitimer(ITIMER_REAL, &raw mut itimerval_zero, core::ptr::null_mut());
        }
    }

    /// Disable the timer
    #[cfg(target_os = "linux")]
    pub fn unset_timer(&mut self) {
        // # Safety
        // Just API calls, no user-provided inputs
        if self.batch_mode {
            unsafe {
                let elapsed = current_time().saturating_sub(self.tmout_start_time);
                // elapsed may be > than tmout in case of received but ingored signal
                if elapsed > self.exec_tmout
                    || self.exec_tmout.saturating_sub(elapsed) < self.avg_exec_time * self.avg_mul_k
                {
                    let disarmed: libc::itimerspec = zeroed();
                    libc::timer_settime(self.timerid, 0, &raw const disarmed, null_mut());
                    // set timer the next exec
                    if self.executions > 0 {
                        self.avg_exec_time = elapsed / self.executions;
                        self.executions = 0;
                    }
                    // readjust K
                    if elapsed > self.exec_tmout * self.avg_mul_k && self.avg_mul_k > 1 {
                        self.avg_mul_k -= 1;
                    }
                } else {
                    self.executions += 1;
                }
            }
        } else {
            #[cfg(not(miri))]
            unsafe {
                let disarmed: libc::itimerspec = zeroed();
                libc::timer_settime(self.timerid, 0, &raw const disarmed, null_mut());
            }
        }
    }

    #[cfg(windows)]
    /// Disable the timer
    pub fn unset_timer(&mut self) {
        // # Safety
        // The value accesses are guarded by a critical section.
        unsafe {
            let data = &raw mut GLOBAL_STATE;

            compiler_fence(Ordering::SeqCst);
            EnterCriticalSection(self.critical_mut());
            compiler_fence(Ordering::SeqCst);
            // Timeout handler will do nothing after we increment in_target value.
            (*data).in_target = 0;
            compiler_fence(Ordering::SeqCst);
            LeaveCriticalSection(self.critical_mut());
            compiler_fence(Ordering::SeqCst);

            // previously this wa post_run_reset
            SetThreadpoolTimer(*self.ptp_timer(), None, 0, None);
        }
    }
}
