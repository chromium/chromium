#![cfg_attr(feature = "used_linker", feature(used_with_arg))]

use linkme::distributed_slice;

pub struct Unit;

#[distributed_slice]
pub static ZEROSIZED: [Unit] = [..];

fn main() {}
