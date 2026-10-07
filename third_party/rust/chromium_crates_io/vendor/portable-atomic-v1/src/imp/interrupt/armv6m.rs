// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Arm M-Profile Architectures

Adapted from https://github.com/rust-embedded/cortex-m.

Refs: https://developer.arm.com/documentation/ddi0419/c/System-Level-Architecture/System-Level-Programmers--Model/Registers/The-special-purpose-mask-register--PRIMASK

See tests/asm-test/asm/portable-atomic for generated assembly.
*/

#[cfg(not(portable_atomic_no_asm))]
use core::arch::asm;
#[cfg_attr(
    portable_atomic_no_cfg_target_has_atomic,
    cfg(any(test, portable_atomic_no_atomic_cas))
)]
#[cfg_attr(
    not(portable_atomic_no_cfg_target_has_atomic),
    cfg(any(test, not(target_has_atomic = "ptr")))
)]
pub(super) use core::sync::atomic;

pub(crate) type State = u32;

/// Disables interrupts and returns the previous interrupt state.
#[inline(always)]
pub(crate) fn disable() -> State {
    let primask: State;
    // SAFETY: reading the priority mask register and disabling interrupts are safe.
    // (see module-level comments of interrupt/mod.rs on the safety of using privileged instructions)
    unsafe {
        asm!(
            "mrs {primask}, PRIMASK", // primask = PRIMASK
            "cpsid i",                // PRIMASK.PM = 1
            primask = out(reg) primask,
            // Do not use `nomem` and `readonly` because prevent subsequent memory accesses from being reordered before interrupts are disabled.
            options(nostack, preserves_flags),
        );
    }
    primask
}

/// Restores the previous interrupt state.
///
/// # Safety
///
/// The state must be the one retrieved by the previous `disable`.
#[inline(always)]
pub(crate) unsafe fn restore(prev_primask: State) {
    // SAFETY: the caller must guarantee that the state was retrieved by the previous `disable`,
    // and we've checked that interrupts were enabled before disabling interrupts.
    //
    // This clobbers the entire PRIMASK register. See msp430.rs for safety on this.
    unsafe {
        asm!(
            "msr PRIMASK, {prev_primask}", // PRIMASK = prev_primask
            prev_primask = in(reg) prev_primask,
            // Do not use `nomem` and `readonly` because prevent preceding memory accesses from being reordered after interrupts are enabled.
            options(nostack, preserves_flags),
        );
    }
}
