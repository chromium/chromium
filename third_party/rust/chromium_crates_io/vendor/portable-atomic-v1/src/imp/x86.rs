// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Atomic operations implementation on x86/x86_64.

This module provides atomic operations not supported by LLVM or optimizes
cases where LLVM code generation is not optimal.

Note: On Miri and ThreadSanitizer which do not support inline assembly, we don't use
this module and use CAS loop instead.

Refs:
- x86 and amd64 instruction reference https://www.felixcloutier.com/x86

See tests/asm-test/asm/portable-atomic for generated assembly.
*/

#[cfg(not(portable_atomic_no_asm))]
use core::arch::asm;
use core::sync::atomic::Ordering;

#[cfg(target_pointer_width = "32")]
macro_rules! ptr_modifier {
    () => {
        ":e"
    };
}
#[cfg(target_pointer_width = "64")]
macro_rules! ptr_modifier {
    () => {
        ""
    };
}

/// An atomic fence.
///
/// # Panics
///
/// Panics if `order` is [`Relaxed`](Ordering::Relaxed).
#[cfg_attr(doc, allow(dead_code))]
#[inline]
#[cfg_attr(all(debug_assertions, not(portable_atomic_no_track_caller)), track_caller)]
pub fn fence(order: Ordering) {
    if order == Ordering::SeqCst {
        let p = core::cell::UnsafeCell::new(core::mem::MaybeUninit::<usize>::uninit());
        // SAFETY: raw pointer passed in is valid because we got it from a reference.
        unsafe {
            // Equivalent to `mfence`, but is up to 3.1x faster on Coffee Lake and up to 2.4x faster on Raptor Lake-H at least in simple cases.
            // - https://github.com/taiki-e/portable-atomic/pull/156
            // - LLVM uses `lock or` https://godbolt.org/z/vv6rjzfYd
            // - Windows uses `xchg` for x86_32 for MemoryBarrier https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-memorybarrier
            // - MSVC STL uses `lock inc` https://github.com/microsoft/STL/pull/740
            // - boost uses `lock or` https://github.com/boostorg/atomic/commit/559eba81af71386cedd99f170dc6101c6ad7bf22
            #[cfg(target_pointer_width = "64")]
            asm!(
                concat!("xchg qword ptr [{p", ptr_modifier!(), "}], {tmp}"),
                p = in(reg) p.get(),
                tmp = out(reg) _,
                options(nostack, preserves_flags),
            );
            #[cfg(target_pointer_width = "32")]
            asm!(
                concat!("xchg dword ptr [{p", ptr_modifier!(), "}], {tmp:e}"),
                p = in(reg) p.get(),
                tmp = out(reg) _,
                options(nostack, preserves_flags),
            );
        }
    } else {
        core::sync::atomic::fence(order);
    }
}

cfg_core_atomic!({
    use super::core_atomic::{
        AtomicI8, AtomicI16, AtomicI32, AtomicI64, AtomicIsize, AtomicPtr, AtomicU8, AtomicU16,
        AtomicU32, AtomicU64, AtomicUsize,
    };

    macro_rules! atomic_int {
        ($atomic_type:ident, $ptr_size:tt) => {
            impl $atomic_type {
                #[inline]
                pub(crate) fn not(&self, _order: Ordering) {
                    let dst = self.as_ptr();
                    // SAFETY: any data races are prevented by atomic intrinsics and the raw
                    // pointer passed in is valid because we got it from a reference.
                    //
                    // https://www.felixcloutier.com/x86/not
                    unsafe {
                        // atomic RMW is always SeqCst.
                        asm!(
                            concat!("lock not ", $ptr_size, " ptr [{dst", ptr_modifier!(), "}]"),
                            dst = in(reg) dst,
                            options(nostack, preserves_flags),
                        );
                    }
                }
                #[inline]
                pub(crate) fn neg(&self, _order: Ordering) {
                    let dst = self.as_ptr();
                    // SAFETY: any data races are prevented by atomic intrinsics and the raw
                    // pointer passed in is valid because we got it from a reference.
                    //
                    // https://www.felixcloutier.com/x86/neg
                    unsafe {
                        // atomic RMW is always SeqCst.
                        asm!(
                            concat!("lock neg ", $ptr_size, " ptr [{dst", ptr_modifier!(), "}]"),
                            dst = in(reg) dst,
                            // Do not use `preserves_flags` because NEG modifies the CF, OF, SF, ZF, AF, and PF flag.
                            options(nostack),
                        );
                    }
                }
            }
        };
    }

    atomic_int!(AtomicI8, "byte");
    atomic_int!(AtomicU8, "byte");
    atomic_int!(AtomicI16, "word");
    atomic_int!(AtomicU16, "word");
    atomic_int!(AtomicI32, "dword");
    atomic_int!(AtomicU32, "dword");
    #[cfg(target_arch = "x86_64")]
    atomic_int!(AtomicI64, "qword");
    #[cfg(target_arch = "x86_64")]
    atomic_int!(AtomicU64, "qword");
    #[cfg(target_pointer_width = "32")]
    atomic_int!(AtomicIsize, "dword");
    #[cfg(target_pointer_width = "32")]
    atomic_int!(AtomicUsize, "dword");
    #[cfg(target_pointer_width = "64")]
    atomic_int!(AtomicIsize, "qword");
    #[cfg(target_pointer_width = "64")]
    atomic_int!(AtomicUsize, "qword");

    #[cfg(target_arch = "x86")]
    impl AtomicI64 {
        #[inline]
        pub(crate) fn not(&self, order: Ordering) {
            self.fetch_not(order);
        }
        #[inline]
        pub(crate) fn neg(&self, order: Ordering) {
            self.fetch_neg(order);
        }
    }
    #[cfg(target_arch = "x86")]
    impl AtomicU64 {
        #[inline]
        pub(crate) fn not(&self, order: Ordering) {
            self.fetch_not(order);
        }
        #[inline]
        pub(crate) fn neg(&self, order: Ordering) {
            self.fetch_neg(order);
        }
    }

    macro_rules! atomic_bit_opts {
        (
            $([$($generics:tt)*])? $atomic_type:ident, $int_type:ident, $val_modifier:tt,
            $ptr_size:tt
        ) => {
            // LLVM 14 and older don't support generating `lock bt{s,r,c}`.
            // LLVM 15 only supports generating `lock bt{s,r,c}` for immediate bit offsets.
            // LLVM 16+ can generate `lock bt{s,r,c}` for both immediate and register bit offsets.
            // https://godbolt.org/z/TGhr5z4ds
            // So, use fetch_* based implementations on LLVM 16+, otherwise use asm based implementations.
            #[cfg(not(portable_atomic_pre_llvm_16))]
            impl_default_bit_opts!($atomic_type, $int_type);
            #[cfg(portable_atomic_pre_llvm_16)]
            impl $(<$($generics)*>)? $atomic_type $(<$($generics)*>)? {
                // `<integer>::BITS` requires Rust 1.53
                const BITS: u32 = (core::mem::size_of::<$int_type>() * 8) as u32;
                #[inline]
                pub(crate) fn bit_set(&self, bit: u32, _order: Ordering) -> bool {
                    let dst = self.as_ptr();
                    // SAFETY: any data races are prevented by atomic intrinsics and the raw
                    // pointer passed in is valid because we got it from a reference.
                    // the masking by the bit size of the type ensures that we do not shift
                    // out of bounds.
                    //
                    // https://www.felixcloutier.com/x86/bts
                    unsafe {
                        let r: u8;
                        // atomic RMW is always SeqCst.
                        asm!(
                            concat!("lock bts ", $ptr_size, " ptr [{dst", ptr_modifier!(), "}], {bit", $val_modifier, "}"),
                            "setb {r}",
                            dst = in(reg) dst,
                            bit = in(reg) (bit & (Self::BITS - 1)) as $int_type,
                            r = out(reg_byte) r,
                            // Do not use `preserves_flags` because BTS modifies the CF flag.
                            options(nostack),
                        );
                        crate::utils::assert_unchecked(r == 0 || r == 1); // may help remove extra test
                        r != 0
                    }
                }
                #[inline]
                pub(crate) fn bit_clear(&self, bit: u32, _order: Ordering) -> bool {
                    let dst = self.as_ptr();
                    // SAFETY: any data races are prevented by atomic intrinsics and the raw
                    // pointer passed in is valid because we got it from a reference.
                    // the masking by the bit size of the type ensures that we do not shift
                    // out of bounds.
                    //
                    // https://www.felixcloutier.com/x86/btr
                    unsafe {
                        let r: u8;
                        // atomic RMW is always SeqCst.
                        asm!(
                            concat!("lock btr ", $ptr_size, " ptr [{dst", ptr_modifier!(), "}], {bit", $val_modifier, "}"),
                            "setb {r}",
                            dst = in(reg) dst,
                            bit = in(reg) (bit & (Self::BITS - 1)) as $int_type,
                            r = out(reg_byte) r,
                            // Do not use `preserves_flags` because BTR modifies the CF flag.
                            options(nostack),
                        );
                        crate::utils::assert_unchecked(r == 0 || r == 1); // may help remove extra test
                        r != 0
                    }
                }
                #[inline]
                pub(crate) fn bit_toggle(&self, bit: u32, _order: Ordering) -> bool {
                    let dst = self.as_ptr();
                    // SAFETY: any data races are prevented by atomic intrinsics and the raw
                    // pointer passed in is valid because we got it from a reference.
                    // the masking by the bit size of the type ensures that we do not shift
                    // out of bounds.
                    //
                    // https://www.felixcloutier.com/x86/btc
                    unsafe {
                        let r: u8;
                        // atomic RMW is always SeqCst.
                        asm!(
                            concat!("lock btc ", $ptr_size, " ptr [{dst", ptr_modifier!(), "}], {bit", $val_modifier, "}"),
                            "setb {r}",
                            dst = in(reg) dst,
                            bit = in(reg) (bit & (Self::BITS - 1)) as $int_type,
                            r = out(reg_byte) r,
                            // Do not use `preserves_flags` because BTC modifies the CF flag.
                            options(nostack),
                        );
                        crate::utils::assert_unchecked(r == 0 || r == 1); // may help remove extra test
                        r != 0
                    }
                }
            }
        };
    }

    impl_default_bit_opts!(AtomicI8, i8);
    impl_default_bit_opts!(AtomicU8, u8);
    atomic_bit_opts!(AtomicI16, i16, ":x", "word");
    atomic_bit_opts!(AtomicU16, u16, ":x", "word");
    atomic_bit_opts!(AtomicI32, i32, ":e", "dword");
    atomic_bit_opts!(AtomicU32, u32, ":e", "dword");
    #[cfg(target_arch = "x86_64")]
    atomic_bit_opts!(AtomicI64, i64, "", "qword");
    #[cfg(target_arch = "x86_64")]
    atomic_bit_opts!(AtomicU64, u64, "", "qword");
    #[cfg(target_arch = "x86")]
    impl_default_bit_opts!(AtomicI64, i64);
    #[cfg(target_arch = "x86")]
    impl_default_bit_opts!(AtomicU64, u64);
    #[cfg(target_pointer_width = "32")]
    atomic_bit_opts!(AtomicIsize, isize, ":e", "dword");
    #[cfg(target_pointer_width = "32")]
    atomic_bit_opts!(AtomicUsize, usize, ":e", "dword");
    #[cfg(target_pointer_width = "32")]
    atomic_bit_opts!([T] AtomicPtr, usize, ":e", "dword");
    #[cfg(target_pointer_width = "64")]
    atomic_bit_opts!(AtomicIsize, isize, "", "qword");
    #[cfg(target_pointer_width = "64")]
    atomic_bit_opts!(AtomicUsize, usize, "", "qword");
    #[cfg(target_pointer_width = "64")]
    atomic_bit_opts!([T] AtomicPtr, usize, "", "qword");
});
