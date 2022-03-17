# The allowlist and `include_cpp` syntax

To include C++ in your Rust codebase using `autocxx`, you will need
at least one [`include_cpp` macro](https://docs.rs/autocxx/latest/autocxx/macro.include_cpp.html).

The simplest is:

```rust,ignore
use autocxx::prelude::*;

include_cpp! {
    #include "my_header.h"
    generate!("MyAPIFunction")
}
```

You need to include [`generate!` directives](https://docs.rs/autocxx/latest/autocxx/macro.generate.html)
for every *type* or *function* you wish to access from Rust. You don't need to specify this for member functions
of types that you've added - they'll be generated automatically. (If a particular member function can't
be generated, some placeholder item with explanatory documentation [will be generated instead](workflow.md)).

Various other directives are possible inside this macro, most notably:

* You can ask to generate all the items in a namespace using
  [`generate_ns!`](https://docs.rs/autocxx/latest/autocxx/macro.generate_ns.html)
* You might sometimes want to ask that a type is generated as 'plain old data' using
  [`generate_pod!`](https://docs.rs/autocxx/latest/autocxx/macro.generate_pod.html) instead of `generate!` -
  see the chapter on [C++ types](cpp_types.md).
* You'll probaly want to specify a [`safety!` policy](safety.md)

See [the docs.rs documentation for the full list](https://docs.rs/autocxx/latest/autocxx/).
