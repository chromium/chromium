# `LibAFL_bolts`: Handy Libary Collection for Everybody

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

The `libafl_bolts` crate is a toolshed combinding a lot of low-level features and crates `LibAFL` uses. It can be a good starting point for low-level projects, even those that are not specifically fuzzers.
Some cross-platform things in bolts include (but are not limited to):

* `SerdeAnyMap`: a map that stores and retrieves elements by type and is serializable and deserializable
* `ShMem`: A cross-platform (`Windows`, `Linux`, `Android`, `macOS`) shared memory implementation
* `LL_MP`: A fast, lock-free IPC mechanism via shared maps
* `Core_affinity`: A maintained version of `core_affinity` that can be used to get core information and bind processes to cores
* `Rands`: Fast random number generators for fuzzing (like [RomuRand](https://www.romu-random.org/))
* `MiniBSOD`: get and print information about the current process state including important registers.
* `Tuples`: Haskel-like compile-time tuple lists
* `Os`: OS specific stuff like signal handling, windows exception handling, pipes, and helpers for `fork`

`LibAFL_bolts` is written and maintained by

* [Andrea Fioraldi](https://twitter.com/andreafioraldi) <andrea@aflplus.plus>
* [Dominik Maier](https://bsky.app/profile/dmnk.bsky.social) <dominik@aflplus.plus>
* [s1341](https://twitter.com/srubenst1341) <github@shmarya.net>
* [Dongjia Zhang](https://github.com/tokatoka) <toka@aflplus.plus>
* [Addison Crump](https://github.com/addisoncrump) <me@addisoncrump.info>

## Contributing

For bugs, feel free to open issues or contact us directly. Thank you for your support. <3

Even though we will gladly assist you in finishing up your PR, try to

* keep all the crates compiling with *stable* rust (hide the eventual non-stable code under `cfg`s.)
* run `cargo nightly fmt` on your code before pushing
* check the output of `cargo clippy --all` or `./clippy.sh`
* run `cargo build --no-default-features` to check for `no_std` compatibility (and possibly add `#[cfg(feature = "std")]`) to hide parts of your code.

Some parts in this list may sound hard, but don't be afraid to open a PR if you cannot fix them by yourself. We will gladly assist.

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
