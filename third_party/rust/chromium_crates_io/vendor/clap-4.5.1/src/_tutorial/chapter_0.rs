//! ## Quick Start
//!
//! You can create an application with several arguments using usage strings.
//!
//! First, ensure `clap` is available:
//! ```console
//! $ cargo add clap
//! ```
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/01_quick.rs")]
//! ```
//!
#![doc = include_str!("../../examples/tutorial_builder/01_quick.md")]
//!
//! See also
//! - [FAQ: When should I use the builder vs derive APIs?][crate::_faq#when-should-i-use-the-builder-vs-derive-apis]
//! - The [cookbook][crate::_cookbook] for more application-focused examples

#![allow(unused_imports)]
use crate::builder::*;

pub use super::chapter_1 as next;
pub use crate::_tutorial as table_of_contents;
