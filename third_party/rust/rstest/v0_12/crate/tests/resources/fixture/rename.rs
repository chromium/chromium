use rstest::*;

#[fixture]
fn very_long_and_boring_name(#[default(42)] inject: u32) -> u32 {
    inject
}

#[fixture(very_long_and_boring_name as foo)]
fn compact(foo: u32) -> u32 {
    foo
}

#[fixture(very_long_and_boring_name(21) as foo)]
fn compact_injected(foo: u32) -> u32 {
    foo
}

#[fixture]
fn attribute(#[from(very_long_and_boring_name)] foo: u32) -> u32 {
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
fn test(compact: u32, attribute: u32, compact_injected: u32, attribute_injected: u32) {
    assert_eq!(compact, attribute);
    assert_eq!(compact_injected, attribute_injected);
}
