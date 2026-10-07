// SPDX-License-Identifier: Apache-2.0 OR MIT

#![cfg_attr(not(all(test, feature = "float")), allow(dead_code, unused_macros))]

#[allow(unused_imports)]
pub(crate) use self::generated::{RegISize, RegSize};
#[macro_use]
#[path = "gen/utils.rs"]
mod generated;

use core::sync::atomic::Ordering;

macro_rules! static_assert {
    ($cond:expr $(,)?) => {{
        let [()] = [(); (true /* type check */ & $cond) as usize];
    }};
}

macro_rules! static_assert_layout {
    ($atomic_type:ty, $value_type:ty) => {
        static_assert!(
            core::mem::align_of::<$atomic_type>() == core::mem::size_of::<$atomic_type>()
        );
        static_assert!(core::mem::size_of::<$atomic_type>() == core::mem::size_of::<$value_type>());
    };
}

// #[doc = concat!(...)] requires Rust 1.54
macro_rules! doc_comment {
    ($doc:expr, $($tt:tt)*) => {
        #[doc = $doc]
        $($tt)*
    };
}

// Adapted from https://github.com/BurntSushi/memchr/blob/2.4.1/src/memchr/x86/mod.rs#L9-L71.
/// # Safety
///
/// - the caller must uphold the safety contract for the function returned by $detect_body.
/// - the memory pointed by the function pointer returned by $detect_body must be visible from any threads.
///
/// The second requirement is always met if the function pointer is to the function definition.
/// (Currently, all uses of this macro in our code are in this case.)
#[allow(unused_macros)]
#[cfg(not(portable_atomic_no_outline_atomics))]
#[cfg(any(
    target_arch = "aarch64",
    target_arch = "arm",
    target_arch = "arm64ec",
    target_arch = "powerpc64",
    target_arch = "riscv32",
    target_arch = "riscv64",
    all(target_arch = "x86_64", not(any(target_env = "sgx", miri))),
))]
macro_rules! ifunc {
    (unsafe fn($($arg_pat:ident: $arg_ty:ty),* $(,)?) $(-> $ret_ty:ty)? { $($init_body:tt)* }) => {{
        type FnTy = unsafe fn($($arg_ty),*) $(-> $ret_ty)?;
        static FUNC: core::sync::atomic::AtomicPtr<()>
            = core::sync::atomic::AtomicPtr::new(init as *mut ());
        #[cold]
        unsafe fn init($($arg_pat: $arg_ty),*) $(-> $ret_ty)? {
            let func: FnTy = { $($init_body)* };
            FUNC.store(func as *mut (), core::sync::atomic::Ordering::Relaxed);
            // SAFETY: the caller must uphold the safety contract for the function returned by $init_body.
            unsafe { func($($arg_pat),*) }
        }
        // SAFETY: `FnTy` is a function pointer, which is always safe to transmute with a `*mut ()`.
        // (To force the caller to use unsafe block for this macro, do not use
        // unsafe block here.)
        let func = {
            core::mem::transmute::<*mut (), FnTy>(FUNC.load(core::sync::atomic::Ordering::Relaxed))
        };
        // SAFETY: the caller must uphold the safety contract for the function returned by $init_body.
        // (To force the caller to use unsafe block for this macro, do not use
        // unsafe block here.)
        func($($arg_pat),*)
    }};
}

#[cfg(not(portable_atomic_no_asm))]
#[allow(unused_macros)]
macro_rules! __asm {
    ($($tt:tt)*) => {
        core::arch::asm!($($tt)*)
    };
}
#[cfg(portable_atomic_no_asm)]
#[allow(unused_macros)]
macro_rules! __asm {
    ($($tt:tt)*) => {
        asm!($($tt)*)
    };
}

#[allow(unused_macros)]
#[cfg(not(portable_atomic_no_outline_atomics))]
#[cfg(any(
    target_arch = "aarch64",
    target_arch = "arm",
    target_arch = "arm64ec",
    target_arch = "powerpc64",
    target_arch = "riscv32",
    target_arch = "riscv64",
    all(target_arch = "x86_64", not(any(target_env = "sgx", miri))),
))]
macro_rules! fn_alias {
    (
        $(#[$($fn_attr:tt)*])*
        $vis:vis unsafe fn($($arg_pat:ident: $arg_ty:ty),*) $(-> $ret_ty:ty)?;
        $(#[$($alias_attr:tt)*])*
        $new:ident = $from:ident($($last_args:tt)*);
        $($rest:tt)*
    ) => {
        $(#[$($fn_attr)*])*
        $(#[$($alias_attr)*])*
        $vis unsafe fn $new($($arg_pat: $arg_ty),*) $(-> $ret_ty)? {
            // SAFETY: the caller must uphold the safety contract.
            unsafe { $from($($arg_pat,)* $($last_args)*) }
        }
        fn_alias! {
            $(#[$($fn_attr)*])*
            $vis unsafe fn($($arg_pat: $arg_ty),*) $(-> $ret_ty)?;
            $($rest)*
        }
    };
    (
        $(#[$($attr:tt)*])*
        $vis:vis unsafe fn($($arg_pat:ident: $arg_ty:ty),*) $(-> $ret_ty:ty)?;
    ) => {}
}

/// Make the given function const if the given condition is true.
macro_rules! const_fn {
    (
        const_if: #[cfg($($cfg:tt)+)];
        $(#[$($attr:tt)*])*
        $vis:vis const $($rest:tt)*
    ) => {
        #[cfg($($cfg)+)]
        $(#[$($attr)*])*
        $vis const $($rest)*
        #[cfg(not($($cfg)+))]
        $(#[$($attr)*])*
        $vis $($rest)*
    };
}

/// Implements `core::fmt::Debug` and `serde::{Serialize, Deserialize}` (when serde
/// feature is enabled) for atomic bool, integer, or float.
macro_rules! impl_debug_and_serde {
    // TODO(f16_and_f128): Implement serde traits for f16 & f128 once stabilized.
    (AtomicF16) => {
        impl_debug!(AtomicF16);
    };
    (AtomicF128) => {
        impl_debug!(AtomicF128);
    };
    ($atomic_type:ident) => {
        impl_debug!($atomic_type);
        #[cfg(feature = "serde")]
        #[cfg_attr(docsrs, doc(cfg(feature = "serde")))]
        impl serde::ser::Serialize for $atomic_type {
            #[allow(clippy::missing_inline_in_public_items)] // serde doesn't use inline on std atomic's Serialize/Deserialize impl
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: serde::ser::Serializer,
            {
                // https://github.com/serde-rs/serde/blob/v1.0.152/serde/src/ser/impls.rs#L958-L959
                self.load(Ordering::Relaxed).serialize(serializer)
            }
        }
        #[cfg(feature = "serde")]
        #[cfg_attr(docsrs, doc(cfg(feature = "serde")))]
        impl<'de> serde::de::Deserialize<'de> for $atomic_type {
            #[allow(clippy::missing_inline_in_public_items)] // serde doesn't use inline on std atomic's Serialize/Deserialize impl
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: serde::de::Deserializer<'de>,
            {
                serde::de::Deserialize::deserialize(deserializer).map(Self::new)
            }
        }
    };
}
macro_rules! impl_debug {
    ($atomic_type:ident) => {
        impl fmt::Debug for $atomic_type {
            #[inline] // fmt is not hot path, but #[inline] on fmt seems to still be useful: https://github.com/rust-lang/rust/pull/117727
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                // std atomic types use Relaxed in Debug::fmt: https://github.com/rust-lang/rust/blob/1.84.0/library/core/src/sync/atomic.rs#L2188
                fmt::Debug::fmt(&self.load(Ordering::Relaxed), f)
            }
        }
    };
}

// We do not provide `nand` because it cannot be optimized on neither x86 nor MSP430.
// https://godbolt.org/z/ahWejchbT
macro_rules! impl_default_no_fetch_ops {
    ($atomic_type:ident, bool) => {
        impl $atomic_type {
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn and(&self, val: bool, order: Ordering) {
                self.fetch_and(val, order);
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn or(&self, val: bool, order: Ordering) {
                self.fetch_or(val, order);
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn xor(&self, val: bool, order: Ordering) {
                self.fetch_xor(val, order);
            }
        }
    };
    ($atomic_type:ident, $int_type:ty) => {
        impl $atomic_type {
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn add(&self, val: $int_type, order: Ordering) {
                self.fetch_add(val, order);
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn sub(&self, val: $int_type, order: Ordering) {
                self.fetch_sub(val, order);
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn and(&self, val: $int_type, order: Ordering) {
                self.fetch_and(val, order);
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn or(&self, val: $int_type, order: Ordering) {
                self.fetch_or(val, order);
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn xor(&self, val: $int_type, order: Ordering) {
                self.fetch_xor(val, order);
            }
        }
    };
}
macro_rules! impl_default_bit_opts {
    (AtomicPtr, $int_type:ty) => {
        impl<T> AtomicPtr<T> {
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn bit_set(&self, bit: u32, order: Ordering) -> bool {
                #[cfg(portable_atomic_no_strict_provenance)]
                use crate::utils::ptr::PtrExt as _;
                let mask = <$int_type>::wrapping_shl(1, bit);
                self.fetch_or(mask, order).addr() & mask != 0
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn bit_clear(&self, bit: u32, order: Ordering) -> bool {
                #[cfg(portable_atomic_no_strict_provenance)]
                use crate::utils::ptr::PtrExt as _;
                let mask = <$int_type>::wrapping_shl(1, bit);
                self.fetch_and(!mask, order).addr() & mask != 0
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn bit_toggle(&self, bit: u32, order: Ordering) -> bool {
                #[cfg(portable_atomic_no_strict_provenance)]
                use crate::utils::ptr::PtrExt as _;
                let mask = <$int_type>::wrapping_shl(1, bit);
                self.fetch_xor(mask, order).addr() & mask != 0
            }
        }
    };
    ($atomic_type:ident, $int_type:ty) => {
        impl $atomic_type {
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn bit_set(&self, bit: u32, order: Ordering) -> bool {
                let mask = <$int_type>::wrapping_shl(1, bit);
                self.fetch_or(mask, order) & mask != 0
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn bit_clear(&self, bit: u32, order: Ordering) -> bool {
                let mask = <$int_type>::wrapping_shl(1, bit);
                self.fetch_and(!mask, order) & mask != 0
            }
            #[inline]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn bit_toggle(&self, bit: u32, order: Ordering) -> bool {
                let mask = <$int_type>::wrapping_shl(1, bit);
                self.fetch_xor(mask, order) & mask != 0
            }
        }
    };
}

// This just outputs the input as is, but can be used like an item-level block by using it with cfg.
// Note: This macro is items!({ }), not items! { }.
// An extra brace is used in input to make contents rustfmt-able.
macro_rules! items {
    ({$($tt:tt)*}) => {
        $($tt)*
    };
}

// rustfmt-compatible cfg_select/cfg_if alternative
// Note: This macro is cfg_sel!({ }), not cfg_sel! { }.
// An extra brace is used in input to make contents rustfmt-able.
macro_rules! cfg_sel {
    ({#[cfg(else)] { $($output:tt)* }}) => {
        $($output)*
    };
    ({
        #[cfg($cfg:meta)]
        { $($output:tt)* }
        $($( $rest:tt )+)?
    }) => {
        #[cfg($cfg)]
        cfg_sel! {{#[cfg(else)] { $($output)* }}}
        $(
            #[cfg(not($cfg))]
            cfg_sel! {{ $($rest)+ }}
        )?
    };
    ({
        #[cfg_attr(portable_atomic_no_cfg_target_has_atomic, cfg($cfg1:meta))]
        #[cfg_attr(not(portable_atomic_no_cfg_target_has_atomic), cfg($cfg2:meta))]
        { $($output:tt)* }
        $($( $rest:tt )+)?
    }) => {
        #[cfg_attr(portable_atomic_no_cfg_target_has_atomic, cfg($cfg1))]
        #[cfg_attr(not(portable_atomic_no_cfg_target_has_atomic), cfg($cfg2))]
        cfg_sel! {{#[cfg(else)] { $($output)* }}}
        $(
            #[cfg_attr(portable_atomic_no_cfg_target_has_atomic, cfg(not($cfg1)))]
            #[cfg_attr(not(portable_atomic_no_cfg_target_has_atomic), cfg(not($cfg2)))]
            cfg_sel! {{ $($rest)+ }}
        )?
    };
}

// Equivalent to core::hint::cold_path, but compatible with pre-1.95 rustc.
#[allow(dead_code)]
#[inline(always)]
#[cold]
fn cold_path() {}
// Stable equivalent of core::hint::{likely, unlikely}.
#[allow(dead_code)]
#[inline(always)]
pub(crate) fn likely(b: bool) -> bool {
    if b {
        true
    } else {
        cold_path();
        false
    }
}
#[allow(dead_code)]
#[inline(always)]
pub(crate) fn unlikely(b: bool) -> bool {
    if b {
        cold_path();
        true
    } else {
        false
    }
}

// Equivalent to core::hint::assert_unchecked, but compatible with pre-1.81 rustc.
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[allow(dead_code)]
#[inline(always)]
#[cfg_attr(all(debug_assertions, not(portable_atomic_no_track_caller)), track_caller)]
pub(crate) unsafe fn assert_unchecked(cond: bool) {
    if !cond {
        #[cfg(debug_assertions)]
        unreachable!();
        #[cfg(not(debug_assertions))]
        // SAFETY: the caller promised `cond` is true.
        unsafe {
            core::hint::unreachable_unchecked()
        }
    }
}

// https://github.com/rust-lang/rust/blob/1.84.0/library/core/src/sync/atomic.rs#L3338
#[inline]
#[cfg_attr(all(debug_assertions, not(portable_atomic_no_track_caller)), track_caller)]
pub(crate) fn assert_load_ordering(order: Ordering) {
    match order {
        Ordering::Acquire | Ordering::Relaxed | Ordering::SeqCst => {}
        Ordering::Release => panic!("there is no such thing as a release load"),
        Ordering::AcqRel => panic!("there is no such thing as an acquire-release load"),
        _ => unreachable!(),
    }
}
// https://github.com/rust-lang/rust/blob/1.84.0/library/core/src/sync/atomic.rs#L3323
#[inline]
#[cfg_attr(all(debug_assertions, not(portable_atomic_no_track_caller)), track_caller)]
pub(crate) fn assert_store_ordering(order: Ordering) {
    match order {
        Ordering::Release | Ordering::Relaxed | Ordering::SeqCst => {}
        Ordering::Acquire => panic!("there is no such thing as an acquire store"),
        Ordering::AcqRel => panic!("there is no such thing as an acquire-release store"),
        _ => unreachable!(),
    }
}
// https://github.com/rust-lang/rust/blob/1.84.0/library/core/src/sync/atomic.rs#L3404
#[inline]
#[cfg_attr(all(debug_assertions, not(portable_atomic_no_track_caller)), track_caller)]
pub(crate) fn assert_compare_exchange_ordering(success: Ordering, failure: Ordering) {
    match success {
        Ordering::AcqRel
        | Ordering::Acquire
        | Ordering::Relaxed
        | Ordering::Release
        | Ordering::SeqCst => {}
        _ => unreachable!(),
    }
    match failure {
        Ordering::Acquire | Ordering::Relaxed | Ordering::SeqCst => {}
        Ordering::Release => panic!("there is no such thing as a release failure ordering"),
        Ordering::AcqRel => panic!("there is no such thing as an acquire-release failure ordering"),
        _ => unreachable!(),
    }
}

// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0418r2.html
// https://github.com/rust-lang/rust/pull/98383
#[allow(dead_code)]
#[inline]
pub(crate) fn upgrade_success_ordering(success: Ordering, failure: Ordering) -> Ordering {
    match (success, failure) {
        (Ordering::Relaxed, Ordering::Acquire) => Ordering::Acquire,
        (Ordering::Release, Ordering::Acquire) => Ordering::AcqRel,
        (_, Ordering::SeqCst) => Ordering::SeqCst,
        _ => success,
    }
}

#[cfg(not(portable_atomic_no_asm_maybe_uninit))]
#[cfg(target_pointer_width = "32")]
// SAFETY: MaybeUninit returned by zero_extend64_ptr is always initialized.
const _: () = assert!(unsafe {
    zero_extend64_ptr(ptr::without_provenance_mut(!0)).assume_init() == !0_u32 as u64
});
/// Zero-extends the given 32-bit pointer to `MaybeUninit<u64>`.
/// This is used for 64-bit architecture's 32-bit ABI (e.g., AArch64 ILP32 ABI).
/// See ptr_reg! macro in src/gen/utils.rs for details.
#[cfg(not(portable_atomic_no_asm_maybe_uninit))]
#[cfg(target_pointer_width = "32")]
#[allow(dead_code)]
#[inline]
pub(crate) const fn zero_extend64_ptr(v: *mut ()) -> core::mem::MaybeUninit<u64> {
    #[repr(C)]
    struct ZeroExtended {
        #[cfg(target_endian = "big")]
        pad: *mut (),
        v: *mut (),
        #[cfg(target_endian = "little")]
        pad: *mut (),
    }
    // SAFETY: we can safely transmute any 64-bit value to MaybeUninit<u64>.
    unsafe { core::mem::transmute(ZeroExtended { v, pad: core::ptr::null_mut() }) }
}

#[allow(dead_code)]
#[cfg(any(
    target_arch = "aarch64",
    target_arch = "arm64ec",
    target_arch = "powerpc64",
    target_arch = "riscv64",
    target_arch = "s390x",
    target_arch = "x86_64",
))]
/// A 128-bit value represented as a pair of 64-bit values.
///
/// This type is `#[repr(C)]`, both fields have the same in-memory representation
/// and are plain old data types, so access to the fields is always safe.
#[derive(Clone, Copy)]
#[repr(C)]
pub(crate) union U128 {
    pub(crate) whole: u128,
    pub(crate) pair: Pair<u64>,
}
#[allow(dead_code)]
#[cfg(any(target_arch = "arm", target_arch = "riscv32"))]
/// A 64-bit value represented as a pair of 32-bit values.
///
/// This type is `#[repr(C)]`, both fields have the same in-memory representation
/// and are plain old data types, so access to the fields is always safe.
#[derive(Clone, Copy)]
#[repr(C)]
pub(crate) union U64 {
    pub(crate) whole: u64,
    pub(crate) pair: Pair<u32>,
}
#[allow(dead_code)]
#[derive(Clone, Copy)]
#[repr(C)]
pub(crate) struct Pair<T: Copy> {
    // little endian order
    #[cfg(any(
        target_endian = "little",
        target_arch = "aarch64",
        target_arch = "arm",
        target_arch = "arm64ec",
    ))]
    pub(crate) lo: T,
    pub(crate) hi: T,
    // big endian order
    #[cfg(not(any(
        target_endian = "little",
        target_arch = "aarch64",
        target_arch = "arm",
        target_arch = "arm64ec",
    )))]
    pub(crate) lo: T,
}

#[cfg(any(target_arch = "riscv32", target_arch = "riscv64"))]
type MinWord = u32;
#[cfg(any(target_arch = "riscv32", target_arch = "riscv64"))]
type RetInt = u32;
// Adapted from https://github.com/taiki-e/atomic-maybe-uninit/blob/v0.3.6/src/utils.rs#L255.
// Helper for implementing sub-word atomic operations using word-sized LL/SC loop or CAS loop.
//
// Refs: https://github.com/llvm/llvm-project/blob/llvmorg-22.1.0-rc1/llvm/lib/CodeGen/AtomicExpandPass.cpp#L811
// (aligned_ptr, shift, mask)
#[cfg(any(target_arch = "riscv32", target_arch = "riscv64"))]
#[allow(dead_code)]
#[inline]
pub(crate) fn create_sub_word_mask_values<T>(ptr: *mut T) -> (*mut MinWord, RetInt, RetInt) {
    use core::mem;

    #[cfg(portable_atomic_no_strict_provenance)]
    use self::ptr::PtrExt as _;

    // RISC-V, MIPS, SPARC, LoongArch, Xtensa, BPF: shift amount of 32-bit shift instructions is 5 bits unsigned (0-31).
    // PowerPC, C-SKY: shift amount of 32-bit shift instructions is 6 bits unsigned (0-63) and shift amount 32-63 means "clear".
    // Arm: shift amount of 32-bit shift instructions is 8 bits unsigned (0-255).
    // Hexagon: shift amount of 32-bit shift instructions is 7 bits signed (-64-63) and negative shift amount means "reverse the direction of the shift".
    // (On s390x, we don't use the mask returned from this function.)
    // (See also https://devblogs.microsoft.com/oldnewthing/20230904-00/?p=108704 for others)
    const SHIFT_MASK: bool = !cfg!(any(
        target_arch = "bpf",
        target_arch = "loongarch32",
        target_arch = "loongarch64",
        target_arch = "mips",
        target_arch = "mips32r6",
        target_arch = "mips64",
        target_arch = "mips64r6",
        target_arch = "riscv32",
        target_arch = "riscv64",
        target_arch = "s390x",
        target_arch = "sparc",
        target_arch = "sparc64",
        target_arch = "xtensa",
    ));
    let ptr_mask = mem::size_of::<MinWord>() - 1;
    let aligned_ptr = ptr.with_addr(ptr.addr() & !ptr_mask) as *mut MinWord;
    let ptr_lsb = if SHIFT_MASK {
        ptr.addr() & ptr_mask
    } else {
        // We use 32-bit wrapping shift instructions in asm on these platforms.
        ptr.addr()
    };
    let shift = if cfg!(any(target_endian = "little", target_arch = "s390x")) {
        ptr_lsb.wrapping_mul(8)
    } else {
        (ptr_lsb ^ (mem::size_of::<MinWord>() - mem::size_of::<T>())).wrapping_mul(8)
    };
    let mut mask: RetInt = (1 << (mem::size_of::<T>() * 8)) - 1; // !(0 as T) as RetInt
    if SHIFT_MASK {
        mask <<= shift;
    }
    #[allow(clippy::cast_possible_truncation)]
    {
        (aligned_ptr, shift as RetInt, mask)
    }
}

// This module provides core::ptr strict_provenance/exposed_provenance polyfill for pre-1.84 rustc.
#[allow(dead_code)]
pub(crate) mod ptr {
    cfg_sel!({
        #[cfg(not(portable_atomic_no_strict_provenance))]
        {
            #[allow(unused_imports)]
            pub(crate) use core::ptr::{
                with_exposed_provenance, with_exposed_provenance_mut, without_provenance_mut,
            };
        }
        #[cfg(else)]
        {
            #[inline(always)]
            #[must_use]
            pub(crate) const fn without_provenance_mut<T>(addr: usize) -> *mut T {
                // An int-to-pointer transmute currently has exactly the intended semantics: it creates a
                // pointer without provenance. Note that this is *not* a stable guarantee about transmute
                // semantics, it relies on sysroot crates having special status.
                // SAFETY: every valid integer is also a valid pointer (as long as you don't dereference that
                // pointer).
                #[cfg(miri)]
                unsafe {
                    core::mem::transmute(addr)
                }
                // const transmute requires Rust 1.56.
                // Using transmute doesn't work with CHERI: https://github.com/kent-weak-memory/rust/blob/0c0ca909de877f889629057e1ddf139527446d75/library/core/src/ptr/mod.rs#L607
                #[cfg(not(miri))]
                {
                    addr as *mut T
                }
            }
            #[inline(always)]
            #[must_use]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn with_exposed_provenance<T>(addr: usize) -> *const T {
                addr as *const T
            }
            #[inline(always)]
            #[must_use]
            #[cfg_attr(miri, track_caller)] // even without panics, this helps for Miri backtraces
            pub(crate) fn with_exposed_provenance_mut<T>(addr: usize) -> *mut T {
                addr as *mut T
            }

            pub(crate) trait PtrExt<T: ?Sized>: Copy {
                #[must_use]
                fn addr(self) -> usize;
                #[must_use]
                fn with_addr(self, addr: usize) -> Self
                where
                    T: Sized;
            }
            impl<T: ?Sized> PtrExt<T> for *mut T {
                #[inline(always)]
                #[must_use]
                fn addr(self) -> usize {
                    // A pointer-to-integer transmute currently has exactly the right semantics: it returns the
                    // address without exposing the provenance. Note that this is *not* a stable guarantee about
                    // transmute semantics, it relies on sysroot crates having special status.
                    // SAFETY: Pointer-to-integer transmutes are valid (if you are okay with losing the
                    // provenance).
                    #[cfg(miri)]
                    unsafe {
                        core::mem::transmute(self as *mut ())
                    }
                    // Using transmute doesn't work with CHERI: https://github.com/kent-weak-memory/rust/blob/0c0ca909de877f889629057e1ddf139527446d75/library/core/src/ptr/mut_ptr.rs#L210
                    #[cfg(not(miri))]
                    {
                        self as *mut () as usize
                    }
                }
                #[inline]
                #[must_use]
                fn with_addr(self, addr: usize) -> Self
                where
                    T: Sized,
                {
                    // This should probably be an intrinsic to avoid doing any sort of arithmetic, but
                    // meanwhile, we can implement it with `wrapping_offset`, which preserves the pointer's
                    // provenance.
                    let self_addr = self.addr() as isize;
                    let dest_addr = addr as isize;
                    let offset = dest_addr.wrapping_sub(self_addr);
                    (self as *mut u8).wrapping_offset(offset) as *mut T
                }
            }
        }
    });
}

// This module provides:
// - core::ffi polyfill (c_* type aliases and CStr) for pre-1.64 rustc compatibility.
//   (core::ffi::* (except c_void) requires Rust 1.64)
// - Safe abstraction (c! macro) for creating static C strings without runtime checks.
//   (c"..." requires Rust 1.77)
// - Helper macros for defining FFI bindings with static signature/type/value assertions.
// - Helper macros for defining asm-based syscalls on Linux.
#[cfg(any(
    test,
    portable_atomic_test_no_std_static_assert_ffi,
    not(any(target_arch = "x86", target_arch = "x86_64"))
))]
#[cfg(any(not(portable_atomic_no_asm), portable_atomic_unstable_asm))]
#[allow(dead_code, non_camel_case_types, unused_macros)]
#[macro_use]
pub(crate) mod ffi {
    // -------------------------------------------------------------------------
    // core::ffi polyfill and c"..." equivalent

    pub(crate) type c_void = core::ffi::c_void;
    // c_{,u}int is {i,u}16 on 16-bit targets, otherwise {i,u}32.
    // https://github.com/rust-lang/rust/blob/1.84.0/library/core/src/ffi/mod.rs#L156
    cfg_sel!({
        #[cfg(target_pointer_width = "16")]
        {
            pub(crate) type c_int = i16;
            pub(crate) type c_uint = u16;
        }
        #[cfg(else)]
        {
            pub(crate) type c_int = i32;
            pub(crate) type c_uint = u32;
        }
    });
    // c_{,u}long is {i,u}64 on non-Windows 64-bit targets, otherwise {i,u}32.
    // https://github.com/rust-lang/rust/blob/1.84.0/library/core/src/ffi/mod.rs#L168
    cfg_sel!({
        #[cfg(all(target_pointer_width = "64", not(windows)))]
        {
            pub(crate) type c_long = i64;
            pub(crate) type c_ulong = u64;
        }
        #[cfg(else)]
        {
            pub(crate) type c_long = i32;
            pub(crate) type c_ulong = u32;
        }
    });
    // c_size_t is currently always usize.
    // https://github.com/rust-lang/rust/blob/1.84.0/library/core/src/ffi/mod.rs#L76
    pub(crate) type c_size_t = usize;
    // c_char is u8 by default on non-Apple/non-Windows/non-Vita Arm/C-SKY/Hexagon/MSP430/PowerPC/RISC-V/s390x/Xtensa targets, otherwise i8 by default.
    // See references in https://github.com/rust-lang/rust/issues/129945 for details.
    cfg_sel!({
        #[cfg(all(
            not(any(target_vendor = "apple", windows, target_os = "vita")),
            any(
                target_arch = "aarch64",
                target_arch = "arm",
                target_arch = "csky",
                target_arch = "hexagon",
                target_arch = "msp430",
                target_arch = "powerpc",
                target_arch = "powerpc64",
                target_arch = "riscv32",
                target_arch = "riscv64",
                target_arch = "s390x",
                target_arch = "xtensa",
            ),
        ))]
        {
            pub(crate) type c_char = u8;
        }
        #[cfg(else)]
        {
            pub(crate) type c_char = i8;
        }
    });

    // Static assertions for C type definitions.
    #[cfg(test)]
    const _: fn() = || {
        let _: c_int = 0 as std::os::raw::c_int;
        let _: c_uint = 0 as std::os::raw::c_uint;
        let _: c_long = 0 as std::os::raw::c_long;
        let _: c_ulong = 0 as std::os::raw::c_ulong;
        #[cfg(unix)]
        let _: c_size_t = 0 as libc::size_t; // std::os::raw::c_size_t is unstable
        let _: c_char = 0 as std::os::raw::c_char;
    };

    #[repr(transparent)]
    pub(crate) struct CStr([c_char]);
    impl CStr {
        #[inline]
        #[must_use]
        pub(crate) const fn as_ptr(&self) -> *const c_char {
            self.0.as_ptr()
        }
        /// # Safety
        ///
        /// The provided slice **must** be nul-terminated and not contain any interior
        /// nul bytes.
        #[inline]
        #[must_use]
        pub(crate) unsafe fn from_bytes_with_nul_unchecked(bytes: &[u8]) -> &CStr {
            // SAFETY: Casting to CStr is safe because *our* CStr is #[repr(transparent)]
            // and its internal representation is a [u8] too. (Note that std's CStr
            // is not #[repr(transparent)].)
            // Dereferencing the obtained pointer is safe because it comes from a
            // reference. Making a reference is then safe because its lifetime
            // is bound by the lifetime of the given `bytes`.
            unsafe { &*(bytes as *const [u8] as *const CStr) }
        }
        #[cfg(test)]
        #[inline]
        #[must_use]
        pub(crate) fn to_bytes_with_nul(&self) -> &[u8] {
            #[allow(clippy::unnecessary_cast)] // triggered for targets that c_char is u8
            // SAFETY: Transmuting a slice of `c_char`s to a slice of `u8`s
            // is safe on all supported targets.
            unsafe {
                &*(&self.0 as *const [c_char] as *const [u8])
            }
        }
    }

    macro_rules! c {
        ($s:expr) => {{
            const BYTES: &[u8] = concat!($s, "\0").as_bytes();
            const _: () = static_assert!(crate::utils::ffi::_const_is_c_str(BYTES));
            #[allow(unused_unsafe)]
            // SAFETY: we've checked `BYTES` is a valid C string
            unsafe {
                crate::utils::ffi::CStr::from_bytes_with_nul_unchecked(BYTES)
            }
        }};
    }

    #[must_use]
    pub(crate) const fn _const_is_c_str(bytes: &[u8]) -> bool {
        #[cfg(portable_atomic_no_track_caller)]
        {
            // const_if_match/const_loop was stabilized (nightly-2020-06-30) 2 days before
            // track_caller was stabilized (nightly-2020-07-02), so we reuse the cfg for
            // track_caller here instead of emitting a cfg for const_if_match/const_loop.
            // https://github.com/rust-lang/rust/pull/72437
            // track_caller was stabilized 11 days after the oldest nightly version
            // that uses this module, and is included in the same 1.46 stable release.
            // The check here is insufficient in this case, but this is fine because this function
            // is internal code that is not used to process input from the user and our CI checks
            // all builtin targets and some custom targets with some versions of newer compilers.
            !bytes.is_empty()
        }
        #[cfg(not(portable_atomic_no_track_caller))]
        {
            // Based on https://github.com/rust-lang/rust/blob/1.84.0/library/core/src/ffi/c_str.rs#L417
            // - bytes must be nul-terminated.
            // - bytes must not contain any interior nul bytes.
            if bytes.is_empty() {
                return false;
            }
            let mut i = bytes.len() - 1;
            if bytes[i] != 0 {
                return false;
            }
            // Ending null byte exists, skip to the rest.
            while i != 0 {
                i -= 1;
                if bytes[i] == 0 {
                    return false;
                }
            }
            true
        }
    }

    // -------------------------------------------------------------------------
    // Helper macros for defining FFI bindings with static signature/type/value assertions.

    /// Defines types with #[cfg(test)] static assertions which checks
    /// types are the same as the platform's latest header files' ones.
    // Note: This macro is sys_ty!({ }), not sys_ty! { }.
    // An extra brace is used in input to make contents rustfmt-able.
    macro_rules! sys_type {
        ({$(
            $(#[$attr:meta])*
            $vis:vis type $([$($windows_path:ident)::+])? $name:ident = $ty:ty;
        )*}) => {
            $(
                $(#[$attr])*
                $vis type $name = $ty;
            )*
            #[cfg(any(test, portable_atomic_test_no_std_static_assert_ffi))]
            test_helper::static_assert_sys_type!($(
                $(#[$attr])*
                type $([$($windows_path)::+])? $name;
            )*);
        };
    }
    /// Defines #[repr(C)] structs with #[cfg(test)] static assertions which checks
    /// fields are the same as the platform's latest header files' ones.
    // Note: This macro is sys_struct!({ }), not sys_struct! { }.
    // An extra brace is used in input to make contents rustfmt-able.
    macro_rules! sys_struct {
        ({$(
            $(#[$attr:meta])*
            $vis:vis struct $([$($windows_path:ident)::+])? $name:ident {$(
                $(#[$field_attr:meta])*
                $field_vis:vis $field_name:ident: $field_ty:ty,
            )*}
        )*}) => {
            $(
                $(#[$attr])*
                #[derive(Clone, Copy)]
                #[cfg_attr(
                    any(test, portable_atomic_test_no_std_static_assert_ffi),
                    derive(Debug, PartialEq)
                )]
                #[repr(C)]
                $vis struct $name {$(
                    $(#[$field_attr])*
                    $field_vis $field_name: $field_ty,
                )*}
            )*
            #[cfg(any(test, portable_atomic_test_no_std_static_assert_ffi))]
            test_helper::static_assert_sys_struct!($(
                $(#[$attr])*
                struct $([$($windows_path)::+])? $name {$(
                    $(#[$field_attr])*
                    $field_name: $field_ty,
                )*}
            )*);
        };
    }
    /// Defines constants with #[cfg(test)] static assertions which checks
    /// values are the same as the platform's latest header files' ones.
    // Note: This macro is sys_const!({ }), not sys_const! { }.
    // An extra brace is used in input to make contents rustfmt-able.
    macro_rules! sys_const {
        ({$(
            $(#[$attr:meta])*
            $vis:vis const $([$($windows_path:ident)::+])? $name:ident: $ty:ty = $val:expr;
        )*}) => {
            $(
                $(#[$attr])*
                $vis const $name: $ty = $val;
            )*
            #[cfg(any(test, portable_atomic_test_no_std_static_assert_ffi))]
            test_helper::static_assert_sys_const!($(
                $(#[$attr])*
                const $([$($windows_path)::+])? $name: $ty;
            )*);
        };
    }
    /// Defines functions with #[cfg(test)] static assertions which checks
    /// signatures are the same as the platform's latest header files' ones.
    // Note: This macro is sys_fn!({ }), not sys_fn! { }.
    // An extra brace is used in input to make contents rustfmt-able.
    macro_rules! sys_fn {
        ({
            $(#[$extern_attr:meta])*
            extern $abi:tt {$(
                $(#[$fn_attr:meta])*
                $vis:vis fn $([$($windows_path:ident)::+])? $name:ident(
                    $($args:tt)*
                ) $(-> $ret_ty:ty)?;
            )*}
        }) => {
            $(#[$extern_attr])*
            extern $abi {$(
                $(#[$fn_attr])*
                $vis fn $name($($args)*) $(-> $ret_ty)?;
            )*}
            #[cfg(any(test, portable_atomic_test_no_std_static_assert_ffi))]
            test_helper::static_assert_sys_fn!(
                $(#[$extern_attr])*
                extern $abi {$(
                    $(#[$fn_attr])*
                    fn $([$($windows_path)::+])? $name($($args)*) $(-> $ret_ty)?;
                )*}
            );
        };
    }

    // -----------------------------------------------------------------------------
    // Helper macros for defining asm-based syscalls on Linux.
    //
    // Use asm-based syscall on Linux for compatibility with non-libc targets if possible.
    //
    // In non-Linux environments (including Android), syscalls should be called via libc,
    // mainly for the following reasons.
    // - asm-based syscalls are not permitted in the first place. e.g.,
    //   - OpenBSD https://github.com/golang/go/issues/36435
    //   - CheriBSD https://www.cheribsd.org/release-notes/25.03/index.html
    // - Stability of asm-based syscalls is not guaranteed. e.g.,
    //   - macOS https://go-review.googlesource.com/c/go/+/25495
    // - Syscalls via libc (or also libsys on FreeBSD) provide additional protections. e.g.,
    //   - OpenBSD https://lwn.net/Articles/806863/
    //   - CheriBSD https://www.cheribsd.org/release-notes/25.03/index.html
    //   - FreeBSD https://www.freebsd.org/status/report-2024-01-2024-03/libsys/
    //   - Android https://android.googlesource.com/platform/bionic/+/HEAD/docs/fdtrack.md
    //
    // Miri and Sanitizer do not support inline assembly.
    #[cfg(all(
        target_os = "linux",
        not(any(miri, portable_atomic_sanitize_thread)),
        not(portable_atomic_no_asm_syscall),
    ))]
    #[macro_use]
    mod syscall_helper {
        // Note:
        // - The syscall number and arguments must be extended to the register size by the caller of macros.
        //   The kernel extends the low bits on some architectures, but it does not on many architectures. e.g.,
        //   x86_64 (pre-5.14): https://github.com/torvalds/linux/commit/0595494891723a1dcca5eaa8eeca8ab54ad953b9
        //   powerpc64: https://github.com/torvalds/linux/blob/v7.1/arch/powerpc/kernel/syscall.c#L16
        //   riscv64: https://github.com/torvalds/linux/blob/v7.1/arch/riscv/kernel/traps.c#L328
        //   loongarch64: https://github.com/torvalds/linux/blob/v7.1/arch/loongarch/kernel/syscall.c#L61
        //
        // Refs:
        // - https://man7.org/linux/man-pages/man2/syscall.2.html
        // - aarch64 (test-only)
        //   https://git.musl-libc.org/cgit/musl/tree/arch/aarch64/syscall_arch.h?h=v1.2.6
        // - arm (test-only)
        //   https://git.musl-libc.org/cgit/musl/tree/arch/arm/syscall_arch.h?h=v1.2.6
        // - powerpc64 (test-only)
        //   https://github.com/torvalds/linux/blob/v7.1/Documentation/arch/powerpc/syscall64-abi.rst
        //   https://git.musl-libc.org/cgit/musl/tree/arch/powerpc64/syscall_arch.h?h=v1.2.6
        // - riscv32/riscv64
        //   https://git.musl-libc.org/cgit/musl/tree/arch/riscv32/syscall_arch.h?h=v1.2.6
        //   https://git.musl-libc.org/cgit/musl/tree/arch/riscv64/syscall_arch.h?h=v1.2.6

        #[cfg(test)] // test-only
        #[cfg(all(target_arch = "aarch64", target_pointer_width = "64"))]
        macro_rules! asm_syscall {
            (
                $number:ident, $r:ident,
                $($arg1:ident $(, $arg2:ident $(, $arg3:ident
                    $(, $arg4:ident $(, $arg5:ident $(, $arg6:ident )?)?)?
                )?)?)?
            ) => {
                __asm!(
                    "svc 0",
                    in("x8") $number,
                    lateout("x0") $r,
                    $(in("x0") $arg1,
                        $(in("x1") $arg2,
                            $(in("x2") $arg3,
                                $(in("x3") $arg4,
                                    $(in("x4") $arg5,
                                        $(in("x5") $arg6, )?
                                    )?
                                )?
                            )?
                        )?
                    )?
                    // Clobber SVE registers and do not use `preserves_flags` because
                    // AArch64 Linux syscalls clears non-v[0-31] bits of z[0-31], and all of p[0-15] and ffr,
                    // and calls SMSTOP SM which clears z[0-31], p[0-15], ffr, and modifies FPSR
                    // when CPU is in streaming SVE mode.
                    // https://github.com/torvalds/linux/blob/v7.1/Documentation/arch/arm64/sve.rst#3--system-call-behaviour
                    // https://github.com/torvalds/linux/blob/v7.1/Documentation/arch/arm64/sme.rst#3--system-call-behaviour
                    // https://developer.arm.com/documentation/109246/0101/SME-Overview/Streaming-SVE-mode
                    out("z0") _,
                    out("z1") _,
                    out("z2") _,
                    out("z3") _,
                    out("z4") _,
                    out("z5") _,
                    out("z6") _,
                    out("z7") _,
                    out("z8") _,
                    out("z9") _,
                    out("z10") _,
                    out("z11") _,
                    out("z12") _,
                    out("z13") _,
                    out("z14") _,
                    out("z15") _,
                    out("z16") _,
                    out("z17") _,
                    out("z18") _,
                    out("z19") _,
                    out("z20") _,
                    out("z21") _,
                    out("z22") _,
                    out("z23") _,
                    out("z24") _,
                    out("z25") _,
                    out("z26") _,
                    out("z27") _,
                    out("z28") _,
                    out("z29") _,
                    out("z30") _,
                    out("z31") _,
                    out("p0") _,
                    out("p1") _,
                    out("p2") _,
                    out("p3") _,
                    out("p4") _,
                    out("p5") _,
                    out("p6") _,
                    out("p7") _,
                    out("p8") _,
                    out("p9") _,
                    out("p10") _,
                    out("p11") _,
                    out("p12") _,
                    out("p13") _,
                    out("p14") _,
                    out("p15") _,
                    out("ffr") _,
                    options(nostack),
                )
            };
        }
        #[cfg(test)] // test-only
        #[cfg(target_arch = "arm")]
        macro_rules! asm_syscall {
            (
                $number_const:path, $number:literal, $r:ident,
                $($arg1:ident $(, $arg2:ident $(, $arg3:ident
                    $(, $arg4:ident $(, $arg5:ident $(, $arg6:ident )?)?)?
                )?)?)?
            ) => {{
                static_assert!($number_const == $number && 0 <= $number && $number <= 255);
                __asm!(
                    // r7 is reserved on thumb and MOV requires Thumb-2 or Arm mode.
                    // cfg(target_feature = "thumb-mode")/cfg(target_feature = "thumb2")
                    // doesn't work on stable and register swapping is much cheaper than syscall,
                    // so we always swap register and use MOVS instead of MOV.
                    // Note: r6 is reserved by LLVM, so this assembly may not work for syscall6
                    //       with Thumb-1 (pre-v7 Arm in Thumb mode) because the allocation of
                    //       `tmp` may fail.
                    //       (syscall6 is needless in the current our use cases.)
                    "movs {tmp}, r7",
                    concat!("movs r7, ", $number),
                    "svc 0",
                    "movs r7, {tmp}",
                    tmp = out(reg) _,
                    lateout("r0") $r,
                    $(in("r0") $arg1,
                        $(in("r1") $arg2,
                            $(in("r2") $arg3,
                                $(in("r3") $arg4,
                                    $(in("r4") $arg5,
                                        $(in("r5") $arg6, )?
                                    )?
                                )?
                            )?
                        )?
                    )?
                    // Do not use `preserves_flags` because MOVS modifies the flags.
                    // Do not use `nostack` because SVC pushes to stack on M-profile architectures.
                    // https://github.com/torvalds/linux/blob/v7.1/arch/arm/kernel/entry-header.S#L61
                )
            }};
        }
        // POWER9+ has fast syscall using SCV, but it is needless in the current our use cases.
        #[cfg(test)] // test-only
        #[cfg(all(target_arch = "powerpc64", target_pointer_width = "64"))]
        macro_rules! asm_syscall {
            (
                $number:ident, $r:ident,
                $($arg1:ident $(, $arg2:ident $(, $arg3:ident
                    $(, $arg4:ident $(, $arg5:ident $(, $arg6:ident )?)?)?
                )?)?)?
            ) => {
                __asm!(
                    "sc",
                    "bns+ 2f",
                    "neg %r3, %r3",
                    "2:",
                    inout("r0") $number => _,
                    lateout("r3") $r,
                    $(in("r3") $arg1,
                        $(in("r4") $arg2,
                            $(in("r5") $arg3,
                                $(in("r6") $arg4,
                                    $(in("r7") $arg5,
                                        $(in("r8") $arg6, )?
                                    )?
                                )?
                            )?
                        )?
                    )?
                    lateout("r4") _,
                    lateout("r5") _,
                    lateout("r6") _,
                    lateout("r7") _,
                    lateout("r8") _,
                    out("r9") _,
                    out("r10") _,
                    out("r11") _,
                    out("r12") _,
                    out("cr0") _,
                    out("ctr") _,
                    out("xer") _,
                    options(nostack, preserves_flags),
                )
            };
        }
        #[cfg(any(
            target_arch = "riscv32",
            all(target_arch = "riscv64", target_pointer_width = "64"),
        ))]
        macro_rules! asm_syscall {
            (
                $number:ident, $r:ident,
                $($arg1:ident $(, $arg2:ident $(, $arg3:ident
                    $(, $arg4:ident $(, $arg5:ident $(, $arg6:ident )?)?)?
                )?)?)?
            ) => {
                __asm!(
                    "ecall",
                    in("a7") $number,
                    lateout("a0") $r,
                    $(in("a0") $arg1,
                        $(in("a1") $arg2,
                            $(in("a2") $arg3,
                                $(in("a3") $arg4,
                                    $(in("a4") $arg5,
                                        $(in("a5") $arg6, )?
                                    )?
                                )?
                            )?
                        )?
                    )?
                    // Clobber vector registers and do not use `preserves_flags` because RISC-V Linux syscalls don't preserve them.
                    // https://github.com/torvalds/linux/blob/v7.1/Documentation/arch/riscv/vector.rst#3--vector-register-state-across-system-calls
                    out("v0") _,
                    out("v1") _,
                    out("v2") _,
                    out("v3") _,
                    out("v4") _,
                    out("v5") _,
                    out("v6") _,
                    out("v7") _,
                    out("v8") _,
                    out("v9") _,
                    out("v10") _,
                    out("v11") _,
                    out("v12") _,
                    out("v13") _,
                    out("v14") _,
                    out("v15") _,
                    out("v16") _,
                    out("v17") _,
                    out("v18") _,
                    out("v19") _,
                    out("v20") _,
                    out("v21") _,
                    out("v22") _,
                    out("v23") _,
                    out("v24") _,
                    out("v25") _,
                    out("v26") _,
                    out("v27") _,
                    out("v28") _,
                    out("v29") _,
                    out("v30") _,
                    out("v31") _,
                    options(nostack),
                )
            };
        }
    }

    #[allow(
        clippy::alloc_instead_of_core,
        clippy::std_instead_of_alloc,
        clippy::std_instead_of_core,
        clippy::undocumented_unsafe_blocks,
        clippy::wildcard_imports
    )]
    #[cfg(test)]
    mod tests {
        #[test]
        fn test_c_macro() {
            #[track_caller]
            fn t(s: &crate::utils::ffi::CStr, raw: &[u8]) {
                assert_eq!(s.to_bytes_with_nul(), raw);
            }
            t(c!(""), b"\0");
            t(c!("a"), b"a\0");
            t(c!("abc"), b"abc\0");
            t(c!(concat!("abc", "d")), b"abcd\0");
        }

        #[test]
        fn test_is_c_str() {
            #[track_caller]
            fn t(bytes: &[u8]) {
                assert_eq!(
                    super::_const_is_c_str(bytes),
                    std::ffi::CStr::from_bytes_with_nul(bytes).is_ok()
                );
            }
            t(b"\0");
            t(b"a\0");
            t(b"abc\0");
            t(b"");
            t(b"a");
            t(b"abc");
            t(b"\0a");
            t(b"\0a\0");
            t(b"ab\0c\0");
            t(b"\0\0");
        }
    }
}
