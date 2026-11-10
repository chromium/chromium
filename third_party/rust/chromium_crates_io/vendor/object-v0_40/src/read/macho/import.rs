use alloc::fmt;
use alloc::vec::Vec;
use core::marker::PhantomData;
use core::slice;

use crate::Endianness;
use crate::macho;
use crate::read::{
    Import, ImportFlags, ImportLibrary, ImportLibraryFlags, NameOrOrdinal, ReadError, ReadRef,
    Result,
};

use super::{
    BindOperation, BindOperationIterator, DyldChainedImportIterator, LoadCommandIterator,
    MachHeader, MachOFile, Nlist, SymbolTable,
};

/// An iterator for the import libraries in a [`MachOFile32`](super::MachOFile32).
pub type MachOImportLibraryIterator32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOImportLibraryIterator<'data, 'file, macho::MachHeader32<Endian>, R>;
/// An iterator for the import libraries in a [`MachOFile64`](super::MachOFile64).
pub type MachOImportLibraryIterator64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOImportLibraryIterator<'data, 'file, macho::MachHeader64<Endian>, R>;

/// An iterator for the import libraries in a [`MachOFile`].
pub struct MachOImportLibraryIterator<'data, 'file, Mach, R = &'data [u8]>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    endian: Mach::Endian,
    commands: LoadCommandIterator<'data, Mach::Endian>,
    ordinal: u32,
    marker: PhantomData<(&'file (), R)>,
}

impl<'data, 'file, Mach, R> MachOImportLibraryIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    pub(super) fn new(file: &'file MachOFile<'data, Mach, R>) -> Result<Self> {
        Ok(MachOImportLibraryIterator {
            endian: file.endian,
            commands: file.macho_load_commands()?,
            ordinal: 0,
            marker: PhantomData,
        })
    }

    fn next(&mut self) -> Result<Option<ImportLibrary<'data>>> {
        loop {
            let Some(command) = self.commands.next()? else {
                return Ok(None);
            };
            // The above iterator either fused or made progress, so errors after here don't
            // need to terminate iteration.

            let Some(dylib) = command.dylib().transpose() else {
                // Above only returned Ok(None) if the command type didn't match.
                continue;
            };
            self.ordinal += 1;
            let dylib = dylib?;

            let name = command.string(self.endian, dylib.dylib.name)?;
            let cmd = command.cmd();
            let use_flags = command.dylib_use_flags(self.endian, dylib)?;
            return Ok(Some(ImportLibrary {
                name,
                flags: ImportLibraryFlags::MachO {
                    ordinal: self.ordinal,
                    cmd,
                    current_version: dylib.dylib.current_version.get(self.endian),
                    compatibility_version: dylib.dylib.compatibility_version.get(self.endian),
                    use_flags,
                },
            }));
        }
    }
}

impl<'data, 'file, Mach, R> fmt::Debug for MachOImportLibraryIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("MachOImportLibraryIterator").finish()
    }
}

impl<'data, 'file, Mach, R> Iterator for MachOImportLibraryIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    type Item = Result<ImportLibrary<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// An iterator for the imports in a [`MachOFile32`](super::MachOFile32).
pub type MachOImportIterator32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOImportIterator<'data, 'file, macho::MachHeader32<Endian>, R>;
/// An iterator for the imports in a [`MachOFile64`](super::MachOFile64).
pub type MachOImportIterator64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOImportIterator<'data, 'file, macho::MachHeader64<Endian>, R>;

/// An iterator for the imports in a [`MachOFile`].
pub struct MachOImportIterator<'data, 'file, Mach, R = &'data [u8]>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    internal: MachOImportIteratorInternal<'data, 'file, Mach, R>,
}

enum MachOImportIteratorInternal<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    ChainedFixups(MachOImportChainedFixups<'data, Mach>),
    DyldInfo(MachOImportDyldInfo<'data>),
    Symbols(MachOImportSymbols<'data, 'file, Mach, R>),
}

impl<'data, 'file, Mach, R> fmt::Debug for MachOImportIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("MachOImportIterator").finish()
    }
}

impl<'data, 'file, Mach, R> MachOImportIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    pub(super) fn new(file: &'file MachOFile<'data, Mach, R>) -> Result<Self> {
        let mut chained_fixups = None;
        let mut dyld_info = None;
        let mut commands = file.macho_load_commands()?;
        while let Some(command) = commands.next()? {
            if let Some(command) = command.dyld_chained_fixups()? {
                chained_fixups = Some(command);
            } else if let Some(command) = command.dyld_info()? {
                dyld_info = Some(command);
            }
        }
        let internal = if let Some(chained_fixups) = chained_fixups {
            MachOImportChainedFixups::new(file, chained_fixups)
                .map(MachOImportIteratorInternal::ChainedFixups)?
        } else if let Some(dyld_info) = dyld_info {
            MachOImportDyldInfo::new(file, dyld_info).map(MachOImportIteratorInternal::DyldInfo)?
        } else {
            MachOImportSymbols::new(file).map(MachOImportIteratorInternal::Symbols)?
        };
        Ok(MachOImportIterator { internal })
    }

    fn next(&mut self) -> Result<Option<Import<'data>>> {
        match &mut self.internal {
            MachOImportIteratorInternal::ChainedFixups(iter) => iter.next(),
            MachOImportIteratorInternal::DyldInfo(iter) => iter.next(),
            MachOImportIteratorInternal::Symbols(iter) => iter.next(),
        }
    }
}

impl<'data, 'file, Mach, R> Iterator for MachOImportIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    type Item = Result<Import<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

struct MachOImportSymbols<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    endian: Mach::Endian,
    libraries: Vec<&'data [u8]>,
    twolevel: bool,
    symbols: &'file SymbolTable<'data, Mach, R>,
    iter: slice::Iter<'data, Mach::Nlist>,
}

impl<'data, 'file, Mach, R> MachOImportSymbols<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    fn new(file: &'file MachOFile<'data, Mach, R>) -> Result<Self> {
        let endian = file.endian;
        let twolevel = file.header.flags(endian).contains(macho::MH_TWOLEVEL);
        let libraries = if twolevel {
            file.libraries()?
        } else {
            Vec::new()
        };
        // `LC_DYSYMTAB` is not required, so use the symbol table directly.
        let iter = file.symbols.symbols().iter();
        Ok(MachOImportSymbols {
            endian,
            libraries,
            twolevel,
            symbols: &file.symbols,
            iter,
        })
    }

    fn next(&mut self) -> Result<Option<Import<'data>>> {
        let Some(symbol) = self.iter.find(|s| s.is_undefined()) else {
            return Ok(None);
        };
        // The above iterator has made progress, so errors after here don't need to
        // terminate iteration.

        let name = symbol.name(self.endian, self.symbols.strings())?;
        let n_desc = symbol.n_desc(self.endian);
        let library = if self.twolevel {
            if let Some(index) = n_desc.library().index() {
                self.libraries
                    .get(index as usize)
                    .copied()
                    .read_error("Invalid Mach-O symbol library ordinal")?
            } else {
                // Reserved ordinal; caller can use `n_desc.library()` from `flags`.
                &[]
            }
        } else {
            // Flat namespace.
            &[]
        };
        Ok(Some(Import {
            library,
            name: NameOrOrdinal::Name(name),
            weak: n_desc.contains(macho::N_WEAK_REF),
            flags: ImportFlags::MachO {
                n_type: symbol.n_type(),
                n_desc,
            },
        }))
    }
}

struct MachOImportChainedFixups<'data, Mach>
where
    Mach: MachHeader,
{
    libraries: Vec<&'data [u8]>,
    iter: DyldChainedImportIterator<'data, Mach::Endian>,
}

impl<'data, Mach> MachOImportChainedFixups<'data, Mach>
where
    Mach: MachHeader,
{
    fn new<R: ReadRef<'data>>(
        file: &MachOFile<'data, Mach, R>,
        chained_fixups: &'data macho::LinkeditDataCommand<Mach::Endian>,
    ) -> Result<Self> {
        let endian = file.endian;
        let data = file
            .linkedit_data
            .read_error("Missing Mach-O linkedit segment")?;
        let iter = chained_fixups
            .chained_fixups(endian, data.0)?
            .imports(endian)?;
        Ok(MachOImportChainedFixups {
            libraries: file.libraries()?,
            iter,
        })
    }

    fn next(&mut self) -> Result<Option<Import<'data>>> {
        loop {
            let Some(import) = self.iter.next()? else {
                return Ok(None);
            };
            // The above iterator has made progress, so errors after here don't need to
            // terminate iteration.

            if import.dylib == macho::BIND_SPECIAL_DYLIB_SELF {
                // Not really an import.
                continue;
            }

            // Synthesise flags.
            let flags = if import.weak_import {
                macho::BIND_SYMBOL_FLAGS_WEAK_IMPORT
            } else {
                macho::BindSymbolFlags(0)
            };

            return Ok(Some(Import {
                library: library(&self.libraries, import.dylib)?,
                name: NameOrOrdinal::Name(import.name),
                weak: import.weak_import,
                flags: ImportFlags::MachOBind {
                    dylib: import.dylib,
                    flags,
                },
            }));
        }
    }
}

struct MachOImportDyldInfo<'data> {
    libraries: Vec<&'data [u8]>,

    /// The bind, weak bind, and lazy bind streams, in that order.
    binds: [BindOperationIterator<'data>; 3],
    index: usize,
    lazy: bool,

    dylib: Option<macho::BindDylib>,
    symbol: Option<(macho::BindSymbolFlags, &'data [u8])>,
    unbound: bool,
}

impl<'data> MachOImportDyldInfo<'data> {
    fn new<Mach, R>(
        file: &MachOFile<'data, Mach, R>,
        dyld_info: &'data macho::DyldInfoCommand<Mach::Endian>,
    ) -> Result<Self>
    where
        Mach: MachHeader,
        R: ReadRef<'data>,
    {
        let endian = file.endian;
        let data = file
            .linkedit_data
            .read_error("Missing Mach-O linkedit segment")?;
        let binds = [
            dyld_info.bind_operations(endian, data.0)?,
            dyld_info.weak_bind_operations(endian, data.0)?,
            dyld_info.lazy_bind_operations(endian, data.0)?,
        ];
        Ok(MachOImportDyldInfo {
            libraries: file.libraries()?,
            binds,
            index: 0,
            lazy: false,
            dylib: None,
            symbol: None,
            unbound: false,
        })
    }

    fn next(&mut self) -> Result<Option<Import<'data>>> {
        loop {
            let Some(iter) = self.binds.get_mut(self.index) else {
                return Ok(None);
            };
            let Some((_opcode, operation)) = iter.next()? else {
                self.next_index();
                continue;
            };
            // The above iterator has made progress, so errors after here don't need to
            // terminate iteration.

            match operation {
                BindOperation::Done => {
                    if self.lazy {
                        // Only the state for the current bind is reset.
                        self.dylib = None;
                        self.symbol = None;
                    } else {
                        self.next_index();
                        continue;
                    }
                }
                BindOperation::SetDylibOrdinal { ordinal } => {
                    let ordinal = i32::try_from(ordinal)
                        .ok()
                        .read_error("Invalid Mach-O bind ordinal")?;
                    self.dylib = Some(macho::BindDylib(ordinal));
                }
                BindOperation::SetDylibSpecial { ordinal } => {
                    self.dylib = Some(ordinal);
                }
                BindOperation::SetSymbol { flags, name } => {
                    self.symbol = Some((flags, name));
                    // Only report a symbol for the first bind that uses it.
                    self.unbound = true;
                }
                BindOperation::DoBindTimesSkipping { count: 0, .. } => {}
                BindOperation::DoBind
                | BindOperation::DoBindAddAddr { .. }
                | BindOperation::DoBindTimesSkipping { .. }
                | BindOperation::DoBindAddAddrScaled { .. } => {
                    if self.unbound {
                        self.unbound = false;
                        let (flags, name) = self.symbol.read_error("Missing Mach-O bind symbol")?;
                        let dylib = self.dylib.read_error("Missing Mach-O bind dylib")?;
                        if dylib != macho::BIND_SPECIAL_DYLIB_SELF {
                            return Ok(Some(Import {
                                library: library(&self.libraries, dylib)?,
                                name: NameOrOrdinal::Name(name),
                                weak: flags.contains(macho::BIND_SYMBOL_FLAGS_WEAK_IMPORT),
                                flags: ImportFlags::MachOBind { dylib, flags },
                            }));
                        }
                    }
                }
                BindOperation::SetType { .. }
                | BindOperation::SetAddend { .. }
                | BindOperation::SetSegmentAndOffset { .. }
                | BindOperation::AddAddr { .. } => {}
            }
        }
    }

    fn next_index(&mut self) {
        self.index += 1;
        self.lazy = self.index == 2;
        self.dylib = if self.index == 1 {
            Some(macho::BIND_SPECIAL_DYLIB_WEAK_LOOKUP)
        } else {
            None
        };
        self.symbol = None;
    }
}

fn library<'data>(libraries: &[&'data [u8]], dylib: macho::BindDylib) -> Result<&'data [u8]> {
    let Some(index) = dylib.index() else {
        return Ok(&[]);
    };
    libraries
        .get(index as usize)
        .copied()
        .read_error("Invalid Mach-O bind dylib ordinal")
}
