# Autocxx

[![GitHub](https://img.shields.io/crates/l/autocxx)](https://github.com/google/autocxx)
[![crates.io](https://img.shields.io/crates/d/autocxx)](https://crates.io/crates/autocxx)
[![docs.rs](https://docs.rs/autocxx/badge.svg)](https://docs.rs/autocxx)

This project is a tool for calling C++ from Rust in a heavily automated, but safe, fashion.

The intention is that it has all the fluent safety from [cxx](https://cxx.rs) whilst generating interfaces automatically from existing C++ headers using a variant of [bindgen](https://docs.rs/bindgen/latest/bindgen/). Think of autocxx as glue which plugs bindgen into cxx.

# Overview

```rust,ignore
autocxx::include_cpp! {
    #include "url/origin.h"
    generate!("url::Origin")
    safety!(unsafe_ffi)
}

fn main() {
    let o = ffi::url::Origin::CreateFromNormalizedTuple("https",
        "google.com", 443);
    let uri = o.Serialize();
    println!("URI is {}", uri.to_str().unwrap());
}
```

# Getting started

If you're here, you want to call some C++ from Rust, right?

You will need:

* Some C++ header files (`.h` files)
* The C++ "include path". That is, the set of directories containing those headers. (That's not necessarily the directory in which each header _file_ lives; C++ might contain `#include "foo/bar.h"` and so your include path would need to include the directory containing the `foo` directory).
* A list of the APIs (types and functions) from those header files which you wish to make available in Rust.
* Either a Cargo or non-Cargo build system.
* To know how to link the C++ libraries into your Cargo project. This is beyond the scope of what `autocxx` helps with, but one solution is to emit a print from your [build script](https://doc.rust-lang.org/cargo/reference/build-scripts.html#rustc-link-lib).
* [LLVM to be installed](https://rust-lang.github.io/rust-bindgen/requirements.html).
* Some patience. This is not a magic solution. C++/Rust interop is hard. Avoid it if you can!

The rest of this 'getting started' section assumes Cargo - if you're using something else, see the [`include_cpp`](https://docs.rs/autocxx/latest/autocxx/macro.include_cpp.html) documentation.

First, add `autocxx` and `cxx` to your `dependencies` and `autocxx-build` to your `build-dependencies` in your `Cargo.toml`.

```toml
[dependencies]
autocxx = "0.16.0"
cxx = "1.0"

[build-dependencies]
autocxx-build = "0.16.0"
```

Now, add a `build.rs`. This is where you need your include path:

```rust,ignore
fn main() {
    let path = std::path::PathBuf::from("src"); // include path
    let mut b = autocxx_build::Builder::new("src/main.rs", &[&path]).expect_build();
        // This assumes all your C++ bindings are in main.rs
    b.flag_if_supported("-std=c++14").compile("autocxx-demo");
    println!("cargo:rerun-if-changed=src/main.rs");
    // Add instructions to link to any C++ libraries you need.
}
```

Finally, in your `main.rs` you can use the [`include_cpp`](https://docs.rs/autocxx/latest/autocxx/macro.include_cpp.html) macro which is the heart of `autocxx`:

```rust,ignore
use autocxx::prelude::*;

include_cpp! {
    #include "my_header.h" // your header file name
    safety!(unsafe) // see details of unsafety policies described in include_cpp
    generate!("MyAPIFunction") // add this line for each function or type you wish to generate
}
```

You should then find you can call the function by referring to an `ffi` namespace:

```rust,ignore
fn main() {
    println!("Hello, world! - answer from C++ is {}", ffi::MyAPIFunction(4));
}
```

C++ types such as `std::string` and `std::unique_ptr` are represented using the types provided by the marvellous [cxx](https://cxx.rs) library. This provides good ergonomics and safety norms, so unlike with normal `bindgen` bindings, you won't _normally_ need to write `unsafe` code for every function call.

Some caveats:

* Not all C++ features are supported. You _will_ come across APIs - possibly many APIs - where autocxx doesn't work. It should emit reasonable diagnostics explaining the problem. See the section on "dealing with failure" in the [`include_cpp`](https://docs.rs/autocxx/latest/autocxx/macro.include_cpp.html) documentation.
* `autocxx` can be frustrating when you run up against its limitations. It's designed to allow importing of APIs from complex existing codebases. It's often a better choice to use [cxx](https://cxx.rs) directly.

A full user manual can be found in the documentation for [`include_cpp`](https://docs.rs/autocxx/latest/autocxx/macro.include_cpp.html). See [demo/src/main.rs](demo/src/main.rs) for a basic example, and the [examples](examples/) directory for more.

# On safety

This crate mostly intends to follow the lead of the `cxx` crate in where and when `unsafe` is required. But, this crate is opinionated. It believes some unsafety requires more careful review than other bits, along the following spectrum:

* Rust unsafe code (requires most review)
* Rust code calling C++ with raw pointers
* Rust code calling C++ with shared pointers, or anything else where there can be concurrent mutation
* Rust code calling C++ with unique pointers, where the Rust single-owner model nearly always applies (but we can't _prove_ that the C++ developer isn't doing something weird)
* Rust safe code (requires least review)

If your project is 90% Rust code, with small bits of C++, don't use this crate. You need something where all C++ interaction is marked with big red "this is terrifying" flags. This crate is aimed at cases where there's 90% C++ and small bits of Rust, and so we want the Rust code to be pragmatically reviewable without the signal:noise ratio of `unsafe` in the Rust code becoming so bad that `unsafe` loses all value.

See [safety!] in the documentation for more details.

# Building without cargo

See instructions in the documentation for [`include_cpp`](https://docs.rs/autocxx/latest/autocxx/macro.include_cpp.html). This interop inevitably involves lots of fiddly small functions. It's likely to perform far better if you can achieve cross-language LTO. [This issue](https://github.com/dtolnay/cxx/issues/371) may give some useful hints - see also all the build-related help in [the cxx manual](https://cxx.rs/) which all applies here too.

# Directory structure

* `demo` - a very simple demo example
* `examples` - will gradually fill with more complex examples
* `parser` - code which parses a single `include_cpp!` macro. Used by both the macro
  (which doesn't do much) and the code generator (which does much more, by means of
  `engine` below)
* `engine` - all the core code for actual code generation.
* `macro` - the procedural macro which expands the Rust code.
* `gen/build` - a library to be used from `build.rs` scripts to generate .cc and .h
  files from an `include_cxx` section.
* `gen/cmd` - a command-line tool which does the same.
* `src` (outermost project) - a wrapper crate which imports the procedural macro and
  a few other things.

# Where to start reading

The main algorithm is in `engine/src/lib.rs`, in the function `generate()`. This asks
`bindgen` to generate a heap of Rust code and then passes it into
`engine/src/conversion` to convert it to be a format suitable for input
to `cxx`.

However, most of the actual code is in `engine/src/conversion/mod.rs`.

At the moment we're using a slightly branched version of `bindgen` called `autocxx-bindgen`.
It's hoped this is temporary; some of our changes are sufficiently weird that it would be
presumptious to try to get them accepted upstream until we're sure `autocxx` has roughly the right approach.

# How to develop

If you're making a change, here's what you need to do to get useful diagnostics etc.
First of all, `cargo run` in the `demo` directory. If it breaks, you don't get much
in the way of useful diagnostics, because `stdout` is swallowed by cargo build scripts.
So, practically speaking, you would almost always move onto running one of the tests
in the test suite. With suitable options, you can get plenty of output. For instance:

```ignore
RUST_BACKTRACE=1 RUST_LOG=autocxx_engine=info cargo test --all test_cycle_string_full_pipeline -- --nocapture
```

This is especially valuable to see the `bindgen` output Rust code, and then the converted Rust code which we pass into cxx. Usually, most problems are due to some mis-conversion somewhere
in `engine/src/conversion`. See [here](https://docs.rs/autocxx-engine/latest/autocxx_engine/struct.IncludeCppEngine.html) for documentation and diagrams on how the engine works.

# Reporting bugs

If you've found a problem, and you're reading this, *thank you*! Your diligence
in reporting the bug is much appreciated and will make `autocxx` better. In
order of preference here's how we would like to hear about your problem:

* Raise a pull request adding a new failing integration test to
  `engine/src/integration_tests.rs`.
* Minimize the test using `tools/reduce`, something like this:
  `target/debug/autocxx-reduce file -d "safety!(unsafe_ffi)" -d
  'generate_pod!("A")' -I ~/my-include-dir -h my-header.h -p
  problem-error-message -- --remove-pass pass_line_markers`
  This is a wrapper for the amazing `creduce` which will take thousands of lines
  of C++, preprocess it, and then identify the minimum required lines to
  reproduce the same problem.
* Use the C++ preprocessor to give a single complete C++ file which demonstrates
  the problem, along with the `include_cpp!` directive you use.
  Alternatively, run your build using `AUTOCXX_REPRO_CASE=repro.json` which should
  put everything we need into `output.h`. If necessary, you can use the `CLANG_PATH`
  or `CXX` environment variables to specify the path to the Clang compiler to use.
* Failing all else, build using
  `cargo clean -p <your package name> && RUST_LOG=autocxx_engine=info cargo build -vvv`
  and send the _entire_ log to us. This will include two key bits of logging:
  the C++ bindings as distilled by `bindgen`, and then the version which
  we've converted and moulded to be suitable for use by `cxx`.

# Credits

David Tolnay did much of the hard work here, by inventing the underlying cxx crate, and in fact nearly all of the parsing infrastructure on which this crate depends. `bindgen` is also awesome. This crate stands on the shoulders of giants!

#### License and usage notes

This is not an officially supported Google product.

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>
