//! Directory: `arch/*/include/uapi/asm/` (arch-specific versions)
//! Directory: `include/uapi/asm-generic/` (default versions)

#[cfg_attr(
    any(
        target_arch = "mips",
        target_arch = "mips32r6",
        target_arch = "mips",
        target_arch = "mips32r6",
    ),
    path = "../arch/mips/asm/socket.rs"
)]
#[cfg_attr(
    any(target_arch = "powerpc", target_arch = "powerpc64"),
    path = "../arch/powerpc/asm/socket.rs"
)]
#[cfg_attr(
    any(target_arch = "sparc", target_arch = "sparc64"),
    path = "../arch/sparc/asm/socket.rs"
)]
pub(crate) mod socket;
