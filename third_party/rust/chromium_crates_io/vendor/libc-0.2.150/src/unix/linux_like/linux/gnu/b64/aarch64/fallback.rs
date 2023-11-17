s! {
    #[repr(align(16))]
    pub struct user_fpsimd_struct {
        pub vregs: [[u64; 2]; 32],
        pub fpsr: ::c_uint,
        pub fpcr: ::c_uint,
    }
}
