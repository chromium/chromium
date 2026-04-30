# transpose

[![crate](https://img.shields.io/crates/v/transpose.svg)](https://crates.io/crates/transpose)
[![license](https://img.shields.io/crates/l/transpose.svg)](https://crates.io/crates/transpose)
[![documentation](https://docs.rs/transpose/badge.svg)](https://docs.rs/transpose/)
![minimum rustc 1.26](https://img.shields.io/badge/rustc-1.26+-red.svg)

Utility for transposing multi-dimensional data See the [API Documentation](https://docs.rs/transpose/) for more details.

`transpose` is `#![no_std]`

## Example
```rust
// Create a 2D array in row-major order: the rows of our 2D array are contiguous,
// and the columns are strided
let input_array = vec![ 1, 2, 3,
                        4, 5, 6];

// Treat our 6-element array as a 2D 3x2 array, and transpose it to a 2x3 array
let mut output_array = vec![0; 6];
transpose::transpose(&input_array, &mut output_array, 3, 2);

// The rows have become the columns, and the columns have become the rows
let expected_array =  vec![ 1, 4,
                            2, 5,
                            3, 6];
assert_eq!(output_array, expected_array);
```

## Compatibility

The `transpose` crate requires rustc 1.26 or greater.

## License

Licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the Apache-2.0
license, shall be dual licensed as above, without any additional terms or
conditions.
