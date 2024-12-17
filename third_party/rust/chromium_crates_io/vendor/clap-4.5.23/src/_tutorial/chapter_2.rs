//! ## Adding Arguments
//!
//! 1. [Positionals](#positionals)
//! 2. [Options](#options)
//! 3. [Flags](#flags)
//! 4. [Subcommands](#subcommands)
//! 5. [Defaults](#defaults)
//!
//!
//! ### Positionals
//!
//! You can have users specify values by their position on the command-line:
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/03_03_positional.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/03_03_positional.md")]
//!
//! Note that the default [`ArgAction`][crate::ArgAction] is [`Set`][crate::ArgAction::Set].  To
//! accept multiple values, override the [action][Arg::action] with [`Append`][crate::ArgAction::Append]:
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/03_03_positional_mult.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/03_03_positional_mult.md")]
//!
//! ### Options
//!
//! You can name your arguments with a flag:
//! - Order doesn't matter
//! - They can be optional
//! - Intent is clearer
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/03_02_option.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/03_02_option.md")]
//!
//! Note that the default [`ArgAction`][crate::ArgAction] is [`Set`][crate::ArgAction::Set].  To
//! accept multiple occurrences, override the [action][Arg::action] with [`Append`][crate::ArgAction::Append]:
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/03_02_option_mult.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/03_02_option_mult.md")]
//!
//! ### Flags
//!
//! Flags can also be switches that can be on/off:
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/03_01_flag_bool.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/03_01_flag_bool.md")]
//!
//! To accept multiple flags, use [`Count`][crate::ArgAction::Count]:
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/03_01_flag_count.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/03_01_flag_count.md")]
//!
//! ### Subcommands
//!
//! Subcommands are defined as [`Command`][crate::Command]s that get added via
//! [`Command::subcommand`][crate::Command::subcommand]. Each instance of a Subcommand can have its
//! own version, author(s), Args, and even its own subcommands.
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/03_04_subcommands.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/03_04_subcommands.md")]
//!
//! ### Defaults
//!
//! We've previously showed that arguments can be [`required`][crate::Arg::required] or optional.
//! When optional, you work with a `Option` and can `unwrap_or`.  Alternatively, you can set
//! [`Arg::default_value`][crate::Arg::default_value].
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/03_05_default_values.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/03_05_default_values.md")]
#![allow(unused_imports)]
use crate::builder::*;

pub use super::chapter_1 as previous;
pub use super::chapter_3 as next;
pub use crate::_tutorial as table_of_contents;
