//! Read DWARF debugging information.
//!
//! * [Example Usage](#example-usage)
//! * [API Structure](#api-structure)
//!
//! ## Example Usage
//!
//! Print out all of the functions in the debuggee program:
//!
//! ```rust,no_run
//! # fn example() -> Result<(), gimli::Error> {
//! # type R = gimli::EndianSlice<'static, gimli::LittleEndian>;
//! # let get_file_section_reader = |name| -> Result<R, gimli::Error> { unimplemented!() };
//! # let get_sup_file_section_reader = |name| -> Result<R, gimli::Error> { unimplemented!() };
//! // Read the DWARF sections with whatever object loader you're using.
//! // These closures should return a `Reader` instance (e.g. `EndianSlice`).
//! let loader = |section: gimli::SectionId| { get_file_section_reader(section.name()) };
//! let sup_loader = |section: gimli::SectionId| { get_sup_file_section_reader(section.name()) };
//! let mut dwarf = gimli::Dwarf::load(loader)?;
//! dwarf.load_sup(sup_loader)?;
//!
//! // Iterate over all compilation units.
//! let mut iter = dwarf.units();
//! while let Some(header) = iter.next()? {
//!     // Parse the abbreviations and other information for this compilation unit.
//!     let unit = dwarf.unit(header)?;
//!
//!     // Iterate over all of this compilation unit's entries.
//!     let mut entries = unit.entries();
//!     while let Some(entry) = entries.next_dfs()? {
//!         // If we find an entry for a function, print it.
//!         if entry.tag() == gimli::DW_TAG_subprogram {
//!             println!("Found a function: {:?}", entry);
//!         }
//!     }
//! }
//! # unreachable!()
//! # }
//! ```
//!
//! Full example programs:
//!
//!   * [A simple `.debug_info` parser](https://github.com/gimli-rs/gimli/blob/main/crates/examples/src/bin/simple.rs)
//!
//!   * [A simple `.debug_line` parser](https://github.com/gimli-rs/gimli/blob/main/crates/examples/src/bin/simple_line.rs)
//!
//!   * [A `dwarfdump`
//!     clone](https://github.com/gimli-rs/gimli/blob/main/crates/examples/src/bin/dwarfdump.rs)
//!
//!   * [An `addr2line` clone](https://github.com/gimli-rs/addr2line)
//!
//!   * [`ddbug`](https://github.com/gimli-rs/ddbug), a utility giving insight into
//!     code generation by making debugging information readable
//!
//!   * [`dwprod`](https://github.com/fitzgen/dwprod), a tiny utility to list the
//!     compilers used to create each compilation unit within a shared library or
//!     executable (via `DW_AT_producer`)
//!
//!   * [`dwarf-validate`](https://github.com/gimli-rs/gimli/blob/main/crates/examples/src/bin/dwarf-validate.rs),
//!     a program to validate the integrity of some DWARF and its references
//!     between sections and compilation units.
//!
//! ## API Structure
//!
//! * Basic familiarity with DWARF is assumed.
//!
//! * The [`Dwarf`](./struct.Dwarf.html) type contains the commonly used DWARF
//!   sections. It has methods that simplify access to debugging data that spans
//!   multiple sections. Use of this type is optional, but recommended.
//!
//! * The [`DwarfPackage`](./struct.Dwarf.html) type contains the DWARF
//!   package (DWP) sections. It has methods to find a DWARF object (DWO)
//!   within the package.
//!
//! * Each section gets its own type. Consider these types the entry points to
//!   the library:
//!
//!   * [`DebugAbbrev`](./struct.DebugAbbrev.html): The `.debug_abbrev` section.
//!
//!   * [`DebugAddr`](./struct.DebugAddr.html): The `.debug_addr` section.
//!
//!   * [`DebugAranges`](./struct.DebugAranges.html): The `.debug_aranges`
//!     section.
//!
//!   * [`DebugFrame`](./struct.DebugFrame.html): The `.debug_frame` section.
//!
//!   * [`DebugInfo`](./struct.DebugInfo.html): The `.debug_info` section.
//!
//!   * [`DebugLine`](./struct.DebugLine.html): The `.debug_line` section.
//!
//!   * [`DebugLineStr`](./struct.DebugLineStr.html): The `.debug_line_str` section.
//!
//!   * [`DebugLoc`](./struct.DebugLoc.html): The `.debug_loc` section.
//!
//!   * [`DebugLocLists`](./struct.DebugLocLists.html): The `.debug_loclists` section.
//!
//!   * [`DebugNames`](./struct.DebugNames.html): The `.debug_names` section.
//!
//!   * [`DebugPubNames`](./struct.DebugPubNames.html): The `.debug_pubnames`
//!     section.
//!
//!   * [`DebugPubTypes`](./struct.DebugPubTypes.html): The `.debug_pubtypes`
//!     section.
//!
//!   * [`DebugRanges`](./struct.DebugRanges.html): The `.debug_ranges` section.
//!
//!   * [`DebugRngLists`](./struct.DebugRngLists.html): The `.debug_rnglists` section.
//!
//!   * [`DebugStr`](./struct.DebugStr.html): The `.debug_str` section.
//!
//!   * [`DebugStrOffsets`](./struct.DebugStrOffsets.html): The `.debug_str_offsets` section.
//!
//!   * [`DebugTypes`](./struct.DebugTypes.html): The `.debug_types` section.
//!
//!   * [`DebugCuIndex`](./struct.DebugCuIndex.html): The `.debug_cu_index` section.
//!
//!   * [`DebugTuIndex`](./struct.DebugTuIndex.html): The `.debug_tu_index` section.
//!
//!   * [`EhFrame`](./struct.EhFrame.html): The `.eh_frame` section.
//!
//!   * [`EhFrameHdr`](./struct.EhFrameHdr.html): The `.eh_frame_hdr` section.
//!
//! * Each section type exposes methods for accessing the debugging data encoded
//!   in that section. For example, the [`DebugInfo`](./struct.DebugInfo.html)
//!   struct has the [`units`](./struct.DebugInfo.html#method.units) method for
//!   iterating over the compilation units defined within it.
//!
//! * Offsets into a section are strongly typed: an offset into `.debug_info` is
//!   the [`DebugInfoOffset`](./struct.DebugInfoOffset.html) type. It cannot be
//!   used to index into the [`DebugLine`](./struct.DebugLine.html) type because
//!   `DebugLine` represents the `.debug_line` section. There are similar types
//!   for offsets relative to a compilation unit rather than a section.

use core::error;
use core::fmt::{self, Debug};
use core::result;
#[cfg(feature = "std")]
use std::io;

use crate::common::{Register, SectionId};
use crate::constants;

mod util;
pub use util::*;

mod addr;
pub use self::addr::*;

mod cfi;
pub use self::cfi::*;

#[cfg(feature = "read")]
mod dwarf;
#[cfg(feature = "read")]
pub use self::dwarf::*;

mod endian_slice;
pub use self::endian_slice::*;

#[cfg(feature = "endian-reader")]
mod endian_reader;
#[cfg(feature = "endian-reader")]
pub use self::endian_reader::*;

mod reader;
pub use self::reader::*;

mod relocate;
pub use self::relocate::*;

#[cfg(feature = "read")]
mod abbrev;
#[cfg(feature = "read")]
pub use self::abbrev::*;

mod aranges;
pub use self::aranges::*;

mod index;
pub use self::index::*;

#[cfg(feature = "read")]
mod line;
#[cfg(feature = "read")]
pub use self::line::*;

mod lists;
#[cfg(feature = "read")]
pub use self::lists::ListsHeader;

mod loclists;
pub use self::loclists::*;

#[cfg(feature = "read")]
mod lookup;

#[cfg(feature = "read")]
mod macros;
#[cfg(feature = "read")]
pub use self::macros::*;

#[cfg(feature = "read")]
mod names;
#[cfg(feature = "read")]
pub use self::names::*;

mod op;
pub use self::op::*;

#[cfg(feature = "read")]
mod pubnames;
#[cfg(feature = "read")]
pub use self::pubnames::*;

#[cfg(feature = "read")]
mod pubtypes;
#[cfg(feature = "read")]
pub use self::pubtypes::*;

mod rnglists;
pub use self::rnglists::*;

mod str;
pub use self::str::*;

/// An offset into the current compilation or type unit.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Ord, PartialOrd, Hash)]
pub struct UnitOffset<T = usize>(pub T);

#[cfg(feature = "read")]
mod unit;
#[cfg(feature = "read")]
pub use self::unit::*;

mod value;
pub use self::value::*;

/// Indicates that storage should be allocated on heap.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct StoreOnHeap;

/// An error that occurred when parsing.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum Error {
    /// An I/O error occurred while reading.
    Io,
    /// Found a PC relative pointer, but the section base is undefined.
    PcRelativePointerButSectionBaseIsUndefined,
    /// Found a `.text` relative pointer, but the `.text` base is undefined.
    TextRelativePointerButTextBaseIsUndefined,
    /// Found a data relative pointer, but the data base is undefined.
    DataRelativePointerButDataBaseIsUndefined,
    /// Found a function relative pointer in a context that does not have a
    /// function base.
    FuncRelativePointerInBadContext,
    /// Cannot parse a pointer with a `DW_EH_PE_omit` encoding.
    CannotParseOmitPointerEncoding,
    /// An error parsing an unsigned LEB128 value.
    BadUnsignedLeb128,
    /// An error parsing a signed LEB128 value.
    BadSignedLeb128,
    /// An abbreviation declared that its tag is zero, but zero is reserved for
    /// null records.
    AbbreviationTagZero,
    /// An attribute specification declared that its name is zero, but zero is
    /// reserved for null records.
    AttributeNameZero,
    /// An attribute specification declared that its form is zero, but zero is
    /// reserved for null records.
    AttributeFormZero,
    /// The abbreviation's has-children byte was not one of
    /// `DW_CHILDREN_{yes,no}`.
    InvalidAbbreviationChildren(constants::DwChildren),
    /// Found an unknown `DW_FORM_*` type.
    UnknownForm(constants::DwForm),
    /// Found an abbreviation code that has already been used.
    DuplicateAbbreviationCode(u64),
    /// Found an unknown reserved length value.
    UnknownReservedLength(u32),
    /// Found an unknown DWARF version.
    UnknownVersion(u64),
    /// Found an entry with an invalid abbreviation code.
    InvalidAbbreviationCode(u64),
    /// Hit the end of input before it was expected.
    UnexpectedEof(ReaderOffsetId),
    /// Found an unknown location-lists format.
    UnknownLocListsEntry(constants::DwLle),
    /// Found an unknown range-lists format.
    UnknownRangeListsEntry(constants::DwRle),
    /// The specified address size is not supported.
    UnsupportedAddressSize(u8),
    /// The specified offset size is not supported.
    UnsupportedOffsetSize(u8),
    /// The minimum instruction length must not be zero.
    MinimumInstructionLengthZero,
    /// The maximum operations per instruction must not be zero.
    MaximumOperationsPerInstructionZero,
    /// The line range must not be zero.
    LineRangeZero,
    /// The opcode base must not be zero.
    OpcodeBaseZero,
    /// Found an invalid UTF-8 string.
    BadUtf8,
    /// Expected to find the CIE ID, but found something else.
    NotCieId(u64),
    /// Expected to find a pointer to a CIE, but found the CIE ID instead.
    NotCiePointer(u64),
    /// Invalid branch target for a DW_OP_bra or DW_OP_skip.
    BadBranchTarget(u64),
    /// DW_OP_push_object_address used but no address passed in.
    InvalidPushObjectAddress,
    /// Not enough items on the stack when evaluating an expression.
    NotEnoughStackItems,
    /// Too many iterations to compute the expression.
    TooManyIterations,
    /// An unrecognized operation was found while parsing a DWARF
    /// expression.
    InvalidExpression(constants::DwOp),
    /// An unsupported operation was found while evaluating a DWARF expression.
    UnsupportedEvaluation,
    /// The expression had a piece followed by an expression
    /// terminator without a piece.
    InvalidPiece,
    /// An expression-terminating operation was followed by something
    /// other than the end of the expression or a piece operation.
    InvalidExpressionTerminator(u64),
    /// Division or modulus by zero when evaluating an expression.
    DivisionByZero,
    /// An expression operation used mismatching types.
    TypeMismatch,
    /// An expression operation required an integral type but saw a
    /// floating point type.
    IntegralTypeRequired,
    /// An expression operation used types that are not supported.
    UnsupportedTypeOperation,
    /// The shift value in an expression must be a non-negative integer.
    InvalidShiftExpression,
    /// The size of a deref expression must not be larger than the size of an address.
    InvalidDerefSize(u8),
    /// The byte size of a `DW_OP_piece` is too large to convert to a bit size.
    InvalidPieceSize(u64),
    /// An unknown DW_CFA_* instruction.
    UnknownCallFrameInstruction(constants::DwCfa),
    /// A `DW_CFA_set_loc` instruction moved the address backward.
    InvalidCfiSetLoc(u64),
    /// An address calculation overflowed.
    ///
    /// This is returned in cases where the address is expected to be
    /// larger than a previous address, but the calculation overflowed.
    AddressOverflow,
    /// Encountered a call frame instruction in a context in which it is not
    /// valid.
    CfiInstructionInInvalidContext,
    /// When evaluating call frame instructions, found a `DW_CFA_restore_state`
    /// stack pop instruction, but the stack was empty, and had nothing to pop.
    PopWithEmptyStack,
    /// Do not have unwind info for the given address.
    NoUnwindInfoForAddress,
    /// An offset value was larger than the maximum supported value.
    UnsupportedOffset,
    /// The given pointer encoding is either unknown or invalid.
    UnknownPointerEncoding(constants::DwEhPe),
    /// Did not find an entry at the given offset.
    NoEntryAtGivenOffset(u64),
    /// The given offset is out of bounds.
    OffsetOutOfBounds(u64),
    /// Found an unknown CFI augmentation.
    UnknownAugmentation,
    /// We do not support the given pointer encoding yet.
    UnsupportedPointerEncoding(constants::DwEhPe),
    /// We do not support dereferencing indirect pointers.
    UnsupportedIndirectPointer,
    /// Registers larger than `u16` are not supported.
    UnsupportedRegister(u64),
    /// The CFI program defined more register rules than we have storage for.
    TooManyRegisterRules,
    /// Attempted to push onto the CFI or evaluation stack, but it was already
    /// at full capacity.
    StackFull,
    /// The `DW_UT_*` value for this unit is not supported yet.
    UnknownUnitType(constants::DwUt),
    /// Nonzero segment selector sizes aren't supported yet.
    UnsupportedSegmentSize(u8),
    /// A compilation unit or type unit is missing its top level DIE.
    MissingUnitDie,
    /// A split DWARF section does not contain the split compilation unit.
    MissingSplitUnit,
    /// A DIE attribute used an unsupported form.
    UnsupportedAttributeForm(constants::DwForm),
    /// Missing DW_LNCT_path in file entry format.
    MissingFileEntryFormatPath,
    /// Expected an attribute value to be a string form.
    ExpectedStringAttributeValue,
    /// An attribute with an indirect form cannot use `DW_FORM_implicit_const`.
    InvalidImplicitConst,
    /// Invalid section count in `.dwp` index.
    UnsupportedIndexSectionCount(u32),
    /// Invalid slot count in `.dwp` index.
    InvalidIndexSlotCount(u32),
    /// Invalid row index in `.dwp` index.
    InvalidIndexRow(u32),
    /// Unknown section type in `.dwp` index.
    UnknownIndexSection(constants::DwSect),
    /// Unknown section type in version 2 `.dwp` index.
    UnknownIndexSectionV2(constants::DwSectV2),
    /// Invalid macinfo type in `.debug_macinfo`.
    InvalidMacinfoType(constants::DwMacinfo),
    /// Invalid macro type in `.debug_macro`.
    InvalidMacroType(constants::DwMacro),
    /// The optional `opcode_operands_table` in `.debug_macro` is currently not supported.
    UnsupportedOpcodeOperandsTable,
    /// Invalid index in a `.debug_names` attribute value.
    InvalidNameAttributeIndex(u64),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> ::core::result::Result<(), fmt::Error> {
        match *self {
            Error::Io => write!(f, "I/O error"),
            Error::PcRelativePointerButSectionBaseIsUndefined => {
                write!(f, "undefined section base for DW_EH_PE_pcrel pointer")
            }
            Error::TextRelativePointerButTextBaseIsUndefined => {
                write!(f, "undefined text base for DW_EH_PE_textrel pointer")
            }
            Error::DataRelativePointerButDataBaseIsUndefined => {
                write!(f, "undefined data base for DW_EH_PE_datarel pointer")
            }
            Error::FuncRelativePointerInBadContext => {
                write!(f, "invalid context for DW_EH_PE_funcrel pointer")
            }
            Error::CannotParseOmitPointerEncoding => {
                write!(f, "invalid encoding for required pointer: DW_EH_PE_omit")
            }
            Error::BadUnsignedLeb128 => write!(f, "unsigned LEB128 overflow"),
            Error::BadSignedLeb128 => write!(f, "signed LEB128 overflow"),
            Error::AbbreviationTagZero => {
                write!(f, "invalid abbreviation tag: zero")
            }
            Error::AttributeNameZero => {
                write!(f, "invalid attribute name: zero")
            }
            Error::AttributeFormZero => {
                write!(f, "invalid attribute form: zero")
            }
            Error::InvalidAbbreviationChildren(val) => {
                write!(f, "invalid abbreviation children: 0x{:x}", val.0)
            }
            Error::UnknownForm(val) => write!(f, "unknown attribute form: 0x{:x}", val.0),
            Error::DuplicateAbbreviationCode(val) => {
                write!(f, "duplicate abbreviation code: {val}")
            }
            Error::UnknownReservedLength(val) => write!(f, "unknown reserved length: 0x{val:x}"),
            Error::UnknownVersion(version) => write!(f, "unknown DWARF version: {version}"),
            Error::InvalidAbbreviationCode(val) => {
                write!(f, "invalid abbreviation code: {val}")
            }
            Error::UnexpectedEof(_) => write!(f, "unexpected end of input"),
            Error::UnknownLocListsEntry(val) => {
                write!(f, "unknown location lists entry: 0x{:x}", val.0)
            }
            Error::UnknownRangeListsEntry(val) => {
                write!(f, "unknown range lists entry: 0x{:x}", val.0)
            }
            Error::UnsupportedAddressSize(val) => {
                write!(f, "unsupported address size: {val}")
            }
            Error::UnsupportedOffsetSize(val) => {
                write!(f, "unsupported offset size: {val}")
            }
            Error::MinimumInstructionLengthZero => {
                write!(f, "invalid minimum line instruction length: zero")
            }
            Error::MaximumOperationsPerInstructionZero => {
                write!(f, "invalid maximum operations per line instruction: zero")
            }
            Error::LineRangeZero => write!(f, "invalid line range: zero"),
            Error::OpcodeBaseZero => write!(f, "invalid line opcode base: zero"),
            Error::BadUtf8 => write!(f, "invalid UTF-8"),
            Error::NotCieId(val) => write!(f, "invalid CIE at offset 0x{val:x}: missing CIE ID"),
            Error::NotCiePointer(val) => {
                write!(f, "invalid FDE at offset 0x{val:x}: missing CIE pointer")
            }
            Error::BadBranchTarget(_) => write!(f, "invalid expression branch target"),
            Error::InvalidPushObjectAddress => {
                write!(f, "undefined object address for DW_OP_push_object_address")
            }
            Error::NotEnoughStackItems => {
                write!(f, "expression stack underflow")
            }
            Error::TooManyIterations => {
                write!(f, "exceeded maximum expression iterations")
            }
            Error::InvalidExpression(val) => write!(f, "unknown expression opcode: 0x{:x}", val.0),
            Error::UnsupportedEvaluation => {
                write!(f, "unsupported evaluation operation")
            }
            Error::InvalidPiece => {
                write!(f, "invalid expression: piece followed by non-piece")
            }
            Error::InvalidExpressionTerminator(_) => {
                write!(f, "invalid expression terminator")
            }
            Error::DivisionByZero => {
                write!(f, "division by zero")
            }
            Error::TypeMismatch => write!(f, "invalid operand type: mismatch"),
            Error::IntegralTypeRequired => {
                write!(f, "invalid operand type: integral required")
            }
            Error::UnsupportedTypeOperation => {
                write!(f, "unsupported operand type")
            }
            Error::InvalidShiftExpression => {
                write!(f, "invalid shift amount")
            }
            Error::InvalidDerefSize(val) => {
                write!(f, "invalid deref size: {val}")
            }
            Error::InvalidPieceSize(val) => {
                write!(f, "invalid piece size: {val}")
            }
            Error::UnknownCallFrameInstruction(val) => {
                write!(f, "unknown call frame instruction: 0x{:x}", val.0)
            }
            Error::InvalidCfiSetLoc(val) => {
                write!(f, "invalid DW_CFA_set_loc: address 0x{val:x} goes backward")
            }
            Error::AddressOverflow => write!(f, "address overflow"),
            Error::CfiInstructionInInvalidContext => {
                write!(f, "invalid context for call frame instruction")
            }
            Error::PopWithEmptyStack => {
                write!(f, "invalid DW_CFA_restore_state: empty stack")
            }
            Error::NoUnwindInfoForAddress => {
                write!(f, "no unwind info for address")
            }
            Error::UnsupportedOffset => {
                write!(f, "offset overflow")
            }
            Error::UnknownPointerEncoding(val) => {
                write!(f, "unknown pointer encoding: 0x{:x}", val.0)
            }
            Error::NoEntryAtGivenOffset(val) => write!(f, "no entry at offset: 0x{val:x}"),
            Error::OffsetOutOfBounds(val) => write!(f, "invalid offset: 0x{val:x}"),
            Error::UnknownAugmentation => write!(f, "unknown CFI augmentation"),
            Error::UnsupportedPointerEncoding(val) => {
                write!(f, "unsupported pointer encoding: 0x{:x}", val.0)
            }
            Error::UnsupportedIndirectPointer => {
                write!(f, "unsupported indirect pointer")
            }
            Error::UnsupportedRegister(val) => {
                write!(f, "unsupported register: 0x{val:x}")
            }
            Error::TooManyRegisterRules => {
                write!(f, "too many CFI register rules")
            }
            Error::StackFull => {
                write!(f, "CFI stack overflow")
            }
            Error::UnknownUnitType(val) => {
                write!(f, "unknown unit type: 0x{:x}", val.0)
            }
            Error::UnsupportedSegmentSize(val) => write!(f, "unsupported segment size: {val}"),
            Error::MissingUnitDie => {
                write!(f, "missing unit DIE")
            }
            Error::MissingSplitUnit => {
                write!(f, "missing split compilation unit")
            }
            Error::UnsupportedAttributeForm(val) => {
                write!(f, "unsupported attribute form: 0x{:x}", val.0)
            }
            Error::MissingFileEntryFormatPath => {
                write!(f, "missing file entry format path")
            }
            Error::ExpectedStringAttributeValue => {
                write!(f, "invalid attribute form for string")
            }
            Error::InvalidImplicitConst => {
                write!(f, "invalid indirect attribute form: DW_FORM_implicit_const")
            }
            Error::UnsupportedIndexSectionCount(val) => {
                write!(f, "unsupported DWP section count: {val}")
            }
            Error::InvalidIndexSlotCount(val) => write!(f, "invalid DWP slot count: 0x{:x}", val),
            Error::InvalidIndexRow(val) => write!(f, "invalid DWP row index: 0x{:x}", val),
            Error::UnknownIndexSection(val) => write!(f, "unknown DWP section type: 0x{:x}", val.0),
            Error::UnknownIndexSectionV2(val) => {
                write!(f, "unknown DWP v2 section type: 0x{:x}", val.0)
            }
            Error::InvalidMacinfoType(val) => write!(f, "unknown macinfo type: 0x{:x}", val.0),
            Error::InvalidMacroType(val) => write!(f, "unknown macro type: 0x{:x}", val.0),
            Error::UnsupportedOpcodeOperandsTable => {
                write!(f, "unsupported macro opcode operands table")
            }
            Error::InvalidNameAttributeIndex(val) => {
                write!(f, "invalid index in name attribute: 0x{val:x}")
            }
        }
    }
}

impl error::Error for Error {}

#[cfg(feature = "std")]
impl From<io::Error> for Error {
    fn from(_: io::Error) -> Self {
        Error::Io
    }
}

/// The result of a parse.
pub type Result<T> = result::Result<T, Error>;

/// A convenience trait for loading DWARF sections from object files.  To be
/// used like:
///
/// ```
/// use gimli::{DebugInfo, EndianSlice, LittleEndian, Reader, Section};
///
/// let buf = [0x00, 0x01, 0x02, 0x03];
/// let reader = EndianSlice::new(&buf, LittleEndian);
/// let loader = |name| -> Result<_, ()> { Ok(reader) };
///
/// let debug_info: DebugInfo<_> = Section::load(loader).unwrap();
/// ```
pub trait Section<R>: From<R> {
    /// Returns the section id for this type.
    fn id() -> SectionId;

    /// Returns the ELF section name for this type.
    fn section_name() -> &'static str {
        Self::id().name()
    }

    /// Returns the ELF section name (if any) for this type when used in a dwo
    /// file.
    fn dwo_section_name() -> Option<&'static str> {
        Self::id().dwo_name()
    }

    /// Returns the XCOFF section name (if any) for this type when used in a XCOFF
    /// file.
    fn xcoff_section_name() -> Option<&'static str> {
        Self::id().xcoff_name()
    }

    /// Try to load the section using the given loader function.
    fn load<F, E>(f: F) -> core::result::Result<Self, E>
    where
        F: FnOnce(SectionId) -> core::result::Result<R, E>,
    {
        f(Self::id()).map(From::from)
    }

    /// Returns the `Reader` for this section.
    fn reader(&self) -> &R
    where
        R: Reader;

    /// Returns the subrange of the section that is the contribution of
    /// a unit in a `.dwp` file.
    fn dwp_range(&self, offset: u32, size: u32) -> Result<Self>
    where
        R: Reader,
    {
        let mut data = self.reader().clone();
        data.skip(R::Offset::from_u32(offset))?;
        data.truncate(R::Offset::from_u32(size))?;
        Ok(data.into())
    }

    /// Returns the `Reader` for this section.
    fn lookup_offset_id(&self, id: ReaderOffsetId) -> Option<(SectionId, R::Offset)>
    where
        R: Reader,
    {
        self.reader()
            .lookup_offset_id(id)
            .map(|offset| (Self::id(), offset))
    }
}

impl Register {
    pub(crate) fn from_u64(x: u64) -> Result<Register> {
        let y = x as u16;
        if u64::from(y) == x {
            Ok(Register(y))
        } else {
            Err(Error::UnsupportedRegister(x))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::Format;
    use crate::endianity::LittleEndian;
    use test_assembler::{Endian, Section};

    #[test]
    fn test_parse_initial_length_32_ok() {
        let section = Section::with_endian(Endian::Little).L32(0x7856_3412);
        let buf = section.get_contents().unwrap();

        let input = &mut EndianSlice::new(&buf, LittleEndian);
        match input.read_initial_length() {
            Ok((length, format)) => {
                assert_eq!(input.len(), 0);
                assert_eq!(format, Format::Dwarf32);
                assert_eq!(0x7856_3412, length);
            }
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        }
    }

    #[test]
    fn test_parse_initial_length_64_ok() {
        let section = Section::with_endian(Endian::Little)
            // Dwarf_64_INITIAL_UNIT_LENGTH
            .L32(0xffff_ffff)
            // Actual length
            .L64(0xffde_bc9a_7856_3412);
        let buf = section.get_contents().unwrap();
        let input = &mut EndianSlice::new(&buf, LittleEndian);

        #[cfg(target_pointer_width = "64")]
        match input.read_initial_length() {
            Ok((length, format)) => {
                assert_eq!(input.len(), 0);
                assert_eq!(format, Format::Dwarf64);
                assert_eq!(0xffde_bc9a_7856_3412, length);
            }
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        }

        #[cfg(target_pointer_width = "32")]
        match input.read_initial_length() {
            Err(Error::UnsupportedOffset) => {}
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        };
    }

    #[test]
    fn test_parse_initial_length_unknown_reserved_value() {
        let section = Section::with_endian(Endian::Little).L32(0xffff_fffe);
        let buf = section.get_contents().unwrap();

        let input = &mut EndianSlice::new(&buf, LittleEndian);
        match input.read_initial_length() {
            Err(Error::UnknownReservedLength(0xffff_fffe)) => {}
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        };
    }

    #[test]
    fn test_parse_initial_length_incomplete() {
        let buf = [0xff, 0xff, 0xff]; // Need at least 4 bytes.

        let input = &mut EndianSlice::new(&buf, LittleEndian);
        match input.read_initial_length() {
            Err(Error::UnexpectedEof(_)) => {}
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        };
    }

    #[test]
    fn test_parse_initial_length_64_incomplete() {
        let section = Section::with_endian(Endian::Little)
            // Dwarf_64_INITIAL_UNIT_LENGTH
            .L32(0xffff_ffff)
            // Actual length is not long enough.
            .L32(0x7856_3412);
        let buf = section.get_contents().unwrap();

        let input = &mut EndianSlice::new(&buf, LittleEndian);
        match input.read_initial_length() {
            Err(Error::UnexpectedEof(_)) => {}
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        };
    }

    #[test]
    fn test_parse_offset_32() {
        let section = Section::with_endian(Endian::Little).L32(0x0123_4567);
        let buf = section.get_contents().unwrap();

        let input = &mut EndianSlice::new(&buf, LittleEndian);
        match input.read_offset(Format::Dwarf32) {
            Ok(val) => {
                assert_eq!(input.len(), 0);
                assert_eq!(val, 0x0123_4567);
            }
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        };
    }

    #[test]
    fn test_parse_offset_64_small() {
        let section = Section::with_endian(Endian::Little).L64(0x0123_4567);
        let buf = section.get_contents().unwrap();

        let input = &mut EndianSlice::new(&buf, LittleEndian);
        match input.read_offset(Format::Dwarf64) {
            Ok(val) => {
                assert_eq!(input.len(), 0);
                assert_eq!(val, 0x0123_4567);
            }
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        };
    }

    #[test]
    #[cfg(target_pointer_width = "64")]
    fn test_parse_offset_64_large() {
        let section = Section::with_endian(Endian::Little).L64(0x0123_4567_89ab_cdef);
        let buf = section.get_contents().unwrap();

        let input = &mut EndianSlice::new(&buf, LittleEndian);
        match input.read_offset(Format::Dwarf64) {
            Ok(val) => {
                assert_eq!(input.len(), 0);
                assert_eq!(val, 0x0123_4567_89ab_cdef);
            }
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        };
    }

    #[test]
    #[cfg(target_pointer_width = "32")]
    fn test_parse_offset_64_large() {
        let section = Section::with_endian(Endian::Little).L64(0x0123_4567_89ab_cdef);
        let buf = section.get_contents().unwrap();

        let input = &mut EndianSlice::new(&buf, LittleEndian);
        match input.read_offset(Format::Dwarf64) {
            Err(Error::UnsupportedOffset) => {}
            otherwise => panic!("Unexpected result: {:?}", otherwise),
        };
    }
}
