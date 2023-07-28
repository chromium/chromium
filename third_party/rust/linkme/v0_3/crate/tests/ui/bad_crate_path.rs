#![cfg_attr(feature = "used_linker", feature(used_with_arg))]

use linkme::distributed_slice;

mod path {
    pub mod to {}
}

#[distributed_slice]
#[linkme(crate = path::to::missing)]
pub static SLICE1: [&'static str] = [..];

#[distributed_slice]
pub static SLICE2: [&'static str] = [..];

#[distributed_slice(SLICE2)]
#[linkme(crate = path::to::missing)]
static ELEMENT: &str = "";

fn main() {}
