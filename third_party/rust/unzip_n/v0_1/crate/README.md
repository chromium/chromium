# unzip-n

[![travis](https://api.travis-ci.org/mexus/unzip-n.svg?branch=master)](https://travis-ci.org/mexus/unzip-n)
[![crates.io](https://img.shields.io/crates/v/unzip-n.svg)](https://crates.io/crates/unzip-n)
[![docs.rs](https://docs.rs/unzip-n/badge.svg)](https://docs.rs/unzip-n)

Procedural macro for unzipping iterators-over-`n`-length-tuples into `n` collections.

Here's a brief example of what it is capable of:

```rust
use unzip_n::unzip_n;

unzip_n!(pub 3);
// // Or simply leave the visibility modifier absent for inherited visibility
// // (which usually means "private").
// unzip_n!(3);

fn main() {
    let v = vec![(1, 2, 3), (4, 5, 6)];
    let (v1, v2, v3) = v.into_iter().unzip_n_vec();

    assert_eq!(v1, &[1, 4]);
    assert_eq!(v2, &[2, 5]);
    assert_eq!(v3, &[3, 6]);
}
```

## License

Licensed under either of

* Apache License, Version 2.0 (LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0)
* MIT license (LICENSE-MIT or http://opensource.org/licenses/MIT)

at your option.

## Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any
additional terms or conditions.

License: MIT/Apache-2.0
