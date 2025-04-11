#![allow(clippy::comparison_chain)]
#![allow(clippy::needless_range_loop)]

/// This is the primary interface for llguidance -- the one on which the others
/// (FFI and LLInterpreter) are built.  While not cleanest of these interfaces,
/// it is the  most inclusive.
///
/// cbindgen:ignore
pub mod earley;

mod matcher;
mod tokenparser;
pub use tokenparser::TokenParser;
pub mod api;
pub mod output;
pub use toktrie;
pub mod panic_utils;

mod constraint;
mod stop_controller;
mod tokenizer_json;
pub use constraint::{CommitResult, Constraint};
pub use matcher::Matcher;

mod factory;
pub use factory::ParserFactory;

mod logging;
pub use logging::Logger;

pub use derivre;
pub use derivre::{HashMap, HashSet};

pub mod ffi;
#[cfg(feature = "rayon")]
mod ffi_par;

mod grammar_builder;
mod json;
#[cfg(feature = "jsonschema_validation")]
mod json_validation;
pub mod substring;
pub use grammar_builder::{GrammarBuilder, NodeRef};
pub use json::compiler::JsonCompileOptions;
pub use json::json_merge;
pub use stop_controller::StopController;
pub use tokenizer_json::token_bytes_from_tokenizer_json;

#[cfg(feature = "lark")]
mod lark;

#[cfg(feature = "wasm")]
pub use instant::Instant;

#[cfg(not(feature = "wasm"))]
pub use std::time::Instant;

#[macro_export]
macro_rules! loginfo {
    ($s:expr, $($arg:tt)*) => {
        if $s.level_enabled(2) {
            use std::fmt::Write;
            writeln!($s.info_logger(), $($arg)*).unwrap();
        }
    };
}

#[macro_export]
macro_rules! infoln {
    ($s:expr, $($arg:tt)*) => {
        if $s.logger.level_enabled(2) {
            use std::fmt::Write;
            writeln!($s.logger.info_logger(), $($arg)*).unwrap();
        }
    };
}

#[macro_export]
macro_rules! warn {
    ($s:expr, $($arg:tt)*) => {
        if $s.logger.level_enabled(1) {
            use std::fmt::Write;
            $s.logger.write_warning("Warning: ");
            writeln!($s.logger.warning_logger(), $($arg)*).unwrap();
        }
    };
}

#[macro_export]
macro_rules! id32_type {
    ($name:ident) => {
        #[derive(serde::Serialize, serde::Deserialize, Hash, PartialEq, Eq, Clone, Copy, Debug)]
        #[serde(transparent)]
        pub struct $name(pub u32);

        impl $name {
            pub fn as_usize(&self) -> usize {
                self.0 as usize
            }

            pub fn new(idx: usize) -> Self {
                $name(idx as u32)
            }
        }
    };
}
