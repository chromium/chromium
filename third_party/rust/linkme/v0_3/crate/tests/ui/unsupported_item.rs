#![cfg_attr(feature = "used_linker", feature(used_with_arg))]

use linkme::distributed_slice;

#[distributed_slice]
pub static SLICE: [&'static str] = [..];

#[distributed_slice(SLICE)]
extern crate std as _std;

fn main() {}
