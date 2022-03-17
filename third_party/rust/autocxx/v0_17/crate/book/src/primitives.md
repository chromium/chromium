# Built-in types

`autocxx` relies primarily on the [standard cxx types](https://cxx.rs/bindings.html).
In particular you should become familiar with [`cxx::UniquePtr`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html) and [`cxx::CxxString`](https://docs.rs/cxx/latest/cxx/struct.CxxString.html).

There are a few additional integer types, such as [`c_int`](https://docs.rs/autocxx/latest/autocxx/struct.c_int.html),
which are not yet upstreamed to `cxx`. These are to support those pesky C/C++ integer types
which do not have a predictable number of bits on different machines.

```rust,ignore,autocxx
autocxx_integration_tests::doctest(
"",
"inline int do_math(int a, int b) { return a+b; }",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("do_math")
}

fn main() {
    assert_eq!(ffi::do_math(c_int(12), c_int(13)), c_int(25));
}
}
)
```

## Strings

`autocxx` uses [`cxx::CxxString`](https://docs.rs/cxx/latest/cxx/struct.CxxString.html). However, as noted above, we can't
just pass a C++ string by value, so we'll box and unbox it automatically
such that you're really dealing with `UniquePtr<CxxString>` on the Rust
side, even if the API just took or returned a plain old `std::string`.

However, to ease ergonomics, functions that accept a `std::string` will
actually accept anything that
implements a trait called `ffi::ToCppString`. That may either be a
`UniquePtr<CxxString>` or just a plain old Rust string - which will be
converted transparently to a C++ string.

This trait, and its implementations, are not present in the `autocxx`
documentation because they're dynamically generated in _your_ code
so that they can call through to a `make_string` implementation in
the C++ that we're injecting into your C++ build system.

(None of that happens if you use [`exclude_utilities`](https://docs.rs/autocxx/latest/autocxx/macro.exclude_utilities.html), so don't do that.)

```rust,ignore,autocxx
autocxx_integration_tests::doctest(
"",
"#include <string>
#include <cstdint>
inline uint32_t take_string(std::string a) { return a.size(); }",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("take_string")
}

fn main() {
    assert_eq!(ffi::take_string("hello"), 5)
}
}
)
```

If you need to create a blank `UniquePtr<CxxString>` in Rust, such that
(for example) you can pass its mutable reference or pointer into some
pre-existing C++ API, call `ffi::make_string("")` which will return
a blank `UniquePtr<CxxString>`.

If all you need is a _reference_ to a `CxxString`, you can alternatively use
[`cxx::let_cpp_string`](https://docs.rs/cxx/latest/cxx/macro.let_cxx_string.html).
