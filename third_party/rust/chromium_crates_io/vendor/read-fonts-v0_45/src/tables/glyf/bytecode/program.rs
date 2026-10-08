//! The three programs a font can carry.

/// Which of a font's three programs some bytecode came from.
///
/// They differ in more than provenance: only the font program may define
/// functions and instructions, and only a glyph program has a glyph to work
/// on.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
#[repr(u8)]
pub enum Program {
    /// Initializes the function and instruction tables. Stored in `fpgm`.
    #[default]
    Font = 0,
    /// Initializes the control value table and storage area for a size and
    /// other parameters. Stored in `prep`.
    ControlValue = 1,
    /// Hints one glyph. Stored with the glyph in `glyf`.
    Glyph = 2,
}
