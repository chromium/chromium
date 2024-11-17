use rstest::fixture;

trait Mult {
    fn mult(&self, n: u32) -> u32;
}

struct M(u32);

impl Mult for M {
    fn mult(&self, n: u32) -> u32 {
        n * self.0
    }
}

#[fixture]
fn my_fixture() -> u32 { 42 }

#[fixture]
fn multiplier() -> M {
    M(2)
}

#[fixture]
fn my_fixture_injected(my_fixture: u32, multiplier: impl Mult) -> u32 { multiplier.mult(my_fixture) }

#[test]
fn resolve_new() {
    assert_eq!(42, my_fixture::get());
}

#[test]
fn resolve_default() {
    assert_eq!(42, my_fixture::default());
}

#[test]
fn injected_new() {
    assert_eq!(63, my_fixture_injected::get(21, M(3)));
}

#[test]
fn injected_default() {
    assert_eq!(84, my_fixture_injected::default());
}
