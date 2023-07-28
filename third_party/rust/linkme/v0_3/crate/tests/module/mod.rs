mod declaration {
    use linkme::distributed_slice;

    #[distributed_slice]
    pub static SLICE: [i32] = [..];

    #[test]
    fn test_mod_slice() {
        assert!(!SLICE.is_empty());
    }
}

mod usage {
    use linkme::distributed_slice;

    #[distributed_slice(super::declaration::SLICE)]
    pub static N: i32 = 9;
}
