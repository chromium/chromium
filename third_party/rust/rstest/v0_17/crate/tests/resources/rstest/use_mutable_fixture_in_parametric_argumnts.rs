use rstest::*;

#[fixture]
fn f() -> String {
    "f".to_owned()
}

fn append(s: &mut String, a: &str) -> String {
    s.push_str("-");
    s.push_str(a);
    s.clone()
}

#[rstest]
#[case(append(&mut f, "a"), "f-a", "f-a-b")]
fn use_mutate_fixture(
    mut f: String,
    #[case] a: String,
    #[values(append(&mut f, "b"))] b: String,
    #[case] expected_a: &str,
    #[case] expected_b: &str,
) {
    assert_eq!(expected_a, a);
    assert_eq!(expected_b, b);
}
