# Callbacks into Rust

`autocxx` is primarily to allow calls from Rust to C++, but like `cxx` it also allows you to expose Rust APIs to C++.

You can:
* Declare that Rust types should be available to C++ using [`extern_rust_type`](https://docs.rs/autocxx/latest/autocxx/extern_rust/attr.extern_rust_type.html)
* Make Rust functions available to C++ using [`extern_rust_function`](https://docs.rs/autocxx/latest/autocxx/extern_rust/attr.extern_rust_function.html).
* Allow Rust subclasses of C++ classes.

This latter option is most commonly used for implementing "listeners" or ["observers"](https://en.wikipedia.org/wiki/Observer_pattern), so is often in practice how C++ will call into Rust. More details below.

## Subclasses

There is limited and experimental support for creating Rust subclasses of
C++ classes. (Yes, even more experimental than all the rest of this!)
See [`subclass::CppSubclass`](https://docs.rs/autocxx/latest/autocxx/subclass/trait.CppSubclass.html) for information about how you do this.
This is useful primarily if you want to listen out for messages broadcast
using the C++ observer/listener pattern.

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"
GoatObserver* obs = NULL;
int goat_feed = 0;

void register_observer(const GoatObserver& observer) {
    obs = const_cast<GoatObserver*>(&observer);
}
void deregister_observer() {
    obs = NULL;
};
void feed_goat() {
    goat_feed++;
    if (goat_feed > 2 && obs) {
        obs->goat_full();
    }
}
",
"#include <memory>
class GoatObserver {
public:
    virtual void goat_full() const = 0;
    virtual ~GoatObserver() {}
};

void register_observer(const GoatObserver& observer);
void deregister_observer();
void feed_goat();
",
{
use autocxx::prelude::*;
use autocxx::subclass::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    generate!("register_observer")
    generate!("deregister_observer")
    generate!("feed_goat")
    subclass!("GoatObserver", MyGoatObserver)
}

use ffi::*;

#[is_subclass(superclass("GoatObserver"))]
#[derive(Default)]
pub struct MyGoatObserver;

impl GoatObserver_methods for MyGoatObserver {
    fn goat_full(&self) {
        println!("BURP!");
    }
}

impl Drop for MyGoatObserver {
    fn drop(&mut self) {
        deregister_observer();
    }
}

fn main() {
    let goat_obs = MyGoatObserver::default_rust_owned();
    // Register a reference to the superclass &ffi::GoatObserver
    register_observer(goat_obs.as_ref().borrow().as_ref());
    feed_goat();
    feed_goat();
    feed_goat(); // prints BURP!
}
}
)
```

## Subclass ownership

See [`subclass::CppSubclass`](https://docs.rs/autocxx/latest/autocxx/subclass/trait.CppSubclass.html)
for full details, but you must decide who owns your subclass:

* C++ owns it
* Rust owns it
* It's self-owned, and only ever frees itself (using [`delete_self`](https://docs.rs/autocxx/latest/autocxx/subclass/trait.CppSubclassSelfOwned.html#method.delete_self)).

Please be careful: the observer pattern is a minefield for use-after-free bugs.
It's recommended that you wrap any such subclass in some sort of Rust newtype
wrapper which [enforces any ownership invariants](rustic.md) so that users
of your types literally can't make any mistakes.

## Calling superclass methods

Each subclass also implements a trait called `<superclass name>_supers` which
includes all superclass methods. You can call methods on that, and if you
don't implement a particular method, that will be used as the default.

```rust,ignore,autocxx,hidecpp
autocxx_integration_tests::doctest(
"",
"
#include <iostream>
class Dinosaur {
public:
    Dinosaur() {}
    virtual void eat() const {
        std::cout << \"Roarrr!! I ate you!\n\";
    }
    virtual ~Dinosaur() {}
};

// Currently, autocxx requires at least one 'generate!' call.
inline void do_a_thing() {};
",
{
use autocxx::prelude::*;
use autocxx::subclass::*;

include_cpp! {
    #include "input.h"
    safety!(unsafe_ffi)
    subclass!("Dinosaur", TRex)
    subclass!("Dinosaur", Diplodocus)
    generate!("do_a_thing")
}

use ffi::*;

#[is_subclass(superclass("Dinosaur"))]
#[derive(Default)]
pub struct TRex;

#[is_subclass(superclass("Dinosaur"))]
#[derive(Default)]
pub struct Diplodocus;

impl Dinosaur_methods for TRex {
    // TRex does NOT implement the 'eat' method
    // so C++ behavior will be used
}

impl Dinosaur_methods for Diplodocus {
    fn eat(&self) {
        println!("Ahh, some nice juicy leaves.");
        // Could call self.eat_super() if we
        // developed unexpected carnivorous cravings.
    }
}

fn main() {
    let trex = TRex::default_rust_owned();
    trex.borrow().as_ref().eat(); // eats human
    let diplo = Diplodocus::default_rust_owned();
    diplo.borrow().as_ref().eat(); // eats shoots and leaves
}
}
)
```

## Subclass casting

Subclasses implement `AsRef` to enable casting to superclasses.
