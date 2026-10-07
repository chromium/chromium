//! Exception handling for Windows

#[cfg(feature = "alloc")]
use alloc::vec::Vec;
use core::{
    cell::UnsafeCell,
    fmt::{self, Display, Formatter},
    ptr::{self, write_volatile},
    sync::atomic::{Ordering, compiler_fence},
};
use std::os::raw::{c_long, c_void};

use libafl_core::Error;
use num_enum::FromPrimitive;
pub use windows::Win32::{
    Foundation::NTSTATUS,
    System::{
        Console::{CTRL_BREAK_EVENT, CTRL_C_EVENT, PHANDLER_ROUTINE, SetConsoleCtrlHandler},
        Diagnostics::Debug::{
            AddVectoredExceptionHandler, EXCEPTION_POINTERS, UnhandledExceptionFilter,
        },
        Threading::{IsProcessorFeaturePresent, PROCESSOR_FEATURE_ID},
    },
};
pub use windows_core::BOOL;

/// The special exit code when the target exited through ctrl-c
pub const CTRL_C_EXIT: i32 = -1073741510;

/// For VEH
const EXCEPTION_CONTINUE_EXECUTION: c_long = -1;

/// For VEH
const EXCEPTION_CONTINUE_SEARCH: c_long = 0;

// For SEH
// const EXCEPTION_EXECUTE_HANDLER: c_long = 1;

pub const SIGINT: i32 = 2;
pub const SIGILL: i32 = 4;
pub const SIGABRT_COMPAT: i32 = 6;
pub const SIGFPE: i32 = 8;
pub const SIGSEGV: i32 = 11;
pub const SIGTERM: i32 = 15;
pub const SIGBREAK: i32 = 21;
pub const SIGABRT: i32 = 22;
pub const SIGABRT2: i32 = 22;

pub const STATUS_WAIT_0: i32 = 0x00000000;
pub const STATUS_ABANDONED_WAIT_0: i32 = 0x00000080;
pub const STATUS_USER_APC: i32 = 0x000000C0;
pub const STATUS_TIMEOUT: i32 = 0x00000102;
pub const STATUS_PENDING: i32 = 0x00000103;
pub const STATUS_SEGMENT_NOTIFICATION: i32 = 0x40000005;
pub const STATUS_FATAL_APP_EXIT: i32 = 0x40000015;
pub const STATUS_GUARD_PAGE_VIOLATION: i32 = 0x80000001;
pub const STATUS_DATATYPE_MISALIGNMENT: i32 = 0x80000002;
pub const STATUS_BREAKPOINT: i32 = 0x80000003;
pub const STATUS_SINGLE_STEP: i32 = 0x80000004;
pub const STATUS_LONGJUMP: i32 = 0x80000026;
pub const STATUS_UNWIND_CONSOLIDATE: i32 = 0x80000029;
pub const STATUS_ACCESS_VIOLATION: i32 = 0xC0000005;
pub const STATUS_IN_PAGE_ERROR: i32 = 0xC0000006;
pub const STATUS_INVALID_HANDLE: i32 = 0xC0000008;
pub const STATUS_NO_MEMORY: i32 = 0xC0000017;
pub const STATUS_ILLEGAL_INSTRUCTION: i32 = 0xC000001D;
pub const STATUS_NONCONTINUABLE_EXCEPTION: i32 = 0xC0000025;
pub const STATUS_INVALID_DISPOSITION: i32 = 0xC0000026;
pub const STATUS_ARRAY_BOUNDS_EXCEEDED: i32 = 0xC000008C;
pub const STATUS_FLOAT_DENORMAL_OPERAND: i32 = 0xC000008D;
pub const STATUS_FLOAT_DIVIDE_BY_ZERO: i32 = 0xC000008E;
pub const STATUS_FLOAT_INEXACT_RESULT: i32 = 0xC000008F;
pub const STATUS_FLOAT_INVALID_OPERATION: i32 = 0xC0000090;
pub const STATUS_FLOAT_OVERFLOW: i32 = 0xC0000091;
pub const STATUS_FLOAT_STACK_CHECK: i32 = 0xC0000092;
pub const STATUS_FLOAT_UNDERFLOW: i32 = 0xC0000093;
pub const STATUS_INTEGER_DIVIDE_BY_ZERO: i32 = 0xC0000094;
pub const STATUS_INTEGER_OVERFLOW: i32 = 0xC0000095;
pub const STATUS_PRIVILEGED_INSTRUCTION: i32 = 0xC0000096;
pub const STATUS_STACK_OVERFLOW: i32 = 0xC00000FD;
pub const STATUS_DLL_NOT_FOUND: i32 = 0xC0000135;
pub const STATUS_ORDINAL_NOT_FOUND: i32 = 0xC0000138;
pub const STATUS_ENTRYPOINT_NOT_FOUND: i32 = 0xC0000139;
pub const STATUS_CONTROL_C_EXIT: i32 = 0xC000013A;
pub const STATUS_DLL_INIT_FAILED: i32 = 0xC0000142;
pub const STATUS_FLOAT_MULTIPLE_FAULTS: i32 = 0xC00002B4;
pub const STATUS_FLOAT_MULTIPLE_TRAPS: i32 = 0xC00002B5;
pub const STATUS_REG_NAT_CONSUMPTION: i32 = 0xC00002C9;
pub const STATUS_HEAP_CORRUPTION: i32 = 0xC0000374;
pub const STATUS_STACK_BUFFER_OVERRUN: i32 = 0xC0000409;
pub const STATUS_INVALID_CRUNTIME_PARAMETER: i32 = 0xC0000417;
pub const STATUS_ASSERTION_FAILURE: i32 = 0xC0000420;
pub const STATUS_SXS_EARLY_DEACTIVATION: i32 = 0xC015000F;
pub const STATUS_SXS_INVALID_DEACTIVATION: i32 = 0xC0150010;
pub const STATUS_NOT_IMPLEMENTED: i32 = 0xC0000002;

pub const STATUS_WX86_UNSIMULATE: i32 = 0x4000001C;
pub const STATUS_WX86_CONTINUE: i32 = 0x4000001D;
pub const STATUS_WX86_SINGLE_STEP: i32 = 0x4000001E;
pub const STATUS_WX86_BREAKPOINT: i32 = 0x4000001F;
pub const STATUS_WX86_EXCEPTION_CONTINUE: i32 = 0x40000020;
pub const STATUS_WX86_EXCEPTION_LASTCHANCE: i32 = 0x40000021;
pub const STATUS_WX86_EXCEPTION_CHAIN: i32 = 0x40000022;
pub const STATUS_WX86_CREATEWX86TIB: i32 = 0x40000028;
pub const DBG_TERMINATE_THREAD: i32 = 0x40010003;
pub const DBG_TERMINATE_PROCESS: i32 = 0x40010004;
pub const DBG_CONTROL_C: i32 = 0x40010005;
pub const DBG_PRINTEXCEPTION_C: i32 = 0x40010006;
pub const DBG_RIPEXCEPTION: i32 = 0x40010007;
pub const DBG_CONTROL_BREAK: i32 = 0x40010008;
pub const DBG_COMMAND_EXCEPTION: i32 = 0x40010009;
pub const DBG_PRINTEXCEPTION_WIDE_C: i32 = 0x4001000A;
pub const EXCEPTION_RO_ORIGINATEERROR: i32 = 0x40080201;
pub const EXCEPTION_RO_TRANSFORMERROR: i32 = 0x40080202;
pub const MS_VC_EXCEPTION: i32 = 0x406D1388;
pub const DBG_EXCEPTION_NOT_HANDLED: i32 = 0x80010001;
pub const STATUS_INVALID_PARAMETER: i32 = 0xC000000D;
pub const STATUS_ILLEGAL_FLOAT_CONTEXT: i32 = 0xC000014A;
pub const EXCEPTION_POSSIBLE_DEADLOCK: i32 = 0xC0000194;
pub const STATUS_INVALID_EXCEPTION_HANDLER: i32 = 0xC00001A5;
pub const STATUS_DATATYPE_MISALIGNMENT_ERROR: i32 = 0xC00002C5;
pub const STATUS_USER_CALLBACK: i32 = 0xC000041D;
pub const CLR_EXCEPTION: i32 = 0xE0434352;
pub const CPP_EH_EXCEPTION: i32 = 0xE06D7363;
pub const VCPP_EXCEPTION_ERROR_INVALID_PARAMETER: i32 = 0xC06D0057;
pub const VCPP_EXCEPTION_ERROR_MOD_NOT_FOUND: i32 = 0xC06D007E;
pub const VCPP_EXCEPTION_ERROR_PROC_NOT_FOUND: i32 = 0xC06D007F;

#[derive(Debug, FromPrimitive, Copy, Clone)]
#[repr(i32)]
pub enum ExceptionCode {
    WaitZero = STATUS_WAIT_0,
    AbandonedWaitZero = STATUS_ABANDONED_WAIT_0,
    UserApc = STATUS_USER_APC,
    Timeout = STATUS_TIMEOUT,
    Pending = STATUS_PENDING,
    SegmentNotification = STATUS_SEGMENT_NOTIFICATION,
    FatalAppExit = STATUS_FATAL_APP_EXIT,
    GuardPageViolation = STATUS_GUARD_PAGE_VIOLATION,
    DatatypeMisalignment = STATUS_DATATYPE_MISALIGNMENT,
    Breakpoint = STATUS_BREAKPOINT,
    SingleStep = STATUS_SINGLE_STEP,
    Longjump = STATUS_LONGJUMP,
    UnwindConsolidate = STATUS_UNWIND_CONSOLIDATE,
    AccessViolation = STATUS_ACCESS_VIOLATION,
    InPageError = STATUS_IN_PAGE_ERROR,
    InvalidHandle = STATUS_INVALID_HANDLE,
    NoMemory = STATUS_NO_MEMORY,
    IllegalInstruction = STATUS_ILLEGAL_INSTRUCTION,
    NoncontinuableException = STATUS_NONCONTINUABLE_EXCEPTION,
    InvalidDisposition = STATUS_INVALID_DISPOSITION,
    ArrayBoundsExceeded = STATUS_ARRAY_BOUNDS_EXCEEDED,
    FloatDenormalOperand = STATUS_FLOAT_DENORMAL_OPERAND,
    FloatDivideByZero = STATUS_FLOAT_DIVIDE_BY_ZERO,
    FloatInexactResult = STATUS_FLOAT_INEXACT_RESULT,
    FloatInvalidOperation = STATUS_FLOAT_INVALID_OPERATION,
    FloatOverflow = STATUS_FLOAT_OVERFLOW,
    FloatStackCheck = STATUS_FLOAT_STACK_CHECK,
    FloatUnderflow = STATUS_FLOAT_UNDERFLOW,
    IntegerDivideByZero = STATUS_INTEGER_DIVIDE_BY_ZERO,
    IntegerOverflow = STATUS_INTEGER_OVERFLOW,
    PrivilegedInstruction = STATUS_PRIVILEGED_INSTRUCTION,
    StackOverflow = STATUS_STACK_OVERFLOW,
    DllNotFound = STATUS_DLL_NOT_FOUND,
    OrdinalNotFound = STATUS_ORDINAL_NOT_FOUND,
    EntrypointNotFound = STATUS_ENTRYPOINT_NOT_FOUND,
    ControlCExit = STATUS_CONTROL_C_EXIT,
    DllInitFailed = STATUS_DLL_INIT_FAILED,
    FloatMultipleFaults = STATUS_FLOAT_MULTIPLE_FAULTS,
    FloatMultipleTraps = STATUS_FLOAT_MULTIPLE_TRAPS,
    RegNatConsumption = STATUS_REG_NAT_CONSUMPTION,
    HeapCorruption = STATUS_HEAP_CORRUPTION,
    StackBufferOverrun = STATUS_STACK_BUFFER_OVERRUN,
    InvalidCruntimeParameter = STATUS_INVALID_CRUNTIME_PARAMETER,
    AssertionFailure = STATUS_ASSERTION_FAILURE,
    SxsEarlyDeactivation = STATUS_SXS_EARLY_DEACTIVATION,
    SxsInvalidDeactivation = STATUS_SXS_INVALID_DEACTIVATION,
    NotImplemented = STATUS_NOT_IMPLEMENTED,

    Wx86Unsimulate = STATUS_WX86_UNSIMULATE,
    Wx86Continue = STATUS_WX86_CONTINUE,
    Wx86SingleStep = STATUS_WX86_SINGLE_STEP,
    Wx86Breakpoint = STATUS_WX86_BREAKPOINT,
    Wx86ExceptionContinue = STATUS_WX86_EXCEPTION_CONTINUE,
    Wx86ExceptionLastchance = STATUS_WX86_EXCEPTION_LASTCHANCE,
    Wx86ExceptionChain = STATUS_WX86_EXCEPTION_CHAIN,
    Wx86Createwx86Tib = STATUS_WX86_CREATEWX86TIB,
    DbgTerminateThread = DBG_TERMINATE_THREAD,
    DbgTerminateProcess = DBG_TERMINATE_PROCESS,
    DbgControlC = DBG_CONTROL_C,
    DbgPrintexceptionC = DBG_PRINTEXCEPTION_C,
    DbgRipexception = DBG_RIPEXCEPTION,
    DbgControlBreak = DBG_CONTROL_BREAK,
    DbgCommandException = DBG_COMMAND_EXCEPTION,
    DbgPrintexceptionWideC = DBG_PRINTEXCEPTION_WIDE_C,
    ExceptionRoOriginateError = EXCEPTION_RO_ORIGINATEERROR,
    ExceptionRoTransformError = EXCEPTION_RO_TRANSFORMERROR,
    MsVcException = MS_VC_EXCEPTION,
    DbgExceptionNotHandled = DBG_EXCEPTION_NOT_HANDLED,
    InvalidParameter = STATUS_INVALID_PARAMETER,
    IllegalFloatContext = STATUS_ILLEGAL_FLOAT_CONTEXT,
    ExceptionPossibleDeadlock = EXCEPTION_POSSIBLE_DEADLOCK,
    InvalidExceptionHandler = STATUS_INVALID_EXCEPTION_HANDLER,
    DatatypeMisalignmentError = STATUS_DATATYPE_MISALIGNMENT_ERROR,
    UserCallback = STATUS_USER_CALLBACK,
    ClrException = CLR_EXCEPTION,
    CppEhException = CPP_EH_EXCEPTION,
    VcppExceptionErrorInvalidParameter = VCPP_EXCEPTION_ERROR_INVALID_PARAMETER,
    VcppExceptionErrorModNotFound = VCPP_EXCEPTION_ERROR_MOD_NOT_FOUND,
    VcppExceptionErrorProcNotFound = VCPP_EXCEPTION_ERROR_PROC_NOT_FOUND,
    #[default]
    Others,
}

pub static CRASH_EXCEPTIONS: &[ExceptionCode] = &[
    ExceptionCode::AccessViolation,
    ExceptionCode::ArrayBoundsExceeded,
    ExceptionCode::FloatDivideByZero,
    ExceptionCode::GuardPageViolation,
    ExceptionCode::IllegalInstruction,
    ExceptionCode::InPageError,
    ExceptionCode::IntegerDivideByZero,
    ExceptionCode::InvalidHandle,
    ExceptionCode::NoncontinuableException,
    ExceptionCode::PrivilegedInstruction,
    ExceptionCode::StackOverflow,
    ExceptionCode::HeapCorruption,
    ExceptionCode::StackBufferOverrun,
    ExceptionCode::AssertionFailure,
];

impl PartialEq for ExceptionCode {
    fn eq(&self, other: &Self) -> bool {
        *self as i32 == *other as i32
    }
}

impl Eq for ExceptionCode {}

unsafe impl Sync for ExceptionCode {}

impl Display for ExceptionCode {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        match self {
            ExceptionCode::WaitZero => write!(f, "STATUS_WAIT_0"),
            ExceptionCode::AbandonedWaitZero => write!(f, "STATUS_ABANDONED_WAIT_0"),
            ExceptionCode::UserApc => write!(f, "STATUS_USER_APC"),
            ExceptionCode::Timeout => write!(f, "STATUS_TIMEOUT"),
            ExceptionCode::Pending => write!(f, "STATUS_PENDING"),
            ExceptionCode::SegmentNotification => write!(f, "STATUS_SEGMENT_NOTIFICATION"),
            ExceptionCode::FatalAppExit => write!(f, "STATUS_FATAL_APP_EXIT"),
            ExceptionCode::GuardPageViolation => write!(f, "STATUS_GUARD_PAGE_VIOLATION"),
            ExceptionCode::DatatypeMisalignment => write!(f, "STATUS_DATATYPE_MISALIGNMENT"),
            ExceptionCode::Breakpoint => write!(f, "STATUS_BREAKPOINT"),
            ExceptionCode::SingleStep => write!(f, "STATUS_SINGLE_STEP"),
            ExceptionCode::Longjump => write!(f, "STATUS_LONGJUMP"),
            ExceptionCode::UnwindConsolidate => write!(f, "STATUS_UNWIND_CONSOLIDATE"),
            ExceptionCode::AccessViolation => write!(f, "STATUS_ACCESS_VIOLATION"),
            ExceptionCode::InPageError => write!(f, "STATUS_IN_PAGE_ERROR"),
            ExceptionCode::InvalidHandle => write!(f, "STATUS_INVALID_HANDLE"),
            ExceptionCode::NoMemory => write!(f, "STATUS_NO_MEMORY"),
            ExceptionCode::IllegalInstruction => write!(f, "STATUS_ILLEGAL_INSTRUCTION"),
            ExceptionCode::NoncontinuableException => write!(f, "STATUS_NONCONTINUABLE_EXCEPTION"),
            ExceptionCode::InvalidDisposition => write!(f, "STATUS_INVALID_DISPOSITION"),
            ExceptionCode::ArrayBoundsExceeded => write!(f, "STATUS_ARRAY_BOUNDS_EXCEEDED"),
            ExceptionCode::FloatDenormalOperand => write!(f, "STATUS_FLOAT_DENORMAL_OPERAND"),
            ExceptionCode::FloatDivideByZero => write!(f, "STATUS_FLOAT_DIVIDE_BY_ZERO"),
            ExceptionCode::FloatInexactResult => write!(f, "STATUS_FLOAT_INEXACT_RESULT"),
            ExceptionCode::FloatInvalidOperation => write!(f, "STATUS_FLOAT_INVALID_OPERATION"),
            ExceptionCode::FloatOverflow => write!(f, "STATUS_FLOAT_OVERFLOW"),
            ExceptionCode::FloatStackCheck => write!(f, "STATUS_FLOAT_STACK_CHECK"),
            ExceptionCode::FloatUnderflow => write!(f, "STATUS_FLOAT_UNDERFLOW"),
            ExceptionCode::IntegerDivideByZero => write!(f, "STATUS_INTEGER_DIVIDE_BY_ZERO"),
            ExceptionCode::IntegerOverflow => write!(f, "STATUS_INTEGER_OVERFLOW"),
            ExceptionCode::PrivilegedInstruction => write!(f, "STATUS_PRIVILEGED_INSTRUCTION"),
            ExceptionCode::StackOverflow => write!(f, "STATUS_STACK_OVERFLOW"),
            ExceptionCode::DllNotFound => write!(f, "STATUS_DLL_NOT_FOUND"),
            ExceptionCode::OrdinalNotFound => write!(f, "STATUS_ORDINAL_NOT_FOUND"),
            ExceptionCode::EntrypointNotFound => write!(f, "STATUS_ENTRYPOINT_NOT_FOUND"),
            ExceptionCode::ControlCExit => write!(f, "STATUS_CONTROL_C_EXIT"),
            ExceptionCode::DllInitFailed => write!(f, "STATUS_DLL_INIT_FAILED"),
            ExceptionCode::FloatMultipleFaults => write!(f, "STATUS_FLOAT_MULTIPLE_FAULTS"),
            ExceptionCode::FloatMultipleTraps => write!(f, "STATUS_FLOAT_MULTIPLE_TRAPS"),
            ExceptionCode::RegNatConsumption => write!(f, "STATUS_REG_NAT_CONSUMPTION"),
            ExceptionCode::HeapCorruption => write!(f, "STATUS_HEAP_CORRUPTION"),
            ExceptionCode::StackBufferOverrun => write!(f, "STATUS_STACK_BUFFER_OVERRUN"),
            ExceptionCode::InvalidCruntimeParameter => {
                write!(f, "STATUS_INVALID_CRUNTIME_PARAMETER")
            }
            ExceptionCode::AssertionFailure => write!(f, "STATUS_ASSERTION_FAILURE"),
            ExceptionCode::SxsEarlyDeactivation => write!(f, "STATUS_SXS_EARLY_DEACTIVATION"),
            ExceptionCode::SxsInvalidDeactivation => write!(f, "STATUS_SXS_INVALID_DEACTIVATION"),
            ExceptionCode::NotImplemented => write!(f, "STATUS_NOT_IMPLEMENTED"),
            ExceptionCode::Wx86Unsimulate => write!(f, "STATUS_WX86_UNSIMULATE"),
            ExceptionCode::Wx86Continue => write!(f, "STATUS_WX86_CONTINUE"),
            ExceptionCode::Wx86SingleStep => write!(f, "STATUS_WX86_SINGLE_STEP"),
            ExceptionCode::Wx86Breakpoint => write!(f, "STATUS_WX86_BREAKPOINT"),
            ExceptionCode::Wx86ExceptionContinue => write!(f, "STATUS_WX86_EXCEPTION_CONTINUE"),
            ExceptionCode::Wx86ExceptionLastchance => write!(f, "STATUS_WX86_EXCEPTION_LASTCHANCE"),
            ExceptionCode::Wx86ExceptionChain => write!(f, "STATUS_WX86_EXCEPTION_CHAIN"),
            ExceptionCode::Wx86Createwx86Tib => write!(f, "STATUS_WX86_CREATEWX86TIB"),
            ExceptionCode::DbgTerminateThread => write!(f, "DBG_TERMINATE_THREAD"),
            ExceptionCode::DbgTerminateProcess => write!(f, "DBG_TERMINATE_PROCESS"),
            ExceptionCode::DbgControlC => write!(f, "DBG_CONTROL_C"),
            ExceptionCode::DbgPrintexceptionC => write!(f, "DBG_PRINTEXCEPTION_C"),
            ExceptionCode::DbgRipexception => write!(f, "DBG_RIPEXCEPTION"),
            ExceptionCode::DbgControlBreak => write!(f, "DBG_CONTROL_BREAK"),
            ExceptionCode::DbgCommandException => write!(f, "DBG_COMMAND_EXCEPTION"),
            ExceptionCode::DbgPrintexceptionWideC => write!(f, "DBG_PRINTEXCEPTION_WIDE_C"),
            ExceptionCode::ExceptionRoOriginateError => write!(f, "EXCEPTION_RO_ORIGINATEERROR"),
            ExceptionCode::ExceptionRoTransformError => write!(f, "EXCEPTION_RO_TRANSFORMERROR"),
            ExceptionCode::MsVcException => write!(f, "MS_VC_EXCEPTION"),
            ExceptionCode::DbgExceptionNotHandled => write!(f, "DBG_EXCEPTION_NOT_HANDLED"),
            ExceptionCode::InvalidParameter => write!(f, "STATUS_INVALID_PARAMETER"),
            ExceptionCode::IllegalFloatContext => write!(f, "STATUS_ILLEGAL_FLOAT_CONTEXT"),
            ExceptionCode::ExceptionPossibleDeadlock => write!(f, "EXCEPTION_POSSIBLE_DEADLOCK"),
            ExceptionCode::InvalidExceptionHandler => write!(f, "STATUS_INVALID_EXCEPTION_HANDLER"),
            ExceptionCode::DatatypeMisalignmentError => {
                write!(f, "STATUS_DATATYPE_MISALIGNMENT_ERROR")
            }
            ExceptionCode::UserCallback => write!(f, "STATUS_USER_CALLBACK"),
            ExceptionCode::ClrException => write!(f, "CLR_EXCEPTION"),
            ExceptionCode::CppEhException => write!(f, "CPP_EH_EXCEPTION"),
            ExceptionCode::VcppExceptionErrorInvalidParameter => {
                write!(f, "VCPP_EXCEPTION_ERROR_INVALID_PARAMETER")
            }
            ExceptionCode::VcppExceptionErrorModNotFound => {
                write!(f, "VCPP_EXCEPTION_ERROR_MOD_NOT_FOUND")
            }
            ExceptionCode::VcppExceptionErrorProcNotFound => {
                write!(f, "VCPP_EXCEPTION_ERROR_PROC_NOT_FOUND")
            }
            ExceptionCode::Others => write!(f, "Unknown exception code"),
        }
    }
}

pub static EXCEPTION_CODES_MAPPING: [ExceptionCode; 79] = [
    ExceptionCode::WaitZero,
    ExceptionCode::AbandonedWaitZero,
    ExceptionCode::UserApc,
    ExceptionCode::Timeout,
    ExceptionCode::Pending,
    ExceptionCode::SegmentNotification,
    ExceptionCode::FatalAppExit,
    ExceptionCode::GuardPageViolation,
    ExceptionCode::DatatypeMisalignment,
    ExceptionCode::Breakpoint,
    ExceptionCode::SingleStep,
    ExceptionCode::Longjump,
    ExceptionCode::UnwindConsolidate,
    ExceptionCode::AccessViolation,
    ExceptionCode::InPageError,
    ExceptionCode::InvalidHandle,
    ExceptionCode::NoMemory,
    ExceptionCode::IllegalInstruction,
    ExceptionCode::NoncontinuableException,
    ExceptionCode::InvalidDisposition,
    ExceptionCode::ArrayBoundsExceeded,
    ExceptionCode::FloatDenormalOperand,
    ExceptionCode::FloatDivideByZero,
    ExceptionCode::FloatInexactResult,
    ExceptionCode::FloatInvalidOperation,
    ExceptionCode::FloatOverflow,
    ExceptionCode::FloatStackCheck,
    ExceptionCode::FloatUnderflow,
    ExceptionCode::IntegerDivideByZero,
    ExceptionCode::IntegerOverflow,
    ExceptionCode::PrivilegedInstruction,
    ExceptionCode::StackOverflow,
    ExceptionCode::DllNotFound,
    ExceptionCode::OrdinalNotFound,
    ExceptionCode::EntrypointNotFound,
    ExceptionCode::ControlCExit,
    ExceptionCode::DllInitFailed,
    ExceptionCode::FloatMultipleFaults,
    ExceptionCode::FloatMultipleTraps,
    ExceptionCode::RegNatConsumption,
    ExceptionCode::HeapCorruption,
    ExceptionCode::StackBufferOverrun,
    ExceptionCode::InvalidCruntimeParameter,
    ExceptionCode::AssertionFailure,
    ExceptionCode::SxsEarlyDeactivation,
    ExceptionCode::SxsInvalidDeactivation,
    ExceptionCode::NotImplemented,
    ExceptionCode::Wx86Unsimulate,
    ExceptionCode::Wx86Continue,
    ExceptionCode::Wx86SingleStep,
    ExceptionCode::Wx86Breakpoint,
    ExceptionCode::Wx86ExceptionContinue,
    ExceptionCode::Wx86ExceptionLastchance,
    ExceptionCode::Wx86ExceptionChain,
    ExceptionCode::Wx86Createwx86Tib,
    ExceptionCode::DbgTerminateThread,
    ExceptionCode::DbgTerminateProcess,
    ExceptionCode::DbgControlC,
    ExceptionCode::DbgPrintexceptionC,
    ExceptionCode::DbgRipexception,
    ExceptionCode::DbgControlBreak,
    ExceptionCode::DbgCommandException,
    ExceptionCode::DbgPrintexceptionWideC,
    ExceptionCode::ExceptionRoOriginateError,
    ExceptionCode::ExceptionRoTransformError,
    ExceptionCode::MsVcException,
    ExceptionCode::DbgExceptionNotHandled,
    ExceptionCode::InvalidParameter,
    ExceptionCode::IllegalFloatContext,
    ExceptionCode::ExceptionPossibleDeadlock,
    ExceptionCode::InvalidExceptionHandler,
    ExceptionCode::DatatypeMisalignmentError,
    ExceptionCode::UserCallback,
    ExceptionCode::ClrException,
    ExceptionCode::CppEhException,
    ExceptionCode::VcppExceptionErrorInvalidParameter,
    ExceptionCode::VcppExceptionErrorModNotFound,
    ExceptionCode::VcppExceptionErrorProcNotFound,
    ExceptionCode::Others,
];

#[cfg(feature = "alloc")]
pub trait ExceptionHandler {
    /// Handle an exception
    ///
    /// # Safety
    /// This is generally not safe to call. It should only be called through the signal it was registered for.
    /// Signal handling is hard, don't mess with it :).
    unsafe fn handle(
        &mut self,
        exception_code: ExceptionCode,
        exception_pointers: *mut EXCEPTION_POINTERS,
    );
    /// Return a list of exceptions to handle
    fn exceptions(&self) -> Vec<ExceptionCode>;
}

struct HandlerHolder {
    handler: UnsafeCell<*mut dyn ExceptionHandler>,
}

pub const EXCEPTION_HANDLERS_SIZE: usize = 96;

unsafe impl Send for HandlerHolder {}

/// Keep track of which handler is registered for which exception
static mut EXCEPTION_HANDLERS: [Option<HandlerHolder>; EXCEPTION_HANDLERS_SIZE] = [
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
];

unsafe fn internal_handle_exception(
    exception_code: ExceptionCode,
    exception_pointers: *mut EXCEPTION_POINTERS,
) -> i32 {
    let index = EXCEPTION_CODES_MAPPING
        .iter()
        .position(|x| *x == exception_code)
        .unwrap();
    if let Some(handler_holder) = unsafe { &EXCEPTION_HANDLERS[index] } {
        log::info!(
            "{:?}: Handling exception {}",
            std::process::id(),
            exception_code
        );
        let handler = unsafe { &mut **handler_holder.handler.get() };
        unsafe {
            handler.handle(exception_code, exception_pointers);
        }
        EXCEPTION_CONTINUE_EXECUTION
    } else {
        log::info!(
            "{:?}: No handler for exception {}",
            std::process::id(),
            exception_code
        );
        // Go to Default one
        let handler_holder = unsafe { &EXCEPTION_HANDLERS[EXCEPTION_HANDLERS_SIZE - 1] }
            .as_ref()
            .unwrap();
        let handler = unsafe { &mut **handler_holder.handler.get() };
        unsafe {
            handler.handle(exception_code, exception_pointers);
        }
        EXCEPTION_CONTINUE_SEARCH
    }
}

/// Function that is being called whenever an exception arrives (stdcall).
/// # Safety
/// This function is unsafe because it is called by the OS
pub unsafe extern "system" fn handle_exception(
    exception_pointers: *mut EXCEPTION_POINTERS,
) -> c_long {
    let code = unsafe {
        exception_pointers
            .as_mut()
            .unwrap()
            .ExceptionRecord
            .as_mut()
            .unwrap()
            .ExceptionCode
    };
    let exception_code = From::from(code.0);
    log::info!("Received exception; code: {exception_code}");
    unsafe { internal_handle_exception(exception_code, exception_pointers) }
}

/// Return `SIGIGN` this is 1 (when represented as u64)
/// Check `https://github.com/ziglang/zig/blob/956f53beb09c07925970453d4c178c6feb53ba70/lib/libc/include/any-windows-any/signal.h#L51`
/// # Safety
/// It is just casting into another type, nothing unsafe.
#[must_use]
pub const unsafe fn sig_ign() -> NativeSignalHandlerType {
    unsafe { core::mem::transmute(1_usize) }
}

type NativeSignalHandlerType = unsafe extern "C" fn(i32);
unsafe extern "C" {
    pub fn signal(signum: i32, func: NativeSignalHandlerType) -> *const c_void;
}

unsafe extern "C" fn handle_signal(_signum: i32) {
    // log::info!("Received signal {}", _signum);
    unsafe {
        internal_handle_exception(ExceptionCode::AssertionFailure, ptr::null_mut());
    }
}

/// Setup Win32 exception handlers in a somewhat rusty way.
///
/// # Safety
/// Exception handlers are usually ugly, handle with care!
#[cfg(feature = "alloc")]
pub unsafe fn setup_exception_handler<T: 'static + ExceptionHandler>(
    handler: *mut T,
) -> Result<(), Error> {
    let exceptions = unsafe { (*handler).exceptions() };
    let mut catch_assertions = false;
    for exception_code in exceptions {
        if exception_code == ExceptionCode::AssertionFailure {
            catch_assertions = true;
        }
        let index = EXCEPTION_CODES_MAPPING
            .iter()
            .position(|x| *x == exception_code)
            .unwrap();
        unsafe {
            write_volatile(
                &raw mut EXCEPTION_HANDLERS[index],
                Some(HandlerHolder {
                    handler: UnsafeCell::new(handler as *mut dyn ExceptionHandler),
                }),
            );
        }
    }

    unsafe {
        write_volatile(
            &raw mut (EXCEPTION_HANDLERS[EXCEPTION_HANDLERS_SIZE - 1]),
            Some(HandlerHolder {
                handler: UnsafeCell::new(handler as *mut dyn ExceptionHandler),
            }),
        );
    }
    compiler_fence(Ordering::SeqCst);
    if catch_assertions {
        unsafe {
            signal(SIGABRT, handle_signal);
        }
    }
    // SetUnhandledFilter does not work with frida since the stack is changed and exception handler is lost with Stalker enabled.
    // See https://github.com/AFLplusplus/LibAFL/pull/403
    unsafe {
        AddVectoredExceptionHandler(
            0,
            Some(core::mem::transmute::<
                *const core::ffi::c_void,
                unsafe extern "system" fn(*mut EXCEPTION_POINTERS) -> i32,
            >(handle_exception as *const c_void)),
        );
    }
    Ok(())
}

#[cfg(feature = "std")]
pub trait CtrlHandler {
    /// Handle an exception
    fn handle(&mut self, ctrl_type: u32) -> bool;
}

#[cfg(feature = "std")]
struct CtrlHandlerHolder {
    handler: UnsafeCell<*mut dyn CtrlHandler>,
}

/// Keep track of which handler is registered for which exception
#[cfg(feature = "std")]
static mut CTRL_HANDLER: Option<CtrlHandlerHolder> = None;

/// Set `ConsoleCtrlHandler` to catch Ctrl-C
///
/// # Safety
/// Same safety considerations as in `setup_exception_handler`
#[cfg(feature = "std")]
pub unsafe fn setup_ctrl_handler<T: 'static + CtrlHandler>(handler: *mut T) -> Result<(), Error> {
    unsafe {
        write_volatile(
            &raw mut (CTRL_HANDLER),
            Some(CtrlHandlerHolder {
                handler: UnsafeCell::new(handler as *mut dyn CtrlHandler),
            }),
        );
    }
    compiler_fence(Ordering::SeqCst);

    // Log the result of SetConsoleCtrlHandler
    let result = unsafe { SetConsoleCtrlHandler(Some(ctrl_handler), true) };
    match result {
        Ok(()) => {
            log::info!("SetConsoleCtrlHandler succeeded");
            Ok(())
        }
        Err(err) => {
            log::info!("SetConsoleCtrlHandler failed");
            Err(Error::from(err))
        }
    }
}

#[cfg(feature = "std")]
unsafe extern "system" fn ctrl_handler(ctrl_type: u32) -> BOOL {
    let handler = unsafe { ptr::read_volatile(&raw const (CTRL_HANDLER)) };
    match handler {
        Some(handler_holder) => {
            log::info!("{:?}: Handling ctrl {}", std::process::id(), ctrl_type);
            let handler = unsafe { &mut *handler_holder.handler.get() };
            if let Some(ctrl_handler) = unsafe { handler.as_mut() } {
                (*ctrl_handler).handle(ctrl_type).into()
            } else {
                false.into()
            }
        }
        None => false.into(),
    }
}
