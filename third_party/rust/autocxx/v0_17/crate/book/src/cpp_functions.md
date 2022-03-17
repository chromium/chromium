# C++ functions

Calling C++ functions is largly as you might expect.

## Value and rvalue parameters

Functions taking [non-POD](cpp_types.md) value parameters can take a `cxx::UniquePtr<T>`
or a `&T`. This gives you the choice of Rust semantics - where a parameter
is absorbed and destroyed - or C++ semantics where the parameter is copied.


```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"
#include <strstream>
Goat::Goat() {}
void feed_goat(Goat g) {}
",
"#include <cstdint>

struct Goat {
    Goat();
    uint32_t horn_count;
};

void feed_goat(Goat g); // takes goat by value
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("Goat")
    generate!("feed_goat")
}

fn main() {
    let goat = ffi::Goat::make_unique(); // returns a cxx::UniquePtr, i.e. a std::unique_ptr
    // C++-like semantics...
    ffi::feed_goat(&goat);
    // ... you've still got the goat!
    ffi::feed_goat(&goat);
    // Or, Rust-like semantics, where the goat is consumed.
    ffi::feed_goat(goat);
    // No goat any more...
    // ffi::feed_goat(&goat); // doesn't compile
}
}
)
```

Specifically, you can pass anything which implements [`ValueParam<T>`](https://docs.rs/autocxx/latest/autocxx/trait.ValueParam.html).

If you're keeping non-POD values on the Rust stack, you need to explicitly use [`as_mov`](https://docs.rs/autocxx/latest/autocxx/prelude/fn.as_mov.html) to indicate that you want to
consume the object using move semantics:

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"
#include <strstream>
Blimp::Blimp() {}
void burst(Blimp g) {}
",
"#include <cstdint>

struct Blimp {
    Blimp();
    uint32_t tons_of_helium;
};

void burst(Blimp b); // consumes blimp,
    // but because C++ is amazing, may copy the blimp first.
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("Blimp")
    generate!("burst")
}

fn main() {
    moveit! {
        let mut blimp = ffi::Blimp::new();
    }
    ffi::burst(&*blimp); // pass by copy
    ffi::burst(as_copy(blimp.as_ref())); // explicitly say you want to pass by copy
    ffi::burst(as_mov(blimp)); // consume, using move constructor
}
}
)
```

Rvalue parameters are not yet supported.

## Default parameters

Are not yet supported[^default].

[^default]: the work is [planned here](https://github.com/google/autocxx/issues/563).

## Return values

At present, return values for [non-POD](cpp_types.md) types are always
a `cxx::UniquePtr<T>`. This is likely to change in future, at least to a type
which is guaranteed not to be null[^not-null.]

[^not-null]: [plans here](https://github.com/google/autocxx/issues/845)

## Overloads - and identifiers ending in digits

C++ allows function overloads; Rust doesn't. `autocxx` follows the lead
of `bindgen` here and generating overloads as `func`, `func1`, `func2` etc.
This is essentially awful without `rust-analyzer` IDE support - see the
[workflows chapter](workflow.md) for why you should be using an IDE.

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"
void saw(const View&) {}
void saw(const Tree&) {}",
"
#include <string>
struct View {
    std::string of_what; // go and watch In Bruges, it's great
};

struct Tree {
    int dendrochronologically_determined_age;
};

void saw(const View&);
void saw(const Tree&);
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("Tree")
    generate!("View")
    generate!("saw")
    generate!("saw1")
}

fn main() {
    let view = ffi::View::make_unique();
    ffi::saw(&view);
    let tree = ffi::Tree::make_unique();
    ffi::saw1(&tree); // yuck, overload
}
}
)
```

`autocxx` doesn't yet support default parameters.

It's fairly likely we'll change the model here in the future, such that
we can pass tuples of different parameter types into a single function
implementation.

## Methods

Calling a *const* method is simple:

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"",
"
class Sloth {
public:
    void sleep() const {} // sloths unchanged by sleep
};
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("Sloth")
}

fn main() {
    let sloth = ffi::Sloth::make_unique();
    sloth.sleep();
    sloth.sleep();
}
}
)
```

Calling a non-const method is a bit more of a pain. Per `cxx` norms, all mutable
references to C++ objects must be [pinned](https://doc.rust-lang.org/std/pin/).
In practice, this means you must call [`.pin_mut()`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html#method.pin_mut)
every time you call a method:

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"",
"
class Sloth {
public:
    void unpeel_from_tree() {} // sloths get agitated when removed from
        // trees, probably shouldn't be const
};
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("Sloth")
}

fn main() {
    let mut sloth = ffi::Sloth::make_unique();
    sloth.pin_mut().unpeel_from_tree();
    sloth.pin_mut().unpeel_from_tree();
}
}
)
```