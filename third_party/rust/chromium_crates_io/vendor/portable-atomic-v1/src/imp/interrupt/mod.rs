// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Fallback implementation based on disabling interrupts or critical-section

- mod.rs contains critical section based fallback implementations.
- Each architecture modules contain implementations of disabling interrupts.

See README.md of this directory for details.
*/

#[cfg(not(feature = "critical-section"))]
#[cfg_attr(
    all(
        target_arch = "arm",
        any(target_feature = "mclass", portable_atomic_target_feature = "mclass"),
    ),
    path = "armv6m.rs"
)]
#[cfg_attr(
    all(
        target_arch = "arm",
        not(any(target_feature = "mclass", portable_atomic_target_feature = "mclass")),
    ),
    path = "armv4t.rs"
)]
#[cfg_attr(target_arch = "avr", path = "avr.rs")]
#[cfg_attr(target_arch = "msp430", path = "msp430.rs")]
#[cfg_attr(any(target_arch = "riscv32", target_arch = "riscv64"), path = "riscv.rs")]
#[cfg_attr(target_arch = "xtensa", path = "xtensa.rs")]
pub(super) mod arch;

#[cfg(not(all(
    portable_atomic_unsafe_assume_privileged,
    not(portable_atomic_unsafe_assume_single_core),
)))]
items!({
    use core::{cell::UnsafeCell, sync::atomic::Ordering};

    // critical-section implementations might use locks internally.
    #[cfg(feature = "critical-section")]
    const IS_ALWAYS_LOCK_FREE: bool = false;
    // Consider atomic operations based on disabling interrupts on single-core
    // systems are lock-free. (We consider the pre-v6 Arm Linux's atomic operations
    // provided in a similar way by the Linux kernel to be lock-free.)
    #[cfg(not(feature = "critical-section"))]
    const IS_ALWAYS_LOCK_FREE: bool = true;

    // Put this in its own module to prevent guard creation.
    use self::guard::disable;
    mod guard {
        // Note: The caller must NOT explicitly modify registers containing fields modified by disable/restore.
        //       (Fields modified as side effects of other operations are covered by the absence of preserves_flags,
        //        so they are fine -- see msp430.rs for more.)
        #[inline(always)]
        pub(super) fn disable() -> Guard {
            Guard {
                #[cfg(feature = "critical-section")]
                // SAFETY: the state will be restored in the subsequent `release`.
                state: unsafe { critical_section::acquire() },
                #[cfg(not(feature = "critical-section"))]
                // Get current interrupt state and disable interrupts.
                state: super::arch::disable(),
            }
        }
        pub(super) struct Guard {
            #[cfg(feature = "critical-section")]
            state: critical_section::RestoreState,
            #[cfg(not(feature = "critical-section"))]
            state: super::arch::State,
        }
        impl Drop for Guard {
            #[inline(always)]
            fn drop(&mut self) {
                #[cfg(feature = "critical-section")]
                // SAFETY: the state was retrieved by the previous `acquire`.
                unsafe {
                    critical_section::release(self.state);
                }
                #[cfg(not(feature = "critical-section"))]
                // Restore interrupt state.
                // SAFETY: the state was retrieved by the previous `disable`.
                unsafe {
                    super::arch::restore(self.state);
                }
            }
        }
    }

    macro_rules! atomic_base {
        (base, $([$($generics:tt)*])? $atomic_type:ident, $value_type:ty, $align:literal) => {
            #[repr(C, align($align))]
            pub(crate) struct $atomic_type $(<$($generics)*>)? {
                v: UnsafeCell<$value_type>,
            }

            // Send is implicitly implemented for atomic integers, but not for atomic pointers.
            // SAFETY: any data races are prevented by disabling interrupts (or
            // atomic intrinsics) or critical-section (see module-level comments).
            unsafe impl $(<$($generics)*>)? Send for $atomic_type $(<$($generics)*>)? {}
            // SAFETY: any data races are prevented by disabling interrupts (or
            // atomic intrinsics) or critical-section (see module-level comments).
            unsafe impl $(<$($generics)*>)? Sync for $atomic_type $(<$($generics)*>)? {}

            impl $(<$($generics)*>)? $atomic_type $(<$($generics)*>)? {
                #[inline]
                pub(crate) const fn new(v: $value_type) -> Self {
                    Self { v: UnsafeCell::new(v) }
                }

                #[inline]
                pub(crate) fn is_lock_free() -> bool {
                    Self::IS_ALWAYS_LOCK_FREE
                }
                pub(crate) const IS_ALWAYS_LOCK_FREE: bool = IS_ALWAYS_LOCK_FREE;

                #[inline]
                fn read(&self, _guard: &guard::Guard) -> $value_type {
                    // SAFETY: any data races are prevented by disabling interrupts or critical-section (see
                    // module-level comments) and the raw pointer is valid because we got it
                    // from a reference.
                    unsafe { self.v.get().read() }
                }
                #[inline]
                fn write(&self, val: $value_type, _guard: &guard::Guard) {
                    // SAFETY: any data races are prevented by disabling interrupts or critical-section (see
                    // module-level comments) and the raw pointer is valid because we got it
                    // from a reference.
                    unsafe { self.v.get().write(val) }
                }

                // As for CAS, there is no corresponding atomic operation on all architectures that use this code.
                // (If the CAS instruction exists, all atomic operations can be implemented by it, so this code will not be used.)
                #[inline]
                #[cfg_attr(
                    all(debug_assertions, not(portable_atomic_no_track_caller)),
                    track_caller
                )]
                pub(crate) fn compare_exchange(
                    &self,
                    current: $value_type,
                    new: $value_type,
                    success: Ordering,
                    failure: Ordering,
                ) -> Result<$value_type, $value_type> {
                    crate::utils::assert_compare_exchange_ordering(success, failure);
                    let guard = disable();
                    let prev = self.read(&guard);
                    if prev == current {
                        self.write(new, &guard);
                        Ok(prev)
                    } else {
                        Err(prev)
                    }
                }
                #[inline]
                #[cfg_attr(
                    all(debug_assertions, not(portable_atomic_no_track_caller)),
                    track_caller
                )]
                pub(crate) fn compare_exchange_weak(
                    &self,
                    current: $value_type,
                    new: $value_type,
                    success: Ordering,
                    failure: Ordering,
                ) -> Result<$value_type, $value_type> {
                    self.compare_exchange(current, new, success, failure)
                }

                #[inline]
                pub(crate) const fn as_ptr(&self) -> *mut $value_type {
                    self.v.get()
                }
            }
        };
        (native_load_store, $([$($generics:tt)*])? $atomic_type:ident, $value_type:ty) => {
            impl $(<$($generics)*>)? core::ops::Deref for $atomic_type $(<$($generics)*>)? {
                type Target = atomic::$atomic_type $(<$($generics)*>)?;
                #[inline(always)]
                fn deref(&self) -> &Self::Target {
                    // SAFETY: $atomic_type and atomic::$atomic_type have the same layout and
                    // guarantee atomicity in a compatible way. (see module-level comments)
                    unsafe {
                        &*(self as *const Self as *const atomic::$atomic_type $(<$($generics)*>)?)
                    }
                }
            }
        };
        (emulate_load_store, $([$($generics:tt)*])? $atomic_type:ident, $value_type:ty) => {
            impl $(<$($generics)*>)? $atomic_type $(<$($generics)*>)? {
                #[inline]
                #[cfg_attr(
                    all(debug_assertions, not(portable_atomic_no_track_caller)),
                    track_caller
                )]
                pub(crate) fn load(&self, order: Ordering) -> $value_type {
                    crate::utils::assert_load_ordering(order);
                    let guard = disable();
                    self.read(&guard)
                }
                #[inline]
                #[cfg_attr(
                    all(debug_assertions, not(portable_atomic_no_track_caller)),
                    track_caller
                )]
                pub(crate) fn store(&self, val: $value_type, order: Ordering) {
                    crate::utils::assert_store_ordering(order);
                    let guard = disable();
                    self.write(val, &guard);
                }
            }
        };
        (emulate_swap, $([$($generics:tt)*])? $atomic_type:ident, $value_type:ty) => {
            impl $(<$($generics)*>)? $atomic_type $(<$($generics)*>)? {
                #[inline]
                pub(crate) fn swap(&self, val: $value_type, _order: Ordering) -> $value_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(val, &guard);
                    prev
                }
            }
        };
    }

    #[cfg_attr(
        portable_atomic_no_cfg_target_has_atomic,
        cfg(any(
            test,
            target_arch = "avr",
            target_arch = "msp430",
            portable_atomic_no_atomic_cas
        ))
    )]
    #[cfg_attr(
        not(portable_atomic_no_cfg_target_has_atomic),
        cfg(any(
            test,
            target_arch = "avr",
            target_arch = "msp430",
            not(target_has_atomic = "ptr")
        ))
    )]
    items!({
        #[cfg(target_pointer_width = "16")]
        atomic_base!(base, [T] AtomicPtr, *mut T, 2);
        #[cfg(target_pointer_width = "32")]
        atomic_base!(base, [T] AtomicPtr, *mut T, 4);
        #[cfg(target_pointer_width = "64")]
        atomic_base!(base, [T] AtomicPtr, *mut T, 8);
        #[cfg(target_pointer_width = "128")]
        atomic_base!(base, [T] AtomicPtr, *mut T, 16);

        impl_default_bit_opts!(AtomicPtr, usize);

        cfg_sel!({
            #[cfg(any(target_arch = "avr", feature = "critical-section"))]
            {
                atomic_base!(emulate_load_store, [T] AtomicPtr, *mut T);
            }
            #[cfg(else)]
            {
                atomic_base!(native_load_store, [T] AtomicPtr, *mut T);
            }
        });

        #[cfg(not(all(
            any(target_arch = "riscv32", target_arch = "riscv64"),
            not(feature = "critical-section"),
            any(
                portable_atomic_force_amo,
                target_feature = "zaamo",
                portable_atomic_target_feature = "zaamo",
            ),
        )))]
        items!({
            atomic_base!(emulate_swap, [T] AtomicPtr, *mut T);
            impl<T> AtomicPtr<T> {
                #[inline]
                pub(crate) fn fetch_byte_add(&self, val: usize, _order: Ordering) -> *mut T {
                    #[cfg(portable_atomic_no_strict_provenance)]
                    use crate::utils::ptr::PtrExt as _;
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev.with_addr(prev.addr().wrapping_add(val)), &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_byte_sub(&self, val: usize, _order: Ordering) -> *mut T {
                    #[cfg(portable_atomic_no_strict_provenance)]
                    use crate::utils::ptr::PtrExt as _;
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev.with_addr(prev.addr().wrapping_sub(val)), &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_and(&self, val: usize, _order: Ordering) -> *mut T {
                    #[cfg(portable_atomic_no_strict_provenance)]
                    use crate::utils::ptr::PtrExt as _;
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev.with_addr(prev.addr() & val), &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_or(&self, val: usize, _order: Ordering) -> *mut T {
                    #[cfg(portable_atomic_no_strict_provenance)]
                    use crate::utils::ptr::PtrExt as _;
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev.with_addr(prev.addr() | val), &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_xor(&self, val: usize, _order: Ordering) -> *mut T {
                    #[cfg(portable_atomic_no_strict_provenance)]
                    use crate::utils::ptr::PtrExt as _;
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev.with_addr(prev.addr() ^ val), &guard);
                    prev
                }
            }
        });

        impl<T> AtomicPtr<T> {
            #[cfg(test)]
            #[inline]
            fn fetch_ptr_add(&self, val: usize, order: Ordering) -> *mut T {
                self.fetch_byte_add(val.wrapping_mul(core::mem::size_of::<T>()), order)
            }
            #[cfg(test)]
            #[inline]
            fn fetch_ptr_sub(&self, val: usize, order: Ordering) -> *mut T {
                self.fetch_byte_sub(val.wrapping_mul(core::mem::size_of::<T>()), order)
            }
        }
    });

    macro_rules! atomic_int {
        (base, $atomic_type:ident, $int_type:ty, $align:literal) => {
            atomic_base!(base, $atomic_type, $int_type, $align);
            // As for nand and neg, there is no corresponding atomic operation on all architectures that use this code.
            impl $atomic_type {
                #[inline]
                pub(crate) fn fetch_nand(&self, val: $int_type, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(!(prev & val), &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_neg(&self, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev.wrapping_neg(), &guard);
                    prev
                }
                #[inline]
                pub(crate) fn neg(&self, order: Ordering) {
                    self.fetch_neg(order);
                }
            }
        };
        (load_store_atomic $([$kind:ident])?, $atomic_type:ident, $int_type:ty, $align:literal) => {
            cfg_sel!({
                #[cfg(feature = "critical-section")]
                {
                    atomic_int!(all_critical_session, $atomic_type, $int_type, $align);
                }
                #[cfg(else)]
                {
                    atomic_int!(base, $atomic_type, $int_type, $align);
                    impl_default_bit_opts!($atomic_type, $int_type);
                    // load/store
                    cfg_sel!({
                        // AVR with very old rustc
                        #[cfg(all(target_arch = "avr", portable_atomic_no_asm))]
                        {
                            atomic_base!(emulate_load_store, $atomic_type, $int_type);
                        }
                        #[cfg(else)]
                        {
                            atomic_base!(native_load_store, $atomic_type, $int_type);
                        }
                    });
                    // RMW
                    cfg_sel!({
                        // AVR 8-bit RMW with RMW instructions
                        #[cfg(all(
                            target_arch = "avr",
                            not(portable_atomic_no_asm),
                            any(target_feature = "rmw", portable_atomic_target_feature = "rmw"),
                        ))]
                        {
                            atomic_int!(emulate_arithmetic, $atomic_type, $int_type);
                        }
                        // RISC-V RMW with Zaamo extension
                        #[cfg(all(
                            any(target_arch = "riscv32", target_arch = "riscv64"),
                            any(
                                portable_atomic_force_amo,
                                target_feature = "zaamo",
                                portable_atomic_target_feature = "zaamo",
                            ),
                        ))]
                        {
                            atomic_int!(cas $([$kind])?, $atomic_type, $int_type);
                        }
                        #[cfg(else)]
                        {
                            atomic_int!(cas[emulate], $atomic_type, $int_type);
                        }
                    });
                    // RMW (no-fetch)
                    #[cfg(not(target_arch = "msp430"))]
                    items!({
                        impl_default_no_fetch_ops!($atomic_type, $int_type);
                        impl $atomic_type {
                            #[inline]
                            pub(crate) fn not(&self, order: Ordering) {
                                self.fetch_not(order);
                            }
                        }
                    });
                }
            });
        };
        (all_critical_session, $atomic_type:ident, $int_type:ty, $align:literal) => {
            atomic_int!(base, $atomic_type, $int_type, $align);
            atomic_base!(emulate_load_store, $atomic_type, $int_type);
            atomic_int!(cas[emulate], $atomic_type, $int_type);
            impl_default_no_fetch_ops!($atomic_type, $int_type);
            impl_default_bit_opts!($atomic_type, $int_type);
            impl $atomic_type {
                #[inline]
                pub(crate) fn not(&self, order: Ordering) {
                    self.fetch_not(order);
                }
            }
        };
        (emulate_arithmetic, $atomic_type:ident, $int_type:ty) => {
            impl $atomic_type {
                #[inline]
                pub(crate) fn fetch_add(&self, val: $int_type, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev.wrapping_add(val), &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_sub(&self, val: $int_type, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev.wrapping_sub(val), &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_max(&self, val: $int_type, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(core::cmp::max(prev, val), &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_min(&self, val: $int_type, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(core::cmp::min(prev, val), &guard);
                    prev
                }
            }
        };
        (emulate_bit, $atomic_type:ident, $int_type:ty) => {
            impl $atomic_type {
                #[inline]
                pub(crate) fn fetch_and(&self, val: $int_type, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev & val, &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_or(&self, val: $int_type, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev | val, &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_xor(&self, val: $int_type, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(prev ^ val, &guard);
                    prev
                }
                #[inline]
                pub(crate) fn fetch_not(&self, _order: Ordering) -> $int_type {
                    let guard = disable();
                    let prev = self.read(&guard);
                    self.write(!prev, &guard);
                    prev
                }
            }
        };
        (cas[emulate], $atomic_type:ident, $int_type:ty) => {
            atomic_base!(emulate_swap, $atomic_type, $int_type);
            atomic_int!(emulate_arithmetic, $atomic_type, $int_type);
            atomic_int!(emulate_bit, $atomic_type, $int_type);
        };
        // RISC-V 32-bit(RV32)/{32,64}-bit(RV64) RMW with Zaamo extension
        // RISC-V 8-bit/16-bit RMW with Zabha extension
        (cas, $atomic_type:ident, $int_type:ty) => {};
        // RISC-V 8-bit/16-bit RMW with Zaamo extension
        (cas[sub_word], $atomic_type:ident, $int_type:ty) => {
            // RISC-V 8-bit/16-bit RMW with Zaamo+Zabha extension
            #[cfg(any(target_feature = "zabha", portable_atomic_target_feature = "zabha"))]
            atomic_int!(cas, $atomic_type, $int_type);

            // RISC-V 8-bit/16-bit RMW with Zaamo extension
            #[cfg(not(any(target_feature = "zabha", portable_atomic_target_feature = "zabha")))]
            atomic_base!(emulate_swap, $atomic_type, $int_type);
            #[cfg(not(any(target_feature = "zabha", portable_atomic_target_feature = "zabha")))]
            atomic_int!(emulate_arithmetic, $atomic_type, $int_type);
        };
    }

    #[cfg_attr(
        portable_atomic_no_cfg_target_has_atomic,
        cfg(any(
            test,
            target_arch = "avr",
            target_arch = "msp430",
            portable_atomic_no_atomic_cas,
        ))
    )]
    #[cfg_attr(
        not(portable_atomic_no_cfg_target_has_atomic),
        cfg(any(
            test,
            target_arch = "avr",
            target_arch = "msp430",
            not(target_has_atomic = "ptr"),
        ))
    )]
    items!({
        // On some platforms, atomic load/store can be implemented in a more efficient
        // way than disabling interrupts. On MSP430, some RMWs that do not return the
        // previous value can also be optimized.
        //
        // Note: On single-core systems, it is okay to use critical session-based
        // CAS together with atomic load/store. The load/store will not be
        // called while interrupts are disabled, and since the load/store is
        // atomic, it is not affected by interrupts even if interrupts are enabled.
        #[cfg(not(any(
            all(target_arch = "avr", portable_atomic_no_asm),
            feature = "critical-section",
        )))]
        use self::arch::atomic;

        #[cfg(target_pointer_width = "16")]
        #[cfg(not(target_arch = "avr"))]
        atomic_int!(load_store_atomic, AtomicIsize, isize, 2);
        #[cfg(target_pointer_width = "16")]
        #[cfg(not(target_arch = "avr"))]
        atomic_int!(load_store_atomic, AtomicUsize, usize, 2);
        #[cfg(target_arch = "avr")]
        atomic_int!(all_critical_session, AtomicIsize, isize, 2);
        #[cfg(target_arch = "avr")]
        atomic_int!(all_critical_session, AtomicUsize, usize, 2);
        #[cfg(target_pointer_width = "32")]
        atomic_int!(load_store_atomic, AtomicIsize, isize, 4);
        #[cfg(target_pointer_width = "32")]
        atomic_int!(load_store_atomic, AtomicUsize, usize, 4);
        #[cfg(target_pointer_width = "64")]
        atomic_int!(load_store_atomic, AtomicIsize, isize, 8);
        #[cfg(target_pointer_width = "64")]
        atomic_int!(load_store_atomic, AtomicUsize, usize, 8);
        #[cfg(target_pointer_width = "128")]
        atomic_int!(load_store_atomic, AtomicIsize, isize, 16);
        #[cfg(target_pointer_width = "128")]
        atomic_int!(load_store_atomic, AtomicUsize, usize, 16);

        #[cfg(not(all(target_arch = "avr", portable_atomic_no_asm)))]
        atomic_int!(load_store_atomic[sub_word], AtomicI8, i8, 1);
        #[cfg(not(all(target_arch = "avr", portable_atomic_no_asm)))]
        atomic_int!(load_store_atomic[sub_word], AtomicU8, u8, 1);
        #[cfg(all(target_arch = "avr", portable_atomic_no_asm))]
        atomic_int!(all_critical_session, AtomicI8, i8, 1);
        #[cfg(all(target_arch = "avr", portable_atomic_no_asm))]
        atomic_int!(all_critical_session, AtomicU8, u8, 1);
        #[cfg(not(target_arch = "avr"))]
        atomic_int!(load_store_atomic[sub_word], AtomicI16, i16, 2);
        #[cfg(not(target_arch = "avr"))]
        atomic_int!(load_store_atomic[sub_word], AtomicU16, u16, 2);
        #[cfg(target_arch = "avr")]
        atomic_int!(all_critical_session, AtomicI16, i16, 2);
        #[cfg(target_arch = "avr")]
        atomic_int!(all_critical_session, AtomicU16, u16, 2);

        #[cfg(not(target_pointer_width = "16"))]
        atomic_int!(load_store_atomic, AtomicI32, i32, 4);
        #[cfg(not(target_pointer_width = "16"))]
        atomic_int!(load_store_atomic, AtomicU32, u32, 4);

        cfg_has_fast_atomic_64!({
            atomic_int!(load_store_atomic, AtomicI64, i64, 8);
            atomic_int!(load_store_atomic, AtomicU64, u64, 8);
        });
    });

    // Double or more width atomics (require fallback feature for consistency with other situations).
    #[cfg(target_pointer_width = "16")]
    #[cfg(any(test, feature = "fallback"))]
    items!({
        atomic_int!(all_critical_session, AtomicI32, i32, 4);
        atomic_int!(all_critical_session, AtomicU32, u32, 4);
    });
    #[cfg(any(
        test,
        all(
            feature = "fallback",
            not(all(
                target_arch = "riscv32",
                not(any(miri, portable_atomic_sanitize_thread)),
                any(not(portable_atomic_no_asm), portable_atomic_unstable_asm),
                any(
                    target_feature = "zacas",
                    portable_atomic_target_feature = "zacas",
                    all(
                        feature = "fallback",
                        not(portable_atomic_no_outline_atomics),
                        any(target_os = "linux", target_os = "android"),
                    ),
                ),
            )),
        ),
    ))]
    #[cfg_attr(
        portable_atomic_no_cfg_target_has_atomic,
        cfg(any(test, portable_atomic_no_atomic_64))
    )]
    #[cfg_attr(
        not(portable_atomic_no_cfg_target_has_atomic),
        cfg(any(test, not(target_has_atomic = "64")))
    )]
    cfg_no_fast_atomic_64!({
        atomic_int!(all_critical_session, AtomicI64, i64, 8);
        atomic_int!(all_critical_session, AtomicU64, u64, 8);
    });
    #[cfg(any(
        test,
        all(
            feature = "fallback",
            not(all(
                target_arch = "riscv64",
                not(any(miri, portable_atomic_sanitize_thread)),
                any(not(portable_atomic_no_asm), portable_atomic_unstable_asm),
                any(
                    target_feature = "zacas",
                    portable_atomic_target_feature = "zacas",
                    all(
                        feature = "fallback",
                        not(portable_atomic_no_outline_atomics),
                        any(target_os = "linux", target_os = "android"),
                    ),
                ),
            )),
        ),
    ))]
    items!({
        atomic_int!(all_critical_session, AtomicI128, i128, 16);
        atomic_int!(all_critical_session, AtomicU128, u128, 16);
    });
});

#[cfg(test)]
mod tests {
    use super::*;

    test_atomic_ptr_single_thread!();
    test_atomic_int_single_thread!(i8);
    test_atomic_int_single_thread!(u8);
    test_atomic_int_single_thread!(i16);
    test_atomic_int_single_thread!(u16);
    test_atomic_int_single_thread!(i32);
    test_atomic_int_single_thread!(u32);
    test_atomic_int_single_thread!(i64);
    test_atomic_int_single_thread!(u64);
    test_atomic_int_single_thread!(i128);
    test_atomic_int_single_thread!(u128);
    test_atomic_int_single_thread!(isize);
    test_atomic_int_single_thread!(usize);
}
