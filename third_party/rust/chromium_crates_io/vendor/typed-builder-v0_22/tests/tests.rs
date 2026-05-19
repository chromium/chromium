#![warn(clippy::pedantic)]
#![allow(clippy::disallowed_names, clippy::type_complexity)]

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
fn test_lifetime_bounded() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo<'a, 'b: 'a> {
        x: &'a i32,
        y: &'b i32,
    }

    assert!(Foo::builder().x(&1).y(&2).build() == Foo { x: &1, y: &2 });
}

#[test]
fn test_mutable_borrows() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo<'a, 'b> {
        x: &'a mut i32,
        y: &'b mut i32,
    }

    let mut a = 1;
    let mut b = 2;
    {
        let foo = Foo::builder().x(&mut a).y(&mut b).build();
        *foo.x *= 10;
        *foo.y *= 100;
    }
    assert!(a == 10);
    assert!(b == 200);
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
fn test_2d_const_generics() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo<const NUM_COLS: usize, const NUM_ROWS: usize> {
        data: [[u32; NUM_ROWS]; NUM_COLS],
    }
    assert!(Foo::builder().data([[]]).build() == Foo { data: [[]] });
}

#[test]
fn test_multiple_const_generics() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo<const A: usize, const B: usize, const C: usize> {
        data: [u32; A],
        data2: [u32; B],
        data3: [u32; C],
    }
    assert!(
        Foo::builder().data([1]).data2([2, 3]).data3([]).build()
            == Foo {
                data: [1],
                data2: [2, 3],
                data3: []
            }
    );
}

#[test]
fn test_const_generics_with_other_generics() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo<const A: usize, B> {
        data: [B; A],
        data2: [B; 3],
    }
    assert!(
        Foo::builder().data([3]).data2([0, 1, 2]).build()
            == Foo {
                data: [3],
                data2: [0, 1, 2]
            }
    );
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
fn test_strip_option_with_into() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(setter(strip_option, into))]
        x: Option<i32>,
    }

    assert!(Foo::builder().x(1_u8).build() == Foo { x: Some(1) });
}

#[test]
fn test_into_with_strip_option() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(setter(into, strip_option))]
        x: Option<i32>,
    }

    assert!(Foo::builder().x(1_u8).build() == Foo { x: Some(1) });
}

#[test]
fn test_strip_option_with_fallback() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(setter(strip_option(fallback = x_opt)))]
        x: Option<i32>,
    }

    assert!(Foo::builder().x(1).build() == Foo { x: Some(1) });
    assert!(Foo::builder().x_opt(Some(1)).build() == Foo { x: Some(1) });
}

#[test]
fn test_into_with_strip_option_with_fallback() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(setter(into, strip_option(fallback = x_opt)))]
        x: Option<i32>,
    }

    assert!(Foo::builder().x(1_u8).build() == Foo { x: Some(1) });
    assert!(Foo::builder().x_opt(Some(1)).build() == Foo { x: Some(1) });
}

#[test]
fn test_strip_bool() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(setter(into, strip_bool))]
        x: bool,
    }

    assert!(Foo::builder().x().build() == Foo { x: true });
    assert!(Foo::builder().build() == Foo { x: false });
}

#[test]
fn test_strip_bool_with_fallback() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(setter(into, strip_bool(fallback = x_bool)))]
        x: bool,
    }

    assert!(Foo::builder().x().build() == Foo { x: true });
    assert!(Foo::builder().x_bool(false).build() == Foo { x: false });
    assert!(Foo::builder().build() == Foo { x: false });
}

#[test]
fn test_default() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(default, setter(strip_option))]
        x: Option<i32>,
        #[builder(default = 10)]
        y: i32,
        #[builder(default = vec![20, 30, 40])]
        z: Vec<i32>,
    }

    assert!(
        Foo::builder().build()
            == Foo {
                x: None,
                y: 10,
                z: vec![20, 30, 40]
            }
    );
    assert!(
        Foo::builder().x(1).build()
            == Foo {
                x: Some(1),
                y: 10,
                z: vec![20, 30, 40]
            }
    );
    assert!(
        Foo::builder().y(2).build()
            == Foo {
                x: None,
                y: 2,
                z: vec![20, 30, 40]
            }
    );
    assert!(
        Foo::builder().x(1).y(2).build()
            == Foo {
                x: Some(1),
                y: 2,
                z: vec![20, 30, 40]
            }
    );
    assert!(
        Foo::builder().z(vec![1, 2, 3]).build()
            == Foo {
                x: None,
                y: 10,
                z: vec![1, 2, 3]
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
        #[builder(default = vec![y, 30, 40])]
        z: Vec<i32>,
    }

    assert!(
        Foo::builder().build()
            == Foo {
                x: None,
                y: 10,
                z: vec![10, 30, 40]
            }
    );
    assert!(
        Foo::builder().x(1).build()
            == Foo {
                x: Some(1),
                y: 10,
                z: vec![10, 30, 40]
            }
    );
    assert!(
        Foo::builder().y(2).build()
            == Foo {
                x: None,
                y: 2,
                z: vec![2, 30, 40]
            }
    );
    assert!(
        Foo::builder().x(1).y(2).build()
            == Foo {
                x: Some(1),
                y: 2,
                z: vec![2, 30, 40]
            }
    );
    assert!(
        Foo::builder().z(vec![1, 2, 3]).build()
            == Foo {
                x: None,
                y: 10,
                z: vec![1, 2, 3]
            }
    );
}

// compile-fail tests for skip are in src/lib.rs out of necessity. These are just the bland
// successful cases.
#[test]
fn test_skip() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(default, setter(skip))]
        x: i32,
        #[builder(setter(into))]
        y: i32,
        #[builder(default = y + 1, setter(skip))]
        z: i32,
    }

    assert!(Foo::builder().y(1_u8).build() == Foo { x: 0, y: 1, z: 2 });
}

#[test]
fn test_docs() {
    #[derive(TypedBuilder)]
    #[builder(
        builder_method(doc = "Point::builder() method docs"),
        builder_type(doc = "PointBuilder type docs"),
        build_method(doc = "PointBuilder.build() method docs")
    )]
    struct Point {
        #[allow(dead_code)]
        x: i32,
        #[builder(
                default = x,
                setter(
                    doc = "Set `z`. If you don't specify a value it'll default to the value specified for `x`.",
                ),
            )]
        #[allow(dead_code)]
        y: i32,
    }

    let _ = Point::builder();
}

#[test]
fn test_builder_name() {
    #[derive(TypedBuilder)]
    struct Foo {}

    let _: FooBuilder<_> = Foo::builder();
}

// NOTE: `test_builder_type_stability` and `test_builder_type_stability_with_other_generics` are
//       meant to ensure we don't break things for people that use custom `impl`s on the builder
//       type before the tuple field generic param transformation traits are in.
//       See:
//        - https://github.com/idanarye/rust-typed-builder/issues/22
//        - https://github.com/idanarye/rust-typed-builder/issues/23
#[test]
fn test_builder_type_stability() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        x: i32,
        y: i32,
        z: i32,
    }

    impl<Y> FooBuilder<((), Y, ())> {
        fn xz(self, x: i32, z: i32) -> FooBuilder<((i32,), Y, (i32,))> {
            self.x(x).z(z)
        }
    }

    assert!(Foo::builder().xz(1, 2).y(3).build() == Foo { x: 1, y: 3, z: 2 });
    assert!(Foo::builder().xz(1, 2).y(3).build() == Foo::builder().x(1).z(2).y(3).build());

    assert!(Foo::builder().y(1).xz(2, 3).build() == Foo { x: 2, y: 1, z: 3 });
    assert!(Foo::builder().y(1).xz(2, 3).build() == Foo::builder().y(1).x(2).z(3).build());
}

#[test]
fn test_builder_type_stability_with_other_generics() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo<X: Default, Y> {
        x: X,
        y: Y,
    }

    impl<X: Default, Y, Y_> FooBuilder<X, Y, ((), Y_)> {
        fn x_default(self) -> FooBuilder<X, Y, ((X,), Y_)> {
            self.x(X::default())
        }
    }

    assert!(Foo::builder().x_default().y(1.0).build() == Foo { x: 0, y: 1.0 });
    assert!(
        Foo::builder().y("hello".to_owned()).x_default().build()
            == Foo {
                x: "",
                y: "hello".to_owned()
            }
    );
}

#[test]
#[allow(clippy::items_after_statements)]
fn test_builder_type_with_default_on_generic_type() {
    #[derive(PartialEq, TypedBuilder)]
    struct Types<X, Y = ()> {
        x: X,
        y: Y,
    }
    assert!(Types::builder().x(()).y(()).build() == Types { x: (), y: () });

    #[derive(PartialEq, TypedBuilder)]
    struct TypeAndLifetime<'a, X, Y: Default, Z = usize> {
        x: X,
        y: Y,
        z: &'a Z,
    }
    let a = 0;
    assert!(TypeAndLifetime::builder().x(()).y(0).z(&a).build() == TypeAndLifetime { x: (), y: 0, z: &0 });

    #[derive(PartialEq, TypedBuilder)]
    struct Foo<'a, X, Y: Default, Z: Default = usize, M = ()> {
        x: X,
        y: &'a Y,
        z: Z,
        m: M,
    }

    impl<'a, X, Y: Default, M, X_, Y_, M_> FooBuilder<'a, X, Y, usize, M, (X_, Y_, (), M_)> {
        fn z_default(self) -> FooBuilder<'a, X, Y, usize, M, (X_, Y_, (usize,), M_)> {
            self.z(usize::default())
        }
    }

    impl<'a, X, Y: Default, Z: Default, X_, Y_, Z_> FooBuilder<'a, X, Y, Z, (), (X_, Y_, Z_, ())> {
        fn m_default(self) -> FooBuilder<'a, X, Y, Z, (), (X_, Y_, Z_, ((),))> {
            self.m(())
        }
    }

    // compile test if rustc can infer type for `z` and `m`
    Foo::<(), _, _, f64>::builder().x(()).y(&a).z_default().m(1.0).build();
    Foo::<(), _, _, _>::builder().x(()).y(&a).z_default().m_default().build();

    assert!(
        Foo::builder().x(()).y(&a).z_default().m(1.0).build()
            == Foo {
                x: (),
                y: &0,
                z: 0,
                m: 1.0
            }
    );
    assert!(
        Foo::builder().x(()).y(&a).z(9).m(1.0).build()
            == Foo {
                x: (),
                y: &0,
                z: 9,
                m: 1.0
            }
    );
}

#[test]
fn test_builder_type_skip_into() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo<X> {
        x: X,
    }

    // compile test if rustc can infer type for `x`
    Foo::builder().x(()).build();

    assert!(Foo::builder().x(()).build() == Foo { x: () });
}

#[test]
fn test_default_code() {
    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(default_code = "\"text1\".to_owned()")]
        x: String,

        #[builder(default_code = r#""text2".to_owned()"#)]
        y: String,
    }

    assert!(
        Foo::builder().build()
            == Foo {
                x: "text1".to_owned(),
                y: "text2".to_owned()
            }
    );
}

#[test]
fn test_field_defaults_default_value() {
    #[derive(PartialEq, TypedBuilder)]
    #[builder(field_defaults(default = 12))]
    struct Foo {
        x: i32,
        #[builder(!default)]
        y: String,
        #[builder(default = 13)]
        z: i32,
    }

    assert!(
        Foo::builder().y("bla".to_owned()).build()
            == Foo {
                x: 12,
                y: "bla".to_owned(),
                z: 13
            }
    );
}

#[test]
fn test_field_defaults_setter_options() {
    #[derive(PartialEq, TypedBuilder)]
    #[builder(field_defaults(setter(strip_option)))]
    struct Foo {
        x: Option<i32>,
        #[builder(setter(!strip_option))]
        y: i32,
    }

    assert!(Foo::builder().x(1).y(2).build() == Foo { x: Some(1), y: 2 });
}

#[test]
fn test_field_defaults_strip_option_ignore_invalid() {
    #[derive(TypedBuilder)]
    #[builder(field_defaults(setter(strip_option(ignore_invalid))))]
    struct Foo {
        x: Option<i32>,
        y: i32,
        #[builder(default = Some(13))]
        z: Option<i32>,
    }

    let foo = Foo::builder().x(42).y(10).build();

    assert_eq!(foo.x, Some(42));
    assert_eq!(foo.y, 10);
    assert_eq!(foo.z, Some(13));
}

#[test]
fn test_field_defaults_strip_option_fallback_suffix() {
    #[derive(TypedBuilder)]
    #[builder(field_defaults(setter(strip_option(fallback_suffix = "_opt"))))]
    struct Foo {
        x: Option<i32>,
        #[builder(default = Some(13))]
        z: Option<i32>,
    }

    let foo1 = Foo::builder().x(42).build();

    let foo2 = Foo::builder().x_opt(None).build();

    assert_eq!(foo1.x, Some(42));
    assert_eq!(foo1.z, Some(13));
    assert_eq!(foo2.x, None);
    assert_eq!(foo2.z, Some(13));
}

#[test]
fn test_field_defaults_strip_option_fallback_prefix() {
    #[derive(TypedBuilder)]
    #[builder(field_defaults(setter(strip_option(fallback_prefix = "opt_"))))]
    struct Foo {
        x: Option<i32>,
        #[builder(default = Some(13))]
        z: Option<i32>,
    }

    let foo1 = Foo::builder().x(42).build();

    let foo2 = Foo::builder().opt_x(None).build();

    assert_eq!(foo1.x, Some(42));
    assert_eq!(foo1.z, Some(13));
    assert_eq!(foo2.x, None);
    assert_eq!(foo2.z, Some(13));
}

#[test]
fn test_field_defaults_strip_option_fallback_prefix_and_suffix() {
    #[derive(TypedBuilder)]
    #[builder(field_defaults(setter(strip_option(fallback_prefix = "opt_", fallback_suffix = "_val"))))]
    struct Foo {
        x: Option<i32>,
        #[builder(default = Some(13))]
        z: Option<i32>,
    }

    let foo1 = Foo::builder().x(42).build();

    let foo2 = Foo::builder().opt_x_val(None).build();

    assert_eq!(foo1.x, Some(42));
    assert_eq!(foo1.z, Some(13));
    assert_eq!(foo2.x, None);
    assert_eq!(foo2.z, Some(13));
}

#[test]
fn test_clone_builder() {
    #[derive(PartialEq, Default)]
    struct Uncloneable;

    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        x: i32,
        y: i32,
        #[builder(default)]
        z: Uncloneable,
    }

    let semi_built = Foo::builder().x(1);

    assert!(
        semi_built.clone().y(2).build()
            == Foo {
                x: 1,
                y: 2,
                z: Uncloneable
            }
    );
    assert!(
        semi_built.y(3).build()
            == Foo {
                x: 1,
                y: 3,
                z: Uncloneable
            }
    );
}

#[test]
#[allow(clippy::items_after_statements)]
fn test_clone_builder_with_generics() {
    #[allow(dead_code)]
    #[derive(PartialEq, Default)]
    struct Uncloneable;

    #[derive(PartialEq, TypedBuilder)]
    struct Foo<T> {
        x: T,
        y: i32,
    }

    let semi_built1 = Foo::builder().x(1);

    assert!(semi_built1.clone().y(2).build() == Foo { x: 1, y: 2 });
    assert!(semi_built1.y(3).build() == Foo { x: 1, y: 3 });

    let semi_built2 = Foo::builder().x("four");

    assert!(semi_built2.clone().y(5).build() == Foo { x: "four", y: 5 });
    assert!(semi_built2.y(6).build() == Foo { x: "four", y: 6 });

    // Just to make sure it can build with generic bounds
    #[allow(dead_code)]
    #[derive(TypedBuilder)]
    struct Bar<T: std::fmt::Debug>
    where
        T: std::fmt::Display,
    {
        x: T,
    }
}

#[test]
fn test_builder_on_struct_with_keywords() {
    #[allow(non_camel_case_types)]
    #[derive(PartialEq, TypedBuilder)]
    struct r#struct {
        r#fn: u32,
        #[builder(default, setter(strip_option))]
        r#type: Option<u32>,
        #[builder(default = Some(()), setter(skip))]
        r#enum: Option<()>,
        #[builder(setter(into))]
        r#union: String,
    }

    assert!(
        r#struct::builder().r#fn(1).r#union("two").build()
            == r#struct {
                r#fn: 1,
                r#type: None,
                r#enum: Some(()),
                r#union: "two".to_owned(),
            }
    );
}

#[test]
fn test_builder_on_struct_with_keywords_prefix_suffix() {
    #[allow(non_camel_case_types)]
    #[derive(PartialEq, TypedBuilder)]
    #[builder(field_defaults(setter(prefix = "set_", suffix = "_value")))]
    struct r#struct {
        r#fn: u32,
        #[builder(default, setter(strip_option))]
        r#type: Option<u32>,
        #[builder(default = Some(()), setter(skip))]
        r#enum: Option<()>,
        #[builder(setter(into))]
        r#union: String,
    }

    assert!(
        r#struct::builder().r#set_fn_value(1).r#set_union_value("two").build()
            == r#struct {
                r#fn: 1,
                r#type: None,
                r#enum: Some(()),
                r#union: "two".to_owned(),
            }
    );
}

#[test]
fn test_unsized_generic_params() {
    use std::marker::PhantomData;

    #[derive(TypedBuilder)]
    struct GenericStructure<K, V>
    where
        K: ?Sized,
    {
        key: PhantomData<K>,
        value: PhantomData<V>,
    }
}

#[test]
fn test_field_setter_transform_closure() {
    #[derive(PartialEq)]
    struct Point {
        x: i32,
        y: i32,
    }

    #[derive(PartialEq, TypedBuilder)]
    struct Foo {
        #[builder(setter(transform = |x: i32, y: i32| Point { x, y }))]
        point: Point,
    }

    assert!(
        Foo::builder().point(1, 2).build()
            == Foo {
                point: Point { x: 1, y: 2 }
            }
    );
}

#[test]
fn test_field_setter_transform_fn() {
    struct MBaseCase;

    struct MClosure;

    // Lifetime is not needed, just added to test.
    trait IntoValue<'a, T, M> {
        fn into_value(self) -> T;
    }

    impl<T, I> IntoValue<'_, T, MBaseCase> for I
    where
        I: Into<T>,
    {
        fn into_value(self) -> T {
            self.into()
        }
    }

    impl<T, F> IntoValue<'_, T, MClosure> for F
    where
        F: FnOnce() -> T,
    {
        fn into_value(self) -> T {
            self()
        }
    }

    #[derive(TypedBuilder)]
    struct Foo {
        #[builder(
            setter(
                fn transform<'a, M>(value: impl IntoValue<'a, String, M>)
                where
                    M: 'a,
                 {
                    value.into_value()
                },
            )
        )]
        s: String,
    }

    // Check where clause and return type
    #[derive(TypedBuilder)]
    struct Bar {
        #[builder(
            setter(
                fn transform<A>(value: A) -> String
                where
                    A: std::fmt::Display,
                 {
                    value.to_string()
                },
            )
        )]
        s: String,
    }

    assert_eq!(Foo::builder().s("foo").build().s, "foo".to_owned());
    assert_eq!(Foo::builder().s(|| "foo".to_owned()).build().s, "foo".to_owned());

    assert_eq!(Bar::builder().s(42).build().s, "42".to_owned());
}

#[test]
fn test_build_method() {
    #[derive(PartialEq, TypedBuilder)]
    #[builder(build_method(vis="", name=__build))]
    struct Foo {
        x: i32,
    }

    assert!(Foo::builder().x(1).__build() == Foo { x: 1 });
}

#[test]
fn test_builder_method() {
    #[derive(PartialEq, TypedBuilder)]
    #[builder(builder_method(vis="", name=__builder))]
    struct Foo {
        x: i32,
    }

    assert!(Foo::__builder().x(1).build() == Foo { x: 1 });
}

#[test]
fn test_builder_type() {
    #[derive(PartialEq, TypedBuilder)]
    #[builder(builder_type(vis="", name=__FooBuilder))]
    struct Foo {
        x: i32,
    }

    let builder: __FooBuilder<_> = Foo::builder();
    assert!(builder.x(1).build() == Foo { x: 1 });
}

#[test]
fn test_default_builder_type() {
    #[derive(Debug, PartialEq, TypedBuilder)]
    #[builder(builder_method(vis = ""), builder_type(name = InnerBuilder), build_method(into = Outer))]
    struct Inner {
        a: i32,
        b: i32,
    }

    #[derive(Debug, PartialEq)]
    struct Outer(Inner);

    impl Outer {
        pub fn builder() -> InnerBuilder {
            Inner::builder()
        }
    }

    impl From<Inner> for Outer {
        fn from(value: Inner) -> Self {
            Self(value)
        }
    }

    let outer = Outer::builder().a(3).b(5).build();
    assert_eq!(outer, Outer(Inner { a: 3, b: 5 }));
}

#[test]
fn test_into_set_generic_impl_from() {
    #[derive(TypedBuilder)]
    #[builder(build_method(into))]
    struct Foo {
        value: i32,
    }

    #[derive(Debug, PartialEq)]
    struct Bar {
        value: i32,
    }

    impl From<Foo> for Bar {
        fn from(value: Foo) -> Self {
            Self { value: value.value }
        }
    }

    let bar: Bar = Foo::builder().value(42).build();
    assert_eq!(bar, Bar { value: 42 });
}

#[test]
fn test_into_angle_bracket_type() {
    #[derive(Debug, PartialEq, TypedBuilder)]
    #[builder(build_method(into = std::sync::Arc<Foo>))]
    struct Foo {
        value: i32,
    }

    let foo: std::sync::Arc<Foo> = Foo::builder().value(42).build();
    assert_eq!(*foo, Foo { value: 42 });
}

#[test]
fn test_into_set_generic_impl_into() {
    #[derive(TypedBuilder)]
    #[builder(build_method(into))]
    struct Foo {
        value: i32,
    }

    #[derive(Debug, PartialEq)]
    struct Bar {
        value: i32,
    }

    impl From<Foo> for Bar {
        fn from(val: Foo) -> Self {
            Self { value: val.value }
        }
    }

    let bar: Bar = Foo::builder().value(42).build();
    assert_eq!(bar, Bar { value: 42 });
}

#[test]
fn test_prefix() {
    #[derive(Debug, PartialEq, TypedBuilder)]
    #[builder(field_defaults(setter(prefix = "with_")))]
    struct Foo {
        x: i32,
        y: i32,
    }

    let foo = Foo::builder().with_x(1).with_y(2).build();
    assert_eq!(foo, Foo { x: 1, y: 2 });
}

#[test]
fn test_suffix() {
    #[derive(Debug, PartialEq, TypedBuilder)]
    #[builder(field_defaults(setter(suffix = "_value")))]
    struct Foo {
        x: i32,
        y: i32,
    }

    let foo = Foo::builder().x_value(1).y_value(2).build();
    assert_eq!(foo, Foo { x: 1, y: 2 });
}

#[test]
fn test_prefix_and_suffix() {
    #[derive(Debug, PartialEq, TypedBuilder)]
    #[builder(field_defaults(setter(prefix = "with_", suffix = "_value")))]
    struct Foo {
        x: i32,
        y: i32,
    }

    let foo = Foo::builder().with_x_value(1).with_y_value(2).build();
    assert_eq!(foo, Foo { x: 1, y: 2 });
}

#[test]
fn test_issue_118() {
    #[derive(TypedBuilder)]
    #[builder(build_method(into=Bar))]
    struct Foo<T> {
        #[builder(default, setter(skip))]
        #[allow(dead_code)]
        foo: Option<T>,
    }

    struct Bar;

    impl<T> From<Foo<T>> for Bar {
        fn from(_value: Foo<T>) -> Self {
            Self
        }
    }

    let _ = Foo::<u32>::builder().build();
}

#[test]
fn test_mutable_defaults() {
    #[derive(TypedBuilder, PartialEq, Debug)]
    struct Foo {
        #[builder(default, mutable_during_default_resolution, setter(strip_option))]
        x: Option<i32>,
        #[builder(default = if let Some(x) = x.as_mut() {
            *x *= 2;
            *x
        } else {
            Default::default()
        })]
        y: i32,
    }

    let foo = Foo::builder().x(5).build();

    assert_eq!(foo, Foo { x: Some(10), y: 10 });
}

#[test]
fn test_preinitialized_fields() {
    #[derive(Debug, PartialEq, TypedBuilder)]
    struct Foo {
        x: i32,
        #[builder(via_mutators)]
        y: i32,
        #[builder(via_mutators = 2)]
        z: i32,
        #[builder(via_mutators(init = 2))]
        w: i32,
    }

    let foo = Foo::builder().x(1).build();
    assert_eq!(foo, Foo { x: 1, y: 0, z: 2, w: 2 });
}

#[test]
fn test_mutators_item() {
    #[derive(Debug, PartialEq, TypedBuilder)]
    #[builder(mutators(
        #[mutator(requires = [x])]
        fn inc_x(self) {
            self.x += 1;
        }
        #[mutator(requires = [x])]
        fn inc_x_by(self, x: i32) {
            self.x += x;
        }
        fn inc_preset(self) {
            self.y += 1;
            self.z += 1;
            self.w += 1;
        }
        #[mutator(requires = [x])]
        fn inc_y_by_x(self) {
            self.y += self.x;
        }
    ))]
    struct Foo {
        x: i32,
        #[builder(via_mutators)]
        y: i32,
        #[builder(via_mutators = 2)]
        z: i32,
        #[builder(via_mutators(init = 2))]
        w: i32,
    }

    let foo = Foo::builder().x(1).inc_x().inc_preset().build();
    assert_eq!(foo, Foo { x: 2, y: 1, z: 3, w: 3 });
    let foo = Foo::builder().x(1).inc_x_by(4).inc_y_by_x().build();
    assert_eq!(foo, Foo { x: 5, y: 5, z: 2, w: 2 });
}

#[test]
fn test_mutators_field() {
    #[derive(Debug, PartialEq, TypedBuilder)]
    #[builder(mutators())]
    struct Foo {
        #[builder(mutators(
            fn inc_x(self) {
                self.x += 1;
            }
            #[mutator(requires = [y])]
            fn inc_y_by_x(self) {
                self.y += self.x;
            }
        ))]
        x: i32,
        #[builder(default)]
        y: i32,
        #[builder(via_mutators = 2, mutators(
            fn inc_preset(self) {
                self.z += 1;
                self.w += 1;
            }
        ))]
        z: i32,
        #[builder(via_mutators(init = 2))]
        w: i32,
    }

    let foo = Foo::builder().x(1).inc_x().inc_preset().build();
    assert_eq!(foo, Foo { x: 2, y: 0, z: 3, w: 3 });
    let foo = Foo::builder().x(1).y(1).inc_y_by_x().build();
    assert_eq!(foo, Foo { x: 1, y: 2, z: 2, w: 2 });
}

#[test]
fn test_mutators_for_generic_fields() {
    use core::ops::AddAssign;

    #[derive(Debug, PartialEq, TypedBuilder)]
    struct Foo<S: Default + AddAssign, T: Default + AddAssign> {
        #[builder(via_mutators, mutators(
            fn x_plus(self, plus: S) {
                self.x += plus;
            }
        ))]
        x: S,
        y: T,
    }

    assert_eq!(Foo::builder().x_plus(1).y(2).build(), Foo { x: 1, y: 2 });
}

#[test]
fn test_mutators_with_type_param() {
    use core::ops::AddAssign;

    trait HasS {
        type MyS: Default + AddAssign;
    }

    #[derive(Debug, PartialEq, TypedBuilder)]
    struct Foo<S: Default + AddAssign> {
        #[builder(via_mutators, mutators(
            fn x_plus<H: HasS<MyS = S>>(self, s: <H as HasS>::MyS) {
                self.x += s;
            }
        ))]
        x: S,
    }

    struct HasSImpl;

    impl HasS for HasSImpl {
        type MyS = u32;
    }

    assert_eq!(Foo::builder().x_plus::<HasSImpl>(1).build(), Foo { x: 1 });
}
