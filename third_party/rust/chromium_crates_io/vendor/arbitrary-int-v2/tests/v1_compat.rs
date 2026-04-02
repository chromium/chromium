#![allow(deprecated)]

// These are some typical patterns in users of arbitrary_int v1 we want to make sure don't break

#[test]
fn use_wildcard() {
    use arbitrary_int::*;

    // Use a constant from `Number` (which conflicts with `Integer`)
    const _: u5 = u5::MAX;
}

#[test]
fn use_number_directly() {
    use arbitrary_int::{u5, Number};

    // Use a constant from `Number` (which conflicts with `Integer`)
    const _: u5 = u5::MAX;
}
