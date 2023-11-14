use rstest::rstest;

#[rstest(one, two, three)]
fn should_show_error_for_no_case(one: u32, two: u32, three: u32) {}
