# Safety

## Unsafety policies

By default, every `autocxx` function is `unsafe`. That means you can only call C++ functions from `unsafe` blocks, and it's up to you to be sure that the C++ code upholds the invariants the Rust compiler expects.

You can optionally specify:

`safety!(unsafe)`

within your `include_cpp!` macro invocation. If you do this, you are promising the Rust compiler that _all_ your C++ function calls are upholding the invariants which `rustc` expects, and thus each individual function is no longer `unsafe`.

See [`safety!`](https://docs.rs/autocxx/latest/autocxx/macro.safety.html) in the documentation for more details.

## Examples with and without `safety!(unsafe)`

Without a `safety!` directive:

```rust,ignore,autocxx
autocxx_integration_tests::doctest(
"",
"#include <cstdint>
inline uint32_t do_math(uint32_t a, uint32_t b) { return a+b; }",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    generate!("do_math")
}

fn main() {
    assert_eq!(unsafe { ffi::do_math(12, 13) }, 25);
}
}
)
```

With a `safety!` directive:

```rust,ignore,autocxx
autocxx_integration_tests::doctest(
"",
"#include <cstdint>
inline uint32_t do_math(uint32_t a, uint32_t b) { return a+b; }",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe)
    generate!("do_math")
}

fn main() {
    assert_eq!(ffi::do_math(12, 13), 25);
}
}
)
```


## Pragmatism in a complex C++ codebase

This crate mostly intends to follow the lead of the `cxx` crate in where and when `unsafe` is required. But, this crate is opinionated. It believes some unsafety requires more careful review than other bits, along the following spectrum:

* Rust unsafe code (requires most review)
* Rust code calling C++ with raw pointers
* Rust code calling C++ with shared pointers, or anything else where there can be concurrent mutation
* Rust code calling C++ with unique pointers, where the Rust single-owner model nearly always applies (but we can't _prove_ that the C++ developer isn't doing something weird)
* Rust safe code (requires least review)

If your project is 90% Rust code, with small bits of C++, _don't use this crate_. You need something where all C++ interaction is marked with big red "this is terrifying" flags. This crate is aimed at cases where there's 90% C++ and small bits of Rust, and so we want the Rust code to be pragmatically reviewable without the signal:noise ratio of `unsafe` in the Rust code becoming so bad that `unsafe` loses all value.

## Worked example

Imagine you have this C++:

```cpp
struct Thing;
void print_thing(const Thing& thing);
```

By using `autocxx` (or `cxx`), you're promising the Rust compiler that the `print_thing` function does sensible things with that
reference:

* It doesn't store a pointer to the thing anywhere and pass it back to Rust later.
* It doesn't mutate it.
* It doesn't delete it.
* or any of the other things that you're not permitted to do in unsafe Rust.

