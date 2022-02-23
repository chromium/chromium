use rstest::*;

#[derive(Debug)]
struct A {}

#[fixture]
fn fu32() -> u32 { 42 }
#[fixture]
fn fstring() -> String { "A String".to_string() }
#[fixture]
fn ftuple() -> (A, String, i32) { (A{}, "A String".to_string(), -12) }

#[rstest(::trace)]
fn should_fail(fu32: u32, fstring: String, ftuple: (A, String, i32)) {
    assert!(false);
}
