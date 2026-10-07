// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Run-time CPU feature detection on AArch64 Apple targets by using sysctlbyname.

On macOS, this module is currently only enabled on tests because there are no
instructions that were not available on the M1 but are now available on the
latest Apple hardware and this library currently wants to use:

```console
$ LC_ALL=C comm -23 <(rustc --print cfg --target aarch64-apple-darwin -C target-cpu=apple-m5 | grep -F target_feature) <(rustc --print cfg --target aarch64-apple-darwin | grep -F target_feature)
target_feature="bf16"
target_feature="bti"
target_feature="cssc"
target_feature="ecv"
target_feature="hbc"
target_feature="i8mm"
target_feature="mte"
target_feature="sme"
target_feature="sme-b16b16"
target_feature="sme-f16f16"
target_feature="sme-f64f64"
target_feature="sme-i16i64"
target_feature="sme2"
target_feature="sme2p1"
target_feature="sve-b16b16"
target_feature="v8.5a"
target_feature="v8.6a"
target_feature="v8.7a"
target_feature="wfxt"
```

Non-macOS targets doesn't always supports FEAT_LSE2, so we use this module on them.
As used in the example in https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms#Enable-DIT-for-constant-time-cryptographic-operations,
sysctlbyname is supported on all Apple platforms.

Refs: https://developer.apple.com/documentation/kernel/1387446-sysctlbyname/determining_instruction_set_characteristics
*/

include!("common.rs");

use core::{mem, ptr};

// libc requires Rust 1.63
mod ffi {
    pub(crate) use crate::utils::ffi::{CStr, c_char, c_int, c_size_t, c_void};

    #[cfg(test)] // test-only
    sys_const!({
        // https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/osfmk/arm/cpu_capabilities_public.h
        pub(crate) const CAP_BIT_FEAT_LSE: usize = 6;
        pub(crate) const CAP_BIT_FEAT_LRCPC: usize = 15;
        pub(crate) const CAP_BIT_FEAT_LRCPC2: usize = 16;
        pub(crate) const CAP_BIT_FEAT_LSE2: usize = 30;
        pub(crate) const CAP_BIT_NB: usize = 92;
    });

    sys_fn!({
        extern "C" {
            // https://developer.apple.com/documentation/kernel/1387446-sysctlbyname
            // https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/bsd/sys/sysctl.h
            pub(crate) fn sysctlbyname(
                name: *const c_char,
                old_p: *mut c_void,
                old_len_p: *mut c_size_t,
                new_p: *mut c_void,
                new_len: c_size_t,
            ) -> c_int;
        }
    });
}

fn sysctlbyname32(name: &ffi::CStr) -> Option<u32> {
    const OUT_LEN: ffi::c_size_t = mem::size_of::<u32>() as ffi::c_size_t;

    let mut out = 0_u32;
    let mut out_len = OUT_LEN;
    // SAFETY:
    // - `name` a valid C string.
    // - `out_len` does not exceed the size of `out`.
    // - `sysctlbyname` is thread-safe.
    let res = unsafe {
        ffi::sysctlbyname(
            name.as_ptr(),
            (&mut out as *mut u32).cast::<ffi::c_void>(),
            &mut out_len,
            ptr::null_mut(),
            0,
        )
    };
    if res != 0 {
        return None;
    }
    debug_assert_eq!(out_len, OUT_LEN);
    Some(out)
}

#[cold]
#[must_use]
fn _detect(mut info: CpuInfo) -> CpuInfo {
    macro_rules! check {
        ($flag:ident, $($name:tt) ||+) => {
            if $(sysctlbyname32(c!($name)).unwrap_or(0) != 0) ||+ {
                info.set(CpuInfoFlag::$flag);
            }
        };
    }

    // On macOS, AArch64 support was added in macOS 11,
    // hw.optional.armv8_1_atomics is available on macOS 11+,
    // hw.optional.arm.FEAT_* are only available on macOS 12+.
    // Query both names in case future versions of macOS remove the old name.
    // https://github.com/golang/go/commit/c15593197453b8bf90fc3a9080ba2afeaf7934ea
    // https://github.com/google/boringssl/commit/91e0b11eba517d83b910b20fe3740eeb39ecb37e
    // hw.optional.arm.caps is available on macOS 15+.
    // https://github.com/llvm/llvm-project/blob/llvmorg-22.1.0/compiler-rt/lib/builtins/cpu_model/aarch64/fmv/apple.inc#L56
    check!(lse, "hw.optional.arm.FEAT_LSE" || "hw.optional.armv8_1_atomics");
    check!(lse2, "hw.optional.arm.FEAT_LSE2");
    #[cfg(test)]
    check!(rcpc, "hw.optional.arm.FEAT_LRCPC");
    #[cfg(test)]
    check!(rcpc2, "hw.optional.arm.FEAT_LRCPC2");
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
    use std::{format, process::Command, str, string::String};

    use super::*;

    #[test]
    fn test_alternative() {
        use crate::utils::ffi::*;

        const CAPS_LEN: usize = (ffi::CAP_BIT_NB + 63) / 64;
        fn arm_caps(caps: &mut [u64; CAPS_LEN]) -> bool {
            let mut caps_len = CAPS_LEN * 8;
            // SAFETY:
            // - name a valid C string.
            // - `caps_len` does not exceed the size of `caps`.
            // - `sysctlbyname` is thread-safe.
            let res = unsafe {
                ffi::sysctlbyname(
                    c!("hw.optional.arm.caps").as_ptr(),
                    caps.as_mut_ptr().cast::<ffi::c_void>(),
                    &mut caps_len,
                    ptr::null_mut(),
                    0,
                )
            };
            res == 0
        }

        // Call sysctl command instead of libc API.
        //
        // This is used only for testing.
        struct SysctlHwOptionalOutput(String);
        impl SysctlHwOptionalOutput {
            fn new() -> Self {
                let output = Command::new("sysctl").arg("hw.optional").output().unwrap();
                assert!(output.status.success());
                let stdout = String::from_utf8(output.stdout).unwrap();
                test_helper::eprintln_nocapture!("sysctl hw.optional:\n{}", stdout);
                Self(stdout)
            }
            fn field(&self, name: &CStr) -> Option<u32> {
                let name = name.to_bytes_with_nul();
                let name = str::from_utf8(&name[..name.len() - 1]).unwrap();
                let prefix = format!("{}: ", name);
                Some(self.0.lines().find_map(|s| s.strip_prefix(&prefix))?.parse().unwrap())
            }
        }

        let mut caps = [0_u64; CAPS_LEN];
        let has_caps = arm_caps(&mut caps);
        let sysctl_output = SysctlHwOptionalOutput::new();
        for (name, expected_on_macos, cap_bit) in [
            (c!("hw.optional.arm.FEAT_LSE"), Some(1), ffi::CAP_BIT_FEAT_LSE),
            (c!("hw.optional.armv8_1_atomics"), Some(1), ffi::CAP_BIT_FEAT_LSE),
            (c!("hw.optional.arm.FEAT_LSE2"), Some(1), ffi::CAP_BIT_FEAT_LSE2),
            (c!("hw.optional.arm.FEAT_LSE128"), None, 0),
            (c!("hw.optional.arm.FEAT_LSFE"), None, 0),
            (c!("hw.optional.arm.FEAT_LRCPC"), Some(1), ffi::CAP_BIT_FEAT_LRCPC),
            (c!("hw.optional.arm.FEAT_LRCPC2"), Some(1), ffi::CAP_BIT_FEAT_LRCPC2),
            (c!("hw.optional.arm.FEAT_LRCPC3"), None, 0),
        ] {
            let res = sysctlbyname32(name);
            if res.is_none() {
                assert_eq!(std::io::Error::last_os_error().kind(), std::io::ErrorKind::NotFound);
            }
            if cfg!(any(target_os = "macos", target_abi = "macabi")) {
                assert_eq!(
                    res,
                    expected_on_macos,
                    "{}",
                    str::from_utf8(name.to_bytes_with_nul()).unwrap()
                );
            }
            if let Some(res) = res {
                assert_eq!(res, sysctl_output.field(name).unwrap());
                if has_caps {
                    assert_eq!(caps[cap_bit / 64] & (1 << (cap_bit & 63)) != 0, res != 0);
                }
            } else {
                assert!(sysctl_output.field(name).is_none());
            }
        }
    }
}
