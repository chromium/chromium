use crate::prelude::*;

pub type c_char = u8;
pub type c_long = i32;
pub type c_ulong = u32;
pub type clock_t = u32;
pub type wchar_t = u32;
pub type time_t = i64;
pub type suseconds_t = i32;
pub type register_t = i32;
pub type __greg_t = c_uint;
pub type __gregset_t = [crate::__greg_t; 17];

s_no_extra_traits! {
    pub struct mcontext_t {
        pub __gregs: crate::__gregset_t,
        pub mc_vfp_size: usize,
        pub mc_vfp_ptr: *mut c_void,
        pub mc_spare: [c_uint; 33],
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        impl PartialEq for mcontext_t {
            fn eq(&self, other: &mcontext_t) -> bool {
                self.__gregs == other.__gregs
                    && self.mc_vfp_size == other.mc_vfp_size
                    && self.mc_vfp_ptr == other.mc_vfp_ptr
                    && self
                        .mc_spare
                        .iter()
                        .zip(other.mc_spare.iter())
                        .all(|(a, b)| a == b)
            }
        }
        impl Eq for mcontext_t {}
        impl fmt::Debug for mcontext_t {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                f.debug_struct("mcontext_t")
                    .field("__gregs", &self.__gregs)
                    .field("mc_vfp_size", &self.mc_vfp_size)
                    .field("mc_vfp_ptr", &self.mc_vfp_ptr)
                    .field("mc_spare", &self.mc_spare)
                    .finish()
            }
        }
        impl hash::Hash for mcontext_t {
            fn hash<H: hash::Hasher>(&self, state: &mut H) {
                self.__gregs.hash(state);
                self.mc_vfp_size.hash(state);
                self.mc_vfp_ptr.hash(state);
                self.mc_spare.hash(state);
            }
        }
    }
}

pub(crate) const _ALIGNBYTES: usize = mem::size_of::<c_int>() - 1;

pub const BIOCSRTIMEOUT: c_ulong = 0x8010426d;
pub const BIOCGRTIMEOUT: c_ulong = 0x4010426e;

pub const MAP_32BIT: c_int = 0x00080000;
pub const MINSIGSTKSZ: size_t = 4096; // 1024 * 4
pub const TIOCTIMESTAMP: c_ulong = 0x40107459;
