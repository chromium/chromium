use rstest::*;

#[fixture]
#[once]
async fn error_async_once_fixture() {
}

#[fixture]
#[once]
fn error_generics_once_fixture<T: std::fmt::Debug>() -> T {
    42
}

#[fixture]
#[once]
fn error_generics_once_fixture() -> impl Iterator<Item = u32> {
    std::iter::once(42)
}

#[fixture]
#[once]
fn error_once_fixture_not_sync() -> std::cell::Cell<u32> {
    std::cell::Cell::new(42)
}
