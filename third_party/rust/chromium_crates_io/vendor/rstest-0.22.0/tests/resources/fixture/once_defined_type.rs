use rstest::{fixture, rstest};

#[fixture]
#[default(u32)]
#[partial_1(u32)]
#[once]
fn once_fixture(#[default(())] a: (), #[default(())] b: ()) -> u32 {
    eprintln!("Exec fixture() just once");
    42
}

#[rstest]
fn base(once_fixture: &u32) {
    assert_eq!(&42, once_fixture);
}

#[rstest]
fn base_partial(#[with(())] once_fixture: &u32) {
    assert_eq!(&42, once_fixture);
}

#[rstest]
fn base_complete(#[with((), ())] once_fixture: &u32) {
    assert_eq!(&42, once_fixture);
}

#[rstest]
#[case(2)]
#[case(3)]
#[case(7)]
fn cases(once_fixture: &u32, #[case] divisor: u32) {
    assert_eq!(0, *once_fixture % divisor);
}
