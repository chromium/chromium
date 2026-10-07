# `build_id2`: A maintained way to uniquely represent the build of the current binary

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

The `build_id2` crate is a maintained and updated fork of `build_id`.
With it, you can obtain a `Uuid` uniquely representing the build of the current binary.

This is intended to be used to check that different processes are indeed invocations of identically laid out binaries.

As such:

* It is guaranteed to be identical within multiple invocations of the same binary.
* It is guaranteed to be different across binaries with different code or data segments or layout.
* Equality is unspecified if the binaries have identical code and data segments and layout but differ immaterially (e.g. if a timestamp is included in the binary at compile time).

## Usage

Add the following to your `Cargo.toml`:

```toml
[dependencies]
build_id2 = "0.16.1"
```

Then, you can use the `get` function to get the build id:

```rust
# let remote_build_id = build_id2::get();
let local_build_id = build_id2::get();
if local_build_id == remote_build_id {
    println!("We're running the same binary as remote!");
} else {
    println!("We're running a different binary to remote");
}
```

## The `LibAFL` Project

The `LibAFL` project is part of [`AFLplusplus`](https://github.com/AFLplusplus) and maintained by

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
