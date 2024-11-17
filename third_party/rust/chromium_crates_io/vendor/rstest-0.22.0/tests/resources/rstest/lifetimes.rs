use rstest::*;

enum E<'a> {
    A(bool),
    B(&'a std::cell::Cell<E<'a>>),
}

#[rstest]
#[case(E::A(true))]
fn case<'a>(#[case] e: E<'a>) {}

#[rstest]
fn values<'a>(#[values(E::A(true))] e: E<'a>) {}

#[fixture]
fn e<'a>() -> E<'a> {
    E::A(true)
}

#[rstest]
fn fixture<'a>(e: E<'a>) {}
