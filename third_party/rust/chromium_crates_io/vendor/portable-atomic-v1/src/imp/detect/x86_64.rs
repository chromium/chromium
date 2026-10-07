// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Run-time CPU feature detection on x86_64 by using CPUID.

Adapted from https://github.com/rust-lang/rust/blob/1.92.0/library/std_detect/src/detect/os/x86.rs.
*/

#![cfg_attr(portable_atomic_sanitize_thread, allow(dead_code))]

// Miri doesn't support inline assembly used in __cpuid: https://github.com/rust-lang/miri/issues/932
// SGX doesn't support CPUID: https://github.com/rust-lang/rust/blob/1.92.0/library/std_detect/src/detect/os/x86.rs#L30-L33
#[cfg(any(target_env = "sgx", miri))]
compile_error!("internal error: this module is not supported on this environment");

include!("common.rs");

#[cfg(not(portable_atomic_no_asm))]
use core::arch::asm;
use core::arch::x86_64::CpuidResult;

// Workaround for https://github.com/rust-lang/rust/issues/101346
// It is not clear if our use cases are affected, but we implement this just in case.
//
// Refs:
// - https://www.felixcloutier.com/x86/cpuid
// - https://en.wikipedia.org/wiki/CPUID
// - https://github.com/rust-lang/stdarch/blob/a0c30f3e3c75adcd6ee7efc94014ebcead61c507/crates/core_arch/src/x86/cpuid.rs
#[cfg(not(target_env = "sgx"))]
fn __cpuid(leaf: u32) -> CpuidResult {
    let eax;
    let mut ebx;
    let ecx;
    let edx;
    // SAFETY: Calling `__cpuid` is safe on all x86_64 CPUs except for SGX,
    // which doesn't support `cpuid`.
    // https://github.com/rust-lang/stdarch/blob/a0c30f3e3c75adcd6ee7efc94014ebcead61c507/crates/core_arch/src/x86/cpuid.rs#L102-L109
    unsafe {
        asm!(
            "mov r8, rbx", // save rbx which is reserved by LLVM
            "cpuid",
            "xchg r8, rbx", // restore rbx
            out("r8") ebx,
            inout("eax") leaf => eax,
            inout("ecx") 0 => ecx,
            out("edx") edx,
            options(nostack, preserves_flags),
        );
    }
    CpuidResult { eax, ebx, ecx, edx }
}

#[cold]
#[must_use]
fn _detect(mut info: CpuInfo) -> CpuInfo {
    let CpuidResult { ecx: proc_info_ecx, .. } = __cpuid(1);

    // https://github.com/rust-lang/rust/blob/1.92.0/library/std_detect/src/detect/os/x86.rs#L104
    if test(proc_info_ecx, 13) {
        info.set(CpuInfoFlag::cmpxchg16b);
    }

    // We only use VMOVDQA when SSE is enabled. See _atomic_load_vmovdqa() in atomic128/x86_64.rs for more.
    #[cfg(target_feature = "sse")]
    {
        use core::arch::x86_64::_xgetbv;

        // https://github.com/rust-lang/rust/blob/1.92.0/library/std_detect/src/detect/os/x86.rs#L166-L236
        let cpu_xsave = test(proc_info_ecx, 26);
        if cpu_xsave {
            let cpu_osxsave = test(proc_info_ecx, 27);
            if cpu_osxsave {
                // SAFETY: Calling `_xgetbv` is safe because the CPU has `xsave` support
                // and OS has set `osxsave`.
                let xcr0 = unsafe { _xgetbv(0) };
                let os_avx_support = xcr0 & 6 == 6;
                if os_avx_support && test(proc_info_ecx, 28) {
                    info.set(CpuInfoFlag::avx);
                }
            }
        }
    }
    info
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
    use super::*;

    #[test]
    #[cfg_attr(portable_atomic_test_detect_false, ignore = "detection disabled")]
    fn test_cpuid() {
        // The recent Rosetta 2 unofficially implements AVX support.
        // (The OS reports it as unsupported, likely due to poor performance.)
        #[cfg(target_vendor = "apple")]
        assert_eq!(
            std::is_x86_feature_detected!("avx"),
            detect().avx() || cfg!(target_feature = "avx")
        );
        #[cfg(not(target_vendor = "apple"))]
        assert_eq!(std::is_x86_feature_detected!("avx"), detect().avx());
        assert_eq!(std::is_x86_feature_detected!("cmpxchg16b"), detect().cmpxchg16b());
    }
}
