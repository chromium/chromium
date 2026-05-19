# LibAFL_bolts: OS and Fuzzer Dev's Libary Collection.

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

The `libafl_bolts` crate exposes a lot of low-level features of LibAFL for projects that are unrelated to fuzzing, or just fuzzers completely different to LibAFL.
Some cross-platform things in bolts include (but are not limited to):

* SerdeAnyMap: a map that stores and retrieves elements by type and is serializable and deserializable
* ShMem: A cross-platform (Windows, Linux, Android, MacOS) shared memory implementation
* LLMP: A fast, lock-free IPC mechanism via SharedMap
* Core_affinity: A maintained version of `core_affinity` that can be used to get core information and bind processes to cores
* Rands: Fast random number generators for fuzzing (like [RomuRand](https://www.romu-random.org/))
* MiniBSOD: get and print information about the current process state including important registers.
* Tuples: Haskel-like compile-time tuple lists
* Os: OS specific stuff like signal handling, windows exception handling, pipes, and helpers for `fork`

LibAFL_bolts is written and maintained by

* [Andrea Fioraldi](https://twitter.com/andreafioraldi) <andrea@aflplus.plus>
* [Dominik Maier](https://twitter.com/domenuk) <dominik@aflplus.plus>
* [s1341](https://twitter.com/srubenst1341) <github@shmarya.net>
* [Dongjia Zhang](https://github.com/tokatoka) <toka@aflplus.plus>
* [Addison Crump](https://github.com/addisoncrump) <me@addisoncrump.info>

## Contributing

For bugs, feel free to open issues or contact us directly. Thank you for your support. <3

Even though we will gladly assist you in finishing up your PR, try to
- keep all the crates compiling with *stable* rust (hide the eventual non-stable code under [`cfg`s](https://github.com/AFLplusplus/LibAFL/blob/main/crates/libafl/build.rs#L26))
- run `cargo nightly fmt` on your code before pushing
- check the output of `cargo clippy --all` or `./clippy.sh`
- run `cargo build --no-default-features` to check for `no_std` compatibility (and possibly add `#[cfg(feature = "std")]`) to hide parts of your code.

Some of the parts in this list may be hard, don't be afraid to open a PR if you cannot fix them by yourself, so we can help.

#### License

<sup>
Licensed under either of <a href="../LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="../LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>

<br>

<sub>
Dependencies under more restrictive licenses, such as GPL or AGPL, can be enabled
using the respective feature in each crate when it is present, such as the
'agpl' feature of the libafl crate.
</sub>
