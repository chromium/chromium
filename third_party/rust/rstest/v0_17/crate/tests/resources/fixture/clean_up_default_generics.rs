use rstest::*;

#[fixture]
fn s() -> &'static str {
    "42"
}

#[fixture]
fn fx<S: ToString>(s: S) -> usize {
    s.to_string().len()
}

#[fixture]
fn sum() -> usize {
    42
}

#[fixture]
fn fx_double<S: ToString>(sum: usize, s: S) -> usize {
    s.to_string().len() + sum
}

#[test]
fn resolve() {
    assert_eq!(2, fx::default())
}

#[test]
fn resolve_partial() {
    assert_eq!(12, fx_double::partial_1(10))
}
