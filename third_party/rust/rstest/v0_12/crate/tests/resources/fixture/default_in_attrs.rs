use rstest::{fixture, rstest};

#[fixture]
pub fn simple(#[default(42)] value: u32) -> u32 {
    value
}

#[fixture]
pub fn double(#[default(20 + 1)] value: u32, #[default(1 + 1)] mult: u32) -> u32 {
    value * mult
}

#[fixture]
pub fn middle() -> u32 {
    2
}

#[fixture]
pub fn mixed(#[default(21)] value: u32, middle: u32, #[default(2 + 2)] mult: u32) -> u32 {
    value * mult / middle
}

#[rstest]
fn test_simple(simple: u32) {
    assert_eq!(simple, 42)
}

#[rstest(simple(21))]
fn test_simple_changed(simple: u32) {
    assert_eq!(simple, 21)
}

#[rstest]
fn test_double(double: u32) {
    assert_eq!(double, 42)
}

#[rstest(double(20, 3))]
fn test_double_changed(double: u32) {
    assert_eq!(double, 60)
}

#[rstest]
fn test_mixed(mixed: u32) {
    assert_eq!(mixed, 42)
}
