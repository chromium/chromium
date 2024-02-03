//! ## Quick Start
//!
//! You can create an application declaratively with a `struct` and some
//! attributes.
//!
//! First, ensure `clap` is available with the [`derive` feature flag][crate::_features]:
//! ```console
//! $ cargo add clap --features derive
//! ```
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/01_quick.rs")]
//! ```
//!
#![doc = include_str!("../../../examples/tutorial_derive/01_quick.md")]
//!
//! See also
//! - [FAQ: When should I use the builder vs derive APIs?][crate::_faq#when-should-i-use-the-builder-vs-derive-apis]
//! - The [cookbook][crate::_cookbook] for more application-focused examples

#![allow(unused_imports)]
use crate::builder::*;

pub use super::chapter_1 as next;
pub use crate::_tutorial as table_of_contents;
