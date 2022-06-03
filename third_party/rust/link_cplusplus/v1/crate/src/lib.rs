//! [![github]](https://github.com/dtolnay/link-cplusplus)&ensp;[![crates-io]](https://crates.io/crates/link-cplusplus)&ensp;[![docs-rs]](https://docs.rs/link-cplusplus)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K
//!
//! <br>
//!
//! # `-lstdc++` or `-lc++`
//!
//! This crate exists for the purpose of passing `-lstdc++` or `-lc++` to the
//! linker, while making it possible for an application to make that choice on
//! behalf of its library dependencies.
//!
//! Without this crate, a library would need to:
//!
//! - pick one or the other to link, with no way for downstream applications to
//!   override the choice;
//! - or link neither and require an explicit link flag provided by downstream
//!   applications even if they would be fine with a default choice;
//!
//! neither of which are good experiences.
//!
//! <br>
//!
//! # Options
//!
//! An application or library that is fine with either of libstdc++ or libc++
//! being linked, whichever is the platform's default, should use the following
//! in Cargo.toml:
//!
//! ```toml
//! [dependencies]
//! link-cplusplus = "1.0"
//! ```
//!
//! An application that wants a particular one or the other linked should use:
//!
//! ```toml
//! [dependencies]
//! link-cplusplus = { version = "1.0", features = ["libstdc++"] }
//!
//! # or
//!
//! link-cplusplus = { version = "1.0", features = ["libc++"] }
//! ```
//!
//! An application that wants to handle its own more complicated logic for link
//! flags from its build script can make this crate do nothing by using:
//!
//! ```toml
//! [dependencies]
//! link-cplusplus = { version = "1.0", features = ["nothing"] }
//! ```
//!
//! Lastly, make sure to add an explicit `extern crate` dependency to your crate
//! root, since the link-cplusplus crate will be otherwise unused and its link
//! flags dropped.
//!
//! ```
//! // src/lib.rs
//!
//! extern crate link_cplusplus;
//! ```
