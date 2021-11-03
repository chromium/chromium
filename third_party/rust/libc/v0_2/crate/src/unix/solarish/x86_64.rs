pub type greg_t = ::c_long;

s! {
    pub struct __c_anonymous_fpchip_state {
        pub cw: u16,
        pub sw: u16,
        pub fctw: u8,
        pub __fx_rsvd: u8,
        pub fop: u16,
        pub rip: u64,
        pub rdp: u64,
        pub mxcsr: u32,
        pub mxcsr_mask: u32,
        pub st: [::upad128_t; 8],
        pub xmm: [::upad128_t; 16],
        pub __fx_ign: [::upad128_t; 6],
        pub status: u32,
        pub xstatus: u32,
    }
}

s_no_extra_traits! {
    #[cfg(libc_union)]
    pub union __c_anonymous_fp_reg_set {
        pub fpchip_state: __c_anonymous_fpchip_state,
        pub f_fpregs: [[u32; 13]; 10],
    }

    pub struct fpregset_t {
        pub fp_reg_set: __c_anonymous_fp_reg_set,
    }

    pub struct mcontext_t {
        pub gregs: [::greg_t; 28],
        pub fpgregs: fpregset_t,
    }

    pub struct ucontext_t {
        pub uc_flags: ::c_ulong,
        pub uc_link: *mut ucontext_t,
        pub uc_sigmask: ::sigset_t,
        pub uc_stack: ::stack_t,
        pub uc_mcontext: mcontext_t,
        pub uc_filler: [::c_long; 5],
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        #[cfg(libc_union)]
        impl PartialEq for __c_anonymous_fp_reg_set {
            fn eq(&self, other: &__c_anonymous_fp_reg_set) -> bool {
                unsafe {
                self.fpchip_state == other.fpchip_state ||
                    self.
                    f_fpregs.
                    iter().
                    zip(other.f_fpregs.iter()).
                    all(|(a, b)| a == b)
                }
            }
        }
        #[cfg(libc_union)]
        impl Eq for __c_anonymous_fp_reg_set {}
        #[cfg(libc_union)]
        impl ::fmt::Debug for __c_anonymous_fp_reg_set {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                unsafe {
                f.debug_struct("__c_anonymous_fp_reg_set")
                    .field("fpchip_state", &{self.fpchip_state})
                    .field("f_fpregs", &{self.f_fpregs})
                    .finish()
                }
            }
        }
        impl PartialEq for fpregset_t {
            fn eq(&self, other: &fpregset_t) -> bool {
                self.fp_reg_set == other.fp_reg_set
            }
        }
        impl Eq for fpregset_t {}
        impl ::fmt::Debug for fpregset_t {
            fn fmt(&self, f:&mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("fpregset_t")
                    .field("fp_reg_set", &self.fp_reg_set)
                    .finish()
            }
        }
        impl PartialEq for mcontext_t {
            fn eq(&self, other: &mcontext_t) -> bool {
                self.gregs == other.gregs &&
                    self.fpgregs == other.fpgregs
            }
        }
        impl Eq for mcontext_t {}
        impl ::fmt::Debug for mcontext_t {
            fn fmt(&self, f:&mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("mcontext_t")
                    .field("gregs", &self.gregs)
                    .field("fpgregs", &self.fpgregs)
                    .finish()
            }
        }
        impl PartialEq for ucontext_t {
            fn eq(&self, other: &ucontext_t) -> bool {
                self.uc_flags == other.uc_flags
                    && self.uc_link == other.uc_link
                    && self.uc_sigmask == other.uc_sigmask
                    && self.uc_stack == other.uc_stack
                    && self.uc_mcontext == other.uc_mcontext
                    && self.uc_filler == other.uc_filler
            }
        }
        impl Eq for ucontext_t {}
        impl ::fmt::Debug for ucontext_t {
            fn fmt(&self, f:&mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("ucontext_t")
                    .field("uc_flags", &self.uc_flags)
                    .field("uc_link", &self.uc_link)
                    .field("uc_sigmask", &self.uc_sigmask)
                    .field("uc_stack", &self.uc_stack)
                    .field("uc_mcontext", &self.uc_mcontext)
                    .field("uc_filler", &self.uc_filler)
                    .finish()
            }
        }

    }
}
