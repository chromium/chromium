//! Source directory: `arch/`
//!
//! * Headers: <https://git.musl-libc.org/cgit/musl/tree/arch> (official)
//! * Headers: <https://github.com/kraj/musl/tree/master/arch> (mirror)

pub(crate) mod generic;

#[cfg(target_arch = "mips")]
pub(crate) mod mips;
#[cfg(target_arch = "mips64")]
pub(crate) mod mips64;
