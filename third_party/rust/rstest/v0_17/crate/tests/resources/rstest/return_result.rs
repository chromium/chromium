use rstest::rstest;

#[rstest]
fn should_success() -> Result<(), &'static str> {
    Ok(())
}

#[rstest]
fn should_fail() -> Result<(), &'static str> {
    Err("Return Error")
}

#[rstest(ret,
    case::should_success(Ok(())),
    case::should_fail(Err("Return Error"))
)]
fn return_type(ret: Result<(), &'static str>) -> Result<(), &'static str> {
    ret
}
