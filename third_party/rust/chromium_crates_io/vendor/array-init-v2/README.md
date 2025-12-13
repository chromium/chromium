# array-init

[![Crates.io](https://img.shields.io/crates/v/array-init?style=flat-square)](https://crates.io/crates/array-init)
[![Docs](https://img.shields.io/badge/docs-doc.rs-blue?style=flat-square)](https://docs.rs/array-init)

The `array-init` crate allows you to initialize arrays
with an initializer closure that will be called
once for each element until the array is filled.

This way you do not need to default-fill an array
before running initializers. Rust currently only
lets you either specify all initializers at once,
individually (`[a(), b(), c(), ...]`), or specify
one initializer for a `Copy` type (`[a(); N]`),
which will be called once with the result copied over.

Care is taken not to leak memory shall the initialization
fail.

## Examples:

```rust
// Initialize an array of length 50 containing
// successive squares

let arr: [usize; 50] = array_init::array_init(|i| i * i);

// Initialize an array from an iterator
// producing an array of [1,2,3,4] repeated

let four = [1,2,3,4];
let mut iter = four.iter().copied().cycle();
let arr: [u32; 50] = array_init::from_iter(iter).unwrap();

// Closures can also mutate state. We guarantee that they will be called
// in order from lower to higher indices.

let mut last = 1u64;
let mut secondlast = 0;
let fibonacci: [u64; 50] = array_init::array_init(|_| {
    let this = last + secondlast;
    secondlast = last;
    last = this;
    this
});
```

## Minimum Supported Rust Version (MSRV)

`array-init` will only increase the MSRV on a new major
or minor release, but not for patch releases.
Any changes of the MSRV will be announced in the changelog.
When increasing the MSRV, the new Rust version must have been
released at least six months ago. The current MSRV is 1.51.0.
MSRV changes can be expected to happen conservatively.

## Licensing

Licensed under either of Apache License, Version 2.0 or MIT license at your option.

Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in this crate by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.

