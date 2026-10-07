// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Refs:
- RISC-V Instruction Set Manual
  Machine Status (mstatus and mstatush) Registers
  https://docs.riscv.org/reference/isa/v20260120/priv/machine.html#3-1-1-6-machine-status-mstatus-and-mstatush-registers
  Supervisor Status (sstatus) Register
  https://docs.riscv.org/reference/isa/v20260120/priv/supervisor.html#sstatus

See also src/imp/riscv.rs.

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
pub(super) use super::super::riscv as atomic;

cfg_sel!({
    // Supervisor-mode (S-mode)
    #[cfg(portable_atomic_s_mode)]
    {
        // Status register
        macro_rules! status {
            () => {
                "sstatus"
            };
        }
        // SIE (Supervisor Interrupt Enable) bit (1 << 1)
        #[cfg(portable_atomic_s_mode)]
        macro_rules! mask {
            () => {
                "0x2"
            };
        }
        const MASK: State = 0x2;
    }
    // Machine-mode (M-mode)
    #[cfg(else)]
    {
        // Status register
        macro_rules! status {
            () => {
                "mstatus"
            };
        }
        // MIE (Machine Interrupt Enable) bit (1 << 3)
        macro_rules! mask {
            () => {
                "0x8"
            };
        }
        const MASK: State = 0x8;
    }
});

pub(crate) type State = crate::utils::RegSize;

/// Disables interrupts and returns the previous interrupt state.
#[inline(always)]
pub(crate) fn disable() -> State {
    let status: State;
    // SAFETY: reading mstatus/sstatus and disabling interrupts is safe.
    // (see module-level comments of interrupt/mod.rs on the safety of using privileged instructions)
    unsafe {
        asm!(
            concat!("csrrci {status}, ", status!(), ", ", mask!()), // atomic { status = status!(); status!() &= !mask!() }
            status = out(reg) status,
            // Do not use `nomem` and `readonly` because prevent subsequent memory accesses from being reordered before interrupts are disabled.
            options(nostack, preserves_flags),
        );
    }
    status
}

/// Restores the previous interrupt state.
///
/// # Safety
///
/// The state must be the one retrieved by the previous `disable`.
#[inline(always)]
pub(crate) unsafe fn restore(prev_status: State) {
    // SAFETY: the caller must guarantee that the state was retrieved by the previous `disable`,
    //
    // This avoids clobbering other fields in the mstatus/sstatus register.
    unsafe {
        asm!(
            concat!("csrs ", status!(), ", {mask}"), // atomic { status!() |= mask }
            mask = in(reg) prev_status & MASK,
            // Do not use `nomem` and `readonly` because prevent preceding memory accesses from being reordered after interrupts are enabled.
            options(nostack, preserves_flags),
        );
    }
}
