use rstest::rstest;

#[rstest(
    expected,
    case::user_test_description(true),
    case(true),
    case::user_test_description_fail(false)
)]
fn description(expected: bool) {
    assert!(expected);
}
