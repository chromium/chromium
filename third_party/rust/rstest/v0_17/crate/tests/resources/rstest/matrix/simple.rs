use rstest::rstest;

#[rstest(
    expected => [4, 2*3-2],
    input => ["ciao", "buzz"],
)]
fn strlen_test(expected: usize, input: &str) {
    assert_eq!(expected, input.len());
}
