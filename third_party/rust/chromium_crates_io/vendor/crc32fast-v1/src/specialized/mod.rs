cfg_if::cfg_if! {
    if #[cfg(all(
        target_feature = "sse2",
        any(target_arch = "x86", target_arch = "x86_64")
    ))] {
        mod pclmulqdq;
        pub use self::pclmulqdq::State;
    } else if #[cfg(all(stable_arm_crc32_intrinsics, target_arch = "aarch64"))] {
        mod aarch64;
        pub use self::aarch64::State;
    } else {
        #[derive(Clone)]
        pub enum State {}
        impl State {
            pub fn new() -> Option<Self> {
                None
            }

            pub fn update(&self, _state: u32, _buf: &[u8]) -> u32 {
                match *self {}
            }
        }
    }
}
