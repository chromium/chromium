use rstest::*;

#[fixture]
async fn fixture() -> u32 {
    42
}

#[rstest]
async fn should_pass(#[future(awt)] fixture: u32) {
    assert_eq!(fixture, 42);
}

#[rstest]
async fn should_fail(#[future(awt)] fixture: u32) {
    assert_ne!(fixture, 42);
}

#[rstest]
#[should_panic]
async fn should_panic_pass(#[future(awt)] fixture: u32) {
    panic!(format!("My panic -> fixture = {}", fixture));
}

#[rstest]
#[should_panic]
async fn should_panic_fail(#[future(awt)] fixture: u32) {
    assert_eq!(fixture, 42);
}
