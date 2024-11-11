s! {
    #[repr(align(4))]
    pub struct in6_addr {
        pub s6_addr: [u8; 16],
    }
}

pub const IN6ADDR_LOOPBACK_INIT: in6_addr = in6_addr {
    s6_addr: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
};

pub const IN6ADDR_ANY_INIT: in6_addr = in6_addr {
    s6_addr: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
};
