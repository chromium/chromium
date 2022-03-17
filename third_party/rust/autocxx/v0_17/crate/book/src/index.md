[![GitHub](https://img.shields.io/crates/l/autocxx)](https://github.com/google/autocxx)
[![crates.io](https://img.shields.io/crates/d/autocxx)](https://crates.io/crates/autocxx)
[![docs.rs](https://docs.rs/autocxx/badge.svg)](https://docs.rs/autocxx)

# autocxx — automatic safe interop between Rust and C++

Welcome to `autocxx` and thank you for reading!

Use `autocxx` if you have a large existing C++ codebase and you want to use its types and functions from Rust with maximal safety and minimal fuss.

`autocxx` is like `bindgen`, in that it enables you to use C++ functions and types from within Rust. But it automates a lot of the fiddly things you need to do with `bindgen` bindings: calling destructors, converting strings, unsafely handling raw pointers. C++ functions and types within `autocxx` bindings should behave naturally and ergonomically, _almost_ as if they were safe Rust functions and types themselves. These ergonomics and safety improvements come from the [`cxx`](https://cxx.rs) project - hence the name of this tool, `autocxx`.

`autocxx` combines the safety and ergonomics of `cxx` with the automatic bindings generation of `bindgen`. It stands on the shoulders of those giants!

## When is `autocxx` the right tool?

Not always:

* If you are making bindings to C code, as opposed to C++, use [`bindgen`](https://rust-lang.github.io/rust-bindgen/) instead.
* If you can make unrestricted changes to the C++ code, use [`cxx`](https://cxx.rs) instead.
* If your C++ to Rust interface is just a few functions or types, use [`cxx`](https://cxx.rs) instead.

But sometimes:

* If you need to call arbitrary functions and use arbitrary types within an existing C++ codebase, use `autocxx`. You're in the right place!
* Like `cxx`, but unlike `bindgen`, `autocxx` helps with calls from C++ to Rust, too.

## Examples to give you a feel for `autocxx`

Here's a code example:

```rust,ignore,autocxx
autocxx_integration_tests::doctest(
"",
"#include <stdint.h>
inline uint32_t do_math(uint32_t a, uint32_t b) { return a+b; }",
{
// Use all the autocxx types which might be handy.
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("do_math") // allowlist a function
}

fn main() {
    assert_eq!(ffi::do_math(12, 13), 25);
}
}
)
```

A more complex example:

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"
#include <strstream>
void Goat::add_a_horn() { horns++; }
Goat::Goat() : horns(0) {}
Goat::~Goat() {}
std::string Goat::describe() const {
    std::ostrstream oss;
    std::string plural = horns == 1 ? \"\" : \"s\";
    oss << \"This goat has \" << horns << \" horn\" << plural << \".\";
    return oss.str();
}
",
"#include <cstdint>
#include <string>

class Goat {
public:
    Goat();
    ~Goat();
    void add_a_horn();
    std::string describe() const;
private:
    uint32_t horns;
};
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("Goat") // allowlist a type and all its methods
}

fn main() {
    let mut goat = ffi::Goat::make_unique(); // returns a cxx::UniquePtr, i.e. a std::unique_ptr
    goat.pin_mut().add_a_horn();
    goat.pin_mut().add_a_horn();
    assert_eq!(goat.describe().as_ref().unwrap().to_string_lossy(), "This goat has 2 horns.");
}
}
)
```

This is typical `autocxx` code: the C++ objects behave much like Rust objects, but
sometimes extra steps are required to handle cases like null pointers or converting strings that may not be UTF-8.

Still, fundamentally, you can interact with C++ objects without using `unsafe` in the majority of cases.
`cxx` takes care of the fundamentals of lifetimes and destructors.

## How to read this book

We'd recommend starting with [tutorial](tutorial.md) and then [workflow](workflow.md).