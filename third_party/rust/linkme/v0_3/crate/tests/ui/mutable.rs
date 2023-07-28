#![cfg_attr(feature = "used_linker", feature(used_with_arg))]

use linkme::distributed_slice;

#[distributed_slice]
pub static mut SLICE: [i32] = [..];

#[distributed_slice(BENCHMARKS)]
static mut ELEMENT: i32 = -1;

fn main() {}
