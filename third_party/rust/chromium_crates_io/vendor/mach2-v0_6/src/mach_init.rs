//! This module corresponds to `mach/mach_init.h`.

use crate::mach_types::thread_port_t;

unsafe extern "C" {
    pub fn mach_thread_self() -> thread_port_t;
    pub fn mach_host_self() -> thread_port_t;
}

#[cfg(test)]
mod tests {
    use crate::mach_init::*;
    use crate::port::*;

    #[test]
    fn mach_thread_self_test() {
        unsafe {
            let this_thread = mach_thread_self();
            assert!(this_thread != MACH_PORT_NULL);
            assert!(this_thread != MACH_PORT_DEAD);
        }
    }
}
