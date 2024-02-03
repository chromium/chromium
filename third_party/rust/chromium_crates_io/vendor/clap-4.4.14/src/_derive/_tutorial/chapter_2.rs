//! ## Adding Arguments
//!
//! 1. [Positionals](#positionals)
//! 2. [Options](#options)
//! 3. [Flags](#flags)
//! 4. [Subcommands](#subcommands)
//! 5. [Defaults](#defaults)
//!
//! Arguments are inferred from the fields of your struct.
//!
//! ### Positionals
//!
//! You can have users specify values by their position on the command-line:
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/03_03_positional.rs")]
//! ```
#![doc = include_str!("../../../examples/tutorial_derive/03_03_positional.md")]
//!
//! Note that the [default `ArgAction` is `Set`][super#arg-types].  To
//! accept multiple values, override the [action][Arg::action] with [`Append`][crate::ArgAction::Append] via `Vec`:
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/03_03_positional_mult.rs")]
//! ```
#![doc = include_str!("../../../examples/tutorial_derive/03_03_positional_mult.md")]
//!
//! ### Options
//!
//! You can name your arguments with a flag:
//! - Order doesn't matter
//! - They can be optional
//! - Intent is clearer
//!
//! To specify the flags for an argument, you can use [`#[arg(short = 'n')]`][Arg::short] and/or
//! [`#[arg(long = "name")]`][Arg::long] attributes on a field.  When no value is given (e.g.
//! `#[arg(short)]`), the flag is inferred from the field's name.
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/03_02_option.rs")]
//! ```
#![doc = include_str!("../../../examples/tutorial_derive/03_02_option.md")]
//!
//! Note that the [default `ArgAction` is `Set`][super#arg-types].  To
//! accept multiple occurrences, override the [action][Arg::action] with [`Append`][crate::ArgAction::Append] via `Vec`:
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/03_02_option_mult.rs")]
//! ```
#![doc = include_str!("../../../examples/tutorial_derive/03_02_option_mult.md")]
//!
//! ### Flags
//!
//! Flags can also be switches that can be on/off:
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/03_01_flag_bool.rs")]
//! ```
#![doc = include_str!("../../../examples/tutorial_derive/03_01_flag_bool.md")]
//!
//! Note that the [default `ArgAction` for a `bool` field is
//! `SetTrue`][super#arg-types].  To accept multiple flags, override the [action][Arg::action] with
//! [`Count`][crate::ArgAction::Count]:
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/03_01_flag_count.rs")]
//! ```
#![doc = include_str!("../../../examples/tutorial_derive/03_01_flag_count.md")]
//!
//! This also shows that any[`Arg`][crate::Args] method may be used as an attribute.
//!
//! ### Subcommands
//!
//! Subcommands are derived with `#[derive(Subcommand)]` and be added via
//! [`#[command(subcommand)]` attribute][super#command-attributes] on the field using that type.
//! Each instance of a [Subcommand][crate::Subcommand] can have its own version, author(s), Args,
//! and even its own subcommands.
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/03_04_subcommands.rs")]
//! ```
//! We used a struct-variant to define the `add` subcommand.
//! Alternatively, you can use a struct for your subcommand's arguments:
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/03_04_subcommands_alt.rs")]
//! ```
//!
#![doc = include_str!("../../../examples/tutorial_derive/03_04_subcommands.md")]
//!
//! ### Defaults
//!
//! We've previously showed that arguments can be [`required`][crate::Arg::required] or optional.
//! When optional, you work with a `Option` and can `unwrap_or`.  Alternatively, you can
//! set [`#[arg(default_value_t)]`][super#arg-attributes].
//!
//! ```rust
#![doc = include_str!("../../../examples/tutorial_derive/03_05_default_values.rs")]
//! ```
#![doc = include_str!("../../../examples/tutorial_derive/03_05_default_values.md")]
#![allow(unused_imports)]
use crate::builder::*;

pub use super::chapter_1 as previous;
pub use super::chapter_3 as next;
pub use crate::_tutorial as table_of_contents;
