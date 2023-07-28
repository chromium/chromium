#![cfg_attr(feature = "used_linker", feature(used_with_arg))]

use linkme as link_me;

mod declaration {
    use crate::link_me::distributed_slice;

    #[distributed_slice]
    #[linkme(crate = crate::link_me)]
    pub static SLICE: [i32] = [..];

    #[test]
    fn test_slice() {
        assert!(!SLICE.is_empty());
    }

    #[distributed_slice]
    #[linkme(crate = crate::link_me)]
    pub static FUNCTIONS: [fn()] = [..];

    #[test]
    fn test_functions() {
        assert!(!FUNCTIONS.is_empty());
    }
}

mod usage {
    use crate::link_me::distributed_slice;

    #[distributed_slice(super::declaration::SLICE)]
    #[linkme(crate = crate::link_me)]
    pub static N: i32 = 9;

    #[distributed_slice(super::declaration::FUNCTIONS)]
    #[linkme(crate = crate::link_me)]
    fn test_me() {}
}
