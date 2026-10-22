//! HelenOS libraries
//!
//! * HelenOS source tree: <https://github.com/HelenOS/helenos> (libraries are in uspace/lib, some bits are in abi and common)

pub(crate) mod abi {
    pub(crate) mod errno;
}
pub(crate) mod bits;
pub(crate) mod dirent_mod;
pub(crate) mod errno;
pub(crate) mod fibril;
pub(crate) mod fibril_synch;
pub(crate) mod inet {
    pub(crate) mod addr;
    pub(crate) mod dnsr;
    pub(crate) mod endpoint;
    pub(crate) mod tcp;
}
pub(crate) mod ipc;
pub(crate) mod loc;
pub(crate) mod offset;
pub(crate) mod stdio;
pub(crate) mod stdlib;
pub(crate) mod time;
pub(crate) mod vfs {
    // The module layout mirrors the `vfs/vfs.h` header path.
    #[allow(clippy::module_inception)]
    pub(crate) mod vfs;
}
