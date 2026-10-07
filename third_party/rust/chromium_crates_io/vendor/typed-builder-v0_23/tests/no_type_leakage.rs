// As long as this test compiles, it passes (test does not occur at runtime)

use typed_builder::TypedBuilder;

#[derive(PartialEq, TypedBuilder)]
#[builder(
    build_method(vis="pub", into=Bar),
    builder_method(vis=""),
    builder_type(vis="pub", name=BarBuilder),
)]
struct Foo {
    x: i32,
}

#[allow(unused)]
pub struct Bar(Foo);

impl Bar {
    pub fn builder() -> BarBuilder {
        Foo::builder()
    }
}

impl From<Foo> for Bar {
    fn from(wrapped: Foo) -> Self {
        Bar(wrapped)
    }
}
