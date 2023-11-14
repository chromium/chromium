use rstest::*;

#[rstest]
#[case::pass(42, async { 42 })]
#[case::fail(42, async { 41 })]
#[should_panic]
#[case::pass_panic(42, async { 41 })]
#[should_panic]
#[case::fail_panic(42, async { 42 })]
async fn my_async_test(
    #[case] expected: u32,
    #[case]
    #[future(awt)]
    value: u32,
) {
    assert_eq!(expected, value);
}

#[rstest]
#[case::pass(42, async { 42 })]
async fn my_async_test_revert(
    #[case] expected: u32,
    #[future(awt)]
    #[case]
    value: u32,
) {
    assert_eq!(expected, value);
}
