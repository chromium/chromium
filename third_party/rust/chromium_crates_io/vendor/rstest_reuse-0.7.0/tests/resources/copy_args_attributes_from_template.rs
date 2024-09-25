use rstest_reuse;

mod cases {
    use rstest::rstest;
    use rstest_reuse::*;

    #[template]
    #[rstest]
    #[case(2, 2)]
    #[case(4/2, 2)]
    fn copy_cases(#[case] a: u32, #[case] b: u32) {}

    #[apply(copy_cases)]
    fn it_works(a: u32, b: u32) {
        assert!(a == b);
    }

    #[apply(copy_cases)]
    fn should_not_copy_attributes_if_already_present(#[case] a: u32, b: u32) {
        assert!(a == b);
    }

    #[apply(copy_cases)]
    #[case::more(8/4, 2)]
    fn add_a_case(a: u32, b: u32) {
        assert!(a == b);
    }

    #[apply(copy_cases)]
    fn add_values(a: u32, b: u32, #[values(1, 2, 3)] _add_some_tests: u32) {
        assert!(a == b);
    }

    #[apply(copy_cases)]
    fn should_copy_cases_also_from_underscored_attrs(_a: u32, _b: u32) {}
}

mod values {
    use rstest::rstest;
    use rstest_reuse::*;

    #[template]
    #[rstest]
    fn copy_values(#[values(1, 2)] cases: u32) {}

    #[apply(copy_values)]
    fn it_works(cases: u32) {
        assert!([1, 2].contains(&cases));
    }

    #[apply(copy_values)]
    #[case::more(8/4, 2)]
    fn add_a_case(#[case] a: u32, #[case] b: u32, cases: u32) {
        assert!([1, 2].contains(&cases));
        assert!(a == b);
    }

    #[apply(copy_values)]
    fn add_values(#[values(3, 4)] a: u32, cases: u32) {
        assert!([1, 2].contains(&cases));
        assert!([3, 4].contains(&a));
    }

    #[apply(copy_values)]
    fn should_copy_values_also_from_underscored_attrs(_cases: u32) {}
}
