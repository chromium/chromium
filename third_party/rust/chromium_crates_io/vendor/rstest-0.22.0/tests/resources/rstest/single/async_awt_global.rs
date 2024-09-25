use rstest::*;

#[fixture]
async fn fixture() -> u32 {
    42
}

#[rstest]
#[awt]
async fn should_pass(#[future] fixture: u32) {
    assert_eq!(fixture, 42);
}

#[rstest]
#[awt]
async fn should_fail(#[future] fixture: u32) {
    assert_ne!(fixture, 42);
}

#[rstest]
#[awt]
#[should_panic]
async fn should_panic_pass(#[future] fixture: u32) {
    panic!(format!("My panic -> fixture = {}", fixture));
}

#[rstest]
#[awt]
#[should_panic]
async fn should_panic_fail(#[future] fixture: u32) {
    assert_eq!(fixture, 42);
}
