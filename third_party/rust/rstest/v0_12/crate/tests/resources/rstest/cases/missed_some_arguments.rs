use rstest::rstest;

#[cfg(test)]
#[rstest(a,b,c, case(1,2,3), case(3,2,1))]
fn error_param_not_exist(b: u32) {}
