use rstest::rstest;

#[rstest(
    expected, input,
    case(4, "ciao"),
    case(3, "Foo")
)]
fn strlen_test(expected: usize, input: &str) {
    assert_eq!(expected, input.len());
}
