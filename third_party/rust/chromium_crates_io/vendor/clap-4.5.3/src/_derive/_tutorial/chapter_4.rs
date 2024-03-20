//! ## Testing
//!
//! clap reports most development errors as `debug_assert!`s.  Rather than checking every
//! subcommand, you should have a test that calls
//! [`Command::debug_assert`][crate::Command::debug_assert]:
//! ```rust,no_run
#![doc = include_str!("../../../examples/tutorial_derive/05_01_assert.rs")]
//! ```

#![allow(unused_imports)]
use crate::builder::*;

pub use super::chapter_3 as previous;
pub use super::chapter_5 as next;
pub use crate::_tutorial as table_of_contents;
