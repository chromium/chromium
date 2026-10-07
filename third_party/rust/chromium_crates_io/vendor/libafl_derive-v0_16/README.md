# `LibAFL_derive`: Derive Macros for `LibAFL`

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

The `libafl_derive` crate offers derive macros, such as `#[derive(SerdeAny)]`.

## Available Derive Macros

### `#[derive(SerdeAny)]`

This macro implements the `SerdeAny` trait for a type. This is necessary to store the type in a `SerdeAnyMap`, a key component for type-safe storage of different data types in `LibAFL`.

**Usage:**

```rust,ignore
use libafl_derive::SerdeAny;
use serde::{Serialize, Deserialize};

#[derive(SerdeAny, Serialize, Deserialize)]
struct MyStruct {
    // ...
}
```

### `#[derive(Display)]`

This macro implements the `core::fmt::Display` trait for a struct. It generates a `Display` implementation that concatenates the string representations of all fields, separated by spaces.

**Special Handling:**

* **`Option<T>`**: If the value is `Some(inner)`, the inner value is displayed. If it is `None`, nothing is displayed.
* **`Vec<T>`**: The elements of the vector are displayed, each separated by a space.

**Example:**

```rust
use libafl_derive::Display;
use std::fmt::Display;

#[derive(Display)]
struct MyStruct {
    foo: String,
    bar: Option<u32>,
    baz: Vec<i32>,
}

let instance = MyStruct {
    foo: "hello".to_string(),
    bar: Some(42),
    baz: vec![1, 2, 3],
};
// The following will print: " hello 42 1 2 3"
println!("{}", instance);
```

## The `LibAFL` Project

The `LibAFL` project is part of [`AFLplusplus`](https://github.com/AFLplusplus) and maintained by

* [Andrea Fioraldi](https://twitter.com/andreafioraldi) <andrea@aflplus.plus>
* [Dominik Maier](https://bsky.app/profile/dmnk.bsky.social) <dominik@aflplus.plus>
* [s1341](https://twitter.com/srubenst1341) <github@shmarya.net>
* [Dongjia Zhang](https://github.com/tokatoka) <toka@aflplus.plus>
* [Addison Crump](https://github.com/addisoncrump) <me@addisoncrump.info>

## Contributing

For bugs, feel free to open issues or contact us directly. Thank you for your support. <3

Even though we will gladly assist you in finishing up your PR, try to

* keep all the crates compiling with *stable* rust (hide the eventual non-stable code under `cfg`s.)
* run `cargo nightly fmt` on your code before pushing
* check the output of `cargo clippy --all` or `./clippy.sh`
* run `cargo build --no-default-features` to check for `no_std` compatibility (and possibly add `#[cfg(feature = "std")]`) to hide parts of your code.

Some parts in this list may sound hard, but don't be afraid to open a PR if you cannot fix them by yourself. We will gladly assist.

#### License

<sup>
Licensed under either of <a href="../LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="../LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>

<br>

<sub>
Dependencies under more restrictive licenses, such as GPL or AGPL, can be enabled
using the respective feature in each crate when it is present, such as the
'agpl' feature of the libafl crate.
</sub>
