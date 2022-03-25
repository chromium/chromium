# Naming

## Namespaces

The C++ namespace structure is reflected in mods within the generated
ffi mod. However, at present there is an internal limitation that
autocxx can't handle multiple types with the same identifier, even
if they're in different namespaces. This will be fixed in future.

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"
void generations::hey_boomer() {}
void submarines::hey_boomer() {}",
"
namespace generations {
  void hey_boomer();
}
namespace submarines {
  void hey_boomer();
}
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("submarines::hey_boomer")
    generate!("generations::hey_boomer")
}

fn main() {
    ffi::generations::hey_boomer(); // insults your elders and betters
    ffi::submarines::hey_boomer(); // launches missiles
}
}
)
```

## Nested types

There is support for generating bindings of nested types, with some
restrictions. Currently the C++ type `A::B` will be given the Rust name
`A_B` in the same module as its enclosing namespace.

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"",
"
struct Turkey {
    struct Duck {
        struct Hen {
            int wings;
        };
    };
};
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate_pod!("Turkey_Duck_Hen")
}

fn main() {
    let _turducken = ffi::Turkey_Duck_Hen::make_unique();
}
}
)
```

## Overloads

See [the chapter on C++ functions](cpp_functions.md).
