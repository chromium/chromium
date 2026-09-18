# memo-map

[![Crates.io](https://img.shields.io/crates/d/memo-map.svg)](https://crates.io/crates/memo-map)
[![License](https://img.shields.io/github/license/mitsuhiko/memo-map)](https://github.com/mitsuhiko/memo-map/blob/main/LICENSE)
[![rustc 1.41.0](https://img.shields.io/badge/rust-1.43%2B-orange.svg)](https://img.shields.io/badge/rust-1.43%2B-orange.svg)
[![Documentation](https://docs.rs/memo-map/badge.svg)](https://docs.rs/memo-map)

A concurrent insert only hash map.

This crate implements a “memo map” which is in many ways similar to a HashMap with some crucial differences:

* Unlike a regular hash map, a memo map is thread safe and synchronized.
* Adding or retrieving keys works through a shared reference, removing only
  through a mutable reference.
* Retrieving a value from a memo map returns a plain old reference.

```rust
use memo_map::MemoMap;

let memo = MemoMap::new();
let one = memo.get_or_insert(&1, || "one".to_string());
let one2 = memo.get_or_insert(&1, || "not one".to_string());
assert_eq!(one, "one");
assert_eq!(one2, "one");
```

## License and Links

- [Documentation](https://docs.rs/memo-map/)
- [Issue Tracker](https://github.com/mitsuhiko/memo-map/issues)
- License: [Apache-2.0](https://github.com/mitsuhiko/memo-map/blob/main/LICENSE)

