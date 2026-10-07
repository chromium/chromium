// SPDX-License-Identifier: Apache-2.0 OR MIT

/*
Run-time CPU feature detection on RISC-V Linux/Android by using riscv_hwprobe.

On RISC-V, detection using auxv only supports single-letter extensions.
So, we use riscv_hwprobe that supports multi-letter extensions.

Refs: https://github.com/torvalds/linux/blob/v6.19/Documentation/arch/riscv/hwprobe.rst
*/

include!("common.rs");

use core::ptr;

// libc requires Rust 1.63
#[allow(non_camel_case_types, non_upper_case_globals)]
mod ffi {
    pub(crate) use crate::utils::ffi::{c_long, c_size_t, c_uint, c_ulong};

    sys_struct!({
        // https://github.com/torvalds/linux/blob/v6.19/arch/riscv/include/uapi/asm/hwprobe.h
        pub(crate) struct riscv_hwprobe {
            pub(crate) key: i64,
            pub(crate) value: u64,
        }
    });

    sys_const!({
        pub(crate) const __NR_riscv_hwprobe: c_long = 258;

        // https://github.com/torvalds/linux/blob/v6.19/arch/riscv/include/uapi/asm/hwprobe.h
        // Linux 6.4+
        // https://github.com/torvalds/linux/commit/00e76e2c6a2bd3976d44d4a1fdd0b7a3c2566607
        pub(crate) const RISCV_HWPROBE_KEY_BASE_BEHAVIOR: i64 = 3;
        pub(crate) const RISCV_HWPROBE_BASE_BEHAVIOR_IMA: u64 = 1 << 0;
        pub(crate) const RISCV_HWPROBE_KEY_IMA_EXT_0: i64 = 4;
        // Linux 6.8+
        // https://github.com/torvalds/linux/commit/154a3706122978eeb34d8223d49285ed4f3c61fa
        pub(crate) const RISCV_HWPROBE_EXT_ZACAS: u64 = 1 << 34;
        // Linux 6.16+
        // https://github.com/torvalds/linux/commit/415a8c81da3dab0a585bd4f8d505a11ad5a171a7
        #[cfg(test)]
        pub(crate) const RISCV_HWPROBE_EXT_ZABHA: u64 = 1 << 58;
        // Linux 6.19+
        // https://github.com/torvalds/linux/commit/f4922b69165735e81752ee47d174f873e989a449
        #[cfg(test)]
        pub(crate) const RISCV_HWPROBE_EXT_ZALASR: u64 = 1 << 59;
    });

    cfg_sel!({
        // Use asm-based syscall on Linux for compatibility with non-libc targets if possible.
        // Do not use this on Android. See comments on syscall_helper module in src/utils.rs for details.
        #[cfg(all(
            target_os = "linux",
            any(
                target_arch = "riscv32",
                all(target_arch = "riscv64", target_pointer_width = "64"),
            ),
            not(portable_atomic_no_asm_syscall),
        ))]
        {
            use crate::utils::{RegISize, RegSize};

            #[inline]
            pub(crate) unsafe fn syscall5(
                number: c_long,
                arg1: *mut riscv_hwprobe,
                arg2: c_size_t,
                arg3: c_size_t,
                arg4: *mut c_ulong,
                arg5: c_uint,
            ) -> c_long {
                #[allow(clippy::cast_possible_truncation)]
                let number = number as RegISize;
                let arg1 = ptr_reg!(arg1);
                let arg2 = arg2 as RegSize;
                let arg3 = arg3 as RegSize;
                let arg4 = ptr_reg!(arg4);
                let arg5 = arg5 as RegSize;
                let r: RegISize;
                // SAFETY: the caller must uphold the safety contract.
                // we've extended the syscall number and arguments to the register size.
                unsafe {
                    asm_syscall!(number, r, arg1, arg2, arg3, arg4, arg5);
                }
                #[allow(clippy::cast_possible_truncation)]
                {
                    r as c_long
                }
            }
        }
        #[cfg(else)]
        {
            sys_fn!({
                extern "C" {
                    // https://man7.org/linux/man-pages/man2/syscall.2.html
                    pub(crate) fn syscall(number: c_long, ...) -> c_long;
                }
            });
            pub(crate) use self::syscall as syscall5;
        }
    });

    // https://github.com/torvalds/linux/blob/v6.19/Documentation/arch/riscv/hwprobe.rst
    pub(crate) unsafe fn __riscv_hwprobe(
        pairs: *mut riscv_hwprobe,
        pair_count: c_size_t,
        cpu_set_size: c_size_t,
        cpus: *mut c_ulong,
        flags: c_uint,
    ) -> c_long {
        // SAFETY: the caller must uphold the safety contract.
        unsafe { syscall5(__NR_riscv_hwprobe, pairs, pair_count, cpu_set_size, cpus, flags) }
    }
}

// syscall returns an unsupported error if riscv_hwprobe is not supported,
// so we can safely use this function on older versions of Linux.
fn riscv_hwprobe(out: &mut [ffi::riscv_hwprobe]) -> bool {
    let len = out.len();
    // SAFETY: We've passed the valid pointer and length,
    // passing null ptr for cpus is safe because cpu_set_size is zero.
    unsafe { ffi::__riscv_hwprobe(out.as_mut_ptr(), len, 0, ptr::null_mut(), 0) == 0 }
}

#[cold]
#[must_use]
fn _detect(mut info: CpuInfo) -> CpuInfo {
    let mut out = [
        ffi::riscv_hwprobe { key: ffi::RISCV_HWPROBE_KEY_BASE_BEHAVIOR, value: 0 },
        ffi::riscv_hwprobe { key: ffi::RISCV_HWPROBE_KEY_IMA_EXT_0, value: 0 },
    ];
    if riscv_hwprobe(&mut out)
        && out[0].key != -1
        && out[0].value & ffi::RISCV_HWPROBE_BASE_BEHAVIOR_IMA != 0
        && out[1].key != -1
    {
        let value = out[1].value;
        macro_rules! check {
            ($flag:ident, $bit:ident) => {
                if value & ffi::$bit != 0 {
                    info.set(CpuInfoFlag::$flag);
                }
            };
        }
        check!(zacas, RISCV_HWPROBE_EXT_ZACAS);
        #[cfg(test)]
        check!(zabha, RISCV_HWPROBE_EXT_ZABHA);
        #[cfg(test)]
        check!(zalasr, RISCV_HWPROBE_EXT_ZALASR);
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

    // We use asm-based syscall for compatibility with non-libc targets.
    // This test tests that our ones and libc::syscall returns the same result.
    #[test]
    fn test_alternative() {
        unsafe fn __riscv_hwprobe_libc(
            pairs: *mut ffi::riscv_hwprobe,
            pair_count: ffi::c_size_t,
            cpu_set_size: ffi::c_size_t,
            cpus: *mut ffi::c_ulong,
            flags: ffi::c_uint,
        ) -> ffi::c_long {
            // SAFETY: the caller must uphold the safety contract.
            unsafe {
                libc::syscall(ffi::__NR_riscv_hwprobe, pairs, pair_count, cpu_set_size, cpus, flags)
            }
        }
        fn riscv_hwprobe_libc(out: &mut [ffi::riscv_hwprobe]) -> bool {
            let len = out.len();
            unsafe { __riscv_hwprobe_libc(out.as_mut_ptr(), len, 0, ptr::null_mut(), 0) == 0 }
        }
        let mut out = [
            ffi::riscv_hwprobe { key: ffi::RISCV_HWPROBE_KEY_BASE_BEHAVIOR, value: 0 },
            ffi::riscv_hwprobe { key: ffi::RISCV_HWPROBE_KEY_IMA_EXT_0, value: 0 },
        ];
        let mut libc_out = out;
        assert_eq!(riscv_hwprobe(&mut out), riscv_hwprobe_libc(&mut libc_out));
        assert_eq!(out, libc_out);
    }
}
