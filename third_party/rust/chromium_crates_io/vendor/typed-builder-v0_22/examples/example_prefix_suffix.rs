use typed_builder::TypedBuilder;

#[derive(Debug, PartialEq, TypedBuilder)]
#[builder(field_defaults(setter(prefix = "with_", suffix = "_value")))]
struct Foo {
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

fn main() {
    assert_eq!(
        Foo::builder().with_x_value(1).with_y_value(2).with_z_value(3).build(),
        Foo { x: 1, y: Some(2), z: 3 }
    );

    // Change the order of construction:
    assert_eq!(
        Foo::builder().with_z_value(1).with_x_value(2).with_y_value(3).build(),
        Foo { x: 2, y: Some(3), z: 1 }
    );

    // Optional fields are optional:
    assert_eq!(Foo::builder().with_x_value(1).build(), Foo { x: 1, y: None, z: 20 });

    // This will not compile - because we did not set x:
    // Foo::builder().build();

    // This will not compile - because we set y twice:
    // Foo::builder().with_x_value(1).with_y_value(2).with_y_value(3);
}
