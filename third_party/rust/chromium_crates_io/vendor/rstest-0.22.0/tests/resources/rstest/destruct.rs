use rstest::*;

struct T {
    a: u32,
    b: u32,
}

impl T {
    fn new(a: u32, b: u32) -> Self {
        Self { a, b }
    }
}

struct S(u32, u32);

#[fixture]
fn fix() -> T {
    T::new(1, 42)
}

#[fixture]
fn named() -> S {
    S(1, 42)
}

#[fixture]
fn tuple() -> (u32, u32) {
    (1, 42)
}

#[fixture]
fn swap(#[from(fix)] T { a, b }: T) -> T {
    T::new(b, a)
}

#[rstest]
fn swapped(#[from(swap)] T { a, b }: T) {
    assert_eq!(a, 42);
    assert_eq!(b, 1);
}

#[rstest]
#[case::two_times_twenty_one(T::new(2, 21))]
#[case::six_times_seven(T{ a: 6, b: 7 })]
fn cases_destruct(
    #[from(fix)] T { a, b }: T,
    #[case] T { a: c, b: d }: T,
    #[values(T::new(42, 1), T{ a: 3, b: 14})] T { a: e, b: f }: T,
) {
    assert_eq!(a * b, 42);
    assert_eq!(c * d, 42);
    assert_eq!(e * f, 42);
}

#[rstest]
#[case::two_times_twenty_one(S(2, 21))]
#[case::six_times_seven(S(6, 7))]
fn cases_destruct_named_tuple(
    #[from(named)] S(a, b): S,
    #[case] S(c, d): S,
    #[values(S(42, 1), S(3, 14))] S(e, f): S,
) {
    assert_eq!(a * b, 42);
    assert_eq!(c * d, 42);
    assert_eq!(e * f, 42);
}

#[rstest]
#[case::two_times_twenty_one((2, 21))]
#[case::six_times_seven((6, 7))]
fn cases_destruct_tuple(
    #[from(tuple)] (a, b): (u32, u32),
    #[case] (c, d): (u32, u32),
    #[values((42, 1), (3, 14))] (e, f): (u32, u32),
) {
    assert_eq!(a * b, 42);
    assert_eq!(c * d, 42);
    assert_eq!(e * f, 42);
}
