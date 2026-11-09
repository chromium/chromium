//! Header: `sys/elf_common.h`
//!
//! <https://github.com/freebsd/freebsd-src/blob/main/sys/sys/elf_common.h>

// Legal values for p_type (segment type).
pub const PT_NULL: u32 = 0;
pub const PT_LOAD: u32 = 1;
pub const PT_DYNAMIC: u32 = 2;
pub const PT_INTERP: u32 = 3;
pub const PT_NOTE: u32 = 4;
pub const PT_SHLIB: u32 = 5;
pub const PT_PHDR: u32 = 6;
pub const PT_TLS: u32 = 7;
pub const PT_LOOS: u32 = 0x60000000;
pub const PT_HIOS: u32 = 0x6fffffff;
pub const PT_LOPROC: u32 = 0x70000000;
pub const PT_HIPROC: u32 = 0x7fffffff;

pub const PT_GNU_EH_FRAME: u32 = 0x6474e550;
pub const PT_GNU_RELRO: u32 = 0x6474e552;
