use std::cell::Cell;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum E<'a> {
    A(bool),
    B(&'a Cell<E<'a>>),
}

fn make_e_from_bool<'a>(_bump: &'a (), b: bool) -> E<'a> {
    E::A(b)
}

#[cfg(test)]
mod tests {
    use rstest::*;

    use super::*;

    #[fixture]
    fn bump() -> () {}

    #[rstest]
    #[case(true, E::A(true))]
    fn it_works<'a>(#[by_ref] bump: &'a (), #[case] b: bool, #[case] expected: E<'a>) {
        let actual = make_e_from_bool(&bump, b);
        assert_eq!(actual, expected);
    }
}
