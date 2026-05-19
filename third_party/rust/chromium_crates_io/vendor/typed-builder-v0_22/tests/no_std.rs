#![warn(clippy::pedantic)]
#![no_std]

use typed_builder::TypedBuilder;

#[test]
fn test_simple() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        x: i32,
        y: i32,
    }

    assert!(Foo::builder().x(1).y(2).build() == Foo { x: 1, y: 2 });
    assert!(Foo::builder().y(1).x(2).build() == Foo { x: 2, y: 1 });
}

#[test]
fn test_lifetime() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo<'a, 'b> {
        x: &'a i32,
        y: &'b i32,
    }

    assert!(Foo::builder().x(&1).y(&2).build() == Foo { x: &1, y: &2 });
}

#[test]
fn test_generics() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo<S, T: Default> {
        x: S,
        y: T,
    }

    assert!(Foo::builder().x(1).y(2).build() == Foo { x: 1, y: 2 });
}

#[test]
fn test_into() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(setter(into))]
        x: i32,
    }

    assert!(Foo::builder().x(1_u8).build() == Foo { x: 1 });
}

#[test]
fn test_default() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        /// x value.
        #[builder(default, setter(strip_option))]
        x: Option<i32>,
        #[builder(default = 10)]
        /// y value.
        y: i32,
        /// z value.
        #[builder(default = [20, 30, 40])]
        z: [i32; 3],
    }

    assert!(
        Foo::builder().build()
            == Foo {
                x: None,
                y: 10,
                z: [20, 30, 40]
            }
    );
    assert!(
        Foo::builder().x(1).build()
            == Foo {
                x: Some(1),
                y: 10,
                z: [20, 30, 40]
            }
    );
    assert!(
        Foo::builder().y(2).build()
            == Foo {
                x: None,
                y: 2,
                z: [20, 30, 40]
            }
    );
    assert!(
        Foo::builder().x(1).y(2).build()
            == Foo {
                x: Some(1),
                y: 2,
                z: [20, 30, 40]
            }
    );
    assert!(
        Foo::builder().z([1, 2, 3]).build()
            == Foo {
                x: None,
                y: 10,
                z: [1, 2, 3]
            }
    );
}

#[test]
fn test_field_dependencies_in_build() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(default, setter(strip_option))]
        x: Option<i32>,
        #[builder(default = 10)]
        y: i32,
        #[builder(default = [y, 30, 40])]
        z: [i32; 3],
    }

    assert!(
        Foo::builder().build()
            == Foo {
                x: None,
                y: 10,
                z: [10, 30, 40]
            }
    );
    assert!(
        Foo::builder().x(1).build()
            == Foo {
                x: Some(1),
                y: 10,
                z: [10, 30, 40]
            }
    );
    assert!(
        Foo::builder().y(2).build()
            == Foo {
                x: None,
                y: 2,
                z: [2, 30, 40]
            }
    );
    assert!(
        Foo::builder().x(1).y(2).build()
            == Foo {
                x: Some(1),
                y: 2,
                z: [2, 30, 40]
            }
    );
    assert!(
        Foo::builder().z([1, 2, 3]).build()
            == Foo {
                x: None,
                y: 10,
                z: [1, 2, 3]
            }
    );
}
