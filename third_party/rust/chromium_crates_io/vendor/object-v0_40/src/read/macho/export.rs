use alloc::borrow::Cow;
use alloc::fmt;
use alloc::vec::Vec;
use core::slice;

use crate::Endianness;
use crate::macho;
use crate::read::{
    Error, Export, ExportFlags, ExportTarget, NameOrOrdinal, ReadError, ReadRef, Result,
    SectionIndex,
};

use super::{ExportData, ExportsTrieIterator, MachHeader, MachOFile, Nlist, Section, Segment};

/// An iterator for the exports in a [`MachOFile32`](super::MachOFile32).
pub type MachOExportIterator32<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOExportIterator<'data, 'file, macho::MachHeader32<Endian>, R>;
/// An iterator for the exports in a [`MachOFile64`](super::MachOFile64).
pub type MachOExportIterator64<'data, 'file, Endian = Endianness, R = &'data [u8]> =
    MachOExportIterator<'data, 'file, macho::MachHeader64<Endian>, R>;

/// An iterator for the exports in a [`MachOFile`].
pub struct MachOExportIterator<'data, 'file, Mach, R = &'data [u8]>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    internal: MachOExportIteratorInternal<'data, 'file, Mach, R>,
}

enum MachOExportIteratorInternal<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    Symbols {
        file: &'file MachOFile<'data, Mach, R>,
        iter: slice::Iter<'data, Mach::Nlist>,
    },
    ExportsTrie {
        file: &'file MachOFile<'data, Mach, R>,
        image_base: u64,
        libraries: Option<Vec<&'data [u8]>>,
        iter: ExportsTrieIterator<'data>,
    },
}

impl<'data, 'file, Mach, R> fmt::Debug for MachOExportIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("MachOExportIterator").finish()
    }
}

impl<'data, 'file, Mach, R> MachOExportIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    pub(super) fn new(file: &'file MachOFile<'data, Mach, R>) -> Result<Self> {
        let mut exports_trie = None;
        let mut dyld_info = None;
        let mut commands = file.macho_load_commands()?;
        while let Some(command) = commands.next()? {
            if let Some(command) = command.dyld_exports_trie()? {
                exports_trie = Some(command);
            } else if let Some(command) = command.dyld_info()? {
                dyld_info = Some(command);
            }
        }
        let trie_iter = if let Some(exports_trie) = exports_trie {
            let data = file
                .linkedit_data
                .read_error("Missing Mach-O linkedit segment")?;
            Some(exports_trie.exports_trie(file.endian, data.0)?)
        } else if let Some(dyld_info) = dyld_info {
            let data = file
                .linkedit_data
                .read_error("Missing Mach-O linkedit segment")?;
            Some(dyld_info.exports_trie(file.endian, data.0)?)
        } else {
            None
        };
        let internal = if let Some(iter) = trie_iter {
            let image_base = file
                .segments
                .iter()
                .find(|s| s.segment.name() == macho::SEG_TEXT.as_bytes())
                .map(|s| s.segment.vmaddr(file.endian).into())
                .unwrap_or(0);
            MachOExportIteratorInternal::ExportsTrie {
                file,
                image_base,
                libraries: None,
                iter,
            }
        } else {
            // `LC_DYSYMTAB` is not required, so use the symbol table directly.
            // This also allows us to find `N_INDR` symbols.
            let iter = file.symbols.symbols().iter();
            MachOExportIteratorInternal::Symbols { file, iter }
        };
        Ok(MachOExportIterator { internal })
    }

    fn next(&mut self) -> Result<Option<Export<'data>>> {
        match &mut self.internal {
            MachOExportIteratorInternal::ExportsTrie {
                file,
                image_base,
                libraries,
                iter,
            } => {
                let Some(symbol) = iter.next()? else {
                    return Ok(None);
                };
                let flags = symbol.flags();
                let target = match symbol.data() {
                    ExportData::Regular { address } => match flags.kind() {
                        macho::EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE => {
                            ExportTarget::Absolute { value: *address }
                        }
                        macho::EXPORT_SYMBOL_FLAGS_KIND_THREAD_LOCAL => {
                            ExportTarget::TlvDescriptor {
                                address: image_base.wrapping_add(*address),
                            }
                        }
                        macho::EXPORT_SYMBOL_FLAGS_KIND_REGULAR => ExportTarget::Address {
                            address: image_base.wrapping_add(*address),
                        },
                        _ => {
                            return Err(Error("Unsupported Mach-O export kind"));
                        }
                    },
                    ExportData::StubAndResolver {
                        stub_address,
                        resolver_address,
                    } => {
                        if flags.kind() != macho::EXPORT_SYMBOL_FLAGS_KIND_REGULAR {
                            return Err(Error("Unsupported Mach-O export kind"));
                        }
                        ExportTarget::Resolver {
                            resolver: image_base.wrapping_add(*resolver_address),
                            stub: Some(image_base.wrapping_add(*stub_address)),
                        }
                    }
                    ExportData::Reexport {
                        dylib_ordinal,
                        import_name,
                    } => {
                        if libraries.is_none() {
                            *libraries = Some(file.libraries()?);
                        }
                        let library = usize::try_from(*dylib_ordinal)
                            .ok()
                            // No special ordinals.
                            .filter(|index| *index != 0)
                            .and_then(|index| libraries.as_deref()?.get(index).copied())
                            .read_error("Invalid Mach-O export dylib ordinal")?;
                        ExportTarget::Reexport {
                            library,
                            name: NameOrOrdinal::Name(import_name),
                        }
                    }
                };
                Ok(Some(Export {
                    name: NameOrOrdinal::Name(Cow::Owned(symbol.into_name())),
                    target,
                    weak: flags.contains(macho::EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION),
                    flags: ExportFlags::None,
                }))
            }
            MachOExportIteratorInternal::Symbols { file, iter } => {
                let endian = file.endian;
                let symbols = &file.symbols;
                loop {
                    let Some(symbol) = iter.next() else {
                        return Ok(None);
                    };
                    // The above iterator has made progress, so errors after here don't need to
                    // terminate iteration.

                    let n_type = symbol.n_type();
                    if n_type.is_stab() || !n_type.is_ext() || n_type.is_pext() {
                        continue;
                    }
                    let name = symbol.name(endian, symbols.strings())?;
                    let value = symbol.n_value(endian).into();
                    let target = match symbol.n_type().typ() {
                        macho::N_ABS => ExportTarget::Absolute { value },
                        macho::N_INDR => {
                            let import_name = u32::try_from(value)
                                .ok()
                                .and_then(|offset| symbols.strings().get(offset).ok())
                                .read_error("Invalid Mach-O indirect symbol value")?;
                            ExportTarget::Reexport {
                                library: &[],
                                name: NameOrOrdinal::Name(import_name),
                            }
                        }
                        macho::N_SECT => {
                            if symbol.n_desc(endian).contains(macho::N_SYMBOL_RESOLVER) {
                                ExportTarget::Resolver {
                                    resolver: value,
                                    stub: None,
                                }
                            } else {
                                let index = SectionIndex(symbol.n_sect().into());
                                let section = file.section_internal(index)?;
                                if section.section.flags(endian).typ()
                                    == macho::S_THREAD_LOCAL_VARIABLES
                                {
                                    ExportTarget::TlvDescriptor { address: value }
                                } else {
                                    ExportTarget::Address { address: value }
                                }
                            }
                        }
                        _ => continue,
                    };
                    break Ok(Some(Export {
                        name: NameOrOrdinal::Name(Cow::Borrowed(name)),
                        target,
                        weak: symbol.n_desc(endian).contains(macho::N_WEAK_DEF),
                        flags: ExportFlags::MachO {
                            n_type: symbol.n_type(),
                            n_desc: symbol.n_desc(endian),
                        },
                    }));
                }
            }
        }
    }
}

impl<'data, 'file, Mach, R> Iterator for MachOExportIterator<'data, 'file, Mach, R>
where
    Mach: MachHeader,
    R: ReadRef<'data>,
{
    type Item = Result<Export<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}
