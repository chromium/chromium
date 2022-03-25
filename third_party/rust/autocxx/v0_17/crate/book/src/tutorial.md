# Tutorial

If you're here, you want to call some C++ from Rust, right?

Let's assume you're calling into some _existing_ C++ code.

You will need:

* Some C++ header files (`.h` files)
* The C++ "include path". That is, the set of directories containing those headers. (That's not necessarily the directory in which each header _file_ lives; C++ might contain `#include "foo/bar.h"` and so your include path would need to include the directory _containing_ the `foo` directory).
* A list of the APIs (types and functions) from those header files which you wish to make available in Rust.
* To know how to link the C++ libraries into your Cargo project. This is beyond the scope of what `autocxx` helps with, but one solution is to emit a print from your [build script](https://doc.rust-lang.org/cargo/reference/build-scripts.html#rustc-link-lib).
* [LLVM to be installed](https://rust-lang.github.io/rust-bindgen/requirements.html).
* Some patience. This is not a magic solution. C++/Rust interop is hard. Avoid it if you can!

The rest of this 'getting started' section assumes Cargo - if you're using something else, see the [building](building.md) section.

First, add `autocxx` *and `cxx`* to your `dependencies` and `autocxx-build` to your `build-dependencies` in your `Cargo.toml`.

```toml
[dependencies]
autocxx = "0.17.4"
cxx = "1.0"

[build-dependencies]
autocxx-build = "0.17.4"
```

Now, add a `build.rs` next to your `Cargo.toml` (this is a standard `cargo` [build script](https://doc.rust-lang.org/cargo/reference/build-scripts.html)). This is where you need your include path:

```rust,ignore
fn main() {
    let path = std::path::PathBuf::from("src"); // include path
    let mut b = autocxx_build::Builder::new("src/main.rs", &[&path]).expect_build();
        // This assumes all your C++ bindings are in main.rs
    b.flag_if_supported("-std=c++14")
     .compile("autocxx-demo"); // arbitrary library name, pick anything
    println!("cargo:rerun-if-changed=src/main.rs");
    // Add instructions to link to any C++ libraries you need.
}
```

See [the standard cargo build script output mechanisms for how you can direct Rust to link against pre-existing libraries](https://doc.rust-lang.org/cargo/reference/build-scripts.html#outputs-of-the-build-script), and see the [building section of this book for more details about build options - for example, enabling C++17](building.md).

Finally, in your `main.rs` you can use the [`include_cpp`](https://docs.rs/autocxx/latest/autocxx/macro.include_cpp.html) macro which is the heart of `autocxx`:

```rust,ignore
use autocxx::prelude::*; // use all the main autocxx functions

include_cpp! {
    #include "my_header.h" // your header file name
    safety!(unsafe) // see details of unsafety policies described in the 'safety' section of the book
    generate!("DeepThought") // add this line for each function or type you wish to generate
}
```

You should then find you can call the function by referring to an `ffi` namespace:

```rust,ignore
fn main() {
    println!("The answer to Life, The Universe and Everything is {}", ffi::DeepThought());
}
```

C++ types such as `std::string` and `std::unique_ptr` are represented using the types provided by the marvellous [cxx](https://cxx.rs) library. This provides good ergonomics and safety norms, so unlike with normal `bindgen` bindings, you won't _normally_ need to write `unsafe` code for every function call.

Next, read the section about [workflows](workflow.md).
