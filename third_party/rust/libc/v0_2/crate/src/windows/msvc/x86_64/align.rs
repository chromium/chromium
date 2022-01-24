#[allow(deprecated)]
pub type XMM_SAVE_AREA32 = XSAVE_FORMAT;

s_no_extra_traits! {
    #[repr(align(16))]
    pub union __c_anonymous_CONTEXT_FP {
        pub FltSave: ::XMM_SAVE_AREA32,
        pub Xmm: __c_anonymous_CONTEXT_XMM,
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        impl PartialEq for __c_anonymous_CONTEXT_FP {
            fn eq(&self, other: &__c_anonymous_CONTEXT_FP) -> bool {
                unsafe {
                    self.FltSave == other.FltSave ||
                        self.Xmm == other.Xmm
                }
            }
        }
        impl Eq for __c_anonymous_CONTEXT_FP {}
        impl ::fmt::Debug for __c_anonymous_CONTEXT_FP {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                unsafe {
                    f.debug_struct("__c_anonymous_CONTEXT_FP")
                        .field("FltSave", &self.FltSave)
                        .finish()
                }
            }
        }
        impl ::hash::Hash for __c_anonymous_CONTEXT_FP {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                unsafe {
                    self.FltSave.hash(state);
                    self.Xmm.hash(state);
                }
            }
        }
    }
}

s! {
    #[doc(hidden)]
    #[deprecated(since = "0.2.104", note = "use the `winapi` crate instead; we're going to
remove it in a future release")]
    #[repr(align(16))]
    pub struct M128A {
        pub Low: ::c_ulonglong,
        pub High: ::c_longlong,
    }

    #[doc(hidden)]
    #[deprecated(since = "0.2.104", note = "use the `winapi` crate instead; we're going to
remove it in a future release")]
    #[repr(align(16))]
    pub struct XSAVE_FORMAT {
        pub ControlWord: ::c_ushort,
        pub StatusWord: ::c_ushort,
        pub TagWord: ::c_uchar,
        _Reserved1: ::c_uchar,
        pub ErrorOpcode: ::c_ushort,
        pub ErrorOffset: ::c_ulong,
        pub ErrorSelector: ::c_ushort,
        _Reserved2: ::c_ushort,
        pub DataOffset: ::c_ulong,
        pub DataSelector: ::c_ushort,
        _Reserved3: ::c_ushort,
        pub MxCsr: ::c_ulong,
        pub MxCsr_Mask: ::c_ulong,
        pub FloatRegisters: [M128A; 8],
        pub XmmRegisters: [M128A; 16],
        _Reserved4: [[::c_uchar; 16]; 6],
    }

    #[repr(align(16))]
    pub struct __c_anonymous_CONTEXT_XMM {
        pub Header: [M128A; 2],
        pub Legacy: [M128A; 8],
        pub Xmm0: M128A,
        pub Xmm1: M128A,
        pub Xmm2: M128A,
        pub Xmm3: M128A,
        pub Xmm4: M128A,
        pub Xmm5: M128A,
        pub Xmm6: M128A,
        pub Xmm7: M128A,
        pub Xmm8: M128A,
        pub Xmm9: M128A,
        pub Xmm10: M128A,
        pub Xmm11: M128A,
        pub Xmm12: M128A,
        pub Xmm13: M128A,
        pub Xmm14: M128A,
        pub Xmm15: M128A,
    }

    #[doc(hidden)]
    #[deprecated(since = "0.2.104", note = "use the `winapi` crate instead; we're going to
remove it in a future release")]
    #[repr(align(16))]
    pub struct CONTEXT {
        pub P1Home: u64,
        pub P2Home: u64,
        pub P3Home: u64,
        pub P4Home: u64,
        pub P5Home: u64,
        pub P6Home: u64,
        pub ContextFlags: ::c_ulong,
        pub MxCsr: ::c_ulong,
        pub SegCs: ::c_ushort,
        pub SegDs: ::c_ushort,
        pub SegEs: ::c_ushort,
        pub SegFs: ::c_ushort,
        pub SegGs: ::c_ushort,
        pub SegSs: ::c_ushort,
        pub EFlags: ::c_ulong,
        pub Dr0: u64,
        pub Dr1: u64,
        pub Dr2: u64,
        pub Dr3: u64,
        pub Dr6: u64,
        pub Dr7: u64,
        pub Rax: u64,
        pub Rcx: u64,
        pub Rdx: u64,
        pub Rbx: u64,
        pub Rsp: u64,
        pub Rbp: u64,
        pub Rsi: u64,
        pub Rdi: u64,
        pub R8: u64,
        pub R9: u64,
        pub R10: u64,
        pub R11: u64,
        pub R12: u64,
        pub R13: u64,
        pub R14: u64,
        pub R15: u64,
        pub Rip: u64,
        pub Fp: __c_anonymous_CONTEXT_FP,
        pub VectorRegister: [M128A; 26],
        pub VectorControl: u64,
        pub DebugControl: u64,
        pub LastBranchToRip: u64,
        pub LastBranchFromRip: u64,
        pub LastExceptionToRip: u64,
        pub LastExceptionFromRip: u64,
    }
}
