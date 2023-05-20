use rstest::*;

#[fixture]
fn val() -> i32 {
    21
}

#[fixture]
fn fortytwo(mut val: i32) -> i32 {
    val *= 2;
    val
}

#[rstest]
fn the_test(fortytwo: i32) {
    assert_eq!(fortytwo, 42);
}
