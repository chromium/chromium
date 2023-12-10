use std::io::prelude::*;

use rstest::*;

#[fixture]
async fn async_u32() -> u32 {
    42
}

#[fixture]
#[awt]
async fn nest_fixture(#[future] async_u32: u32) -> u32 {
    async_u32
}

#[fixture]
#[awt]
async fn nest_fixture_with_default(
    #[future]
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
#[awt]
async fn use_async_nest_fixture_default(#[future] nest_fixture: u32) {
    assert_eq!(42, nest_fixture);
}

#[rstest]
#[awt]
async fn use_async_nest_fixture_injected(
    #[future]
    #[with(async { 24 })]
    nest_fixture: u32,
) {
    assert_eq!(24, nest_fixture);
}

#[rstest]
#[awt]
async fn use_async_nest_fixture_with_default(#[future] nest_fixture_with_default: u32) {
    assert_eq!(42, nest_fixture_with_default);
}

#[rstest]
#[awt]
async fn use_async_fixture(#[future] async_u32: u32) {
    assert_eq!(42, async_u32);
}

#[fixture]
async fn async_impl_output() -> impl Read {
    std::io::Cursor::new(vec![1, 2, 3, 4, 5])
}

#[rstest]
#[awt]
async fn use_async_impl_output<T: Read>(#[future] async_impl_output: T) {
    let reader = async_impl_output;
}

#[fixture]
#[awt]
async fn two_args_mix_fixture(
    #[future]
    #[default(async { 4 })]
    four: u32,
    #[default(2)] two: u32,
) -> u32 {
    four * 10 + two
}

#[rstest]
#[awt]
async fn use_two_args_mix_fixture(#[future] two_args_mix_fixture: u32) {
    assert_eq!(42, two_args_mix_fixture);
}

#[rstest]
#[awt]
async fn use_two_args_mix_fixture_inject_first(
    #[future]
    #[with(async { 5 })]
    two_args_mix_fixture: u32,
) {
    assert_eq!(52, two_args_mix_fixture);
}

#[rstest]
#[awt]
async fn use_two_args_mix_fixture_inject_both(
    #[future]
    #[with(async { 3 }, 1)]
    two_args_mix_fixture: u32,
) {
    assert_eq!(31, two_args_mix_fixture);
}
