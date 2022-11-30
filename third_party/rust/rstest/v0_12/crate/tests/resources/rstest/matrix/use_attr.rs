use rstest::rstest;

#[rstest]
fn both(#[values(4, 2*3-2)] expected: usize, #[values("ciao", "buzz")] input: &str) {
    assert_eq!(expected, input.len());
}

#[rstest(
    input => ["ciao", "buzz"]
)]
fn first(#[values(4, 2*3-2)] expected: usize, input: &str) {
    assert_eq!(expected, input.len());
}

#[rstest(
    expected => [4, 2*3-2]
)]
fn second(expected: usize, #[values("ciao", "buzz")] input: &str) {
    assert_eq!(expected, input.len());
}
