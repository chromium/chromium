//! TrueType hinting bytecode.

mod decode;
mod error;
mod instruction;
mod opcode;
mod program;

pub use decode::{decode_all, DecodeError, Decoder};
pub use error::{HintError, HintErrorKind};
pub use instruction::{InlineOperands, Instruction};
pub use opcode::Opcode;
pub use program::Program;

// Exported publicly for use by skrifa when the scaler_test feature is
// enabled.
#[cfg(any(test, feature = "scaler_test"))]
pub use instruction::MockInlineOperands;
