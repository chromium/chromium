use rstest::*;

#[rstest]
#[case::pass(async { 3 })]
#[awt]
async fn my_mut_test_global_awt(
    #[future]
    #[case]
    mut a: i32,
) {
    a = 4;
    assert_eq!(a, 4);
}

#[rstest]
#[case::pass(async { 3 })]
async fn my_mut_test_local_awt(
    #[future(awt)]
    #[case]
    mut a: i32,
) {
    a = 4;
    assert_eq!(a, 4);
}
