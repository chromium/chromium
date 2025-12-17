#[test]
fn expandtest() {
    macrotest::expand_args("tests/expand/*.rs", vec!["--features", "derive"]);
}
