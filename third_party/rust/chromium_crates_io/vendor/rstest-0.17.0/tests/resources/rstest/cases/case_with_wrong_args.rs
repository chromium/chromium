use rstest::rstest;

#[cfg(test)]
#[rstest(a, b, case(42), case(1, 2), case(43))]
fn error_less_arguments(a: u32, b: u32) {}

#[cfg(test)]
#[rstest(a, case(42, 43), case(12), case(24, 34))]
fn error_too_much_arguments(a: u32) {}

