use rstest::*;

#[fixture]
fn dyn_box() -> Box<dyn Iterator<Item=i32>> {
    Box::new(std::iter::once(42))
}

#[fixture]
fn dyn_ref() -> &'static dyn ToString {
    &42
}

#[fixture]
fn dyn_box_resolve(mut dyn_box: Box<dyn Iterator<Item=i32>>) -> i32 {
    dyn_box.next().unwrap()
}

#[fixture]
fn dyn_ref_resolve(dyn_ref: &dyn ToString) -> String {
    dyn_ref.to_string()
}

#[rstest]
fn test_dyn_box(mut dyn_box: Box<dyn Iterator<Item=i32>>) {
    assert_eq!(42, dyn_box.next().unwrap())
}

#[rstest]
fn test_dyn_ref(dyn_ref: &dyn ToString) {
    assert_eq!("42", dyn_ref.to_string())
}

#[rstest]
fn test_dyn_box_resolve(dyn_box_resolve: i32) {
    assert_eq!(42, dyn_box_resolve)
}

#[rstest]
fn test_dyn_ref_resolve(dyn_ref_resolve: String) {
    assert_eq!("42", dyn_ref_resolve)
}
