use rstest::*;

#[fixture]
pub fn fixture() -> u32 {
    42
}

#[fixture]
fn error_inner(fixture: u32) {
    let a: u32 = "";
}

#[fixture]
fn error_cannot_resolve_fixture(no_fixture: u32) {}

#[fixture]
fn error_fixture_wrong_type(fixture: String) {}

#[fixture(not_a_fixture(24))]
fn error_inject_an_invalid_fixture(fixture: String) {}

#[fixture]
fn name() -> &'static str {
    "name"
}

#[fixture]
fn f(name: &str) -> String {
    name.to_owned()
}

#[fixture(f("first"), f("second"))]
fn error_inject_a_fixture_more_than_once(f: String) {}

struct T(u32);

#[fixture]
fn structed() -> T {
    T(42)
}

#[fixture]
fn structed_injectd(fixture: u32) -> T {
    T(fixture)
}

#[fixture]
fn error_destruct_without_resolve(T(a): T) {}

#[fixture]
fn error_destruct_without_resolve_also_with(#[with(21)] T(a): T) {}
