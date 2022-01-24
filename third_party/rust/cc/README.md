The `cc` crate is used by various `build.rs` build scripts to compile C/C++
dependencies of a Rust crate.

In Chromium, these should be compiled as a GN source_set target, and then linked
in with the Rust crate explicitly.

Thus no `cc` crate should ever be imported to third_party. Usage of `cc` from
`build.rs` files should be commented out, and the dependency should be removed
from `Cargo.toml` files.
