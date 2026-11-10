//! Interface for reading object files.
//!
//! ## Unified read API
//!
//! The [`Object`] trait provides a unified read API for accessing common features of
//! object files, such as sections and symbols. There is an implementation of this
//! trait for [`File`], which allows reading any file format, as well as implementations
//! for each file format:
//! [`ElfFile`](elf::ElfFile), [`MachOFile`](macho::MachOFile), [`CoffFile`](coff::CoffFile),
//! [`PeFile`](pe::PeFile), [`WasmFile`](wasm::WasmFile), [`XcoffFile`](xcoff::XcoffFile).
//!
//! ## Low level read API
//!
//! The submodules for each file format define helpers that operate on the raw structs.
//! These can be used instead of the unified API, or in conjunction with it to access
//! details that are not available via the unified API.
//!
//! See the [submodules](#modules) for examples of the low level read API.
//!
//! ## Naming Convention
//!
//! Types that form part of the unified API for a file format are prefixed with the
//! name of the file format.
//!
//! ## Example for unified read API
//!  ```no_run
//! use object::{Object, ObjectSection};
//! use std::error::Error;
//! use std::fs;
//!
//! /// Reads a file and displays the name of each section.
//! fn main() -> Result<(), Box<dyn Error>> {
//! #   #[cfg(all(feature = "read", feature = "std"))] {
//!     let data = fs::read("path/to/binary")?;
//!     let file = object::File::parse(&*data)?;
//!     for section in file.sections() {
//!         println!("{}", section.name()?);
//!     }
//! #   }
//!     Ok(())
//! }
//! ```

use alloc::borrow::Cow;
use core::{fmt, result};

#[cfg(not(feature = "std"))]
use alloc::collections::btree_map::BTreeMap as Map;
#[cfg(feature = "std")]
use std::collections::hash_map::HashMap as Map;

pub use crate::common::*;

mod read_ref;
pub use read_ref::*;

mod read_cache;
pub use read_cache::*;

mod symbol_map;
pub use symbol_map::*;

mod util;
pub use util::*;

#[cfg(any(feature = "elf", feature = "macho"))]
mod gnu_compression;

#[cfg(any(
    feature = "coff",
    feature = "elf",
    feature = "macho",
    feature = "pe",
    feature = "wasm",
    feature = "xcoff"
))]
mod any;
#[cfg(any(
    feature = "coff",
    feature = "elf",
    feature = "macho",
    feature = "pe",
    feature = "wasm",
    feature = "xcoff"
))]
pub use any::*;

#[cfg(feature = "archive")]
pub mod archive;

#[cfg(feature = "coff")]
pub mod coff;

#[cfg(feature = "elf")]
pub mod elf;

#[cfg(feature = "macho")]
pub mod macho;

#[cfg(feature = "pe")]
pub mod pe;

#[cfg(feature = "wasm")]
pub mod wasm;

#[cfg(feature = "xcoff")]
pub mod xcoff;

mod traits;
pub use traits::*;

mod private {
    pub trait Sealed {}
}

/// The error type used within the read module.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Error(pub(crate) &'static str);

impl fmt::Display for Error {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.0)
    }
}

impl core::error::Error for Error {}

/// The result type used within the read module.
pub type Result<T> = result::Result<T, Error>;

trait ReadError<T> {
    fn read_error(self, error: &'static str) -> Result<T>;
}

impl<T> ReadError<T> for result::Result<T, ()> {
    fn read_error(self, error: &'static str) -> Result<T> {
        self.map_err(|()| Error(error))
    }
}

impl<T> ReadError<T> for result::Result<T, Error> {
    fn read_error(self, error: &'static str) -> Result<T> {
        self.map_err(|_| Error(error))
    }
}

impl<T> ReadError<T> for Option<T> {
    fn read_error(self, error: &'static str) -> Result<T> {
        self.ok_or(Error(error))
    }
}

/// The native executable file for the target platform.
#[cfg(all(
    unix,
    not(target_vendor = "apple"),
    not(target_os = "aix"),
    feature = "elf"
))]
pub type NativeFile<'data, R = &'data [u8]> = elf::NativeElfFile<'data, R>;

/// The native executable file for the target platform.
#[cfg(all(target_vendor = "apple", feature = "macho"))]
pub type NativeFile<'data, R = &'data [u8]> = macho::NativeMachOFile<'data, R>;

/// The native executable file for the target platform.
#[cfg(all(target_os = "windows", feature = "pe"))]
pub type NativeFile<'data, R = &'data [u8]> = pe::NativePeFile<'data, R>;

/// The native executable file for the target platform.
#[cfg(all(target_family = "wasm", feature = "wasm"))]
pub type NativeFile<'data, R = &'data [u8]> = wasm::WasmFile<'data, R>;

/// The native executable file for the target platform.
#[cfg(all(target_os = "aix", feature = "xcoff"))]
pub type NativeFile<'data, R = &'data [u8]> = xcoff::NativeXcoffFile<'data, R>;

/// A file format kind.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum FileKind {
    /// A Unix archive.
    ///
    /// See [`archive::ArchiveFile`].
    #[cfg(feature = "archive")]
    Archive,
    /// A COFF object file.
    ///
    /// See [`coff::CoffFile`].
    #[cfg(feature = "coff")]
    Coff,
    /// A COFF bigobj object file.
    ///
    /// This supports a larger number of sections.
    ///
    /// See [`coff::CoffBigFile`].
    #[cfg(feature = "coff")]
    CoffBig,
    /// A Windows short import file.
    ///
    /// See [`coff::ImportFile`].
    #[cfg(feature = "coff")]
    CoffImport,
    /// A dyld cache file containing Mach-O images.
    ///
    /// See [`macho::DyldCache`]
    #[cfg(feature = "macho")]
    DyldCache,
    /// A 32-bit ELF file.
    ///
    /// See [`elf::ElfFile32`].
    #[cfg(feature = "elf")]
    Elf32,
    /// A 64-bit ELF file.
    ///
    /// See [`elf::ElfFile64`].
    #[cfg(feature = "elf")]
    Elf64,
    /// A 32-bit Mach-O file.
    ///
    /// See [`macho::MachOFile32`].
    #[cfg(feature = "macho")]
    MachO32,
    /// A 64-bit Mach-O file.
    ///
    /// See [`macho::MachOFile64`].
    #[cfg(feature = "macho")]
    MachO64,
    /// A 32-bit Mach-O fat binary.
    ///
    /// See [`macho::MachOFatFile32`].
    #[cfg(feature = "macho")]
    MachOFat32,
    /// A 64-bit Mach-O fat binary.
    ///
    /// See [`macho::MachOFatFile64`].
    #[cfg(feature = "macho")]
    MachOFat64,
    /// A 32-bit PE file.
    ///
    /// See [`pe::PeFile32`].
    #[cfg(feature = "pe")]
    Pe32,
    /// A 64-bit PE file.
    ///
    /// See [`pe::PeFile64`].
    #[cfg(feature = "pe")]
    Pe64,
    /// A Wasm file.
    ///
    /// See [`wasm::WasmFile`].
    #[cfg(feature = "wasm")]
    Wasm,
    /// A 32-bit XCOFF file.
    ///
    /// See [`xcoff::XcoffFile32`].
    #[cfg(feature = "xcoff")]
    Xcoff32,
    /// A 64-bit XCOFF file.
    ///
    /// See [`xcoff::XcoffFile64`].
    #[cfg(feature = "xcoff")]
    Xcoff64,
}

impl FileKind {
    /// Determine a file kind by parsing the start of the file.
    pub fn parse<'data, R: ReadRef<'data>>(data: R) -> Result<FileKind> {
        Self::parse_at(data, 0)
    }

    /// Determine a file kind by parsing at the given offset.
    pub fn parse_at<'data, R: ReadRef<'data>>(data: R, offset: u64) -> Result<FileKind> {
        let magic = data
            .read_bytes_at(offset, 16)
            .read_error("Could not read file magic")?;
        if magic.len() < 16 {
            return Err(Error("File too short"));
        }

        let kind = match [magic[0], magic[1], magic[2], magic[3], magic[4], magic[5], magic[6], magic[7]] {
            #[cfg(feature = "archive")]
            [b'!', b'<', b'a', b'r', b'c', b'h', b'>', b'\n']
            | [b'!', b'<', b't', b'h', b'i', b'n', b'>', b'\n'] => FileKind::Archive,
            #[cfg(feature = "macho")]
            [b'd', b'y', b'l', b'd', b'_', b'v', b'1', b' '] => FileKind::DyldCache,
            #[cfg(feature = "elf")]
            [0x7f, b'E', b'L', b'F', 1, ..] => FileKind::Elf32,
            #[cfg(feature = "elf")]
            [0x7f, b'E', b'L', b'F', 2, ..] => FileKind::Elf64,
            #[cfg(feature = "macho")]
            [0xfe, 0xed, 0xfa, 0xce, ..]
            | [0xce, 0xfa, 0xed, 0xfe, ..] => FileKind::MachO32,
            #[cfg(feature = "macho")]
            | [0xfe, 0xed, 0xfa, 0xcf, ..]
            | [0xcf, 0xfa, 0xed, 0xfe, ..] => FileKind::MachO64,
            #[cfg(feature = "macho")]
            [0xca, 0xfe, 0xba, 0xbe, ..] => FileKind::MachOFat32,
            #[cfg(feature = "macho")]
            [0xca, 0xfe, 0xba, 0xbf, ..] => FileKind::MachOFat64,
            #[cfg(feature = "wasm")]
            [0x00, b'a', b's', b'm', _, _, 0x00, 0x00] => FileKind::Wasm,
            #[cfg(feature = "pe")]
            [b'M', b'Z', ..] if offset == 0 => {
                // offset == 0 restriction is because optional_header_magic only looks at offset 0
                match pe::optional_header_magic(data) {
                    Ok(crate::pe::IMAGE_NT_OPTIONAL_HDR32_MAGIC) => {
                        FileKind::Pe32
                    }
                    Ok(crate::pe::IMAGE_NT_OPTIONAL_HDR64_MAGIC) => {
                        FileKind::Pe64
                    }
                    _ => return Err(Error("Unknown MS-DOS file")),
                }
            }
            // TODO: more COFF machines
            #[cfg(feature = "coff")]
            // COFF arm
            [0xc4, 0x01, ..]
            // COFF arm64
            | [0x64, 0xaa, ..]
            // COFF arm64ec
            | [0x41, 0xa6, ..]
            // COFF ppc
            | [0xf0, 0x01, ..]
            | [0xf1, 0x01, ..]
            | [0xf2, 0x01, ..]
            // COFF x86
            | [0x4c, 0x01, ..]
            // COFF x86-64
            | [0x64, 0x86, ..] => FileKind::Coff,
            #[cfg(feature = "coff")]
            [0x00, 0x00, 0xff, 0xff, 0x00, 0x00, ..] => FileKind::CoffImport,
            #[cfg(feature = "coff")]
            [0x00, 0x00, 0xff, 0xff, 0x02, 0x00, ..] if offset == 0 => {
                // offset == 0 restriction is because anon_object_class_id only looks at offset 0
                match coff::anon_object_class_id(data) {
                    Ok(crate::pe::ANON_OBJECT_HEADER_BIGOBJ_CLASS_ID) => FileKind::CoffBig,
                    _ => return Err(Error("Unknown anon object file")),
                }
            }
            #[cfg(feature = "xcoff")]
            [0x01, 0xdf, ..] => FileKind::Xcoff32,
            #[cfg(feature = "xcoff")]
            [0x01, 0xf7, ..] => FileKind::Xcoff64,
            _ => return Err(Error("Unknown file magic")),
        };
        Ok(kind)
    }
}

/// An object kind.
///
/// Returned by [`Object::kind`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum ObjectKind {
    /// The object kind is unknown.
    Unknown,
    /// Relocatable object.
    Relocatable,
    /// Executable.
    Executable,
    /// Dynamic shared object.
    Dynamic,
    /// Core.
    Core,
}

/// The index used to identify a section in a file.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SectionIndex(pub usize);

impl fmt::Display for SectionIndex {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

/// The index used to identify a symbol in a symbol table.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SymbolIndex(pub usize);

impl fmt::Display for SymbolIndex {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

/// The section where an [`ObjectSymbol`] is defined.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum SymbolSection {
    /// The section is unknown.
    Unknown,
    /// The section is not applicable for this symbol (such as file symbols).
    None,
    /// The symbol is undefined.
    Undefined,
    /// The symbol has an absolute value.
    Absolute,
    /// The symbol is a zero-initialized symbol that will be combined with duplicate definitions.
    Common,
    /// The symbol is defined in the given section.
    Section(SectionIndex),
}

impl SymbolSection {
    /// Returns the section index for the section where the symbol is defined.
    ///
    /// May return `None` if the symbol is not defined in a section.
    #[inline]
    pub fn index(self) -> Option<SectionIndex> {
        if let SymbolSection::Section(index) = self {
            Some(index)
        } else {
            None
        }
    }
}

/// An imported symbol.
///
/// Returned by [`Object::imports`].
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Import<'data> {
    library: &'data [u8],
    name: NameOrOrdinal<&'data [u8]>,
    weak: bool,
    flags: ImportFlags<'data>,
}

impl<'data> fmt::Debug for Import<'data> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut s = f.debug_struct("Import");
        s.field("library", &ByteString(self.library));
        match &self.name {
            NameOrOrdinal::Name(name) => s.field("name", &ByteString(name)),
            NameOrOrdinal::Ordinal(ordinal) => s.field("ordinal", ordinal),
        };
        if self.weak {
            s.field("weak", &self.weak);
        }
        s.field("flags", &self.flags);
        s.finish()
    }
}

impl<'data> Import<'data> {
    /// The name of the library to import the symbol from.
    ///
    /// This is empty if the library name is not specified.
    ///
    /// For Mach-O, this will also be empty for a special library ordinal,
    /// which can be obtained from the `n_desc` or `dylib` field in the flags.
    #[inline]
    pub fn library(&self) -> &'data [u8] {
        self.library
    }

    /// The name or ordinal of the symbol to import.
    #[inline]
    pub fn name(&self) -> NameOrOrdinal<&'data [u8]> {
        self.name
    }

    /// Return true if the import is a weak reference.
    #[inline]
    pub fn is_weak(&self) -> bool {
        self.weak
    }

    /// The format-specific flags for the imported symbol.
    #[inline]
    pub fn flags(&self) -> ImportFlags<'data> {
        self.flags
    }
}

/// Import flags that are specific to each file format.
#[derive(Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum ImportFlags<'data> {
    /// No import flags.
    None,
    /// ELF import flags.
    #[cfg(feature = "elf")]
    Elf {
        /// `st_info` field in the ELF symbol (binding and type).
        st_info: crate::elf::SymbolInfo,
        /// `st_other` field in the ELF symbol (visibility).
        st_other: crate::elf::SymbolOther,
        /// The GNU symbol version that is required.
        version: Option<&'data [u8]>,
    },
    /// Mach-O import flags.
    #[cfg(feature = "macho")]
    MachO {
        /// `n_type` field in the nlist symbol.
        n_type: crate::macho::SymbolFlags,
        /// `n_desc` field in the nlist symbol.
        ///
        /// For a file using `MH_TWOLEVEL`, this contains the library ordinal.
        n_desc: crate::macho::SymbolDesc,
    },
    /// Mach-O dynamic linker import flags.
    ///
    /// Used for imports that are obtained from `LC_DYLD_CHAINED_FIXUPS` or `LC_DYLD_INFO`.
    #[cfg(feature = "macho")]
    MachOBind {
        /// The library ordinal that the symbol is imported from.
        dylib: crate::macho::BindDylib,
        /// The bind symbol flags.
        flags: crate::macho::BindSymbolFlags,
    },
    /// PE import flags.
    #[cfg(feature = "pe")]
    Pe {
        /// The symbol is from the delay-load import table.
        delay: bool,
    },
    #[doc(hidden)]
    #[cfg(not(feature = "elf"))]
    _Phantom(
        core::marker::PhantomData<&'data ()>,
        core::convert::Infallible,
    ),
}

impl<'data> fmt::Debug for ImportFlags<'data> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ImportFlags::None => f.write_str("None"),
            #[cfg(feature = "elf")]
            ImportFlags::Elf {
                st_info,
                st_other,
                version,
            } => {
                let mut s = f.debug_struct("Elf");
                s.field("st_info", st_info);
                s.field("st_other", st_other);
                if let Some(version) = version {
                    s.field("version", &ByteString(version));
                }
                s.finish()
            }
            #[cfg(feature = "macho")]
            ImportFlags::MachO { n_type, n_desc } => f
                .debug_struct("MachO")
                .field("n_type", n_type)
                .field("n_desc", n_desc)
                .finish(),
            #[cfg(feature = "macho")]
            ImportFlags::MachOBind { dylib, flags } => f
                .debug_struct("MachOBind")
                .field("dylib", dylib)
                .field("flags", flags)
                .finish(),
            #[cfg(feature = "pe")]
            ImportFlags::Pe { delay } => {
                let mut s = f.debug_struct("Pe");
                if *delay {
                    s.field("delay", delay);
                }
                s.finish()
            }
            #[cfg(not(feature = "elf"))]
            ImportFlags::_Phantom(_, i) => match *i {},
        }
    }
}

/// A library that the file depends on.
///
/// Returned by [`Object::import_libraries`].
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct ImportLibrary<'data> {
    name: &'data [u8],
    flags: ImportLibraryFlags,
}

impl<'data> fmt::Debug for ImportLibrary<'data> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ImportLibrary")
            .field("name", &ByteString(self.name))
            .field("flags", &self.flags)
            .finish()
    }
}

impl<'data> ImportLibrary<'data> {
    /// The name of the library.
    #[inline]
    pub fn name(&self) -> &'data [u8] {
        self.name
    }

    /// The format-specific flags for the library.
    #[inline]
    pub fn flags(&self) -> ImportLibraryFlags {
        self.flags
    }
}

/// Import library flags that are specific to each file format.
#[derive(Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum ImportLibraryFlags {
    /// No import library flags.
    ///
    /// Used for ELF, which has no additional information for `DT_NEEDED` entries.
    None,
    /// Mach-O import library flags.
    #[cfg(feature = "macho")]
    MachO {
        /// The 1-based index that is used to refer to this library.
        ordinal: u32,
        /// The type of the dylib load command.
        ///
        /// One of `LC_LOAD_DYLIB`, `LC_LOAD_WEAK_DYLIB`, `LC_REEXPORT_DYLIB`,
        /// `LC_LAZY_LOAD_DYLIB`, or `LC_LOAD_UPWARD_DYLIB`.
        cmd: crate::macho::LoadCommandType,
        /// The `current_version` field in the dylib.
        current_version: crate::macho::Version,
        /// The `compatibility_version` field in the dylib.
        compatibility_version: crate::macho::Version,
        /// The dylib use flags.
        use_flags: Option<crate::macho::DylibUseFlags>,
    },
    /// PE import library flags.
    #[cfg(feature = "pe")]
    Pe {
        /// The library is from the delay-load import table.
        delay: bool,
    },
}

impl fmt::Debug for ImportLibraryFlags {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ImportLibraryFlags::None => f.write_str("None"),
            #[cfg(feature = "macho")]
            ImportLibraryFlags::MachO {
                ordinal,
                cmd,
                current_version,
                compatibility_version,
                use_flags,
            } => {
                let mut s = f.debug_struct("MachO");
                s.field("ordinal", ordinal);
                s.field("cmd", cmd);
                s.field("current_version", current_version);
                s.field("compatibility_version", compatibility_version);
                if let Some(use_flags) = use_flags {
                    s.field("use_flags", use_flags);
                }
                s.finish()
            }
            #[cfg(feature = "pe")]
            ImportLibraryFlags::Pe { delay } => {
                let mut s = f.debug_struct("Pe");
                if *delay {
                    s.field("delay", delay);
                }
                s.finish()
            }
        }
    }
}

/// An exported symbol.
///
/// Returned by [`Object::exports`].
#[derive(Clone, PartialEq, Eq)]
pub struct Export<'data> {
    name: NameOrOrdinal<Cow<'data, [u8]>>,
    target: ExportTarget<'data>,
    weak: bool,
    flags: ExportFlags<'data>,
}

impl<'data> fmt::Debug for Export<'data> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut s = f.debug_struct("Export");
        match &self.name {
            NameOrOrdinal::Name(name) => s.field("name", &ByteString(name)),
            NameOrOrdinal::Ordinal(ordinal) => s.field("ordinal", ordinal),
        };
        s.field("target", &self.target());
        if self.weak {
            s.field("weak", &self.weak);
        }
        s.field("flags", &self.flags);
        s.finish()
    }
}

impl<'data> Export<'data> {
    /// The name or ordinal that the symbol is exported as.
    #[inline]
    pub fn name(&self) -> NameOrOrdinal<&[u8]> {
        self.name.as_ref()
    }

    /// Consume the export and return the name.
    ///
    /// Use this to avoid copying the name when it is owned.
    #[inline]
    pub fn into_name(self) -> NameOrOrdinal<Cow<'data, [u8]>> {
        self.name
    }

    /// The target of the export.
    #[inline]
    pub fn target(&self) -> ExportTarget<'_> {
        match self.target {
            // Handle empty name for EXPORT_SYMBOL_FLAGS_REEXPORT in Mach-O exports trie.
            ExportTarget::Reexport {
                library,
                name: NameOrOrdinal::Name(b""),
            } => ExportTarget::Reexport {
                library,
                name: self.name.as_ref(),
            },
            target => target,
        }
    }

    /// Return true if the export is a weak definition.
    #[inline]
    pub fn is_weak(&self) -> bool {
        self.weak
    }

    /// The format-specific flags for the exported symbol.
    #[inline]
    pub fn flags(&self) -> ExportFlags<'data> {
        self.flags
    }
}

/// The target of an [`Export`].
#[derive(Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum ExportTarget<'data> {
    /// A regular export.
    Address {
        /// The address to export.
        address: u64,
    },
    /// A symbol with an absolute value.
    ///
    /// The value is not an address in the image.
    Absolute {
        /// The value of the symbol.
        value: u64,
    },
    /// A symbol in thread local storage.
    ///
    /// This is used for ELF `STT_TLS`.
    Tls {
        /// The offset of the symbol in the TLS block for this module.
        offset: u64,
    },
    /// A symbol in thread local storage that is accessed using a descriptor.
    ///
    /// This is used for Mach-O.
    TlvDescriptor {
        /// The virtual address of the thread local variable descriptor.
        ///
        /// The descriptor contains a function that must be called to obtain the
        /// address of the symbol for the current thread.
        address: u64,
    },
    /// An address that is determined by a resolver function.
    ///
    /// This is used for Mach-O, and for ELF `STT_GNU_IFUNC`.
    Resolver {
        /// The resolver function used to determine the target address.
        resolver: u64,
        /// The address of the stub to export, which will contain a call to the resolved address.
        ///
        /// For Mach-O, this is obtained from the exports trie.
        ///
        /// ELF may have a corresponding PLT entry, but we don't attempt to determine it.
        stub: Option<u64>,
    },
    /// A symbol from an external library which is reexported.
    ///
    /// This is used for PE and Mach-O.
    Reexport {
        /// The name of the library.
        ///
        /// This is empty if the library is not specified.
        library: &'data [u8],
        /// The name or ordinal of the symbol in the external library.
        name: NameOrOrdinal<&'data [u8]>,
    },
}

impl<'data> fmt::Debug for ExportTarget<'data> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ExportTarget::Address { address } => {
                f.debug_struct("Address").field("address", address).finish()
            }
            ExportTarget::Absolute { value } => {
                f.debug_struct("Absolute").field("value", value).finish()
            }
            ExportTarget::Tls { offset } => f.debug_struct("Tls").field("offset", offset).finish(),
            ExportTarget::TlvDescriptor { address } => f
                .debug_struct("TlvDescriptor")
                .field("address", address)
                .finish(),
            ExportTarget::Resolver { resolver, stub } => f
                .debug_struct("Resolver")
                .field("resolver", resolver)
                .field("stub", stub)
                .finish(),
            ExportTarget::Reexport { library, name } => {
                let mut s = f.debug_struct("Reexport");
                s.field("library", &ByteString(library));
                match name {
                    NameOrOrdinal::Name(name) => s.field("name", &ByteString(name)),
                    NameOrOrdinal::Ordinal(ordinal) => s.field("ordinal", ordinal),
                };
                s.finish()
            }
        }
    }
}

/// Export flags that are specific to each file format.
#[derive(Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum ExportFlags<'data> {
    /// No export flags.
    None,
    /// ELF export flags.
    #[cfg(feature = "elf")]
    Elf {
        /// `st_info` field in the ELF symbol (binding and type).
        st_info: crate::elf::SymbolInfo,
        /// `st_other` field in the ELF symbol (visibility).
        st_other: crate::elf::SymbolOther,
        /// The GNU symbol version.
        version: Option<&'data [u8]>,
        /// Whether the `VERSYM_HIDDEN` bit is set.
        version_hidden: bool,
    },
    /// Mach-O export flags.
    ///
    /// This is only used for exports from the symbol table.
    #[cfg(feature = "macho")]
    MachO {
        /// `n_type` field in the nlist symbol.
        n_type: crate::macho::SymbolFlags,
        /// `n_desc` field in the nlist symbol.
        n_desc: crate::macho::SymbolDesc,
    },
    /// PE export flags.
    #[cfg(feature = "pe")]
    Pe {
        /// The export ordinal.
        ordinal: crate::read::pe::ExportOrdinal,
    },
    #[doc(hidden)]
    #[cfg(not(feature = "elf"))]
    _Phantom(
        core::marker::PhantomData<&'data ()>,
        core::convert::Infallible,
    ),
}

impl<'data> fmt::Debug for ExportFlags<'data> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ExportFlags::None => f.write_str("None"),
            #[cfg(feature = "elf")]
            ExportFlags::Elf {
                st_info,
                st_other,
                version,
                version_hidden,
            } => {
                let mut s = f.debug_struct("Elf");
                s.field("st_info", st_info);
                s.field("st_other", st_other);
                if let Some(version) = version {
                    s.field("version", &ByteString(version));
                }
                if *version_hidden {
                    s.field("version_hidden", version_hidden);
                }
                s.finish()
            }
            #[cfg(feature = "macho")]
            ExportFlags::MachO { n_type, n_desc } => f
                .debug_struct("MachO")
                .field("n_type", n_type)
                .field("n_desc", n_desc)
                .finish(),
            #[cfg(feature = "pe")]
            ExportFlags::Pe { ordinal } => f.debug_struct("Pe").field("ordinal", ordinal).finish(),
            #[cfg(not(feature = "elf"))]
            ExportFlags::_Phantom(_, i) => match *i {},
        }
    }
}

/// The name or ordinal of an exported symbol in a library.
///
/// Only PE supports identifying a symbol by ordinal instead of by name.
///
/// `T` is the storage for the name. This is normally `&[u8]`, but it is
/// `Cow<[u8]>` where the name may need to be owned (see [`Export::into_name`]).
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum NameOrOrdinal<T> {
    /// The name of the symbol.
    Name(T),
    /// The ordinal of the symbol in the library.
    Ordinal(u16),
}

impl<T: AsRef<[u8]>> fmt::Debug for NameOrOrdinal<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            NameOrOrdinal::Name(name) => f
                .debug_tuple("Name")
                .field(&ByteString(name.as_ref()))
                .finish(),
            NameOrOrdinal::Ordinal(ordinal) => f.debug_tuple("Ordinal").field(ordinal).finish(),
        }
    }
}

impl<T> NameOrOrdinal<T> {
    /// Consume and return the name, or `None` if it is identified by ordinal.
    ///
    /// Use this instead of [`Self::name`] to retain the lifetime of the name.
    #[inline]
    pub fn into_name(self) -> Option<T> {
        match self {
            NameOrOrdinal::Name(name) => Some(name),
            NameOrOrdinal::Ordinal(_) => None,
        }
    }

    /// The ordinal of the symbol, or `None` if it is identified by name.
    #[inline]
    pub fn ordinal(&self) -> Option<u16> {
        match self {
            NameOrOrdinal::Name(_) => None,
            NameOrOrdinal::Ordinal(ordinal) => Some(*ordinal),
        }
    }
}

impl<T: AsRef<[u8]>> NameOrOrdinal<T> {
    /// The name of the symbol, or `None` if it is identified by ordinal.
    #[inline]
    pub fn name(&self) -> Option<&[u8]> {
        match self {
            NameOrOrdinal::Name(name) => Some(name.as_ref()),
            NameOrOrdinal::Ordinal(_) => None,
        }
    }

    /// Borrow the name.
    #[inline]
    pub fn as_ref(&self) -> NameOrOrdinal<&[u8]> {
        match self {
            NameOrOrdinal::Name(name) => NameOrOrdinal::Name(name.as_ref()),
            NameOrOrdinal::Ordinal(ordinal) => NameOrOrdinal::Ordinal(*ordinal),
        }
    }
}

/// PDB information from the debug directory in a PE file.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CodeView<'data> {
    guid: [u8; 16],
    path: ByteString<'data>,
    age: u32,
}

impl<'data> CodeView<'data> {
    /// The path to the PDB as stored in CodeView.
    #[inline]
    pub fn path(&self) -> &'data [u8] {
        self.path.0
    }

    /// The age of the PDB.
    #[inline]
    pub fn age(&self) -> u32 {
        self.age
    }

    /// The GUID of the PDB.
    #[inline]
    pub fn guid(&self) -> [u8; 16] {
        self.guid
    }
}

/// The target referenced by a [`Relocation`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum RelocationTarget {
    /// The target is a symbol.
    Symbol(SymbolIndex),
    /// The target is a section.
    Section(SectionIndex),
    /// The offset is an absolute address.
    Absolute,
}

/// A relocation entry.
///
/// Returned by [`Object::dynamic_relocations`] or [`ObjectSection::relocations`].
pub struct Relocation {
    kind: RelocationKind,
    encoding: RelocationEncoding,
    size: u8,
    target: RelocationTarget,
    subtractor: Option<SymbolIndex>,
    addend: i64,
    implicit_addend: bool,
    flags: RelocationFlags,
}

impl fmt::Debug for Relocation {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut s = f.debug_struct("Relocation");
        s.field("kind", &self.kind)
            .field("encoding", &self.encoding)
            .field("size", &self.size)
            .field("target", &self.target);
        if let Some(subtractor) = self.subtractor {
            s.field("subtractor", &subtractor);
        }
        s.field("addend", &self.addend)
            .field("implicit_addend", &self.implicit_addend)
            .field("flags", &self.flags)
            .finish()
    }
}

impl Relocation {
    /// The operation used to calculate the result of the relocation.
    #[inline]
    pub fn kind(&self) -> RelocationKind {
        self.kind
    }

    /// Information about how the result of the relocation operation is encoded in the place.
    #[inline]
    pub fn encoding(&self) -> RelocationEncoding {
        self.encoding
    }

    /// The size in bits of the place of the relocation.
    ///
    /// If 0, then the size is determined by the relocation kind.
    #[inline]
    pub fn size(&self) -> u8 {
        self.size
    }

    /// The target of the relocation.
    #[inline]
    pub fn target(&self) -> RelocationTarget {
        self.target
    }

    /// A subtractor symbol.
    ///
    /// The relocation calculation subtracts the value of this symbol, if any.
    #[inline]
    pub fn subtractor(&self) -> Option<SymbolIndex> {
        self.subtractor
    }

    /// The addend to use in the relocation calculation.
    #[inline]
    pub fn addend(&self) -> i64 {
        self.addend
    }

    /// Set the addend to use in the relocation calculation.
    #[inline]
    pub fn set_addend(&mut self, addend: i64) {
        self.addend = addend;
    }

    /// Returns true if there is an implicit addend stored in the data at the offset
    /// to be relocated.
    #[inline]
    pub fn has_implicit_addend(&self) -> bool {
        self.implicit_addend
    }

    /// Relocation flags that are specific to each file format.
    ///
    /// The values returned by `kind`, `encoding` and `size` are derived
    /// from these flags.
    #[inline]
    pub fn flags(&self) -> RelocationFlags {
        self.flags
    }
}

/// A map from section offsets to relocation information.
///
/// This can be used to apply relocations to a value at a given section offset.
/// This is intended for use with DWARF in relocatable object files, and only
/// supports relocations that are used in DWARF.
///
/// Returned by [`ObjectSection::relocation_map`].
#[derive(Debug, Default)]
pub struct RelocationMap(Map<u64, RelocationMapEntry>);

impl RelocationMap {
    /// Construct a new relocation map for a section.
    ///
    /// Fails if any relocation cannot be added to the map.
    /// You can manually use `add` if you need different error handling,
    /// such as to list all errors or to ignore them.
    pub fn new<'data, 'file, T>(file: &'file T, section: &T::Section<'file>) -> Result<Self>
    where
        T: Object<'data>,
    {
        let mut map = RelocationMap(Map::new());
        for (offset, relocation) in section.relocations() {
            map.add(file, offset, relocation)?;
        }
        Ok(map)
    }

    /// Add a single relocation to the map.
    pub fn add<'data: 'file, 'file, T>(
        &mut self,
        file: &'file T,
        offset: u64,
        relocation: Relocation,
    ) -> Result<()>
    where
        T: Object<'data>,
    {
        let mut entry = RelocationMapEntry {
            implicit_addend: relocation.has_implicit_addend(),
            addend: relocation.addend() as u64,
        };
        match relocation.kind() {
            RelocationKind::None => return Ok(()),
            RelocationKind::Absolute => match relocation.target() {
                RelocationTarget::Symbol(symbol_idx) => {
                    let symbol = file
                        .symbol_by_index(symbol_idx)
                        .read_error("Relocation with invalid symbol")?;
                    entry.addend = symbol.address().wrapping_add(entry.addend);
                }
                RelocationTarget::Section(section_idx) => {
                    let section = file
                        .section_by_index(section_idx)
                        .read_error("Relocation with invalid section")?;
                    // DWARF parsers expect references to DWARF sections to be section offsets,
                    // not addresses. Addresses are useful for everything else.
                    if section.kind() != SectionKind::Debug {
                        entry.addend = section.address().wrapping_add(entry.addend);
                    }
                }
                _ => {
                    return Err(Error("Unsupported relocation target"));
                }
            },
            _ => {
                return Err(Error("Unsupported relocation type"));
            }
        }
        if relocation.encoding() != RelocationEncoding::Generic {
            return Err(Error("Unsupported relocation encoding"));
        }
        if self.0.insert(offset, entry).is_some() {
            return Err(Error("Multiple relocations for offset"));
        }
        Ok(())
    }

    /// Relocate a value that was read from the section at the given offset.
    pub fn relocate(&self, offset: u64, value: u64) -> u64 {
        if let Some(relocation) = self.0.get(&offset) {
            if relocation.implicit_addend {
                // Use the explicit addend too, because it may have the symbol value.
                value.wrapping_add(relocation.addend)
            } else {
                relocation.addend
            }
        } else {
            value
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
struct RelocationMapEntry {
    implicit_addend: bool,
    addend: u64,
}

/// A data compression format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum CompressionFormat {
    /// The data is uncompressed.
    None,
    /// The data is compressed, but the compression format is unknown.
    Unknown,
    /// ZLIB/DEFLATE.
    ///
    /// Used for ELF compression and GNU compressed debug information.
    Zlib,
    /// Zstandard.
    ///
    /// Used for ELF compression.
    Zstandard,
}

/// A range in a file that may be compressed.
///
/// Returned by [`ObjectSection::compressed_file_range`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct CompressedFileRange {
    /// The data compression format.
    pub format: CompressionFormat,
    /// The file offset of the compressed data.
    pub offset: u64,
    /// The compressed data size.
    pub compressed_size: u64,
    /// The uncompressed data size.
    pub uncompressed_size: u64,
}

impl CompressedFileRange {
    /// Data that is uncompressed.
    #[inline]
    pub fn none(range: Option<(u64, u64)>) -> Self {
        if let Some((offset, size)) = range {
            CompressedFileRange {
                format: CompressionFormat::None,
                offset,
                compressed_size: size,
                uncompressed_size: size,
            }
        } else {
            CompressedFileRange {
                format: CompressionFormat::None,
                offset: 0,
                compressed_size: 0,
                uncompressed_size: 0,
            }
        }
    }

    /// Convert to [`CompressedData`] by reading from the file.
    pub fn data<'data, R: ReadRef<'data>>(self, file: R) -> Result<CompressedData<'data>> {
        let data = file
            .read_bytes_at(self.offset, self.compressed_size)
            .read_error("Invalid compressed data size or offset")?;
        Ok(CompressedData {
            format: self.format,
            data,
            uncompressed_size: self.uncompressed_size,
        })
    }
}

/// Data that may be compressed.
///
/// Returned by [`ObjectSection::compressed_data`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct CompressedData<'data> {
    /// The data compression format.
    pub format: CompressionFormat,
    /// The compressed data.
    pub data: &'data [u8],
    /// The uncompressed data size.
    pub uncompressed_size: u64,
}

impl<'data> CompressedData<'data> {
    /// Data that is uncompressed.
    #[inline]
    pub fn none(data: &'data [u8]) -> Self {
        CompressedData {
            format: CompressionFormat::None,
            data,
            uncompressed_size: data.len() as u64,
        }
    }

    /// Return the uncompressed data.
    ///
    /// If decompression is required, this allocates [`Self::uncompressed_size`] bytes.
    /// This is typically a value recorded in the file, and is not bounded, so check it
    /// first if you need to bound it for untrusted files.
    ///
    /// Returns an error if the decompressed length does not equal
    /// [`Self::uncompressed_size`].
    ///
    /// Returns an error for invalid data or unsupported compression.
    /// This includes if the data is compressed but the `compression` feature
    /// for this crate is disabled.
    pub fn decompress(self) -> Result<Cow<'data, [u8]>> {
        match self.format {
            CompressionFormat::None => Ok(Cow::Borrowed(self.data)),
            #[cfg(feature = "compression")]
            CompressionFormat::Zlib | CompressionFormat::Zstandard => {
                use alloc::vec::Vec;
                use core::convert::TryInto;
                let size = self
                    .uncompressed_size
                    .try_into()
                    .ok()
                    .read_error("Uncompressed data size is too large.")?;
                let mut decompressed = Vec::new();
                decompressed
                    .try_reserve_exact(size)
                    .ok()
                    .read_error("Uncompressed data allocation failed")?;

                match self.format {
                    CompressionFormat::Zlib => {
                        let mut decompress = flate2::Decompress::new(true);
                        decompress
                            .decompress_vec(
                                self.data,
                                &mut decompressed,
                                flate2::FlushDecompress::Finish,
                            )
                            .ok()
                            .read_error("Invalid zlib compressed data")?;
                    }
                    CompressionFormat::Zstandard => {
                        let mut decoder = ruzstd::decoding::FrameDecoder::new();
                        decoder
                            .decode_all_to_vec(self.data, &mut decompressed)
                            .ok()
                            .read_error("Invalid zstd compressed data")?;
                    }
                    _ => unreachable!(),
                }
                if size != decompressed.len() {
                    return Err(Error(
                        "Uncompressed data size does not match compression header",
                    ));
                }

                Ok(Cow::Owned(decompressed))
            }
            _ => Err(Error("Unsupported compressed data.")),
        }
    }
}
