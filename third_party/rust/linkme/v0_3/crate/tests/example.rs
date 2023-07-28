#![cfg_attr(feature = "used_linker", feature(used_with_arg))]
#![deny(warnings)]
#![allow(clippy::no_effect_underscore_binding)]

use linkme::distributed_slice;

pub struct Bencher;

#[distributed_slice]
pub static BENCHMARKS: [fn(&mut Bencher)] = [..];

#[distributed_slice(BENCHMARKS)]
static BENCH_DESERIALIZE: fn(&mut Bencher) = bench_deserialize;

fn bench_deserialize(_b: &mut Bencher) {
    /* ... */
}

#[test]
fn readme() {
    // Iterate the elements.
    for _bench in BENCHMARKS { /* ... */ }

    // Index into the elements.
    let _first = BENCHMARKS[0];

    // Slice the elements.
    let _except_first = &BENCHMARKS[1..];

    // Invoke methods on the underlying slice.
    let _len = BENCHMARKS.len();
}
