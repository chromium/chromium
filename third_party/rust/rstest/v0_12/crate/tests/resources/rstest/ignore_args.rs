use rstest::*;

#[rstest]
#[case(42, 2)]
#[case(43, 3)]
fn test(#[case] _ignore1: u32, #[case] _ignore2: u32, #[values(1, 2, 3, 4)] _ignore3: u32) {}
