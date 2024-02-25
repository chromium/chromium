use rstest::*;

#[fixture]
fn fx_base_impl_return() -> impl Iterator<Item=u32> { std::iter::once(42) }

#[fixture]
fn fx_base_impl_input(mut fx_base_impl_return: impl Iterator<Item=u32>) -> u32 {
    fx_base_impl_return.next().unwrap()
}

#[rstest]
fn base_impl_return(mut fx_base_impl_return: impl Iterator<Item=u32>) {
    assert_eq!(42, fx_base_impl_return.next().unwrap());
}

#[rstest]
fn base_impl_input(mut fx_base_impl_input: u32) {
    assert_eq!(42, fx_base_impl_input);
}

#[fixture]
fn fx_nested_impl_return() -> impl Iterator<Item=impl ToString> { std::iter::once(42) }

#[fixture]
fn fx_nested_impl_input(mut fx_nested_impl_return: impl Iterator<Item=impl ToString>) -> String {
    fx_nested_impl_return.next().unwrap().to_string()
}

#[rstest]
fn nested_impl_return(mut fx_nested_impl_return: impl Iterator<Item=impl ToString>) {
    assert_eq!("42", fx_nested_impl_return.next().unwrap().to_string());
}

#[rstest]
fn nested_impl_input(mut fx_nested_impl_input: String) {
    assert_eq!("42", &fx_nested_impl_input);
}

#[fixture]
fn fx_nested_multiple_impl_return() -> (impl Iterator<Item=impl ToString>, impl ToString) {
    (std::iter::once(42), 42i32)
}

#[fixture]
fn fx_nested_multiple_impl_input(mut fx_nested_multiple_impl_return: (impl Iterator<Item=impl ToString>, impl ToString)) -> bool {
    fx_nested_multiple_impl_return.0.next().unwrap().to_string() == fx_nested_multiple_impl_return.1.to_string()
}

#[rstest]
fn nested_multiple_impl_return(mut fx_nested_multiple_impl_return: (impl Iterator<Item=impl ToString>, impl ToString)) {
    assert_eq!(fx_nested_multiple_impl_return.0.next().unwrap().to_string(), fx_nested_multiple_impl_return.1.to_string());
}

#[rstest]
fn nested_multiple_impl_input(fx_nested_multiple_impl_input: bool) {
    assert!(fx_nested_multiple_impl_input);
}
