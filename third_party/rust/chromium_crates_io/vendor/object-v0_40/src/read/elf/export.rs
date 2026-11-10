use alloc::borrow::Cow;
use alloc::fmt;

use crate::elf;
use crate::endian::Endianness;
use crate::read::{self, Export, ExportFlags, ExportTarget, NameOrOrdinal, ReadRef, SymbolIndex};

use super::{FileHeader, Sym, SymbolTable, VersionTable};

/// An iterator for the exports in an [`ElfFile32`](super::ElfFile32).
pub type ElfExportIterator32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    ElfExportIterator<'data, 'file, elf::FileHeader32<Endian>, R>;
/// An iterator for the exports in an [`ElfFile64`](super::ElfFile64).
pub type ElfExportIterator64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    ElfExportIterator<'data, 'file, elf::FileHeader64<Endian>, R>;

/// An iterator for the exports in an [`ElfFile`](super::ElfFile).
pub struct ElfExportIterator<'data, 'file, Elf, R = &'data [u8]>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
    endian: Elf::Endian,
    versions: Option<VersionTable<'data, Elf>>,
    symbols: &'file SymbolTable<'data, Elf, R>,
    index: SymbolIndex,
}

impl<'data, 'file, Elf, R> ElfExportIterator<'data, 'file, Elf, R>
where
    Elf: FileHeader,
    R: ReadRef<'data>,
{
    pub(super) fn new(
        endian: Elf::Endian,
        versions: Option<VersionTable<'data, Elf>>,
        symbols: &'file SymbolTable<'data, Elf, R>,
    ) -> Self {
        ElfExportIterator {
            endian,
            versions,
            symbols,
            index: SymbolIndex(1),
        }
    }

    fn next(&mut self) -> read::Result<Option<Export<'data>>> {
        loop {
            let index = self.index;
            let Some(symbol) = self.symbols.symbols().get(index.0) else {
                return Ok(None);
            };
            // Ensure progress is made, so errors after here don't need to terminate iteration.
            self.index.0 += 1;

            // Skip local symbols and unsupported st_shndx values (includes SHN_UNDEF).
            if symbol.is_local() {
                continue;
            }
            let shndx = symbol.st_shndx(self.endian);
            if shndx.is_special() && shndx != elf::SHN_ABS && shndx != elf::SHN_XINDEX {
                continue;
            }

            let value = symbol.st_value(self.endian).into();
            let target = match symbol.st_type() {
                elf::STT_NOTYPE | elf::STT_OBJECT | elf::STT_FUNC => {
                    if shndx == elf::SHN_ABS {
                        ExportTarget::Absolute { value }
                    } else {
                        ExportTarget::Address { address: value }
                    }
                }
                elf::STT_TLS => ExportTarget::Tls { offset: value },
                elf::STT_GNU_IFUNC => ExportTarget::Resolver {
                    resolver: value,
                    stub: None,
                },
                // Skip unsupported st_type values.
                _ => continue,
            };
            let name = symbol.name(self.endian, self.symbols.strings())?;
            let (version, version_hidden) = if let Some(versions) = self.versions.as_ref() {
                let vi = versions.version_index(self.endian, index);
                let version = versions.version(vi.index())?;
                let version_name = version.map(|v| v.name());
                (version_name, vi.is_hidden())
            } else {
                (None, false)
            };
            return Ok(Some(Export {
                name: NameOrOrdinal::Name(Cow::Borrowed(name)),
                target,
                weak: symbol.st_bind() == elf::STB_WEAK,
                flags: ExportFlags::Elf {
                    st_info: symbol.st_info(),
                    st_other: symbol.st_other(),
                    version,
                    version_hidden,
                },
            }));
        }
    }
}

impl<'data, 'file, Elf: FileHeader, R: ReadRef<'data>> fmt::Debug
    for ElfExportIterator<'data, 'file, Elf, R>
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ElfExportIterator").finish()
    }
}

impl<'data, 'file, Elf: FileHeader, R: ReadRef<'data>> Iterator
    for ElfExportIterator<'data, 'file, Elf, R>
{
    type Item = read::Result<Export<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}
