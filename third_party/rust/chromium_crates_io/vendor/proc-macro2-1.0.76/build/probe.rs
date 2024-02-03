// This code exercises the surface area that we expect of Span's unstable API.
// If the current toolchain is able to compile it, then proc-macro2 is able to
// offer these APIs too.

#![feature(proc_macro_span)]

extern crate proc_macro;

use core::ops::RangeBounds;
use proc_macro::{Literal, Span};

pub fn join(this: &Span, other: Span) -> Option<Span> {
    this.join(other)
}

pub fn subspan<R: RangeBounds<usize>>(this: &Literal, range: R) -> Option<Span> {
    this.subspan(range)
}

// Include in sccache cache key.
const _: Option<&str> = option_env!("RUSTC_BOOTSTRAP");
