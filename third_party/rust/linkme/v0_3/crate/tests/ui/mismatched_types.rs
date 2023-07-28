#![cfg_attr(feature = "used_linker", feature(used_with_arg))]

use linkme::distributed_slice;

pub struct Bencher;

#[distributed_slice]
pub static BENCHMARKS: [fn(&mut Bencher)] = [..];

#[distributed_slice(BENCHMARKS)]
static BENCH_WTF: usize = 999;

#[distributed_slice(BENCHMARKS)]
fn wrong_bench_fn<'a>(_: &'a mut ()) {}

fn main() {}
