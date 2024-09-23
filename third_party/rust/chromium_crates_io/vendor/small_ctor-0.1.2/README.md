# small-ctor

[![Build Status](https://github.com/mitsuhiko/small-ctor/workflows/Tests/badge.svg?branch=main)](https://github.com/mitsuhiko/small-ctor/actions?query=workflow%3ATests)
[![rustc 1.46.0](https://img.shields.io/badge/rust-1.46%2B-orange.svg)](https://img.shields.io/badge/rust-1.46%2B-orange.svg)
[![Crates.io](https://img.shields.io/crates/d/small-ctor.svg)](https://crates.io/crates/small-ctor)
[![License](https://img.shields.io/github/license/mitsuhiko/small-ctor)](https://github.com/mitsuhiko/small-ctor/blob/main/LICENSE)
[![Documentation](https://docs.rs/small_ctor/badge.svg)](https://docs.rs/small_ctor)

Minimal, dependency free implementation of the [`ctor`](https://crates.io/crates/ctor) crate.

Supports Rust 1.46 and later on Linux, Windows and macOS.  Other platforms best effort.

```rust
struct MyPlugin;

#[small_ctor::ctor]
unsafe fn register_plugin() {
    PLUGINS.register(MyPlugin);
}
```

## License and Links

- [Documentation](https://docs.rs/small_ctor/)
- [Issue Tracker](https://github.com/mitsuhiko/small-ctor/issues)
- [Examples](https://github.com/mitsuhiko/small-ctor/tree/main/examples)
- License: [Apache-2.0](https://github.com/mitsuhiko/small-ctor/blob/main/LICENSE)
