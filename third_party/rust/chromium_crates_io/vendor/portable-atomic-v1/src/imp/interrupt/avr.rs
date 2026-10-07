// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Adapted from https://github.com/Rahix/avr-device.

See also src/imp/avr.rs.

Refs:
- AVR® Instruction Set Manual, Rev. DS40002198B
  https://ww1.microchip.com/downloads/en/DeviceDoc/AVR-InstructionSet-Manual-DS40002198.pdf

See tests/asm-test/asm/portable-atomic for generated assembly.
*/

#[cfg(not(portable_atomic_no_asm))]
use core::arch::asm;

#[cfg(not(portable_atomic_no_asm))]
#[cfg_attr(
    portable_atomic_no_cfg_target_has_atomic,
    cfg(any(test, portable_atomic_no_atomic_cas))
)]
#[cfg_attr(
    not(portable_atomic_no_cfg_target_has_atomic),
    cfg(any(test, not(target_has_atomic = "ptr")))
)]
pub(super) use super::super::avr as atomic;

pub(crate) type State = u8;

/// Disables interrupts and returns the previous interrupt state.
#[inline(always)]
pub(crate) fn disable() -> State {
    let sreg: State;
    // SAFETY: reading the status register (SREG) and disabling interrupts are safe.
    // (see module-level comments of interrupt/mod.rs on the safety of using privileged instructions)
    unsafe {
        // Refs: https://ww1.microchip.com/downloads/en/DeviceDoc/AVR-InstructionSet-Manual-DS40002198.pdf#page=58
        #[cfg(not(portable_atomic_no_asm))]
        asm!(
            "in {sreg}, 0x3F", // sreg = SREG
            "cli",             // SREG.I = 0
            sreg = out(reg) sreg,
            // Do not use `nomem` and `readonly` because prevent subsequent memory accesses from being reordered before interrupts are disabled.
            // Do not use `preserves_flags` because CLI modifies the I bit of the status register (SREG).
            options(nostack),
        );
        #[cfg(portable_atomic_no_asm)]
        {
            llvm_asm!("in $0, 0x3F" : "=r"(sreg) ::: "volatile");
            llvm_asm!("cli" ::: "memory" : "volatile");
        }
    }
    sreg
}

/// Restores the previous interrupt state.
///
/// # Safety
///
/// The state must be the one retrieved by the previous `disable`.
#[inline(always)]
pub(crate) unsafe fn restore(prev_sreg: State) {
    // SAFETY: the caller must guarantee that the state was retrieved by the previous `disable`.
    //
    // This clobbers the entire status register, but all other flags are the arithmetic flags that
    // are changed as a side effect of arithmetic operations, etc., which LLVM recognizes unless
    // preserves_flags is set, so it is safe to clobber them here.
    // See also the discussion at https://github.com/taiki-e/portable-atomic/pull/40.
    unsafe {
        #[cfg(not(portable_atomic_no_asm))]
        asm!(
            "out 0x3F, {prev_sreg}", // SREG = prev_sreg
            prev_sreg = in(reg) prev_sreg,
            // Do not use `nomem` and `readonly` because prevent preceding memory accesses from being reordered after interrupts are enabled.
            // Do not use `preserves_flags` because OUT modifies the status register (SREG).
            options(nostack),
        );
        #[cfg(portable_atomic_no_asm)]
        llvm_asm!("out 0x3F, $0" :: "r"(prev_sreg) : "memory" : "volatile");
    }
}
