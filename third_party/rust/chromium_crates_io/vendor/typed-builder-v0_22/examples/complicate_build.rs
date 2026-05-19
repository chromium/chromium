#![allow(clippy::disallowed_names)]
mod scope {
    use typed_builder::TypedBuilder;

    #[derive(Debug, PartialEq, TypedBuilder)]
    #[builder(build_method(vis="", name=__build))]
    pub struct Foo {
        // Mandatory Field:
        x: i32,

        // #[builder(default)] without parameter - use the type's default
        // #[builder(setter(strip_option))] - wrap the setter argument with `Some(...)`
        #[builder(default, setter(strip_option))]
        y: Option<i32>,

        // Or you can set the default
        #[builder(default = 20)]
        z: i32,
    }

    // Customize build method to add complicated logic.
    //
    // The signature might be frightening at first glance,
    // but we don't need to infer this whole ourselves.
    //
    // We can use `cargo expand` to show code expanded by `TypedBuilder`,
    // copy the generated `__build` method, and modify the content of the build method.
    #[allow(non_camel_case_types)]
    impl<__z: typed_builder::Optional<i32>, __y: typed_builder::Optional<Option<i32>>> FooBuilder<((i32,), __y, __z)> {
        pub fn build(self) -> Bar {
            let foo = self.__build();
            Bar {
                x: foo.x + 1,
                y: foo.y.map(|y| y + 1),
                z: foo.z + 1,
            }
        }
    }

    #[derive(Debug, PartialEq)]
    pub struct Bar {
        pub x: i32,
        pub y: Option<i32>,
        pub z: i32,
    }
}

use scope::{Bar, Foo};

fn main() {
    assert_eq!(Foo::builder().x(1).y(2).z(3).build(), Bar { x: 2, y: Some(3), z: 4 });

    // This will not compile - because `__build` is a private method
    // Foo::builder().x(1).y(2).z(3).__build()
}
