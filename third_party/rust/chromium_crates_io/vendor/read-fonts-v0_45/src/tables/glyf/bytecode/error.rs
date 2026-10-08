//! What can go wrong while executing TrueType bytecode.
//!
//! Nothing in this crate executes bytecode. These live here because the set
//! is closed by the specification rather than by any one interpreter, so an
//! interpreter reports what happened in terms its caller already has, and two
//! of them describe the same failure the same way.

use super::{DecodeError, Opcode, Program};
use crate::types::GlyphId;

/// What went wrong, without the context of where.
#[derive(Clone, PartialEq, Debug)]
pub enum HintErrorKind {
    /// An instruction's operands ran past the end of the bytecode.
    UnexpectedEndOfBytecode,
    /// The interpreter does not implement this instruction.
    UnhandledOpcode(Opcode),
    /// A glyph program tried to define a function or instruction, which only
    /// the font program may do.
    DefinitionInGlyphProgram,
    /// A function or instruction definition began inside another.
    NestedDefinition,
    /// A definition exceeded the maximum size of 64k.
    DefinitionTooLarge,
    /// More definitions than the interpreter has room for.
    TooManyDefinitions,
    /// A call referenced a definition that was never made.
    InvalidDefinition(usize),
    /// A push exceeded the value stack.
    ValueStackOverflow,
    /// A pop found the value stack empty.
    ValueStackUnderflow,
    /// Calls nested deeper than the interpreter allows.
    CallStackOverflow,
    /// A return found the call stack empty.
    CallStackUnderflow,
    /// The value on the stack made no sense for the instruction reading it.
    InvalidStackValue(i32),
    /// A point index was past the end of the zone.
    InvalidPointIndex(usize),
    /// A point range ran past the end of the zone.
    InvalidPointRange(usize, usize),
    /// A contour index was past the end of the glyph.
    InvalidContourIndex(usize),
    /// A control value table index was out of bounds.
    InvalidCvtIndex(usize),
    /// A storage area index was out of bounds.
    InvalidStorageIndex(usize),
    /// A division had a zero divisor.
    DivideByZero,
    /// A zone index other than 0 (twilight) or 1 (glyph).
    InvalidZoneIndex(i32),
    /// An attempt to set the loop counter to a negative value.
    NegativeLoopCounter,
    /// A jump left the bounds of the program.
    InvalidJump,
    /// The program ran longer than the interpreter's budget allows.
    ExceededExecutionBudget,
}

impl core::fmt::Display for HintErrorKind {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::UnexpectedEndOfBytecode => write!(f, "unexpected end of bytecode"),
            Self::UnhandledOpcode(opcode) => write!(f, "unhandled instruction opcode {opcode}"),
            Self::DefinitionInGlyphProgram => write!(
                f,
                "function or instruction definition present in glyph program"
            ),
            Self::NestedDefinition => write!(f, "nested function or instruction definition"),
            Self::DefinitionTooLarge => write!(
                f,
                "function or instruction definition exceeded the maximum size of 64k"
            ),
            Self::TooManyDefinitions => write!(f, "too many function or instruction definitions"),
            Self::InvalidDefinition(key) => {
                write!(f, "function or instruction definition {key} not found")
            }
            Self::ValueStackOverflow => write!(f, "value stack overflow"),
            Self::ValueStackUnderflow => write!(f, "value stack underflow"),
            Self::CallStackOverflow => write!(f, "call stack overflow"),
            Self::CallStackUnderflow => write!(f, "call stack underflow"),
            Self::InvalidStackValue(value) => write!(
                f,
                "stack value {value} was invalid for the current operation"
            ),
            Self::InvalidPointIndex(index) => write!(f, "point index {index} was out of bounds"),
            Self::InvalidPointRange(start, end) => {
                write!(f, "point range {start}..{end} was out of bounds")
            }
            Self::InvalidContourIndex(index) => {
                write!(f, "contour index {index} was out of bounds")
            }
            Self::InvalidCvtIndex(index) => write!(f, "cvt index {index} was out of bounds"),
            Self::InvalidStorageIndex(index) => {
                write!(f, "storage area index {index} was out of bounds")
            }
            Self::DivideByZero => write!(f, "attempt to divide by 0"),
            Self::InvalidZoneIndex(index) => write!(
                f,
                "zone index {index} was invalid (only 0 or 1 are permitted)"
            ),
            Self::NegativeLoopCounter => {
                write!(f, "attempt to set the loop counter to a negative value")
            }
            Self::InvalidJump => write!(f, "the target of a jump instruction was invalid"),
            Self::ExceededExecutionBudget => write!(f, "too many instructions executed"),
        }
    }
}

impl From<DecodeError> for HintErrorKind {
    fn from(_: DecodeError) -> Self {
        Self::UnexpectedEndOfBytecode
    }
}

/// What went wrong, and where.
#[derive(Clone, PartialEq, Debug)]
pub struct HintError {
    /// The program that was running.
    pub program: Program,
    /// The glyph being hinted, if this was a glyph program.
    pub glyph_id: Option<GlyphId>,
    /// Offset of the instruction within its program.
    pub pc: usize,
    /// The instruction being executed, if the failure had reached one.
    pub opcode: Option<Opcode>,
    /// What went wrong.
    pub kind: HintErrorKind,
}

impl core::fmt::Display for HintError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(match self.program {
            Program::Font => "fpgm",
            Program::ControlValue => "prep",
            Program::Glyph => "glyf",
        })?;
        if let Some(glyph_id) = self.glyph_id {
            write!(f, "[{}]", glyph_id.to_u32())?;
        }
        let (opcode, colon) = match self.opcode {
            Some(opcode) => (opcode.name(), ":"),
            _ => ("", ""),
        };
        write!(f, "@{}:{opcode}{colon} {}", self.pc, self.kind)
    }
}

impl core::error::Error for HintError {}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::string::ToString;

    /// The message names the program, the glyph, the offset and the opcode.
    #[test]
    fn error_message_carries_its_context() {
        let error = HintError {
            program: Program::Glyph,
            glyph_id: Some(GlyphId::new(42)),
            pc: 17,
            opcode: Some(Opcode::SVTCA0),
            kind: HintErrorKind::DivideByZero,
        };
        assert_eq!(
            error.to_string(),
            "glyf[42]@17:SVTCA[y]: attempt to divide by 0"
        );
    }

    /// Without a glyph or an opcode there is nothing in their place.
    #[test]
    fn error_message_omits_what_it_does_not_have() {
        let error = HintError {
            program: Program::ControlValue,
            glyph_id: None,
            pc: 0,
            opcode: None,
            kind: HintErrorKind::ValueStackUnderflow,
        };
        assert_eq!(error.to_string(), "prep@0: value stack underflow");
    }

    /// A truncated instruction reads as running out of bytecode.
    #[test]
    fn decode_errors_become_unexpected_end() {
        assert_eq!(
            HintErrorKind::from(DecodeError),
            HintErrorKind::UnexpectedEndOfBytecode
        );
    }
}
