use rstest::{rstest, fixture};

#[fixture]
fn root() -> u32 { 21 }

#[fixture]
fn injection(root: u32) -> u32 { 2 * root }

#[rstest]
fn success(injection: u32) {
    assert_eq!(42, injection);
}

#[rstest]
fn fail(injection: u32) {
    assert_eq!(41, injection);
}
