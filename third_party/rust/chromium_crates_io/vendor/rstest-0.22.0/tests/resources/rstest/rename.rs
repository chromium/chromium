use rstest::*;

#[fixture]
fn very_long_and_boring_name(#[default(42)] inject: u32) -> u32 {
    inject
}

mod sub_module {
    use super::*;

    #[fixture]
    pub fn mod_fixture() -> u32 {
        42
    }
}

#[rstest(very_long_and_boring_name as foo)]
fn compact(foo: u32) {
    assert!(42 == foo);
}

#[rstest(sub_module::mod_fixture as foo)]
fn compact_mod(foo: u32) {
    assert!(42 == foo);
}

#[rstest(very_long_and_boring_name(21) as foo)]
fn compact_injected(foo: u32) {
    assert!(21 == foo);
}

#[rstest]
fn attribute(#[from(very_long_and_boring_name)] foo: u32) {
    assert!(42 == foo);
}

#[rstest]
fn attribute_mod(#[from(sub_module::mod_fixture)] foo: u32) {
    assert!(42 == foo);
}

#[rstest]
fn attribute_injected(
    #[from(very_long_and_boring_name)]
    #[with(21)]
    foo: u32,
) {
    assert!(21 == foo);
}
