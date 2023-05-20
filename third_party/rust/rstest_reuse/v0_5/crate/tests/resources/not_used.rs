mod foo {
    use rstest_reuse::template;

    #[template]
    #[rstest]
    #[case("bar")]
    fn not_used(#[case] s: &str) {}
}
