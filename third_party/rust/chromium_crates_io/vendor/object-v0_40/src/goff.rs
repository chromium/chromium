//! GOFF definitions
//!
//! These definitions are independent of read/write support, although we do implement
//! some traits useful for those.
//!
//! This module is based of the official documentation for z/OS <https://www.ibm.com/docs/en/zos/3.2.0?topic=goff-record-formats>

#![allow(missing_docs)]

use crate::endian::{BigEndian as BE, U16, U32};
use crate::pod::Pod;

/// The module header ("HDR") record at the start of every 64-bit GOFF file.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct HeaderRecord64 {
    /// Type of record. Must be 0x03F000.
    pub ptv: [u8; 3],
    /// Reserved. Must be 45 bytes of 0.
    pub reserved1: [u8; 45],
    /// Architecture Level. Must be 1.
    pub archlvl: U32<BE>,
    /// Reserved. Must be 28 bytes of 0.
    pub reserved2: [u8; 28],
}

/// ESD data has 8 bytes till the end of the record
/// (longer symbol names are finished in continuation records)
pub const SIZEOF_ESD_DATA: usize = 8;

/// The external symbol definition ("ESD") record.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SymbolRecord64 {
    /// Type of record. Must be 0x030000 or 0x030100.
    pub ptv: [u8; 3],
    /// Symbol Type.
    pub symboltype: SymbolType,
    /// ESD Identifier (ESDID).
    pub esdid: U32<BE>,
    /// Parent of Owning ESDID
    pub parentesdid: U32<BE>,
    /// Reserved. Must be 4 bytes of 0.
    pub reserved1: U32<BE>,
    /// Offset.
    pub offset: U32<BE>,
    /// Reserved. Must be 4 bytes of 0.
    pub reserved2: U32<BE>,
    /// Length
    pub length: U32<BE>,
    /// Extended Attribute ESDID
    pub eaesdid: U32<BE>,
    /// Extended Attribute Data Offset
    pub eadataoffset: U32<BE>,
    /// Reserved. Must be 4 bytes of 0.
    pub reserved3: U32<BE>,
    /// Name Space ID
    pub namespaceid: u8,
    /// Symbol Flags.
    pub symflags: u8,
    /// Fill Byte Value.
    pub fillbytevalue: u8,
    /// Reserved. Must be 1 bytes of 0.
    pub reserved4: u8,
    /// Associated data ID
    pub adaesdid: U32<BE>,
    /// Priority
    pub priority: U32<BE>,
    /// Reserved. Must be 8 bytes of 0.
    pub reserved5: [u8; 8],
    /// Behavioral Attributes
    pub behavioralattributes: [u8; 10],
    /// Name Length
    pub namelength: U16<BE>,
    /// Name
    pub name: [u8; SIZEOF_ESD_DATA],
}

/// TXT data has 56 bytes till the end of the record,
/// can be finished in a continuation record
pub const SIZEOF_TXT_DATA: usize = 56;

/// The text ("TXT") record.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct TextRecord64 {
    /// Type of record. Must be 0x031000 or 0x031100.
    pub ptv: [u8; 3],
    /// Text Record Style
    pub recordstyle: u8,
    /// Element ESDID
    pub elementesdid: U32<BE>,
    /// Reserved. Must be 4 bytes of 0.
    pub reserved1: U32<BE>,
    /// Offset.
    pub offset: U32<BE>,
    /// Text Field True Length
    pub truelength: U32<BE>,
    /// Text Encoding
    pub textencoding: U16<BE>,
    /// Data Length
    pub datalength: U16<BE>,
    /// Data
    pub data: [u8; SIZEOF_TXT_DATA],
}

/// The relocation directory ("RLD") record.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct RelocationRecord64 {
    /// Type of record. Must be 0x032000 or 0x032100.
    pub ptv: [u8; 3],
    /// Reserved. Must be 1 byte of 0.
    pub reserved: u8,
    /// Length.
    pub length: U16<BE>,
    /// Relocation Data
    pub data: [u8; 74],
}

/// Each continuation record will have a payload of 77 bytes
pub const SIZEOF_CONTINUATION_RECORD_DATA: usize = 77;

/// The generic continuation record.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ContinuationRecord64 {
    /// Type of record.
    pub ptv: [u8; 3],
    pub data: [u8; SIZEOF_CONTINUATION_RECORD_DATA],
}

/// The module end ("END") record at the end of every 64-bit GOFF file.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct EndRecord64 {
    /// Type of record. Must be 0x034000.
    pub ptv: [u8; 3],
    /// Flags.  Upper 6 bits are reserved to 0
    pub flags: u8,
    /// AMODE.
    pub amode: u8,
    /// Reserved. Must be 3 bytes of 0.
    pub reserved1: [u8; 3],
    /// Record Count.
    pub recordcnt: U32<BE>,
    /// ESDID
    pub esdid: U32<BE>,
    /// Reserved. Must be 64 bytes of 0.
    pub reserved2: [u8; 64],
}

/// Only fixed length records are supported (UNIX compatible file
/// systems only support fixed length records). Each record is exactly
/// 80 bytes, unused space must be padded
pub const RECORD_LEN: u64 = 80;

newtype!(
    /// Values for `FileEnd64::flags`.
    ///
    /// The lower 2 bits indicate entry point presence/type.
    /// Upper 6 bits are reserved and must be 0.
    struct FileFlags(u8);
);

newtype_flag_names!(NAMES_F_FLAGS: FileFlags(u8) = {
    /// No entry point is suggested or requested.
    /// No subsequent fields (other than Record Count) are valid.
    F_NO_ENTRY_POINT = 0x00,
    /// Entry point requested by internal offset and ESDID.
    /// ESDID can be EDID (within module) or ERID (external reference).
    F_ENTRY_BY_OFFSET = 0x01,
    /// Entry point requested by external name.
    /// ESDID and Offset fields must be zero.
    F_ENTRY_BY_NAME = 0x02,
    /// Reserved value.
    F_ENTRY_RESERVED = 0x03,
});

/// Mask for the entry point indicator bits (lower 2 bits)
pub const F_ENTRY_MASK: u8 = 0x03;

newtype!(
    /// GOFF record type values.
    ///
    /// These appear in the second byte of the `ptv` field (bits masked with 0xFC).
    struct RecordType(u8);
);

newtype_flag_names!(NAMES_RECORD_TYPE: RecordType(u8) = {
    /// External Symbol Dictionary record.
    RT_ESD = 0x00,
    /// Text (code/data) record.
    RT_TXT = 0x10,
    /// Relocation Dictionary record.
    RT_RLD = 0x20,
    /// Length record (continuation).
    RT_LEN = 0x30,
    /// End of module record.
    RT_END = 0x40,
    /// Module header record.
    RT_HDR = 0xF0,
});

// Values for `ptv`, the GOFF Record prefix
//
/// GOFF Record Prefix (every GOFF file begins with this)
pub const GOFF_PREFIX: u8 = 0x03;
/// GOFF Version (only supported version)
pub const GOFF_VERSION: u8 = 0x00;
/// The GOFF HDR Record Magic Number
pub const GOFF_HDR_BYTES: [u8; 3] = [GOFF_PREFIX, RT_HDR.0, GOFF_VERSION];
/// The GOFF ESD Record Magic Number
pub const GOFF_ESD_BYTES: [u8; 3] = [GOFF_PREFIX, RT_ESD.0, GOFF_VERSION];
/// The GOFF TXT Record Magic Number
pub const GOFF_TXT_BYTES: [u8; 3] = [GOFF_PREFIX, RT_TXT.0, GOFF_VERSION];
/// The GOFF RLD Record Magic Number
pub const GOFF_RLD_BYTES: [u8; 3] = [GOFF_PREFIX, RT_RLD.0, GOFF_VERSION];
/// The GOFF LEN Record Magic Number
pub const GOFF_LEN_BYTES: [u8; 3] = [GOFF_PREFIX, RT_LEN.0, GOFF_VERSION];
/// The GOFF END Record Magic Number
pub const GOFF_END_BYTES: [u8; 3] = [GOFF_PREFIX, RT_END.0, GOFF_VERSION];

newtype!(
    /// GOFF symbol type values from ESD records.
    struct SymbolType(u8);
);

newtype_flag_names!(NAMES_SYMBOL_TYPE: SymbolType(u8) = {
    /// Section Definition (SD) - defines a control section.
    ESD_SYMTYPE_SD = 0,
    /// Element Definition (ED) - defines an element (part/class).
    ESD_SYMTYPE_ED = 1,
    /// Label Definition (LD) - defines a label within a section.
    ESD_SYMTYPE_LD = 2,
    /// Part Reference (PR) - references a part of an element.
    ESD_SYMTYPE_PR = 3,
    /// External Reference (ER) - references an external symbol.
    ESD_SYMTYPE_ER = 4,
});

newtype!(
    /// ESD Namespace
    struct EsdNameSpace(u8);
);

newtype_flag_names!(NAMES_ESD_NAMSPACE: EsdNameSpace(u8) = {
    ESD_NS_PROGRAM_MANAGEMENT_BINDER = 0,
    ESD_NS_NORMAL_NAME = 1,
    ESD_NS_PSEUDO_REGISTER = 2,
    ESD_NS_PARTS = 3,
});

newtype!(
    /// TXT Record Style
    struct TxtRecordStyle(u8);
);

newtype_flag_names!(NAMES_TXT_RECORD: TxtRecordStyle(u8) = {
    TXT_RS_BYTE = 0,
    TXT_RS_STRUCTURED = 1,
    TXT_RS_UNSTRUCTURED = 2,
});

unsafe_impl_pod!(
    HeaderRecord64,
    SymbolRecord64,
    TextRecord64,
    RelocationRecord64,
    ContinuationRecord64,
    EndRecord64,
);
