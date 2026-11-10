# `object`

The `object` crate provides a unified interface to working with object files
across platforms. It supports reading relocatable object files and executable
files, and writing COFF/ELF/Mach-O/XCOFF relocatable object files and ELF/PE
executable files.

For reading files, it provides multiple levels of support:

* raw struct definitions suitable for zero copy access
* low level APIs for accessing the raw structs
  ([example](crates/examples/src/readobj))
* a higher level unified API for accessing common features of object files, such
  as sections and symbols ([example](crates/examples/src/objdump.rs))

Supported file formats for reading: ELF, Mach-O, Windows PE/COFF, Wasm, XCOFF,
and Unix archive.

For writing files, it provides:

* low level writers for ELF, PE, and COFF
* higher level builder for ELF ([example](crates/rewrite/src))
* a unified API for writing relocatable object files (ELF, Mach-O, COFF, XCOFF)
  ([example](crates/examples/src/bin/simple_write.rs))

## Example for unified read API

```rust
use object::{Object, ObjectSection};
use std::error::Error;
use std::fs;

/// Reads a file and displays the name of each section.
fn main() -> Result<(), Box<dyn Error>> {
    let binary_data = fs::read("path/to/binary")?;
    let file = object::File::parse(&*binary_data)?;
    for section in file.sections() {
        println!("{}", section.name()?);
    }
    Ok(())
}
```

See [`crates/examples`](crates/examples) for more examples.

## Security

This crate is intended to be used with trusted inputs. For example, it is
suitable for use as part of a compiler toolchain, but not for malware analysis
or a service exposed to arbitrary inputs.

The crate aims to be reliable: it provides memory safety (with some use of
`unsafe` for performance), and malformed input is expected to result in an
error rather than a panic or incorrect parsing.

However, there are significant limitations:

* There is little effort to mitigate denial of service attacks (such as
  algorithmic complexity attacks due to overlapping structures).

* The crate does not aim to replicate the behaviour of platform loaders such as
  the Windows loader or dynamic linkers. Data returned by the parser may not
  match the data used by a loader.

Non-intrusive fixes (low complexity or overhead) for these are welcome, but it
is not a focus of development.

## Minimum supported Rust version (MSRV)

Changes to MSRV are not considered breaking changes. We are conservative about
changing the MSRV, but sometimes are required to due to dependencies. The MSRV
with all features enabled is 1.87.0. The MSRV with some features disabled is
1.85.0.

## License

Licensed under either of

  * Apache License, Version 2.0 ([`LICENSE-APACHE`](./LICENSE-APACHE) or
    https://www.apache.org/licenses/LICENSE-2.0)
  * MIT license ([`LICENSE-MIT`](./LICENSE-MIT) or
    https://opensource.org/license/mit)

at your option.

## Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
