// SPDX-License-Identifier: Apache-2.0 OR MIT

#[derive(Clone, Copy)]
#[repr(transparent)]
pub(crate) struct CpuInfo(u32);

impl CpuInfo {
    #[inline]
    fn set(&mut self, bit: CpuInfoFlag) {
        self.0 = set(self.0, bit as u32);
    }
    #[inline]
    #[must_use]
    fn test(self, bit: CpuInfoFlag) -> bool {
        test(self.0, bit as u32)
    }
}

#[inline]
#[must_use]
fn set(x: u32, bit: u32) -> u32 {
    x | (1 << bit)
}
#[inline]
#[must_use]
fn test(x: u32, bit: u32) -> bool {
    x & (1 << bit) != 0
}

#[inline]
pub(crate) fn detect() -> CpuInfo {
    use core::sync::atomic::{AtomicU32, Ordering};

    static CACHE: AtomicU32 = AtomicU32::new(0);
    let mut info = CpuInfo(CACHE.load(Ordering::Relaxed));
    if info.0 != 0 {
        return info;
    }
    info.set(CpuInfoFlag::Init);
    // Note: detect_false cfg is intended to make it easy for developers to test
    // cases where features usually available is not available, and is not a public API.
    if !cfg!(portable_atomic_test_detect_false) {
        info = _detect(info);
    }
    if cfg!(any(
        all(
            target_arch = "x86_64",
            not(any(target_feature = "cmpxchg16b", portable_atomic_target_feature = "cmpxchg16b")),
        ),
        all(
            target_arch = "powerpc64",
            not(any(
                target_feature = "quadword-atomics",
                portable_atomic_target_feature = "quadword-atomics",
            )),
        ),
        all(
            any(target_arch = "riscv32", target_arch = "riscv64"),
            not(any(target_feature = "zacas", portable_atomic_target_feature = "zacas")),
        ),
    )) {
        // Use CAS if the detection result affects lock-freeness.
        match CACHE.compare_exchange(0, info.0, Ordering::Relaxed, Ordering::Relaxed) {
            Ok(_) => info,
            Err(info) => {
                debug_assert_ne!(info, 0);
                CpuInfo(info)
            }
        }
    } else {
        CACHE.store(info.0, Ordering::Relaxed);
        info
    }
}

macro_rules! flags {
    ($(
        $(#[$attr:meta])*
        $func:ident($name:literal, any($($cfg:ident),*)),
    )*) => {
        #[allow(dead_code, non_camel_case_types)]
        #[derive(Clone, Copy)]
        #[cfg_attr(test, derive(PartialEq, Eq, PartialOrd, Ord))]
        #[repr(u32)]
        enum CpuInfoFlag {
            Init = 0,
            $($func,)*
        }
        impl CpuInfo {
            $(
                $(#[$attr])*
                #[cfg(any(test, not(any($($cfg = $name),*))))]
                #[inline]
                #[must_use]
                pub(crate) fn $func(self) -> bool {
                    self.test(CpuInfoFlag::$func)
                }
            )*
            #[cfg(test)] // for test
            const ALL_FLAGS: &'static [(&'static str, CpuInfoFlag, bool)] = &[$(
                ($name, CpuInfoFlag::$func, cfg!(any($($cfg = $name),*))),
            )*];
        }
        #[test]
        #[cfg_attr(portable_atomic_test_detect_false, ignore = "detection disabled")]
        fn test_detect() {$(
            $(#[$attr])*
            {
                const _: u32 = 1_u32 << CpuInfoFlag::$func as u32;
                assert_eq!($name.replace(|c: char| c == '-' || c == '.', "_"), stringify!($func));
                if detect().$func() {
                    assert!(detect().test(CpuInfoFlag::$func));
                } else {
                    assert!(!detect().test(CpuInfoFlag::$func));
                }
            }
        )*}
    };
}

// rustc definitions: https://github.com/rust-lang/rust/blob/ddaf12390d3ffb7d5ba74491a48f3cd528e5d777/compiler/rustc_target/src/target_features.rs

// LLVM definitions: https://github.com/llvm/llvm-project/blob/llvmorg-22.1.0-rc1/llvm/lib/Target/AArch64/AArch64Features.td
#[cfg(any(target_arch = "aarch64", target_arch = "arm64ec"))]
flags! {
    // The Armv8.1 architecture extension
    // https://developer.arm.com/documentation/109697/2025_06/Feature-descriptions/The-Armv8-1-architecture-extension
    // FEAT_LSE, Large System Extensions
    // > This feature is supported in AArch64 state only.
    // > FEAT_LSE is OPTIONAL from Armv8.0.
    // > FEAT_LSE is mandatory from Armv8.1.
    lse("lse", any(target_feature /* 1.61+ */, portable_atomic_target_feature)),

    // The Armv8.3 architecture extension
    // https://developer.arm.com/documentation/109697/2025_06/Feature-descriptions/The-Armv8-3-architecture-extension
    // FEAT_LRCPC, Load-Acquire RCpc instructions
    // > These instructions are added to the A64 instruction set only.
    // > FEAT_LRCPC is OPTIONAL from Armv8.2.
    // > FEAT_LRCPC is mandatory from Armv8.3.
    #[cfg(test)] // test-only
    rcpc("rcpc", any(target_feature /* 1.61+ */)),

    // The Armv8.4 architecture extension
    // https://developer.arm.com/documentation/109697/2025_06/Feature-descriptions/The-Armv8-4-architecture-extension
    // FEAT_LSE2, Large System Extensions version 2
    // > This feature is supported in AArch64 state only.
    // > FEAT_LSE2 is OPTIONAL from Armv8.2.
    // > FEAT_LSE2 is mandatory from Armv8.4.
    #[cfg_attr(not(test), allow(dead_code))]
    lse2("lse2", any(target_feature /* nightly */, portable_atomic_target_feature)),
    // FEAT_LRCPC2, Load-Acquire RCpc instructions version 2
    // > These instructions are added to the A64 instruction set only.
    // > FEAT_LRCPC2 is OPTIONAL from Armv8.2.
    // > FEAT_LRCPC2 is mandatory from Armv8.4.
    // > If FEAT_LRCPC2 is implemented, then FEAT_LRCPC is implemented.
    #[cfg(test)] // test-only
    rcpc2("rcpc2", any(target_feature /* 1.61+ */)),

    // The Armv8.9 architecture extension
    // https://developer.arm.com/documentation/109697/2025_06/Feature-descriptions/The-Armv8-9-architecture-extension
    // FEAT_LRCPC3, Load-Acquire RCpc instructions version 3
    // > This feature is supported in AArch64 state only.
    // > FEAT_LRCPC3 is OPTIONAL from Armv8.2.
    // > If FEAT_LRCPC3 is implemented, then FEAT_LRCPC2 is implemented.
    #[cfg_attr(not(test), allow(dead_code))]
    rcpc3("rcpc3", any(target_feature /* nightly */, portable_atomic_target_feature)),

    // The Armv9.4 architecture extension
    // https://developer.arm.com/documentation/109697/2025_06/Feature-descriptions/The-Armv9-4-architecture-extension
    // FEAT_LSE128, 128-bit Atomics
    // > This feature is supported in AArch64 state only.
    // > FEAT_LSE128 is OPTIONAL from Armv9.3.
    // > If FEAT_LSE128 is implemented, then FEAT_LSE is implemented.
    #[cfg_attr(not(test), allow(dead_code))]
    lse128("lse128", any(target_feature /* nightly */, portable_atomic_target_feature)),

    // The Armv9.6 architecture extension
    // https://developer.arm.com/documentation/109697/2025_06/Feature-descriptions/The-Armv9-6-architecture-extension
    // FEAT_LS64WB, LS64 for Write-back cacheable memory
    // > This feature is supported in AArch64 state only.
    // > FEAT_LS64WB is OPTIONAL from Armv9.2.
    // > If FEAT_LS64WB is implemented, then FEAT_LS64_ACCDATA is implemented.
    #[cfg(test)] // test-only
    ls64wb("ls64wb", any(/* no corresponding target feature */)),
    // FEAT_LSFE, Large System Float Extension
    // > This feature is supported in AArch64 state only.
    // > FEAT_LSFE is OPTIONAL from Armv9.3.
    // > If FEAT_LSFE is implemented, then FEAT_FP is implemented.
    #[cfg(test)] // test-only
    lsfe("lsfe", any(target_feature /* N/A */, portable_atomic_target_feature)),

    #[cfg(test)] // test-only
    cpuid("cpuid", any(/* no corresponding target feature */)),
}

// LLVM definitions: https://github.com/llvm/llvm-project/blob/llvmorg-22.1.0-rc1/llvm/lib/Target/ARM/ARMFeatures.td
#[cfg(target_arch = "arm")]
flags! {
    #[cfg(test)] // test-only
    lpae("lpae", any(/* no corresponding target feature */)),
}

// LLVM definitions: https://github.com/llvm/llvm-project/blob/llvmorg-22.1.0-rc1/llvm/lib/Target/PowerPC/PPC.td
#[cfg(target_arch = "powerpc64")]
flags! {
    // lqarx and stqcx.
    quadword_atomics("quadword-atomics", any(target_feature /* nightly */, portable_atomic_target_feature)),
}

// LLVM definitions: https://github.com/llvm/llvm-project/blob/llvmorg-22.1.0-rc1/llvm/lib/Target/RISCV/RISCVFeatures.td
#[cfg(any(target_arch = "riscv32", target_arch = "riscv64"))]
flags! {
    // amocas.{w,d,q}
    zacas("zacas", any(target_feature /* 1.94+ */, portable_atomic_target_feature)),
    #[cfg(test)] // test-only
    zabha("zabha", any(target_feature /* 1.94+ */, portable_atomic_target_feature)),
    #[cfg(test)] // test-only
    zalasr("zalasr", any(/* no corresponding target feature */)),
}

// LLVM definitions: https://github.com/llvm/llvm-project/blob/llvmorg-22.1.0-rc1/llvm/lib/Target/X86/X86.td
#[cfg(target_arch = "x86_64")]
flags! {
    // avx
    #[cfg(target_feature = "sse")]
    avx("avx", any(target_feature)),
    // cmpxchg16b
    cmpxchg16b("cmpxchg16b", any(target_feature /* 1.69+ */, portable_atomic_target_feature)),
}

#[allow(
    clippy::alloc_instead_of_core,
    clippy::std_instead_of_alloc,
    clippy::std_instead_of_core,
    clippy::undocumented_unsafe_blocks,
    clippy::wildcard_imports
)]
#[cfg(test)]
mod tests_common {
    use std::{collections::BTreeSet, vec};

    use super::*;

    #[test]
    fn test_bit_flags() {
        let mut flags = vec![("init", CpuInfoFlag::Init)];
        flags.extend(CpuInfo::ALL_FLAGS.iter().map(|&(name, flag, _)| (name, flag)));
        let flag_set = flags.iter().map(|(_, flag)| flag).collect::<BTreeSet<_>>();
        let name_set = flags.iter().map(|(name, _)| name).collect::<BTreeSet<_>>();
        if flag_set.len() != flags.len() {
            panic!("CpuInfo flag values must be unique")
        }
        if name_set.len() != flags.len() {
            panic!("CpuInfo flag names must be unique")
        }

        let mut x = CpuInfo(0);
        for &(_, f) in &flags {
            assert!(!x.test(f));
        }
        for i in 0..flags.len() {
            x.set(flags[i].1);
            for &(_, f) in &flags[..i + 1] {
                assert!(x.test(f));
            }
            for &(_, f) in &flags[i + 1..] {
                assert!(!x.test(f));
            }
        }
        for &(_, f) in &flags {
            assert!(x.test(f));
        }
    }

    #[test]
    fn print_features() {
        use std::{fmt::Write as _, string::String};

        let mut features = String::new();
        features.push_str("\nfeatures:\n");
        for &(name, flag, compile_time) in CpuInfo::ALL_FLAGS {
            let run_time = detect().test(flag);
            if run_time == compile_time {
                let _ = writeln!(features, "  {}: {}", name, run_time);
            } else {
                let _ = writeln!(
                    features,
                    "  {}: {} (compile-time), {} (run-time)",
                    name, compile_time, run_time
                );
            }
        }
        test_helper::eprintln_nocapture!("{}", features);
    }

    // Static assertions for C type definitions.
    // Assertions with core::ffi types are in crate::utils::ffi module.
    #[cfg(not(any(windows, target_arch = "x86", target_arch = "x86_64")))]
    const _: fn() = || {
        use test_helper::sys;
        let _: crate::utils::ffi::c_char = 0 as sys::c_char;
    };
}
