[![Build Status](https://github.com/idanarye/rust-typed-builder/workflows/CI/badge.svg)](https://github.com/idanarye/rust-typed-builder/actions)
[![Latest Version](https://img.shields.io/crates/v/typed-builder.svg)](https://crates.io/crates/typed-builder)
[![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)](https://idanarye.github.io/rust-typed-builder/)

# Rust Typed Builder

Creates a compile-time verified builder:

```rust
use typed_builder::TypedBuilder;

#[derive(TypedBuilder)]
struct Foo {
    // Mandatory Field:
    x: i32,

    // #[builder(default)] without parameter - use the type's default
    // #[builder(setter(strip_option))] - wrap the setter argument with `Some(...)`
    #[builder(default, setter(strip_option))]
    y: Option<i32>,

    // Or you can set the default
    #[builder(default=20)]
    z: i32,
}
```

Build in any order:
```rust
Foo::builder().x(1).y(2).z(3).build();
Foo::builder().z(1).x(2).y(3).build();
```

Omit optional fields(the one marked with `#[default]`):
```rust
Foo::builder().x(1).build()
```

But you can't omit non-optional arguments - or it won't compile:
```rust
Foo::builder().build(); // missing x
Foo::builder().x(1).y(2).y(3); // y is specified twice
```

## Features

* Custom derive for generating the builder pattern.
* Ability to annotate fields with `#[builder(setter(into))]` to make their setters accept `Into` values.
* Compile time verification that all fields are set before calling `.build()`.
* Compile time verification that no field is set more than once.
* Ability to annotate fields with `#[builder(default)]` to make them optional and specify a default value when the user does not set them.
* Generates simple documentation for the `.builder()` method.
* Customizable method name and visibility of the `.build()` method.

## Limitations

* The build errors when you neglect to set a field or set a field describe the actual problem as a deprecation warning, not as the main error.
* The generated builder type has ugly internal name and many generic parameters. It is not meant for passing around and doing fancy builder tricks - only for nicer object creation syntax(constructor with named arguments and optional arguments).
    * For the that reason, all builder methods are call-by-move and the builder is not cloneable. Saves the trouble of determining if the fields are cloneable...
    * If you want a builder you can pass around, check out [derive-builder](https://crates.io/crates/derive_builder). It's API does not conflict with typed-builder's so you can be able to implement them both on the same type.

## Conflicts

* `TypedBuilder` accepts arbitrary Rust code for `#[builder(default = ...)]`, but other custom derive proc-macro crates may try to parse them using the older restrictions that allow only literals. To solve this, use `#[builder(default_code = "...")]` instead.

## Alternatives - and why typed-builder is better

* [derive-builder](https://crates.io/crates/derive_builder) - does all the checks in runtime, returning a `Result` you need to unwrap.
* [safe-builder-derive](https://crates.io/crates/safe-builder-derive) - this one does compile-time checks - by generating a type for each possible state of the builder. Rust can remove the dead code, but your build time will still be exponential. typed-builder is encoding the builder's state in the generics arguments - so Rust will only generate the path you actually use.

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any
additional terms or conditions.
