// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Refs:
- Xtensa Instruction Set Architecture (ISA) Summary for all Xtensa LX Processors
  https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf
- Linux kernel's Xtensa atomic implementation
  https://github.com/torvalds/linux/blob/v6.19/arch/xtensa/include/asm/atomic.h

See tests/asm-test/asm/portable-atomic for generated assembly.
*/

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
    let ps: State;
    // SAFETY: reading the PS special register and disabling all interrupts is safe.
    // (see module-level comments of interrupt/mod.rs on the safety of using privileged instructions)
    //
    // Interrupt level 15 to disable all interrupts.
    // SYNC after RSIL is not required.
    // Use a14 per https://github.com/torvalds/linux/blob/v6.16/arch/xtensa/include/asm/atomic.h#L33-L37.
    unsafe {
        asm!(
            "rsil a14, 15", // a14 = PS; PS.INTLEVEL = 15
            out("a14") ps,
            // Do not use `nomem` and `readonly` because prevent subsequent memory accesses from being reordered before interrupts are disabled.
            options(nostack),
        );
    }
    ps
}

/// Restores the previous interrupt state.
///
/// # Safety
///
/// The state must be the one retrieved by the previous `disable`.
#[inline(always)]
pub(crate) unsafe fn restore(prev_ps: State) {
    // SAFETY: the caller must guarantee that the state was retrieved by the previous `disable`,
    // and we've checked that interrupts were enabled before disabling interrupts.
    //
    // SYNC after WSR is required to guarantee that subsequent RSIL read the written value.
    // See also 3.8.10 Processor Control Instructions of Xtensa Instruction Set Architecture (ISA) Summary for all Xtensa LX Processors.
    unsafe {
        asm!(
            "wsr.ps a14", // PS = a14
            "rsync",      // wait
            in("a14") prev_ps,
            // Do not use `nomem` and `readonly` because prevent preceding memory accesses from being reordered after interrupts are enabled.
            options(nostack),
        );
    }
}
