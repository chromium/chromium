# How to Contribute

We'd love to accept your patches and contributions to this project. There are
just a few small guidelines you need to follow.

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution;
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Code reviews

All submissions, including submissions by project members, require review. We
use GitHub pull requests for this purpose. Consult
[GitHub Help](https://help.github.com/articles/about-pull-requests/) for more
information on using pull requests.

## Community Guidelines

This project follows [Google's Open Source Community
Guidelines](https://opensource.google/conduct/).

## Directory structure

* `book` - you're reading it!
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

## Where to start reading

The main algorithm is in `engine/src/lib.rs`, in the function `generate()`. This asks
`bindgen` to generate a heap of Rust code and then passes it into
`engine/src/conversion` to convert it to be a format suitable for input
to `cxx`.

However, most of the actual code is in `engine/src/conversion/mod.rs`.

At the moment we're using a slightly branched version of `bindgen` called `autocxx-bindgen`.
It's hoped this is temporary; some of our changes are sufficiently weird that it would be
presumptious to try to get them accepted upstream until we're sure `autocxx` has roughly the right approach.

## How to develop

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

## Reporting bugs

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

## How to contribute to this manual

More examples in this manual are _very_ welcome!

Because `autocxx` examples require both Rust and C++ code to be linked together,
a custom preprocessor is used for this manual. See one of the existing examples
such as in `index.md` to see how to do this.