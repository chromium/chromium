use rstest_reuse;

mod inner1 {
    use rstest::rstest;
    use rstest_reuse::*;

    #[template]
    #[rstest(a,  b, case(2, 2), case(4/2, 2))]
    fn my_template(a: u32, b: u32) {}

    #[apply(my_template)]
    fn it_works(a: u32, b: u32) {
        assert!(a == b);
    }
}

mod inner2 {
    use rstest::rstest;
    use rstest_reuse::*;

    #[template]
    #[rstest(a, case(2), case(4))]
    fn my_template(a: u32) {}

    #[apply(my_template)]
    fn it_works(a: u32) {}
}
