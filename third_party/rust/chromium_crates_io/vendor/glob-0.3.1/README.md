glob
====

Support for matching file paths against Unix shell style patterns.

[![Continuous integration](https://github.com/rust-lang/glob/actions/workflows/rust.yml/badge.svg)](https://github.com/rust-lang/glob/actions/workflows/rust.yml)

[Documentation](https://docs.rs/glob)

## Usage

To use `glob`, add this to your `Cargo.toml`:

```toml
[dependencies]
glob = "0.3.1"
```

And add this to your crate root:

```rust
extern crate glob;
```

## Examples

Print all jpg files in /media/ and all of its subdirectories.

```rust
use glob::glob;

for entry in glob("/media/**/*.jpg").expect("Failed to read glob pattern") {
    match entry {
        Ok(path) => println!("{:?}", path.display()),
        Err(e) => println!("{:?}", e),
    }
}
```
