use rstest::*;

#[fixture]
#[once]
fn once_fixture() {
    eprintln!("Exec fixture() just once");
}

#[rstest]
fn base(_once_fixture: ()) {
    assert!(true);
}

#[rstest]
#[case()]
#[case()]
#[case()]
fn cases(_once_fixture: ()) {
    assert!(true);
}
