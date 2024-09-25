use rstest::{fixture, rstest};

#[fixture]
#[once]
fn once_fixture() -> u32 {
    eprintln!("Exec fixture() just once");
    42
}

#[rstest]
fn base(once_fixture: &u32) {
    assert_eq!(&42, once_fixture);
}

#[rstest]
#[case(2)]
#[case(3)]
#[case(7)]
fn cases(once_fixture: &u32, #[case] divisor: u32) {
    assert_eq!(0, *once_fixture % divisor);
}
