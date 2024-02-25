//! State for managing active programs and decoding instructions.

mod args;

pub use args::Args;

/// Describes the source for a piece of bytecode.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
#[repr(u8)]
pub enum ProgramKind {
    /// Program that initializes the function and instruction tables. Stored
    /// in the `fpgm` table.
    #[default]
    Font = 0,
    /// Program that initializes CVT and storage based on font size and other
    /// parameters. Stored in the `prep` table.
    ControlValue = 1,
    /// Glyph specified program. Stored per-glyph in the `glyf` table.
    Glyph = 2,
}

#[cfg(test)]
pub(crate) use args::MockArgs;
