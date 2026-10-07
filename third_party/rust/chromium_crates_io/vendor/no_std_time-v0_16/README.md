# `No_Std_Time`: `no_std`-friendly timing and timestamping

<img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

The `no_std_time` crate provides `no_std`-friendly timing and high-performance timestamping utilities. It is a core component of the [LibAFL](https://github.com/AFLplusplus/LibAFL) fuzzing framework.

This crate offers two main pieces of functionality:
1. A way to get the current system time as a `Duration`, even in `no_std` environments.
2. A high-performance time counter using CPU-specific instructions (`rdtsc`, etc.) for fast measurements.

## Usage

### Getting the Current Time

You can get the current time using the `current_time()` function.

With the `std` feature enabled (the default), this works out of the box:
```rust
use no_std_time::current_time;

let time = current_time();
println!("Current time: {:?}", time);
```

In a `no_std` environment, you must provide an implementation for `external_current_millis` that returns the milliseconds since the UNIX epoch.

```rust
// In your no_std binary:
#[unsafe(no_mangle)]
pub extern "C" fn external_current_millis() -> u64 {
    // Return time from your platform-specific source, e.g., an RTC.
    1678886400000
}
```

### High-Performance Timestamping

For high-performance measurements, `read_time_counter()` provides access to fast, low-level CPU cycle counters. This is useful for profiling and performance-critical code.

```rust
use no_std_time::read_time_counter;

let start = read_time_counter();
// ... do some work ...
let end = read_time_counter();
let elapsed = end - start;
println!("Work took {} cycles", elapsed);
```

This function is optimized for the following architectures:
- `x86` and `x86_64` (using `rdtsc`)
- `aarch64` (using `cntvct_el0`)
- `arm` (using the performance monitor unit)
- `riscv32` and `riscv64` (using `rdcycle`)

On other architectures, it falls back to a system-time-based implementation.

### Formatting Durations

The crate includes a utility to format a `Duration` into a human-readable string. This requires the `alloc` feature.

```rust
use core::time::Duration;
use no_std_time::format_duration;

let duration = Duration::from_secs(3661);
let formatted = format_duration(&duration);
assert_eq!(formatted, "1h-1m-1s");
```

## Features

- `std` (default): Enables functionality that depends on the standard library, such as getting the system time automatically.
- `alloc` (default): Enables features that require heap allocation, such as `format_duration`.

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