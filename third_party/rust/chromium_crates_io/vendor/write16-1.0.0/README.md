# write16

[![crates.io](https://img.shields.io/crates/v/write16.svg)](https://crates.io/crates/write16)
[![docs.rs](https://docs.rs/write16/badge.svg)](https://docs.rs/write16/)

`write16` provides the trait `Write16`, which a UTF-16 analog of the
`core::fmt::Write` trait (the sink part—not the formatting part).

This is a `no_std` crate.

## Licensing

TL;DR: `Apache-2.0 OR MIT`

Please see the file named
[COPYRIGHT](https://github.com/hsivonen/write16/blob/master/COPYRIGHT).

## Documentation

Generated [API documentation](https://docs.rs/write16/) is available
online.

## Features

`alloc`: An implementation of `Write16` for `alloc::vec::Vec`.
`smallvec`: An implementation of `Write16` for `smallvec::SmallVec`
`arrayvec`: An implementation of `Write16` for `arrayvec::ArrayVec`

## Release Notes

### 1.0.0

The initial release.
