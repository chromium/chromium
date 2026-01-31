use crate::mod2::S7;
use better_any::{impl_tid, tid, type_id, Tid, TidAble, TidExt};
use std::any::Any;

#[derive(Tid)]
struct S1(usize);

#[derive(Tid)]
struct S2<'a>(&'a str);

#[derive(Tid)]
struct S3<'a, T>(&'a T);

trait Trait {}
impl Trait for S1 {}

#[derive(Tid)]
struct S4<T>(T);
// type_id!(S4<T>);

#[derive(Tid)]
struct S5<'a, T: Trait = S1>(&'a T);

#[derive(Tid)]
struct S51<'a, T: ?Sized + Trait>(&'a T);

trait TraitLT<'a>: TidAble<'a> {}
impl<'a> TraitLT<'a> for S2<'a> {}
#[derive(Tid)]
struct S6<'a, T>(&'a T);

#[derive(Tid)]
struct S8<T, X: 'static>(T, X);

trait Big<'a>: Tid<'a> {}

impl<'a, T: TraitLT<'a>> Big<'a> for S6<'a, T> {}

struct S6macro<'a, T>(&'a T);
tid! {impl<'a,T> TidAble<'a> for S6macro<'a,T>}

impl<'a, T: TraitLT<'a>> Big<'a> for S6macro<'a, T> {}

trait Trait2<'a> {}

tid! { impl<'a> TidAble<'a> for dyn Trait2<'a> + 'a }

tid! { impl<'b> TidAble<'b> for Box<dyn Trait + 'b> }

// #[derive(Tid)]
// struct S6<'a, T>(&'a T)
// where
//     T: Trait;
mod mod2 {
    pub use mod1::S7;
    mod mod1 {
        use better_any::{Tid, TidAble, TidExt};

        #[derive(Tid)]
        pub struct S7<T>(pub T);
    }
}

fn test_bound<'a, T: Tid<'a>>() {}

fn test_start<'a>() {
    test_bound::<S1>();
    test_bound::<S2<'a>>();
    test_bound::<S3<'a, S2<'a>>>();
    test_bound::<S4<S3<'a, S1>>>();
    test_bound::<S5<'a, S1>>();
    test_bound::<S51<'a, S1>>();
    test_bound::<S6<'a, S2<'a>>>();
    test_bound::<S7<S1>>();
    test_bound::<S8<S1, usize>>();
}

#[test]
fn test_downcast_trait_object() {
    trait T<'a>: Tid<'a> {}
    #[derive(Tid)]
    struct S<'a>(&'a str);
    impl<'a> T<'a> for S<'a> {}
    let s = String::from("xx");
    let orig = S(&s);
    let to = &orig as &dyn T;
    let downcasted = to.downcast_ref::<S>().unwrap();
    assert_eq!(orig.0, downcasted.0);
    assert!(!to.is::<S2>());
}

#[test]
fn test_static() {
    let a = S1(5);
    let a = &a as &dyn Tid;
    assert_eq!(a.downcast_ref::<S1>().unwrap().0, 5);

    let a = S1(5);
    let a: &dyn Tid = (&a).into();
    assert_eq!(a.downcast_any_ref::<S1>().unwrap().0, 5);
}

#[test]
fn test_simple() {
    let s7 = S7(S1(5));
    let s7 = &s7 as &dyn Tid;

    let s = String::from("test");
    let a = S2(&s);
    let a = &a as &dyn Tid;
    assert_eq!(a.downcast_ref::<S2>().unwrap().0, "test");
}

#[test]
fn test_generic_context() {
    use std::borrow::Cow;

    fn generic<'a, H: Tid<'a>>(x: H) -> Cow<'a, str> {
        if let Some(s1) = x.downcast_ref::<S1>() {
            return Cow::Owned(s1.0.to_string());
        }
        if let Some(s2) = x.downcast_ref::<S2>() {
            return Cow::Borrowed(s2.0);
        }
        panic!("unsupported type")
    }

    assert_eq!(generic(S1(5)).as_ref(), "5");
    assert_eq!(generic(S2("x")).as_ref(), "x");
}
