use rstest::*;

#[fixture]
fn fixture() -> String { "str".to_owned() }

#[rstest]
fn simple(fixture: impl AsRef<str>) {
    assert_eq!(3, fixture.as_ref().len());
}

#[rstest(
    expected, input,
    case(4, String::from("ciao")),
    case(3, "Foo")
)]
fn strlen_test(expected: usize, input: impl AsRef<str>) {
    assert_eq!(expected, input.as_ref().len());
}
