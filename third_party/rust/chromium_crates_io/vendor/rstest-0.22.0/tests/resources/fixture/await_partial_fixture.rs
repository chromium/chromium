use std::io::prelude::*;

use rstest::*;

#[fixture]
async fn async_u32() -> u32 {
    42
}

#[fixture]
async fn nest_fixture(#[future(awt)] async_u32: u32) -> u32 {
    async_u32
}

#[fixture]
async fn nest_fixture_with_default(
    #[future(awt)]
    #[default(async { 42 })]
    fortytwo: u32,
) -> u32 {
    fortytwo
}

#[rstest]
async fn default_is_async() {
    assert_eq!(42, async_u32::default().await);
}

#[rstest]
async fn use_async_nest_fixture_default(#[future(awt)] nest_fixture: u32) {
    assert_eq!(42, nest_fixture);
}

#[rstest]
async fn use_async_nest_fixture_injected(
    #[future(awt)]
    #[with(async { 24 })]
    nest_fixture: u32,
) {
    assert_eq!(24, nest_fixture);
}

#[rstest]
async fn use_async_nest_fixture_with_default(#[future(awt)] nest_fixture_with_default: u32) {
    assert_eq!(42, nest_fixture_with_default);
}

#[rstest]
async fn use_async_fixture(#[future(awt)] async_u32: u32) {
    assert_eq!(42, async_u32);
}

#[fixture]
async fn async_impl_output() -> impl Read {
    std::io::Cursor::new(vec![1, 2, 3, 4, 5])
}

#[rstest]
async fn use_async_impl_output<T: Read>(#[future(awt)] async_impl_output: T) {
    let reader = async_impl_output;
}

#[fixture]
async fn two_args_mix_fixture(
    #[future(awt)]
    #[default(async { 4 })]
    four: u32,
    #[default(2)] two: u32,
) -> u32 {
    four * 10 + two
}

#[rstest]
async fn use_two_args_mix_fixture(#[future(awt)] two_args_mix_fixture: u32) {
    assert_eq!(42, two_args_mix_fixture);
}

#[rstest]
async fn use_two_args_mix_fixture_inject_first(
    #[future(awt)]
    #[with(async { 5 })]
    two_args_mix_fixture: u32,
) {
    assert_eq!(52, two_args_mix_fixture);
}

#[rstest]
async fn use_two_args_mix_fixture_inject_both(
    #[future(awt)]
    #[with(async { 3 }, 1)]
    two_args_mix_fixture: u32,
) {
    assert_eq!(31, two_args_mix_fixture);
}
