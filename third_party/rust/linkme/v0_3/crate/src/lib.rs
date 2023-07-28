//! [![github]](https://github.com/dtolnay/linkme)&ensp;[![crates-io]](https://crates.io/crates/linkme)&ensp;[![docs-rs]](https://docs.rs/linkme)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs
//!
//! <br>
//!
//! **A library for safe cross-platform linker shenanigans.**
//!
//! <br>
//!
//! # Platform support
//!
//! | Component | Linux | macOS | Windows | FreeBSD | illumos | Other...<sup>†</sup> |
//! |:---|:---:|:---:|:---:|:---:|:---:|:---:|
//! | Distributed slice | 💚 | 💚 | 💚 | 💚 | 💚 | |
//!
//! <br>***<sup>†</sup>*** We welcome PRs adding support for any platforms not
//! listed here.
//!
//! <br>
//!
//! # Distributed slice
//!
//! A distributed slice is a collection of static elements that are gathered
//! into a contiguous section of the binary by the linker. Slice elements may be
//! defined individually from anywhere in the dependency graph of the final
//! binary.
//!
//! Refer to [`linkme::DistributedSlice`][DistributedSlice] for complete details
//! of the API. The basic idea is as follows.
//!
//! A static distributed slice is declared by writing `#[distributed_slice]` on
//! a static item whose type is `[T]` for some type `T`. The initializer
//! expression must be `[..]` to indicate that elements come from elsewhere.
//!
//! ```
//! # #![cfg_attr(feature = "used_linker", feature(used_with_arg))]
//! #
//! # struct Bencher;
//! #
//! use linkme::distributed_slice;
//!
//! #[distributed_slice]
//! pub static BENCHMARKS: [fn(&mut Bencher)] = [..];
//! ```
//!
//! Slice elements may be registered into a distributed slice by a
//! `#[distributed_slice(...)]` attribute in which the path to the distributed
//! slice is given in the parentheses. The initializer is required to be a const
//! expression.
//!
//! ```
//! # #![cfg_attr(feature = "used_linker", feature(used_with_arg))]
//! #
//! # mod other_crate {
//! #     use linkme::distributed_slice;
//! #
//! #     pub struct Bencher;
//! #
//! #     #[distributed_slice]
//! #     pub static BENCHMARKS: [fn(&mut Bencher)] = [..];
//! # }
//! #
//! # use other_crate::Bencher;
//! #
//! use linkme::distributed_slice;
//! use other_crate::BENCHMARKS;
//!
//! #[distributed_slice(BENCHMARKS)]
//! static BENCH_DESERIALIZE: fn(&mut Bencher) = bench_deserialize;
//!
//! fn bench_deserialize(b: &mut Bencher) {
//!     /* ... */
//! }
//! ```
//!
//! The distributed slice behaves in all ways like `&'static [T]`.
//!
//! ```no_run
//! # #![cfg_attr(feature = "used_linker", feature(used_with_arg))]
//! #
//! # use linkme::distributed_slice;
//! #
//! # struct Bencher;
//! #
//! # #[distributed_slice]
//! # static BENCHMARKS: [fn(&mut Bencher)] = [..];
//! #
//! fn main() {
//!     // Iterate the elements.
//!     for bench in BENCHMARKS {
//!         /* ... */
//!     }
//!
//!     // Index into the elements.
//!     let first = BENCHMARKS[0];
//!
//!     // Slice the elements.
//!     let except_first = &BENCHMARKS[1..];
//!
//!     // Invoke methods on the underlying slice.
//!     let len = BENCHMARKS.len();
//! }
//! ```

#![no_std]
#![doc(html_root_url = "https://docs.rs/linkme/0.3.11")]
#![allow(
    clippy::doc_markdown,
    clippy::empty_enum,
    clippy::expl_impl_clone_on_copy,
    clippy::manual_assert,
    clippy::missing_panics_doc,
    clippy::must_use_candidate,
    clippy::unused_self
)]

mod distributed_slice;

// Not public API.
#[doc(hidden)]
#[path = "private.rs"]
pub mod __private;

pub use linkme_impl::*;

pub use crate::distributed_slice::DistributedSlice;
