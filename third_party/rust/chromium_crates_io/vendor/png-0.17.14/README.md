# PNG Decoder/Encoder
[![Build Status](https://github.com/image-rs/image-png/workflows/Rust%20CI/badge.svg)](https://github.com/image-rs/image-png/actions)
[![Documentation](https://docs.rs/png/badge.svg)](https://docs.rs/png)
[![Crates.io](https://img.shields.io/crates/v/png.svg)](https://crates.io/crates/png)
[![License](https://img.shields.io/crates/l/png.svg)](https://github.com/image-rs/image-png)

PNG decoder/encoder in pure Rust.

It contains all features required to handle the entirety of [the PngSuite by
Willem van Schack][PngSuite].

[PngSuite]: http://www.schaik.com/pngsuite2011/pngsuite.html

## pngcheck

The `pngcheck` utility is a small demonstration binary that checks and prints
metadata on every `.png` image provided via parameter. You can run it (for
example on the test directories) with

```bash
cargo run --release --example pngcheck ./tests/pngsuite/*
```

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or https://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or https://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any
additional terms or conditions.
