//! Fast implementations for specific CPU architectures.

#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
use core::arch::asm;

#[cfg(not(any(
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "aarch64",
    target_arch = "arm",
    target_arch = "riscv64",
    target_arch = "riscv32"
)))]
use crate::current_nanos;

// TODO: Add more architectures, using C code, see
// https://github.com/google/benchmark/blob/master/src/cycleclock.h
// Or using llvm intrinsics (if they ever should become available in stable rust?)

/// Read a timestamp for measurements.
///
/// This function is a wrapper around different ways to get a timestamp, fast.
/// In this way, an experiment only has to
/// change this implementation rather than every instead of `read_time_counter`.
/// It is using `rdtsc` on `x86_64` and `x86`.
#[cfg(any(target_arch = "x86_64", target_arch = "x86"))]
#[must_use]
pub fn read_time_counter() -> u64 {
    #[cfg(target_arch = "x86_64")]
    unsafe {
        core::arch::x86_64::_rdtsc()
    }
    #[cfg(target_arch = "x86")]
    unsafe {
        core::arch::x86::_rdtsc()
    }
}

/// Read a timestamp for measurements
///
/// Fetches the counter-virtual count register
/// as we do not need to remove the `cntvct_el2` offset.
#[cfg(target_arch = "aarch64")]
#[must_use]
pub fn read_time_counter() -> u64 {
    let mut v: u64;
    unsafe {
        // TODO pushing a change in core::arch::aarch64 ?
        asm!("mrs {v}, cntvct_el0", v = out(reg) v);
    }
    v
}

/// Read a timestamp for measurements
#[cfg(target_arch = "arm")]
#[must_use]
pub fn read_time_counter() -> u64 {
    let mut v: u32;
    unsafe {
        asm!("mrc p15, 0, {v}, c9, c13, 0", v = out(reg) v);
    }
    u64::from(v)
}

/// Read a timestamp for measurements
///
/// Fetches the full 64 bits of the cycle counter in one instruction.
#[cfg(target_arch = "riscv64")]
#[must_use]
pub fn read_time_counter() -> u64 {
    let mut v: u64;
    unsafe {
        asm!("rdcycle {v}", v = out(reg) v);
    }
    v
}

/// Read a timestamp for measurements
///
/// Fetches the high 32 bits of the cycle counter, its low end
/// If the high part has changed, we branch again.
/// FIXME: see if the latter is overkill.
#[cfg(target_arch = "riscv32")]
#[must_use]
pub fn read_time_counter() -> u64 {
    let mut v: u64;
    let mut hg: u32;
    let mut lw: u32;
    let mut cmp: u32;
    unsafe {
        asm!("jmp%=:",
             "rdcycleh {hg}",
             "rdcycle {lw}",
             "rdcycleh {cmp}",
             "bne {hg}, {cmp}, jmp%=",
             hg = out(reg) hg,
             lw = out(reg) lw,
             cmp = out(reg) cmp);
        v = ((hg as u64) << 32) | lw as u64;
    }
    v
}

/// Read a timestamp for measurements.
///
/// This function is a wrapper around different ways to get a timestamp, fast.
/// In this way, an experiment only has to
/// change this implementation rather than every instead of [`read_time_counter`]
/// On unsupported architectures, it's falling back to normal system time, in millis.
#[cfg(not(any(
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "aarch64",
    target_arch = "arm",
    target_arch = "riscv64",
    target_arch = "riscv32"
)))]
#[must_use]
pub fn read_time_counter() -> u64 {
    current_nanos()
}
