use rstest::rstest;

#[rstest(
    val,
    case::no_panic(0),
    #[should_panic]
    case::panic(2),
    #[should_panic(expected="expected")]
    case::panic_with_message(3),
    case::no_panic_but_fail(1),
    #[should_panic]
    case::panic_but_fail(0),
    #[should_panic(expected="other")]
    case::panic_with_wrong_message(3),
)]
fn attribute_per_case(val: i32) {
    match val {
        0 => assert!(true),
        1 => assert!(false),
        2 => panic!("No catch"),
        3 => panic!("expected"),
        _ => panic!("Not defined"),
    }
}
