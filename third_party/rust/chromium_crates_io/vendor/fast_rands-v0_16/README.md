# `Fast_Rands`: Get (non-cryptographically secure!) Random Numbers Faster.

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

The `faster_rands` crate is a collection of a range of fast RNGs, such as [RomuRand](https://www.romu-random.org/) and others.
It can be used together with [`LibAFL`](https://github.com/AFLplusplus/LibAFL) or any place that needs random numbers, _as long as you don't rely on the randomness for security_.

## Usage

```rust
use fast_rands::{Rand, StdRand};
use core::num::NonZeroUsize;

// Create a new random number generator with a random seed
let mut rand = StdRand::new();

// Get a random u64
let n = rand.next();

// Get a random number below 100
let below_100 = rand.below(NonZeroUsize::new(100).unwrap());

// Get a random number between 50 and 100 (inclusive)
let between_50_and_100 = rand.between(50, 100);
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