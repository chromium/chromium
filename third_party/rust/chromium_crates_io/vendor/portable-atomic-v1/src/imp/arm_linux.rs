// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Atomic operations implementation on pre-v6 ARM Linux/Android.

This module optimizes cases where LLVM code generation is not optimal,
and provides workaround for core::sync::atomic bug (https://github.com/rust-lang/compiler-builtins/pull/1234).

See "Atomic operation overview by architecture" in atomic-maybe-uninit for a more comprehensive and
detailed description of the atomic and synchronize instructions in this architecture:
https://github.com/taiki-e/atomic-maybe-uninit/blob/HEAD/src/arch/README.md#arm

Refs:
- https://github.com/torvalds/linux/blob/v6.19/Documentation/arch/arm/kernel_user_helpers.rst
- https://github.com/rust-lang/compiler-builtins/blob/compiler_builtins-v0.1.124/src/arm_linux.rs
- https://github.com/taiki-e/atomic-maybe-uninit

Note: __kuser_memory_barrier is always SeqCst.
https://github.com/taiki-e/atomic-maybe-uninit/blob/v0.3.21/src/arch/arm.rs#L215

Note: On Miri and ThreadSanitizer which do not go through compiler-builtins, we don't use
this module and use normal core::sync::atomic instead.
*/

use core::{mem, sync::atomic::Ordering};

// https://github.com/torvalds/linux/blob/v6.19/Documentation/arch/arm/kernel_user_helpers.rst
// __kuser_helper_version >= 3 (kernel version 2.6.15+)
const KUSER_MEMORY_BARRIER: usize = 0xFFFF0FA0;
#[inline]
fn __kuser_memory_barrier() {
    // SAFETY: kernel docs specify a known address with the given signature.
    // core assumes __kuser_helper_version >= 3.
    unsafe {
        let f: extern "C" fn() =
            mem::transmute(crate::utils::ptr::with_exposed_provenance::<()>(KUSER_MEMORY_BARRIER));
        f();
    }
}

/// An atomic fence.
///
/// # Panics
///
/// Panics if `order` is [`Relaxed`](Ordering::Relaxed).
#[cfg_attr(doc, allow(dead_code))]
#[inline]
#[cfg_attr(any(all(debug_assertions, not(portable_atomic_no_track_caller)), miri), track_caller)]
pub fn fence(order: Ordering) {
    // Call __kuser_memory_barrier without going through __sync_synchronize.
    match order {
        Ordering::Relaxed => panic!("there is no such thing as a relaxed fence"),
        _ => __kuser_memory_barrier(),
    }
}

cfg_core_atomic!({
    use super::core_atomic::{
        AtomicI8, AtomicI16, AtomicI32, AtomicIsize, AtomicPtr, AtomicU8, AtomicU16, AtomicU32,
        AtomicUsize,
    };
    macro_rules! atomic {
        ($([$($generics:tt)*])? $atomic_type:ident, $value_type:ty) => {
            impl $(<$($generics)*>)? $atomic_type $(<$($generics)*>)? {
                #[inline]
                #[cfg_attr(
                    any(all(debug_assertions, not(portable_atomic_no_track_caller)), miri),
                    track_caller
                )]
                pub(crate) fn load(&self, order: Ordering) -> $value_type {
                    crate::utils::assert_load_ordering(order);
                    match order {
                        Ordering::Relaxed => self.inner.load(Ordering::Relaxed),
                        // Acquire and SeqCst loads are equivalent.
                        // This matches with LLVM, but GCC emits `dmb; ldr*; dmb` for SeqCst load.
                        Ordering::Acquire | Ordering::SeqCst => {
                            let out = self.inner.load(Ordering::Relaxed);
                            __kuser_memory_barrier();
                            out
                        }
                        _ => unreachable!(),
                    }
                }
                #[inline]
                #[cfg_attr(
                    any(all(debug_assertions, not(portable_atomic_no_track_caller)), miri),
                    track_caller
                )]
                pub(crate) fn store(&self, val: $value_type, order: Ordering) {
                    crate::utils::assert_store_ordering(order);
                    match order {
                        Ordering::Relaxed => self.inner.store(val, Ordering::Relaxed),
                        Ordering::Release => {
                            __kuser_memory_barrier();
                            self.inner.store(val, Ordering::Relaxed);
                        }
                        Ordering::SeqCst => {
                            __kuser_memory_barrier();
                            self.inner.store(val, Ordering::Relaxed);
                            __kuser_memory_barrier();
                        }
                        _ => unreachable!(),
                    }
                }
            }
            #[cfg_attr(
                portable_atomic_no_cfg_target_has_atomic,
                cfg(not(portable_atomic_no_atomic_cas))
            )]
            #[cfg_attr(
                not(portable_atomic_no_cfg_target_has_atomic),
                cfg(target_has_atomic = "ptr")
            )]
            impl $(<$($generics)*>)? $atomic_type $(<$($generics)*>)? {
                #[inline]
                #[cfg_attr(
                    any(all(debug_assertions, not(portable_atomic_no_track_caller)), miri),
                    track_caller
                )]
                pub(crate) fn compare_exchange(
                    &self,
                    current: $value_type,
                    new: $value_type,
                    success: Ordering,
                    failure: Ordering,
                ) -> Result<$value_type, $value_type> {
                    crate::utils::assert_compare_exchange_ordering(success, failure); // for track_caller (compiler can omit double check)
                    #[cfg(portable_atomic_no_stronger_failure_ordering)]
                    let success = crate::utils::upgrade_success_ordering(success, failure);
                    let res = self.inner.compare_exchange(current, new, success, failure);
                    if failure != Ordering::Relaxed && res.is_err() {
                        __kuser_memory_barrier();
                    }
                    res
                }
                #[inline]
                #[cfg_attr(
                    any(all(debug_assertions, not(portable_atomic_no_track_caller)), miri),
                    track_caller
                )]
                pub(crate) fn compare_exchange_weak(
                    &self,
                    current: $value_type,
                    new: $value_type,
                    success: Ordering,
                    failure: Ordering,
                ) -> Result<$value_type, $value_type> {
                    crate::utils::assert_compare_exchange_ordering(success, failure); // for track_caller (compiler can omit double check)
                    #[cfg(portable_atomic_no_stronger_failure_ordering)]
                    let success = crate::utils::upgrade_success_ordering(success, failure);
                    let res = self.inner.compare_exchange_weak(current, new, success, failure);
                    if failure != Ordering::Relaxed && res.is_err() {
                        __kuser_memory_barrier();
                    }
                    res
                }
            }
        };
    }

    atomic!(AtomicI8, i8);
    atomic!(AtomicU8, u8);
    atomic!(AtomicI16, i16);
    atomic!(AtomicU16, u16);
    atomic!(AtomicI32, i32);
    atomic!(AtomicU32, u32);
    atomic!(AtomicIsize, isize);
    atomic!(AtomicUsize, usize);
    atomic!([T] AtomicPtr, *mut T);
});

#[cfg_attr(portable_atomic_no_cfg_target_has_atomic, cfg(not(portable_atomic_no_atomic_64)))]
#[cfg_attr(
    not(portable_atomic_no_cfg_target_has_atomic),
    cfg(any(
        target_has_atomic = "64",
        not(any(target_pointer_width = "16", target_pointer_width = "32")),
    ))
)]
items!({
    // This is not normally reached on this platform.
    // It is only reached if something strange occurs, such as using target-feature=-v6 on armv6 target.
    // Since 64-bit atomic operations differ from 32-bit or smaller ones on Arm, similar optimizations
    // or workarounds are useless here, so do the same as atomic_int! macro in core_atomic.rs.
    // NB: Sync with atomic_int! macro in core_atomic.rs
    use super::core_atomic::{AtomicI64, AtomicU64};
    macro_rules! atomic_int {
        ($atomic_type:ident, $int_type:ident) => {
            impl $atomic_type {
                #[inline]
                #[cfg_attr(
                    any(all(debug_assertions, not(portable_atomic_no_track_caller)), miri),
                    track_caller
                )]
                pub(crate) fn load(&self, order: Ordering) -> $int_type {
                    crate::utils::assert_load_ordering(order); // for track_caller (compiler can omit double check)
                    self.inner.load(order)
                }
                #[inline]
                #[cfg_attr(
                    any(all(debug_assertions, not(portable_atomic_no_track_caller)), miri),
                    track_caller
                )]
                pub(crate) fn store(&self, val: $int_type, order: Ordering) {
                    crate::utils::assert_store_ordering(order); // for track_caller (compiler can omit double check)
                    self.inner.store(val, order);
                }
            }
            #[cfg_attr(
                portable_atomic_no_cfg_target_has_atomic,
                cfg(not(portable_atomic_no_atomic_cas))
            )]
            #[cfg_attr(
                not(portable_atomic_no_cfg_target_has_atomic),
                cfg(target_has_atomic = "ptr")
            )]
            impl $atomic_type {
                #[inline]
                #[cfg_attr(
                    any(all(debug_assertions, not(portable_atomic_no_track_caller)), miri),
                    track_caller
                )]
                pub(crate) fn compare_exchange(
                    &self,
                    current: $int_type,
                    new: $int_type,
                    success: Ordering,
                    failure: Ordering,
                ) -> Result<$int_type, $int_type> {
                    crate::utils::assert_compare_exchange_ordering(success, failure); // for track_caller (compiler can omit double check)
                    #[cfg(portable_atomic_no_stronger_failure_ordering)]
                    let success = crate::utils::upgrade_success_ordering(success, failure);
                    self.inner.compare_exchange(current, new, success, failure)
                }
                #[inline]
                #[cfg_attr(
                    any(all(debug_assertions, not(portable_atomic_no_track_caller)), miri),
                    track_caller
                )]
                pub(crate) fn compare_exchange_weak(
                    &self,
                    current: $int_type,
                    new: $int_type,
                    success: Ordering,
                    failure: Ordering,
                ) -> Result<$int_type, $int_type> {
                    crate::utils::assert_compare_exchange_ordering(success, failure); // for track_caller (compiler can omit double check)
                    #[cfg(portable_atomic_no_stronger_failure_ordering)]
                    let success = crate::utils::upgrade_success_ordering(success, failure);
                    self.inner.compare_exchange_weak(current, new, success, failure)
                }
            }
        };
    }
    atomic_int!(AtomicI64, i64);
    atomic_int!(AtomicU64, u64);
});
