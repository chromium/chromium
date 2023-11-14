use rstest::rstest;

#[cfg(test)]
#[rstest(f, case(42), case(24))]
fn error_param_not_exist() {}
