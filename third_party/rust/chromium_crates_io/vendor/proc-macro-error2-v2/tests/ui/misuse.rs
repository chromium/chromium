use proc_macro_error2::abort;

struct Foo;

#[allow(unused)]
fn foo() {
    abort!(Foo, "BOOM");
}

fn main() {}
