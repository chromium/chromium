//! Signal handling for unix
#[cfg(feature = "std")]
use alloc::ffi::CString;
#[cfg(feature = "alloc")]
use alloc::vec::Vec;
#[cfg(all(target_vendor = "apple", target_arch = "aarch64"))]
use core::mem::size_of;
#[cfg(feature = "alloc")]
use core::{
    cell::UnsafeCell,
    ptr::{self, write_volatile},
    sync::atomic::{Ordering, compiler_fence},
};
use core::{
    fmt::{self, Display, Formatter},
    mem,
};

/// armv7 `libc` does not feature a `uncontext_t` implementation
#[cfg(target_arch = "arm")]
pub use libc::c_ulong;
#[cfg(feature = "std")]
use nix::errno::Errno;

/// The special exit code when the target exited through ctrl-c
pub const CTRL_C_EXIT: i32 = 100;

/// ARMv7-specific representation of a saved context
#[cfg(target_arch = "arm")]
#[derive(Debug)]
#[expect(non_camel_case_types)]
#[repr(C)]
pub struct mcontext_t {
    /// Signal Number
    pub trap_no: c_ulong,
    /// Error Code
    pub error_code: c_ulong,
    /// Old signal mask
    pub oldmask: c_ulong,
    /// GPR R0
    pub arm_r0: c_ulong,
    /// GPR R1
    pub arm_r1: c_ulong,
    /// GPR R2
    pub arm_r2: c_ulong,
    /// GPR R3
    pub arm_r3: c_ulong,
    /// GPR R4
    pub arm_r4: c_ulong,
    /// GPR R5
    pub arm_r5: c_ulong,
    /// GPR R6
    pub arm_r6: c_ulong,
    /// GPR R7
    pub arm_r7: c_ulong,
    /// GPR R8
    pub arm_r8: c_ulong,
    /// GPR R9
    pub arm_r9: c_ulong,
    /// GPR R10
    pub arm_r10: c_ulong,
    /// Frame Pointer
    pub arm_fp: c_ulong,
    /// Intra-Procedure Scratch Register
    pub arm_ip: c_ulong,
    /// Stack Pointer
    pub arm_sp: c_ulong,
    /// Link Register
    pub arm_lr: c_ulong,
    /// Program Counter
    pub arm_pc: c_ulong,
    /// Current Program Status Register
    pub arm_cpsr: c_ulong,
    /// Fault Address
    pub fault_address: c_ulong,
}

/// User Context Struct on `arm` `linux`
#[cfg(all(target_os = "linux", target_arch = "arm"))]
#[derive(Debug)]
#[expect(non_camel_case_types)]
#[repr(C)]
pub struct ucontext_t {
    /// Flags
    pub uc_flags: u32,
    /// Pointer to the context that will be resumed when this context returns
    pub uc_link: *mut ucontext_t,
    /// Stack used by this context
    pub uc_stack: stack_t,
    /// Machine-specific representation of the saved context
    pub uc_mcontext: mcontext_t,
    /// Set of signals that are blocked when this context is active
    pub uc_sigmask: libc::sigset_t,
}

/// # Internal representation
///
/// ```c
/// _STRUCT_ARM_EXCEPTION_STATE64
/// {
///    __uint64_t far;         /* Virtual Fault Address */
///    __uint32_t esr;         /* Exception syndrome */
///    __uint32_t exception;   /* number of arm exception taken */
/// };
/// ```
#[cfg(all(target_vendor = "apple", target_arch = "aarch64"))]
#[derive(Debug)]
#[repr(C)]
#[expect(clippy::pub_underscore_fields)]
#[allow(non_camel_case_types)] // expect breaks for some reason
pub struct arm_exception_state64 {
    /// Virtual Fault Address
    pub __far: u64,
    /// Exception syndrome
    pub __esr: u32,
    /// number of arm exception taken
    pub __exception: u32,
}

/// ```c
/// _STRUCT_ARM_THREAD_STATE64
/// {
/// __uint64_t __x[29]; /* General purpose registers x0-x28 */
/// __uint64_t __fp;    /* Frame pointer x29 */
/// __uint64_t __lr;    /* Link register x30 */
/// __uint64_t __sp;    /* Stack pointer x31 */
/// __uint64_t __pc;    /* Program counter */
/// __uint32_t __cpsr;  /* Current program status register */
/// __uint32_t __pad;   /* Same size for 32-bit or 64-bit clients */
/// };
/// ```
#[cfg(all(target_vendor = "apple", target_arch = "aarch64"))]
#[derive(Debug)]
#[repr(C)]
#[expect(clippy::pub_underscore_fields)]
#[allow(non_camel_case_types)] // expect breaks for some reason
pub struct arm_thread_state64 {
    /// General purpose registers x0-x28
    pub __x: [u64; 29],
    /// Frame pointer x29
    pub __fp: u64,
    /// Link register x30
    pub __lr: u64,
    /// Stack pointer x31
    pub __sp: u64,
    /// Program counter
    pub __pc: u64,
    /// Current program status register
    pub __cpsr: u32,
    /// Same size for 32-bit or 64-bit clients
    pub __pad: u32,
}

/// ```c
/// _STRUCT_ARM_NEON_STATE64
/// {
/// char opaque[(32 * 16) + (2 * sizeof(__uint32_t))];
/// } __attribute__((aligned(16)));
/// ````
#[cfg(all(target_vendor = "apple", target_arch = "aarch64"))]
#[derive(Debug)]
#[repr(C, align(16))]
#[allow(non_camel_case_types)] // expect breaks for some reason
pub struct arm_neon_state64 {
    /// opaque
    pub opaque: [u8; (32 * 16) + (2 * size_of::<u32>())],
}

/// ```c
/// _STRUCT_MCONTEXT64
/// {
///    _STRUCT_ARM_EXCEPTION_STATE64   es;
///    _STRUCT_ARM_THREAD_STATE64      ss;
///    _STRUCT_ARM_NEON_STATE64        ns;
/// };
/// ```
#[cfg(all(target_vendor = "apple", target_arch = "aarch64"))]
#[derive(Debug)]
#[repr(C)]
#[expect(clippy::pub_underscore_fields)]
#[allow(non_camel_case_types)] // expect breaks for some reason
pub struct mcontext64 {
    /// `_STRUCT_ARM_EXCEPTION_STATE64`
    pub __es: arm_exception_state64,
    /// `_STRUCT_ARM_THREAD_STATE64`
    pub __ss: arm_thread_state64,
    /// `_STRUCT_ARM_NEON_STATE64`
    pub __ns: arm_neon_state64,
}

/// ```c
/// _STRUCT_SIGALTSTACK
/// {
/// void            *ss_sp;         /* signal stack base */
/// __darwin_size_t ss_size;        /* signal stack length */
/// int             ss_flags;       /* SA_DISABLE and/or SA_ONSTACK */
/// };
/// ```
#[cfg(all(target_vendor = "apple", target_arch = "aarch64"))]
#[derive(Debug)]
#[repr(C)]
#[allow(non_camel_case_types)] // expect breaks for some reason
pub struct sigaltstack {
    /// signal stack base
    pub ss_sp: *mut c_void,
    /// signal stack length
    pub ss_size: libc::size_t,
    /// `SA_DISABLE` and/or `SA_ONSTACK`
    pub ss_flags: c_int,
}

/// User Context Struct on apple `aarch64`
///
/// ```c
/// _STRUCT_UCONTEXT
/// {
///    int                     uc_onstack;
///    __darwin_sigset_t       uc_sigmask;     /* signal mask used by this context */
///    _STRUCT_SIGALTSTACK     uc_stack;       /* stack used by this context */
///    _STRUCT_UCONTEXT        *uc_link;       /* pointer to resuming context */
///    __darwin_size_t         uc_mcsize;      /* size of the machine context passed in */
///    _STRUCT_MCONTEXT        *uc_mcontext;   /* pointer to machine specific context */
/// #ifdef _XOPEN_SOURCE
///    _STRUCT_MCONTEXT        __mcontext_data;
/// #endif /* _XOPEN_SOURCE */
/// };
/// ```
#[cfg(all(target_vendor = "apple", target_arch = "aarch64"))]
#[derive(Debug)]
#[repr(C)]
#[allow(non_camel_case_types)] // expect breaks for some reason
pub struct ucontext_t {
    /// onstack
    pub uc_onstack: c_int,
    /// signal mask used by this context
    pub uc_sigmask: u32,
    /// stack used by this context
    pub uc_stack: sigaltstack,
    /// pointer to resuming context
    pub uc_link: *mut c_void,
    /// size of the machine context passed in
    pub uc_mcsize: ssize_t,
    /// pointer to machine specific context
    pub uc_mcontext: *mut mcontext64,
    /// The mcontext data in multiple steps.
    pub mcontext_data: mcontext64,
}

#[cfg(all(target_vendor = "apple", target_arch = "aarch64"))]
use libc::ssize_t;
#[cfg(not(any(
    all(target_os = "linux", target_arch = "arm"),
    all(target_vendor = "apple", target_arch = "aarch64")
)))]
pub use libc::ucontext_t;
#[cfg(feature = "alloc")]
use libc::{
    SA_NODEFER, SA_ONSTACK, SA_SIGINFO, malloc, sigaction, sigaddset, sigaltstack, sigemptyset,
    stack_t,
};
use libc::{
    SIGABRT, SIGALRM, SIGBUS, SIGFPE, SIGHUP, SIGILL, SIGINT, SIGKILL, SIGPIPE, SIGQUIT, SIGSEGV,
    SIGTERM, SIGTRAP, SIGUSR2, c_int,
};
pub use libc::{c_void, siginfo_t};
use num_enum::{IntoPrimitive, TryFromPrimitive};

use crate::Error;

unsafe extern "C" {
    /// The `libc` `getcontext`
    /// For some reason, it's not available on `MacOS`.
    fn getcontext(ucp: *mut ucontext_t) -> c_int;
}

/// All signals on this system, as `enum`.
#[derive(Debug, IntoPrimitive, TryFromPrimitive, Copy, Clone)]
#[repr(i32)]
pub enum Signal {
    /// `SIGABRT` signal id
    SigAbort = SIGABRT,
    /// `SIGBUS` signal id
    SigBus = SIGBUS,
    /// `SIGFPE` signal id
    SigFloatingPointException = SIGFPE,
    /// `SIGILL` signal id
    SigIllegalInstruction = SIGILL,
    /// `SIGPIPE` signal id
    SigPipe = SIGPIPE,
    /// `SIGSEGV` signal id
    SigSegmentationFault = SIGSEGV,
    /// `SIGUSR2` signal id
    SigUser2 = SIGUSR2,
    /// `SIGALARM` signal id
    SigAlarm = SIGALRM,
    /// `SIGHUP` signal id
    SigHangUp = SIGHUP,
    /// `SIGKILL` signal id
    SigKill = SIGKILL,
    /// `SIGQUIT` signal id
    SigQuit = SIGQUIT,
    /// `SIGTERM` signal id
    SigTerm = SIGTERM,
    /// `SIGINT` signal id
    SigInterrupt = SIGINT,
    /// `SIGTRAP` signal id
    SigTrap = SIGTRAP,
}

#[cfg(feature = "std")]
impl Signal {
    /// Handle an incoming signal
    pub fn handle(&self) {
        match self {
            Signal::SigInterrupt | Signal::SigQuit | Signal::SigTerm => {
                std::process::exit(CTRL_C_EXIT)
            }
            _ => {}
        }
    }
}

impl TryFrom<&str> for Signal {
    type Error = Error;

    fn try_from(value: &str) -> Result<Self, Self::Error> {
        Ok(match value {
            "SIGABRT" => Signal::SigAbort,
            "SIGBUS" => Signal::SigBus,
            "SIGFPE" => Signal::SigFloatingPointException,
            "SIGILL" => Signal::SigIllegalInstruction,
            "SIGPIPE" => Signal::SigPipe,
            "SIGSEGV" => Signal::SigSegmentationFault,
            "SIGUSR2" => Signal::SigUser2,
            "SIGALRM" => Signal::SigAlarm,
            "SIGHUP" => Signal::SigHangUp,
            "SIGKILL" => Signal::SigKill,
            "SIGQUIT" => Signal::SigQuit,
            "SIGTERM" => Signal::SigTerm,
            "SIGINT" => Signal::SigInterrupt,
            "SIGTRAP" => Signal::SigTrap,
            _ => return Err(Error::illegal_argument(format!("No signal named {value}"))),
        })
    }
}

#[cfg(feature = "std")]
impl From<Signal> for nix::sys::signal::Signal {
    fn from(value: Signal) -> Self {
        // we can be semi-certain that all signals exist in nix.
        i32::from(value).try_into().unwrap()
    }
}

impl PartialEq for Signal {
    fn eq(&self, other: &Self) -> bool {
        *self as i32 == *other as i32
    }
}

impl Eq for Signal {}

unsafe impl Sync for Signal {}

impl Display for Signal {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), fmt::Error> {
        match self {
            Signal::SigAbort => write!(f, "SIGABRT")?,
            Signal::SigBus => write!(f, "SIGBUS")?,
            Signal::SigFloatingPointException => write!(f, "SIGFPE")?,
            Signal::SigIllegalInstruction => write!(f, "SIGILL")?,
            Signal::SigPipe => write!(f, "SIGPIPE")?,
            Signal::SigSegmentationFault => write!(f, "SIGSEGV")?,
            Signal::SigUser2 => write!(f, "SIGUSR2")?,
            Signal::SigAlarm => write!(f, "SIGALRM")?,
            Signal::SigHangUp => write!(f, "SIGHUP")?,
            Signal::SigKill => write!(f, "SIGKILL")?,
            Signal::SigQuit => write!(f, "SIGQUIT")?,
            Signal::SigTerm => write!(f, "SIGTERM")?,
            Signal::SigInterrupt => write!(f, "SIGINT")?,
            Signal::SigTrap => write!(f, "SIGTRAP")?,
        }

        Ok(())
    }
}

/// A trait for `LibAFL` signal handling
#[cfg(feature = "alloc")]
pub trait SignalHandler {
    /// Handle a signal
    ///
    /// # Safety
    /// This is generally not safe to call. It should only be called through the signal it was registered for.
    /// Signal handling is hard, don't mess with it :).
    unsafe fn handle(
        &mut self,
        signal: Signal,
        info: &mut siginfo_t,
        _context: Option<&mut ucontext_t>,
    );
    /// Return a list of signals to handle
    fn signals(&self) -> Vec<Signal>;
}

#[cfg(feature = "alloc")]
struct HandlerHolder {
    handler: UnsafeCell<*mut dyn SignalHandler>,
}

#[cfg(feature = "alloc")]
unsafe impl Send for HandlerHolder {}

/// Let's get 8 mb for now.
#[cfg(feature = "alloc")]
const SIGNAL_STACK_SIZE: usize = 2 << 22;

/// To be able to handle SIGSEGV when the stack is exhausted, we need our own little stack space.
#[cfg(feature = "alloc")]
static mut SIGNAL_STACK_PTR: *mut c_void = ptr::null_mut();

/// Keep track of which handler is registered for which signal
#[cfg(feature = "alloc")]
static mut SIGNAL_HANDLERS: [Option<HandlerHolder>; 32] = [
    // We cannot use [None; 32] because it requires Copy. Ugly, but I don't think there's an
    // alternative.
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
];

/// Internal function that is being called whenever a signal we are registered for arrives.
///
/// # Safety
/// This should be somewhat safe to call for signals previously registered,
/// unless the signal handlers registered using [`setup_signal_handler()`] are broken.
#[cfg(feature = "alloc")]
unsafe fn handle_signal(sig: c_int, info: *mut siginfo_t, void: *mut c_void) {
    unsafe {
        let signal = &Signal::try_from(sig).unwrap();
        let handler = {
            match &SIGNAL_HANDLERS[*signal as usize] {
                Some(handler_holder) => &mut **handler_holder.handler.get(),
                None => return,
            }
        };
        handler.handle(
            *signal,
            &mut ptr::read_unaligned(info),
            (void as *mut ucontext_t).as_mut(),
        );
    }
}

/// Setup signal handlers in a somewhat rusty way.
///
/// This will allocate a signal stack and set the signal handlers accordingly.
/// It is, for example, used in `LibAFL's` `InProcessExecutor` to restart the fuzzer in case of a crash,
/// or to handle `SIGINT` in the broker process.
///
/// # Safety
///
/// The signal handlers will be called on any signal. They should (tm) be async safe.
/// The handler pointer will be dereferenced, and the data the pointer points to may therefore not move.
/// A lot can go south in signal handling. Be sure you know what you are doing.
#[cfg(feature = "alloc")]
pub unsafe fn setup_signal_handler<T: 'static + SignalHandler>(
    handler: *mut T,
) -> Result<(), Error> {
    unsafe {
        // First, set up our own stack to be used during segfault handling. (and specify `SA_ONSTACK` in `sigaction`)
        if SIGNAL_STACK_PTR.is_null() {
            SIGNAL_STACK_PTR = malloc(SIGNAL_STACK_SIZE);

            // Rust always panics on OOM, so we will, too.
            assert!(
                !SIGNAL_STACK_PTR.is_null(),
                "Failed to allocate signal stack with {SIGNAL_STACK_SIZE} bytes!"
            );
        }
        let mut ss: stack_t = mem::zeroed();
        ss.ss_size = SIGNAL_STACK_SIZE;
        ss.ss_sp = SIGNAL_STACK_PTR;
        sigaltstack(&raw mut ss, ptr::null_mut() as _);

        let mut sa: sigaction = mem::zeroed();
        sigemptyset(&raw mut sa.sa_mask);
        sigaddset(&raw mut sa.sa_mask, SIGALRM);
        sa.sa_flags = SA_NODEFER | SA_SIGINFO | SA_ONSTACK;
        sa.sa_sigaction = handle_signal as *const () as usize;
        let signals = (*handler).signals();
        for sig in signals {
            write_volatile(
                &raw mut SIGNAL_HANDLERS[sig as usize],
                Some(HandlerHolder {
                    handler: UnsafeCell::new(handler as *mut dyn SignalHandler),
                }),
            );

            if sigaction(sig as i32, &raw mut sa, ptr::null_mut()) < 0 {
                #[cfg(feature = "std")]
                {
                    let err_str = CString::new(format!("Failed to setup {sig} handler")).unwrap();
                    libc::perror(err_str.as_ptr());
                }
                return Err(Error::unknown(format!("Could not set up {sig} handler")));
            }
        }
        compiler_fence(Ordering::SeqCst);

        Ok(())
    }
}

/// Function to get the current [`ucontext_t`] for this process.
///
/// This calls the libc `getcontext` function under the hood.
/// It can be useful, for example for `dump_regs`.
/// Note that calling this method may, of course, alter the state.
/// We wrap it here, as it seems to be (currently)
/// not available on `MacOS` in the `libc` crate.
#[cfg(unix)]
#[expect(clippy::inline_always)] // we assume that inlining will destroy less state
#[inline(always)]
pub fn ucontext() -> Result<ucontext_t, Error> {
    let mut ucontext = unsafe { mem::zeroed() };
    if cfg!(not(any(target_os = "openbsd", target_os = "haiku"))) {
        if unsafe { getcontext(&raw mut ucontext) } == 0 {
            Ok(ucontext)
        } else {
            #[cfg(not(feature = "std"))]
            unsafe {
                libc::perror(b"Failed to get ucontext\n".as_ptr() as _);
            };
            #[cfg(not(feature = "std"))]
            return Err(Error::unknown("Failed to get ucontex"));

            #[cfg(feature = "std")]
            Err(Error::unknown(format!(
                "Failed to get ucontext: {:?}",
                Errno::last()
            )))
        }
    } else {
        Ok(ucontext)
    }
}
