// Contributing
//
// New example code:
// - Please update the corresponding section in the derive tutorial
// - Building: They must be added to `Cargo.toml` with the appropriate `required-features`.
// - Testing: Ensure there is a markdown file with [trycmd](https://docs.rs/trycmd) syntax
//
// See also the general CONTRIBUTING

//! # Documentation: Builder Tutorial
//!
//! 1. [Quick Start][chapter_0]
//! 2. [Configuring the Parser][chapter_1]
//! 3. [Adding Arguments][chapter_2]
//!     1. [Positionals][chapter_2#positionals]
//!     2. [Options][chapter_2#options]
//!     3. [Flags][chapter_2#flags]
//!     4. [Subcommands][chapter_2#subcommands]
//!     5. [Defaults][chapter_2#defaults]
//! 4. [Validation][chapter_3]
//!     1. [Enumerated values][chapter_3#enumerated-values]
//!     2. [Validated values][chapter_3#validated-values]
//!     3. [Argument Relations][chapter_3#argument-relations]
//!     4. [Custom Validation][chapter_3#custom-validation]
//! 5. [Testing][chapter_4]
//! 6. [Next Steps][chapter_5]

#![allow(unused_imports)]
use crate::builder::*;

pub mod chapter_0;
pub mod chapter_1;
pub mod chapter_2;
pub mod chapter_3;
pub mod chapter_4;
pub mod chapter_5;
