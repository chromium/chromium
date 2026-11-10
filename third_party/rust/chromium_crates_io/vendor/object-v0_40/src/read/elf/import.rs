use alloc::fmt;
use core::marker::PhantomData;

use crate::elf;
use crate::endian::Endianness;
use crate::read::{
    self, Import, ImportFlags, ImportLibrary, ImportLibraryFlags, NameOrOrdinal, ReadRef,
    SymbolIndex,
};

use super::{DynamicIterator, DynamicTable, FileHeader, Sym, SymbolTable, VersionTable};

/// An iterator for the import libraries in an [`ElfFile32`](super::ElfFile32).
pub type ElfImportLibraryIterator32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    ElfImportLibraryIterator<'data, 'file, elf::FileHeader32<Endian>, R>;
/// An iterator for the import libraries in an [`ElfFile64`](super::ElfFile64).
pub type ElfImportLibraryIterator64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    ElfImportLibraryIterator<'data, 'file, elf::FileHeader64<Endian>, R>;

/// An iterator for the import libraries in an [`ElfFile`](super::ElfFile).
pub struct ElfImportLibraryIterator<'data, 'file, Elf, R = &'data [u8]>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
    dynamic: DynamicTable<'data, Elf, R>,
    iter: DynamicIterator<'data, Elf>,
    marker: PhantomData<&'file ()>,
}

impl<'data, 'file, Elf, R> ElfImportLibraryIterator<'data, 'file, Elf, R>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
    pub(super) fn new(dynamic: DynamicTable<'data, Elf, R>) -> Self {
        let iter = dynamic.iter();
        ElfImportLibraryIterator {
            dynamic,
            iter,
            marker: PhantomData,
        }
    }

    fn next(&mut self) -> read::Result<Option<ImportLibrary<'data>>> {
        loop {
            let Some(d) = self.iter.next() else {
                return Ok(None);
            };
            // The above iterator has made progress, so errors after here don't need to
            // terminate iteration.

            if d.tag != elf::DT_NEEDED {
                continue;
            }
            return Ok(Some(ImportLibrary {
                name: self.dynamic.string(d)?,
                flags: ImportLibraryFlags::None,
            }));
        }
    }
}

impl<'data, 'file, Elf: FileHeader, R: ReadRef<'data>> fmt::Debug
    for ElfImportLibraryIterator<'data, 'file, Elf, R>
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ElfImportLibraryIterator").finish()
    }
}

impl<'data, 'file, Elf: FileHeader, R: ReadRef<'data>> Iterator
    for ElfImportLibraryIterator<'data, 'file, Elf, R>
{
    type Item = read::Result<ImportLibrary<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// An iterator for the imports in an [`ElfFile32`](super::ElfFile32).
pub type ElfImportIterator32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    ElfImportIterator<'data, 'file, elf::FileHeader32<Endian>, R>;
/// An iterator for the imports in an [`ElfFile64`](super::ElfFile64).
pub type ElfImportIterator64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    ElfImportIterator<'data, 'file, elf::FileHeader64<Endian>, R>;

/// An iterator for the imports in an [`ElfFile`](super::ElfFile).
pub struct ElfImportIterator<'data, 'file, Elf, R = &'data [u8]>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
    endian: Elf::Endian,
    versions: Option<VersionTable<'data, Elf>>,
    symbols: &'file SymbolTable<'data, Elf, R>,
    index: SymbolIndex,
}

impl<'data, 'file, Elf, R> ElfImportIterator<'data, 'file, Elf, R>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
    pub(super) fn new(
        endian: Elf::Endian,
        versions: Option<VersionTable<'data, Elf>>,
        symbols: &'file SymbolTable<'data, Elf, R>,
    ) -> Self {
        ElfImportIterator {
            endian,
            versions,
            symbols,
            index: SymbolIndex(1),
        }
    }

    fn next(&mut self) -> read::Result<Option<Import<'data>>> {
        loop {
            let index = self.index;
            let Some(symbol) = self.symbols.symbols().get(index.0) else {
                return Ok(None);
            };
            // Ensure progress is made, so errors after here don't need to terminate iteration.
            self.index.0 += 1;

            if !symbol.is_undefined(self.endian) {
                continue;
            }
            let name = symbol.name(self.endian, self.symbols.strings())?;
            if name.is_empty() {
                continue;
            }
            let version = if let Some(versions) = self.versions.as_ref() {
                let vi = versions.version_index(self.endian, index).index();
                versions.version(vi)?
            } else {
                None
            };
            return Ok(Some(Import {
                library: version.and_then(|v| v.file()).unwrap_or(&[]),
                name: NameOrOrdinal::Name(name),
                weak: symbol.st_bind() == elf::STB_WEAK,
                flags: ImportFlags::Elf {
                    st_info: symbol.st_info(),
                    st_other: symbol.st_other(),
                    version: version.map(|v| v.name()),
                },
            }));
        }
    }
}

impl<'data, 'file, Elf: FileHeader, R: ReadRef<'data>> fmt::Debug
    for ElfImportIterator<'data, 'file, Elf, R>
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ElfImportIterator").finish()
    }
}

impl<'data, 'file, Elf: FileHeader, R: ReadRef<'data>> Iterator
    for ElfImportIterator<'data, 'file, Elf, R>
{
    type Item = read::Result<Import<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}
