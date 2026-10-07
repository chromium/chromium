# Fallback implementation based on disabling interrupts or critical-section

This module supports two different critical section implementations:

- Built-in "disable all interrupts".
  - On MSP430 and AVR, they are always single-core and has no unprivileged mode, so this is enabled by default.
  - On Armv6-M (thumbv6m), pre-v6 Arm (e.g., thumbv4t, thumbv5te), RISC-V without A-extension, and Xtensa, they could be multi-core or unprivileged mode, so this is enabled when the user explicitly declares that the system is single-core and that privileged instructions are available using `unsafe-assume-single-core` feature (or `portable_atomic_unsafe_assume_single_core` cfg).
- Call into the `critical-section` crate (which allows the user to plug any implementation).
  - This is enabled when the user asks for it with the [`critical-section` feature](../../../README.md#optional-features-critical-section).

`unsafe-assume-privileged` feature (`portable_atomic_unsafe_assume_privileged` cfg) also uses this module's interrupt disable implementation part.

The `unsafe-assume-single-core` implementation uses privileged instructions to disable interrupts, so it usually doesn't work on unprivileged mode.
Enabling this feature in an environment where privileged instructions are not available, or if the instructions used are not sufficient to disable interrupts in the system, it is also usually considered **unsound**, although the details are system-dependent.

Consider using the [`critical-section` feature](../../../README.md#optional-features-critical-section) for systems that cannot use the `unsafe-assume-single-core` feature (or `portable_atomic_unsafe_assume_single_core` cfg).

For some targets, the implementation can be changed by explicitly enabling features.

- On Arm M-Profile architectures, this disables interrupts by modifying the PRIMASK register.
- On Arm (except for M-Profile architectures), this disables interrupts by modifying the I (IRQ mask) bit of the CPSR.
- On Arm (except for M-Profile architectures) with the `disable-fiq` feature (or `portable_atomic_disable_fiq` cfg), this disables interrupts by modifying the I (IRQ mask) bit and F (FIQ mask) bit of the CPSR.
- On RISC-V, this disables interrupts by modifying the MIE (Machine Interrupt Enable) bit of the `mstatus` register.
- On RISC-V with the `s-mode` feature (or `portable_atomic_s_mode` cfg), this disables interrupts by modifying the SIE (Supervisor Interrupt Enable) bit of the `sstatus` register.
- On RISC-V with the `zaamo` target feature (or `force-amo` feature or `portable_atomic_force_amo` cfg), this uses AMO instructions for RMWs that have corresponding AMO instructions even if A-extension is disabled. For other RMWs, this disables interrupts as usual.
- On MSP430, this disables interrupts by modifying the GIE (Global Interrupt Enable) bit of the status register (SR).
- On AVR, this disables interrupts by modifying the I (Global Interrupt Enable) bit of the status register (SREG).
- On Xtensa, this disables interrupts by modifying the PS special register.

<a name="no-disable-interrupts"></a>Some operations don't require disabling interrupts:

- On architectures except for AVR: loads and stores with pointer size or smaller
- On AVR: 8-bit loads and stores
- On AVR with `rmw` target feature additionally: 8-bit `swap`
- On MSP430 additionally: {8,16}-bit `add,sub,and,or,xor,not`
- On RISC-V with the `zaamo` target feature (or `portable_atomic_target_feature="zaamo"` cfg or `force-amo` feature or `portable_atomic_force_amo` cfg) additionally: 32-bit(RV32)/{32,64}-bit(RV64) `swap,fetch_{add,sub,and,or,xor,not,max,min},add,sub,and,or,xor,not`, {8,16}-bit `fetch_{and,or,xor,not},and,or,xor,not`[^1], and all operations of `AtomicBool`

However, when the `critical-section` feature is enabled, critical sections are taken for all atomic operations.

Feel free to submit an issue if your target is not supported yet.

[^1]: With the `zabha` target feature, {8,16}-bit `swap,fetch_{add,sub,max,min},add,sub` too.
