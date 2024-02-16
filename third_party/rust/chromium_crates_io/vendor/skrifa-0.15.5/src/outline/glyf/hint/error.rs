//! Hinting error definitions.

/// Errors that may occur when interpreting TrueType bytecode.
#[derive(Clone, Debug)]
pub enum HintErrorKind {
    UnexpectedEndOfBytecode,
    InvalidOpcode(u8),
    DefinitionInGlyphProgram,
    NestedDefinition,
    InvalidDefintionIndex(usize),
    ValueStackOverflow,
    ValueStackUnderflow,
    CallStackOverflow,
    CallStackUnderflow,
    InvalidStackValue(i32),
    InvalidPointIndex(usize),
    InvalidPointRange(usize, usize),
    InvalidContourIndex(usize),
    InvalidCvtIndex(usize),
    InvalidStorageIndex(usize),
    DivideByZero,
    InvalidZoneIndex(i32),
    NegativeLoopCounter,
    InvalidJump,
}

impl core::fmt::Display for HintErrorKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UnexpectedEndOfBytecode => write!(f, "unexpected end of bytecode"),
            Self::InvalidOpcode(opcode) => write!(f, "invalid instruction opcode {opcode}"),
            Self::DefinitionInGlyphProgram => {
                write!(f, "FDEF or IDEF instruction present in glyph program")
            }
            Self::NestedDefinition => write!(
                f,
                "FDEF or IDEF instruction present in another FDEF or IDEF block"
            ),
            Self::InvalidDefintionIndex(index) => write!(
                f,
                "invalid function or instruction definition index {index}"
            ),
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
        }
    }
}
