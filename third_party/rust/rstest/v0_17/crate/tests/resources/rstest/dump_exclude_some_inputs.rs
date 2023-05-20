use rstest::*;

struct A;
struct B;
#[derive(Debug)]
struct D;

#[fixture]
fn fu32() -> u32 {
    42
}
#[fixture]
fn fb() -> B {
    B {}
}
#[fixture]
fn fd() -> D {
    D {}
}
#[fixture]
fn fa() -> A {
    A {}
}

#[rstest]
#[trace]
fn simple(fu32: u32, #[notrace] fa: A, #[notrace] fb: B, fd: D) {
    assert!(false);
}

#[rstest]
#[trace]
#[case(A{}, B{}, D{})]
fn cases(fu32: u32, #[case] #[notrace] a: A, #[case] #[notrace] b: B, #[case] d: D) {
    assert!(false);
}

#[rstest]
#[trace]
fn matrix(
    fu32: u32,
    #[notrace]
    #[values(A{})]
    a: A,
    #[notrace]
    #[values(B{})]
    b: B,
    #[values(D{}) ] dd: D,
) {
    assert!(false);
}
