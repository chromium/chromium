//! ## Configuring the Parser
//!
//! You use derive [`Parser`][crate::Parser] to start building a parser.
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/02_apps.rs")]
//! ```
//!
#![doc = include_str!("../../../examples/tutorial_derive/02_apps.md")]
//!
//! You can use [`#[command(author, version, about)]` attribute defaults][super#command-attributes] on the struct to fill these fields in from your `Cargo.toml` file.
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/02_crate.rs")]
//! ```
#![doc = include_str!("../../../examples/tutorial_derive/02_crate.md")]
//!
//! You can use `#[command]` attributes on the struct to change the application level behavior of clap.  Any [`Command`][crate::Command] builder function can be used as an attribute, like [`Command::next_line_help`].
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/02_app_settings.rs")]
//! ```
#![doc = include_str!("../../../examples/tutorial_derive/02_app_settings.md")]
#![allow(unused_imports)]
use crate::builder::*;

pub use super::chapter_0 as previous;
pub use super::chapter_2 as next;
pub use crate::_tutorial as table_of_contents;
