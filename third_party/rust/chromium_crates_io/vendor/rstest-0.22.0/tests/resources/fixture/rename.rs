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

#[fixture(very_long_and_boring_name as foo)]
fn compact(foo: u32) -> u32 {
    foo
}

#[fixture(very_long_and_boring_name(21) as foo)]
fn compact_injected(foo: u32) -> u32 {
    foo
}

#[fixture(sub_module::mod_fixture as foo)]
fn compact_from_mod(foo: u32) -> u32 {
    foo
}

#[fixture]
fn attribute(#[from(very_long_and_boring_name)] foo: u32) -> u32 {
    foo
}

#[fixture]
fn attribute_mod(#[from(sub_module::mod_fixture)] foo: u32) -> u32 {
    foo
}

#[fixture]
fn attribute_injected(
    #[from(very_long_and_boring_name)]
    #[with(21)]
    foo: u32,
) -> u32 {
    foo
}

#[rstest]
fn test(
    compact: u32,
    attribute: u32,
    attribute_mod: u32,
    compact_from_mod: u32,
    compact_injected: u32,
    attribute_injected: u32,
) {
    assert_eq!(compact, attribute);
    assert_eq!(attribute, attribute_mod);
    assert_eq!(attribute_mod, compact_from_mod);
    assert_eq!(compact_injected, attribute_injected);
}
