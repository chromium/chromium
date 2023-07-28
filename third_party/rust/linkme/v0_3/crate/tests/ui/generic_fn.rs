#![cfg_attr(feature = "used_linker", feature(used_with_arg))]

use linkme::distributed_slice;

#[distributed_slice]
pub static SLICES: [fn()] = [..];

#[distributed_slice(SLICES)]
fn type_param<T>() {}

#[distributed_slice(SLICES)]
fn const_param<const N: usize>() {}

fn main() {}
