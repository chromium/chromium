//! This module corresponds to `mach/thread_state.h`.

use crate::kern_return::kern_return_t;
use crate::mach_types::thread_t;

unsafe extern "C" {
    pub fn thread_get_register_pointer_values(
        thread: thread_t,
        sp: *mut usize,
        length: *mut usize,
        values: *mut usize,
    ) -> kern_return_t;
}
