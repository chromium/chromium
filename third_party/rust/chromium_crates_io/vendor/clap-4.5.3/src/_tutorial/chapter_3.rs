//! ## Validation
//!
//! 1. [Enumerated values](#enumerated-values)
//! 2. [Validated values](#validated-values)
//! 3. [Argument Relations](#argument-relations)
//! 4. [Custom Validation](#custom-validation)
//!
//! An appropriate default parser/validator will be selected for the field's type.  See
//! [`value_parser!`][crate::value_parser!] for more details.
//!
//! ### Enumerated values
//!
//! If you have arguments of specific values you want to test for, you can use the
//! [`PossibleValuesParser`][crate::builder::PossibleValuesParser] or [`Arg::value_parser(["val1",
//! ...])`][crate::Arg::value_parser] for short.
//!
//! This allows you specify the valid values for that argument. If the user does not use one of
//! those specific values, they will receive a graceful exit with error message informing them
//! of the mistake, and what the possible valid values are
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/04_01_possible.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/04_01_possible.md")]
//!
//! When enabling the [`derive` feature][crate::_features], you can use
//! [`ValueEnum`][crate::ValueEnum] to take care of the boiler plate for you, giving the same
//! results.
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/04_01_enum.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/04_01_enum.md")]
//!
//! ### Validated values
//!
//! More generally, you can validate and parse into any data type with [`Arg::value_parser`].
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/04_02_parse.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/04_02_parse.md")]
//!
//! A [custom parser][TypedValueParser] can be used to improve the error messages or provide additional validation:
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/04_02_validate.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/04_02_validate.md")]
//!
//! See [`Arg::value_parser`][crate::Arg::value_parser] for more details.
//!
//! ### Argument Relations
//!
//! You can declare dependencies or conflicts between [`Arg`][crate::Arg]s or even
//! [`ArgGroup`][crate::ArgGroup]s.
//!
//! [`ArgGroup`][crate::ArgGroup]s  make it easier to declare relations instead of having to list
//! each individually, or when you want a rule to apply "any but not all" arguments.
//!
//! Perhaps the most common use of [`ArgGroup`][crate::ArgGroup]s is to require one and *only* one
//! argument to be present out of a given set. Imagine that you had multiple arguments, and you
//! want one of them to be required, but making all of them required isn't feasible because perhaps
//! they conflict with each other.
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/04_03_relations.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/04_03_relations.md")]
//!
//! ### Custom Validation
//!
//! As a last resort, you can create custom errors with the basics of clap's formatting.
//!
//! ```rust
#![doc = include_str!("../../examples/tutorial_builder/04_04_custom.rs")]
//! ```
#![doc = include_str!("../../examples/tutorial_builder/04_04_custom.md")]
#![allow(unused_imports)]
use crate::builder::*;

pub use super::chapter_2 as previous;
pub use super::chapter_4 as next;
pub use crate::_tutorial as table_of_contents;
