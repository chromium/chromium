# C++ structs, enums and classes

If you add a C++ struct, class or enum to the [allowlist](allowlist.md), Rust bindings will be generated to that type and to any methods it has.
Even if you don't add it to the allowlist, the type may be generated if it's required by some other function - but in this case
all its methods won't be generated.

Rust and C++ differ in an important way:

* In Rust, the compiler is free to pick up some data and move it to somewhere else (in a `memcpy` sense). The object is none the wiser.
* In C++, once created, an object stays where it is, until or unless it has its "move constructor" invoked.

This makes a big difference: C++ objects can have self-referential pointers, and any such pointer would be invalidated by Rust doing
a memcpy. Such self-referential pointers are common - even some implementations of `std::string` do it.

## POD and non-POD

When asking `autocxx` to generate bindings for a type, then, you have to make a choice.

* *This C++ type is trivial*. It has no destructor or move constructor (or they're trivial), and thus Rust is free to move it around the stack as it wishes. `autocxx` calls these types POD ("plain old data"). Alternatively,
* *This C++ type has a non-trivial destructor or move constructor, so we can't allow Rust to move this around*. `autocxx` calls these types non-POD.

POD types are nicer:

* You can just use them as regular Rust types.
* You get direct field access.
* No funny business.

Non-POD types are awkward:

* You can't just _have_ one as a Rust variable. Normally you hold them in a [`cxx::UniquePtr`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html), though there are other options.
* There is no access to fields (yet).
* You can't even have a `&mut` reference to one, because then you might be able to use [`std::mem::swap`](https://doc.rust-lang.org/stable/std/mem/fn.swap.html) or similar. You can have a `Pin<&mut>` reference, which is more fiddly.

By default, `autocxx` generates non-POD types. You can request a POD type using [`generate_pod!`](https://docs.rs/autocxx/latest/autocxx/macro.generate_pod.html). Don't worry: you can't mess this up. If the C++ type doesn't in fact comply with the requirements for a POD type, your build will fail thanks to some static assertions generated in the C++. (If you're _really_ sure your type is freely relocatable, because you implemented the move constructor and destructor and you promise they're trivial, you can override these assertions using the C++ trait `IsRelocatable` per the instructions in [cxx.h](https://github.com/dtolnay/cxx/blob/master/include/cxx.h)).

See [the chapter on storage](storage.md) for lots more detail on how you can hold onto non-POD types.

## Construction

Each constructor results in _two_ Rust functions.

* A `new` function exists, which
  offers a constructor per the standards of the `moveit` crate, and thus can be used
  to place the object on the Rust stack (as well as in various containers such as `Box`
  and `UniquePtr`)
* A `make_unique` function is also created, which constructs the item directly into
  a `cxx::UniquePtr`. This is more commonly what you want.

Multiple constructors (aka constructor overloading) follows the same [rules as other functions](cpp_functions.html#overloads---and-identifiers-ending-in-digits).

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"void A::set(uint32_t val) { a = val; }
uint32_t A::get() const { return a; }",
"#include <stdint.h>
#include <string>
struct A {
    A() {}
    void set(uint32_t val);
    uint32_t get() const;
    uint32_t a;
};
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("A")
}

fn main() {
    moveit! {
        let mut stack_obj = ffi::A::new();
    }
    stack_obj.as_mut().set(42);
    assert_eq!(stack_obj.get(), 42);

    let mut heap_obj = ffi::A::make_unique();
    heap_obj.pin_mut().set(42);
    assert_eq!(heap_obj.get(), 42);
}
}
)
```

## Forward declarations

A type which is incomplete in the C++ headers (i.e. represented only by a forward
declaration) can't be held in a `UniquePtr` within Rust (because Rust can't know
if it has a destructor that will need to be called if the object is dropped.)
Naturally, such an object can't be passed by value either; it can still be
referenced in Rust references.

## Generic (templated) types

If you're using one of the generic types which is supported natively by cxx,
e.g. `std::unique_ptr`, it should work as you expect. For other generic types,
we synthesize a concrete Rust type, corresponding to a C++ typedef, for each
concrete instantiation of the type. Such generated types are always opaque,
and never have methods attached. That's therefore enough to pass them
between return types and parameters of other functions within [`cxx::UniquePtr`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html)s
but not really enough to do anything else with these types yet[^templated].

[^templated]: Future improvements tracked [here](https://github.com/google/autocxx/issues/349)

To make them more useful, you might have to add extra C++ functions to extract
data or otherwise deal with them.

Usually, such concrete types are synthesized automatically because they're
parameters or return values from functions. Very rarely, you may
want to synthesize them yourself - you can do this using the
[`concrete!`](https://docs.rs/autocxx/latest/autocxx/macro.concrete.html)
directive. As noted, though, these types are currently opaque and fairly
useless without passing them back and forth to C++, so this is not a commonly
used facility. It does, however, allow you to give a more descriptive name
to the type in Rust:

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"",
"#include <string>
struct Tapioca {
  std::string yuck;
};
template<typename Floaters>
struct Tea {
  Tea() : floaters(nullptr) {}
  Floaters* floaters;
};
inline Tea<Tapioca> prepare() {
  Tea<Tapioca> mixture;
  // prepare...
  return mixture;
}
inline void drink(const Tea<Tapioca>&) {}
",
{
use autocxx::prelude::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("prepare")
    generate!("drink")
    concrete!("Tea<Tapioca>", Boba)
}

fn main() {
    let nicer_than_it_sounds: cxx::UniquePtr<ffi::Boba> = ffi::prepare();
    ffi::drink(&nicer_than_it_sounds);
}
}
)
```

## Implicit member functions

Most of the API of a C++ type is contained within the type, so `autocxx` can
understand what is available for Rust to call when that type is analyzed.
However, there is an important exception for the so-called special
member functions, which will be implicitly generated by the C++ compiler for
some types. `autocxx` makes use of these types of special members:
* Default constructor
* Destructor
* Copy constructor
* Move constructor

Explicitly declared versions of these special members are easy: `autocxx` knows
they exist and uses them.

`autocxx` currently uses its own analysis to determine when implicit versions of
these exist. This analysis tries to be conservative (avoid generating wrappers
that require the existence of C++ functions that don't exist), but sometimes
this goes wrong and understanding the details is necessary to get the correct
Rust wrappers generated.

In particular, determing whether an implicit version of any of these exists
requires analyzing the types of all bases and members. `autocxx` only analyzes
types when requested, because some may be un-analyzable. If the types of any
bases or members are not analyzed, `autocxx` will assume a public destructor
exists (in the absence of any other destructors), and avoid using any other
implicit special member functions. Notably this includes the default
constructor, so types with un-analyzed bases or members and no explicit
constructors will not get a `make_unique` or `new` generated. If `autocxx` isn't
generating a `make_unique` or `CopyNew` or `MoveNew` for a type which permits
the corresponding operations in C++, make sure the types of all bases and
members are analyzed or implement it explicitly.

`autocxx` currently does not take member initializers (`const int x = 5`) into
account when determining whether a default constructor
exists[^member-initializers]. Explicitly declared default destructors still
work though.

Currently, `autocxx` assumes that an explicitly defaulted (`= default`) member
function exists, although it is valid C++ for that to be
deleted[^explicitly-defaulted]. Clang's
[-Wdefaulted-function-deleted](https://clang.llvm.org/docs/DiagnosticsReference.html#wdefaulted-function-deleted)
flag (enabled by default) will warn about types like this.

A C++ type which can be instantiated but has an inaccessible constructor will
be leaked by Rust[^inaccessible-destructor]. The object's memory itself will be
freed without calling any C++ destructor, which will leak any resources tracked
by the C++ implementation.

Many of the special members may be overloaded in C++. This generally means
adding `const` or `volatile` qualifiers or extra arguments with defaults.
`autocxx` avoids using any overloaded special members because choosing which
one to call from Rust gets tricky.

[^member-initializers]: Handling of member initializers is tracked
[here](https://github.com/google/autocxx/issues/816).
[^explicitly-defaulted]: Fix for explicitly defaulted special member functions
that are deleted is tracked [here](https://github.com/google/autocxx/issues/815).
[^inaccessible-destructor]: Discussion around what to do about inaccessible or
deleted destructors [here](https://github.com/google/autocxx/issues/829).

## Abstract types

`autocxx` does not allow instantiation of abstract types[^abstract] (aka types with pure virtual methods).

[^abstract]: `autocxx`'s determination of abstract types is a bit approximate and
[could be improved](https://github.com/google/autocxx/issues/774).