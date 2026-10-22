//! HelenOS IPC types
//!
//! Headers: <https://github.com/HelenOS/helenos/tree/master/uspace/lib/c/include/ipc>

pub(crate) mod loc {
    pub type service_id_t = crate::sysarg_t;
}

pub(crate) mod vfs {
    pub type fs_handle_t = i16;
    pub type fs_index_t = u32;
}
