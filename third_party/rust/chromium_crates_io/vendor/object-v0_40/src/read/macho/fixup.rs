use core::mem;

use crate::endian::{Endian, I32, U16, U32, U64};
use crate::macho;
use crate::read::{Bytes, Error, ReadError, ReadRef, Result};

impl<E: Endian> macho::LinkeditDataCommand<E> {
    /// Parse the data referenced by an `LC_DYLD_CHAINED_FIXUPS` load command.
    pub fn chained_fixups<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<DyldChainedFixups<'data, E>> {
        let fixups_data = data
            .read_bytes_at(
                self.dataoff.get(endian).into(),
                self.datasize.get(endian).into(),
            )
            .read_error("Invalid Mach-O chained fixups offset or size")?;
        DyldChainedFixups::parse(endian, fixups_data)
    }
}

/// The parsed data of an `LC_DYLD_CHAINED_FIXUPS` load command.
///
/// This gives access to the [`macho::DyldChainedFixupsHeader`], the imports table,
/// and the per-segment chain starts. The fixup chains themselves are located in the
/// segment data, and can be walked using [`DyldChainedSegment::fixups`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedFixups<'data, E: Endian> {
    data: Bytes<'data>,
    header: &'data macho::DyldChainedFixupsHeader<E>,
}

impl<'data, E: Endian> DyldChainedFixups<'data, E> {
    /// Parse the chained fixups data.
    ///
    /// `data` should be the data referenced by the `LC_DYLD_CHAINED_FIXUPS` load command.
    pub fn parse(endian: E, data: &'data [u8]) -> Result<Self> {
        let header = Bytes(data)
            .read_at::<macho::DyldChainedFixupsHeader<E>>(0)
            .read_error("Invalid Mach-O chained fixups header")?;
        if header.fixups_version.get(endian) != 0 {
            return Err(Error("Unsupported Mach-O chained fixups version"));
        }
        Ok(DyldChainedFixups {
            data: Bytes(data),
            header,
        })
    }

    /// Return the chained fixups header.
    pub fn header(&self) -> &'data macho::DyldChainedFixupsHeader<E> {
        self.header
    }

    /// Return an iterator over the imports.
    pub fn imports(&self, endian: E) -> Result<DyldChainedImportIterator<'data, E>> {
        if self.header.symbols_format.get(endian) != 0 {
            return Err(Error(
                "Unsupported Mach-O chained fixups compressed symbols",
            ));
        }
        // The imports table and the symbol string pool each extend to the end of the
        // chain data.
        let data = self.data.0;
        let imports = data
            .get(self.header.imports_offset.get(endian) as usize..)
            .read_error("Invalid Mach-O chained fixups imports offset")?;
        let symbols = data
            .get(self.header.symbols_offset.get(endian) as usize..)
            .read_error("Invalid Mach-O chained fixups symbols offset")?;
        Ok(DyldChainedImportIterator {
            endian,
            imports: Bytes(imports),
            symbols: Bytes(symbols),
            count: self.header.imports_count.get(endian),
            format: self.header.imports_format.get(endian),
        })
    }

    /// Return an iterator over the chain starts for each segment.
    pub fn segments(&self, endian: E) -> Result<DyldChainedSegmentIterator<'data, E>> {
        let context = "Invalid Mach-O chained starts offset or count";
        let mut data = self.data;
        data.skip(self.header.starts_offset.get(endian) as usize)
            .read_error(context)?;

        let mut header_data = data;
        let starts_in_image = header_data
            .read::<macho::DyldChainedStartsInImage<E>>()
            .read_error(context)?;
        let seg_count = starts_in_image.seg_count.get(endian);
        let seg_info_offsets = header_data
            .read_slice::<U32<E>>(seg_count as usize)
            .read_error(context)?;

        Ok(DyldChainedSegmentIterator {
            endian,
            data,
            seg_info_offsets,
            index: 0,
        })
    }
}

/// An iterator over the imports in an `LC_DYLD_CHAINED_FIXUPS` load command.
///
/// Returned by [`DyldChainedFixups::imports`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedImportIterator<'data, E: Endian> {
    endian: E,
    imports: Bytes<'data>,
    symbols: Bytes<'data>,
    count: u32,
    format: macho::DyldChainedImportFormat,
}

impl<'data, E: Endian> DyldChainedImportIterator<'data, E> {
    /// Return the next import.
    pub fn next(&mut self) -> Result<Option<DyldChainedImport<'data>>> {
        if self.count == 0 {
            return Ok(None);
        }
        let import =
            DyldChainedImport::parse(self.endian, self.format, self.symbols, &mut self.imports)
                .map(Some);
        if import.is_ok() {
            self.count -= 1;
        } else {
            self.count = 0;
        }
        import
    }
}

impl<'data, E: Endian> Iterator for DyldChainedImportIterator<'data, E> {
    type Item = Result<DyldChainedImport<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// An imported symbol in an `LC_DYLD_CHAINED_FIXUPS` load command.
///
/// Returned by [`DyldChainedImportIterator::next`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedImport<'data> {
    /// The ordinal of the library that the symbol is imported from.
    pub dylib: macho::BindDylib,
    /// Whether this is a weak import.
    pub weak_import: bool,
    /// The addend to add to the symbol value.
    pub addend: i64,
    /// The symbol name.
    pub name: &'data [u8],
}

impl<'data> DyldChainedImport<'data> {
    fn parse<E: Endian>(
        endian: E,
        format: macho::DyldChainedImportFormat,
        symbols: Bytes<'data>,
        imports: &mut Bytes<'data>,
    ) -> Result<Self> {
        let context = "Invalid Mach-O chained fixups import";
        let (dylib, weak_import, name_offset, addend) = match format {
            macho::DYLD_CHAINED_IMPORT | macho::DYLD_CHAINED_IMPORT_ADDEND => {
                let import = imports
                    .read::<U32<E, macho::DyldChainedImport32>>()
                    .read_error(context)?
                    .get(endian);
                let addend = if format == macho::DYLD_CHAINED_IMPORT_ADDEND {
                    imports.read::<I32<E>>().read_error(context)?.get(endian)
                } else {
                    0
                };
                (
                    import.dylib(),
                    import.weak_import(),
                    import.name_offset(),
                    i64::from(addend),
                )
            }
            macho::DYLD_CHAINED_IMPORT_ADDEND64 => {
                let import = imports
                    .read::<U64<E, macho::DyldChainedImport64>>()
                    .read_error(context)?
                    .get(endian);
                let addend = imports.read::<U64<E>>().read_error(context)?.get(endian);
                (
                    import.dylib(),
                    import.weak_import(),
                    import.name_offset(),
                    addend as i64,
                )
            }
            _ => return Err(Error("Unsupported Mach-O chained fixups import format")),
        };
        let name = symbols
            .read_string_at(name_offset as usize)
            .read_error("Invalid Mach-O chained fixups import name offset")?;
        Ok(DyldChainedImport {
            dylib,
            weak_import,
            addend,
            name,
        })
    }
}

/// An iterator over the chain starts for each segment.
///
/// Returned by [`DyldChainedFixups::segments`]. Segments without any fixups are skipped.
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedSegmentIterator<'data, E: Endian> {
    endian: E,
    data: Bytes<'data>,
    seg_info_offsets: &'data [U32<E>],
    index: u32,
}

impl<'data, E: Endian> DyldChainedSegmentIterator<'data, E> {
    /// Return the chain starts for the next segment that has fixups.
    pub fn next(&mut self) -> Result<Option<DyldChainedSegment<'data, E>>> {
        let context = "Invalid Mach-O chained starts in segment";
        while let Some(seg_info_offset) = self.seg_info_offsets.get(self.index as usize) {
            let index = self.index;
            self.index += 1;
            let seg_info_offset = seg_info_offset.get(self.endian);
            if seg_info_offset == 0 {
                // No fixups in this segment.
                continue;
            }
            let mut data = self.data;
            data.skip(seg_info_offset as usize).read_error(context)?;
            let header = data
                .read::<macho::DyldChainedStartsInSegment<E>>()
                .read_error(context)?;
            let starts_size = header
                .size
                .get(self.endian)
                .checked_sub(mem::size_of_val(header) as u32)
                .read_error(context)?;
            let starts = data
                .read_slice::<U16<E>>(starts_size as usize / 2)
                .read_error(context)?;
            return Ok(Some(DyldChainedSegment {
                index,
                header,
                starts,
            }));
        }
        Ok(None)
    }
}

impl<'data, E: Endian> Iterator for DyldChainedSegmentIterator<'data, E> {
    type Item = Result<DyldChainedSegment<'data, E>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// The chain starts for a single segment in an `LC_DYLD_CHAINED_FIXUPS` load command.
///
/// Returned by [`DyldChainedSegmentIterator::next`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedSegment<'data, E: Endian> {
    index: u32,
    header: &'data macho::DyldChainedStartsInSegment<E>,
    /// Page indices and chain indices both start at index 0 of the slice.
    starts: &'data [U16<E>],
}

impl<'data, E: Endian> DyldChainedSegment<'data, E> {
    /// The index of the segment that these fixups belong to.
    pub fn index(&self) -> u32 {
        self.index
    }

    /// The `DyldChainedStartsInSegment` for this segment.
    pub fn header(&self) -> &'data macho::DyldChainedStartsInSegment<E> {
        self.header
    }

    /// Return an iterator over the fixups in this segment.
    ///
    /// `preferred_load_address` should be the vmaddr of the `__TEXT` segment.
    /// It is used to convert the absolute target addresses used by some pointer
    /// formats into vm offsets.
    ///
    /// `segment_data` should be the file contents of the segment that these fixups
    /// belong to.
    pub fn fixups(
        &self,
        endian: E,
        preferred_load_address: u64,
        segment_data: &'data [u8],
    ) -> DyldChainedFixupIterator<'data, E> {
        DyldChainedFixupIterator {
            endian,
            preferred_load_address,
            segment_data: Bytes(segment_data),
            pointer_format: self.header.pointer_format.get(endian),
            page_size: self.header.page_size.get(endian),
            page_count: self.header.page_count.get(endian) as usize,
            starts: self.starts,

            state: FixupState::Start,
            page_index: 0,
            chain_index: 0,
            page_offset: 0,
            offset: 0,
        }
    }
}

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
enum FixupState {
    Start,
    Chain,
    Page,
    PageChain,
}

/// An iterator over the fixups in a segment.
///
/// Returned by [`DyldChainedSegment::fixups`].
#[derive(Debug, Clone, Copy)]
pub struct DyldChainedFixupIterator<'data, E: Endian> {
    endian: E,
    preferred_load_address: u64,
    segment_data: Bytes<'data>,
    pointer_format: macho::DyldChainedPtrFormat,
    page_size: u16,
    page_count: usize,
    starts: &'data [U16<E>],

    state: FixupState,
    /// The next index within page_starts.
    page_index: usize,
    /// The next index within chain_starts.
    chain_index: usize,
    /// The current page offset within the segment.
    page_offset: u64,
    /// The offset of the next linked list entry within the segment.
    offset: u64,
}

impl<'data, E: Endian> DyldChainedFixupIterator<'data, E> {
    /// Return the next fixup in the segment.
    ///
    /// Returns the offset of the fixup within the segment, and the decoded fixup.
    pub fn next(&mut self) -> Result<Option<(u64, Fixup)>> {
        let result = self.next_inner();
        if result.is_err() {
            self.state = FixupState::Start;
            self.page_index = self.page_count;
        }
        result
    }

    fn next_inner(&mut self) -> Result<Option<(u64, Fixup)>> {
        loop {
            match self.state {
                FixupState::Start => {
                    if self.page_index >= self.page_count {
                        return Ok(None);
                    }
                    let page_start = self
                        .starts
                        .get(self.page_index)
                        .read_error("Invalid Mach-O chained fixup page count")?
                        .get(self.endian);
                    self.page_offset = self.page_index as u64 * u64::from(self.page_size);
                    self.page_index += 1;
                    if page_start == macho::DYLD_CHAINED_PTR_START_NONE {
                        // No fixups for this page.
                    } else if page_start & macho::DYLD_CHAINED_PTR_START_MULTI != 0 {
                        self.state = FixupState::Chain;
                        self.chain_index =
                            usize::from(page_start & !macho::DYLD_CHAINED_PTR_START_MULTI);
                    } else {
                        self.state = FixupState::Page;
                        self.offset = self.page_offset + u64::from(page_start);
                    }
                }
                FixupState::Chain => {
                    let chain_start = self
                        .starts
                        .get(self.chain_index)
                        .read_error("Invalid Mach-O chained fixup chain index")?
                        .get(self.endian);
                    self.chain_index += 1;
                    self.offset = self.page_offset
                        + u64::from(chain_start & !macho::DYLD_CHAINED_PTR_START_LAST);
                    if chain_start & macho::DYLD_CHAINED_PTR_START_LAST != 0 {
                        self.state = FixupState::Page;
                    } else {
                        self.state = FixupState::PageChain;
                    }
                }
                FixupState::Page | FixupState::PageChain => {
                    let offset = self.offset;
                    let pointer = match self.pointer_format {
                        macho::DYLD_CHAINED_PTR_32
                        | macho::DYLD_CHAINED_PTR_32_CACHE
                        | macho::DYLD_CHAINED_PTR_32_FIRMWARE => u64::from(
                            self.segment_data
                                .read_at::<U32<E>>(offset as usize)
                                .read_error("Invalid Mach-O chained fixup offset")?
                                .get(self.endian),
                        ),
                        _ => self
                            .segment_data
                            .read_at::<U64<E>>(offset as usize)
                            .read_error("Invalid Mach-O chained fixup offset")?
                            .get(self.endian),
                    };
                    let (next, fixup) =
                        Fixup::parse(self.pointer_format, pointer, self.preferred_load_address)?;
                    if next == 0 {
                        if self.state == FixupState::PageChain {
                            self.state = FixupState::Chain
                        } else {
                            self.state = FixupState::Start
                        };
                    } else {
                        self.offset = offset + next;
                    }
                    return Ok(Some((offset, fixup)));
                }
            }
        }
    }
}

impl<'data, E: Endian> Iterator for DyldChainedFixupIterator<'data, E> {
    type Item = Result<(u64, Fixup)>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// A fixup for a value in the segment data.
#[derive(Debug, Clone, Copy)]
pub enum Fixup {
    /// A rebase fixup.
    Rebase(FixupRebase),
    /// A bind fixup for an import symbol.
    Bind(FixupBind),
    /// A kernel cache rebase fixup.
    KernelCacheRebase(FixupKernelCacheRebase),
    /// A segmented rebase fixup.
    SegmentedRebase(FixupSegmentedRebase),
}

/// A rebase fixup.
#[derive(Debug, Clone, Copy)]
pub struct FixupRebase {
    /// The value to be relocated. This is a vm offset.
    pub target_offset: u64,
    /// The pointer authentication data, if present.
    pub auth: Option<FixupAuth>,
}

/// A bind fixup.
#[derive(Debug, Clone, Copy)]
pub struct FixupBind {
    /// The import ordinal.
    pub ordinal: u32,
    /// The addend to add to the symbol value.
    pub addend: i32,
    /// The pointer authentication data, if present.
    pub auth: Option<FixupAuth>,
}

/// A kernel cache rebase fixup.
#[derive(Debug, Clone, Copy)]
pub struct FixupKernelCacheRebase {
    /// The value to be relocated.
    pub target_offset: u64,
    /// The cache level to bind to.
    pub cache_level: u8,
    /// The pointer authentication data, if present.
    pub auth: Option<FixupAuth>,
}

/// A segmented rebase fixup.
#[derive(Debug, Clone, Copy)]
pub struct FixupSegmentedRebase {
    /// The index into the segment address table.
    pub target_segment_index: u8,
    /// The offset in the segment.
    pub target_segment_offset: u64,
    /// The pointer authentication data, if present.
    pub auth: Option<FixupAuth>,
}

/// Pointer authentication data.
///
/// This is used for signing pointers for the arm64e ABI.
#[derive(Debug, Clone, Copy)]
pub struct FixupAuth {
    /// The key used to generate the signed value.
    pub key: macho::PtrauthKey,
    /// The integer diversity value.
    pub diversity: u16,
    /// Whether the address should be blended with the diversity value.
    pub addr_div: bool,
}

impl Fixup {
    fn parse(format: macho::DyldChainedPtrFormat, value: u64, vmaddr: u64) -> Result<(u64, Self)> {
        match format {
            macho::DYLD_CHAINED_PTR_ARM64E => Ok(Fixup::parse_arm64e(value, 8, vmaddr)),
            macho::DYLD_CHAINED_PTR_ARM64E_KERNEL => Ok(Fixup::parse_arm64e(value, 4, 0)),
            macho::DYLD_CHAINED_PTR_ARM64E_USERLAND => Ok(Fixup::parse_arm64e(value, 8, 0)),
            macho::DYLD_CHAINED_PTR_ARM64E_FIRMWARE => Ok(Fixup::parse_arm64e(value, 4, vmaddr)),
            macho::DYLD_CHAINED_PTR_ARM64E_USERLAND24 => Ok(Fixup::parse_userland24(value)),
            macho::DYLD_CHAINED_PTR_64 => Ok(Fixup::parse_64(value, vmaddr)),
            macho::DYLD_CHAINED_PTR_64_OFFSET => Ok(Fixup::parse_64(value, 0)),
            macho::DYLD_CHAINED_PTR_64_KERNEL_CACHE => Ok(Fixup::parse_64_kernel_cache(value, 4)),
            macho::DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE => {
                Ok(Fixup::parse_64_kernel_cache(value, 1))
            }
            macho::DYLD_CHAINED_PTR_32 => Ok(Fixup::parse_32(value, vmaddr)),
            macho::DYLD_CHAINED_PTR_32_CACHE => Ok(Fixup::parse_32_cache(value)),
            macho::DYLD_CHAINED_PTR_32_FIRMWARE => Ok(Fixup::parse_32_firmware(value)),
            macho::DYLD_CHAINED_PTR_ARM64E_SEGMENTED => Ok(Fixup::parse_arm64e_segmented(value)),
            macho::DYLD_CHAINED_PTR_ARM64E_SHARED_CACHE => {
                Ok(Fixup::parse_arm64e_shared_cache(value))
            }
            _ => Err(Error("Unsupported Mach-O chained pointer format")),
        }
    }

    fn parse_arm64e(ptr: u64, stride: u64, vmaddr: u64) -> (u64, Self) {
        let ptr = macho::DyldChainedPtrArm64e(ptr);
        let fixup = if ptr.is_bind() {
            Self::parse_arm64e_bind(ptr)
        } else {
            Self::parse_arm64e_rebase(ptr, vmaddr)
        };
        (ptr.next() * stride, fixup)
    }

    fn parse_arm64e_bind(ptr: macho::DyldChainedPtrArm64e) -> Self {
        if ptr.is_auth() {
            let ptr = ptr.auth_bind();
            let key = match ptr.key() {
                1 => macho::PtrauthKey::IB,
                2 => macho::PtrauthKey::DA,
                3 => macho::PtrauthKey::DB,
                _ => macho::PtrauthKey::IA,
            };
            let auth = Some(FixupAuth {
                key,
                diversity: ptr.diversity(),
                addr_div: ptr.addr_div(),
            });
            Fixup::Bind(FixupBind {
                ordinal: ptr.ordinal(),
                addend: 0,
                auth,
            })
        } else {
            let ptr = ptr.bind();
            Fixup::Bind(FixupBind {
                ordinal: ptr.ordinal(),
                addend: ptr.addend(),
                auth: None,
            })
        }
    }

    fn parse_arm64e_rebase(ptr: macho::DyldChainedPtrArm64e, vmaddr: u64) -> Self {
        if ptr.is_auth() {
            let ptr = ptr.auth_rebase();
            let target_offset = ptr.runtime_offset();
            let key = match ptr.key() {
                1 => macho::PtrauthKey::IB,
                2 => macho::PtrauthKey::DA,
                3 => macho::PtrauthKey::DB,
                _ => macho::PtrauthKey::IA,
            };
            let auth = Some(FixupAuth {
                key,
                diversity: ptr.diversity(),
                addr_div: ptr.addr_div(),
            });
            Fixup::Rebase(FixupRebase {
                target_offset,
                auth,
            })
        } else {
            let ptr = ptr.rebase();
            let target_offset = ptr.target().wrapping_sub(vmaddr) | ptr.high8() << 56;
            Fixup::Rebase(FixupRebase {
                target_offset,
                auth: None,
            })
        }
    }

    fn parse_userland24(ptr: u64) -> (u64, Self) {
        let ptr = macho::DyldChainedPtrArm64e(ptr);
        let fixup = if ptr.is_bind() {
            Self::parse_arm64e_bind24(ptr)
        } else {
            Self::parse_arm64e_rebase(ptr, 0)
        };
        (ptr.next() * 8, fixup)
    }

    fn parse_arm64e_bind24(ptr: macho::DyldChainedPtrArm64e) -> Self {
        if ptr.is_auth() {
            let ptr = ptr.auth_bind24();
            let key = match ptr.key() {
                1 => macho::PtrauthKey::IB,
                2 => macho::PtrauthKey::DA,
                3 => macho::PtrauthKey::DB,
                _ => macho::PtrauthKey::IA,
            };
            let auth = Some(FixupAuth {
                key,
                diversity: ptr.diversity(),
                addr_div: ptr.addr_div(),
            });
            Fixup::Bind(FixupBind {
                ordinal: ptr.ordinal(),
                addend: 0,
                auth,
            })
        } else {
            let ptr = ptr.bind24();
            Fixup::Bind(FixupBind {
                ordinal: ptr.ordinal(),
                addend: ptr.addend(),
                auth: None,
            })
        }
    }

    fn parse_64(ptr: u64, vmaddr: u64) -> (u64, Self) {
        let ptr = macho::DyldChainedPtr64(ptr);
        let fixup = if ptr.is_bind() {
            let ptr = ptr.bind();
            Fixup::Bind(FixupBind {
                ordinal: ptr.ordinal(),
                addend: ptr.addend(),
                auth: None,
            })
        } else {
            let ptr = ptr.rebase();
            let target_offset = ptr.target().wrapping_sub(vmaddr) | ptr.high8() << 56;
            Fixup::Rebase(FixupRebase {
                target_offset,
                auth: None,
            })
        };
        (ptr.next() * 4, fixup)
    }

    fn parse_64_kernel_cache(ptr: u64, stride: u64) -> (u64, Self) {
        let ptr = macho::DyldChainedPtr64KernelCacheRebase(ptr);
        let auth = if ptr.is_auth() {
            let key = match ptr.key() {
                1 => macho::PtrauthKey::IB,
                2 => macho::PtrauthKey::DA,
                3 => macho::PtrauthKey::DB,
                _ => macho::PtrauthKey::IA,
            };
            Some(FixupAuth {
                key,
                diversity: ptr.diversity(),
                addr_div: ptr.addr_div(),
            })
        } else {
            None
        };
        let fixup = Fixup::KernelCacheRebase(FixupKernelCacheRebase {
            target_offset: ptr.target(),
            cache_level: ptr.cache_level(),
            auth,
        });
        (ptr.next() * stride, fixup)
    }

    fn parse_32(ptr: u64, vmaddr: u64) -> (u64, Self) {
        let ptr = macho::DyldChainedPtr32(ptr as u32);
        let fixup = if ptr.is_bind() {
            let ptr = ptr.bind();
            Fixup::Bind(FixupBind {
                ordinal: ptr.ordinal(),
                addend: ptr.addend(),
                auth: None,
            })
        } else {
            let ptr = ptr.rebase();
            let target_offset = u64::from(ptr.target()).wrapping_sub(vmaddr);
            Fixup::Rebase(FixupRebase {
                target_offset,
                auth: None,
            })
        };
        (u64::from(ptr.next()) * 4, fixup)
    }

    fn parse_32_cache(ptr: u64) -> (u64, Self) {
        let ptr = macho::DyldChainedPtr32CacheRebase(ptr as u32);
        let fixup = Fixup::Rebase(FixupRebase {
            target_offset: u64::from(ptr.target()),
            auth: None,
        });
        (u64::from(ptr.next()) * 4, fixup)
    }

    fn parse_32_firmware(ptr: u64) -> (u64, Self) {
        let ptr = macho::DyldChainedPtr32FirmwareRebase(ptr as u32);
        let fixup = Fixup::Rebase(FixupRebase {
            target_offset: u64::from(ptr.target()),
            auth: None,
        });
        (u64::from(ptr.next()) * 4, fixup)
    }

    fn parse_arm64e_segmented(ptr: u64) -> (u64, Self) {
        let ptr = macho::DyldChainedPtrArm64eSegmentedRebase(ptr);
        let auth = if ptr.is_auth() {
            let key = match ptr.key() {
                1 => macho::PtrauthKey::IB,
                2 => macho::PtrauthKey::DA,
                3 => macho::PtrauthKey::DB,
                _ => macho::PtrauthKey::IA,
            };
            Some(FixupAuth {
                key,
                diversity: ptr.diversity(),
                addr_div: ptr.addr_div(),
            })
        } else {
            None
        };
        let fixup = Fixup::SegmentedRebase(FixupSegmentedRebase {
            target_segment_index: ptr.target_seg_index(),
            target_segment_offset: ptr.target_seg_offset(),
            auth,
        });
        (ptr.next() * 4, fixup)
    }

    fn parse_arm64e_shared_cache(ptr: u64) -> (u64, Self) {
        let ptr = macho::DyldChainedPtrArm64eSharedCache(ptr);
        let fixup = if ptr.is_auth() {
            let ptr = ptr.auth_rebase();
            let key = if ptr.key_is_data() {
                macho::PtrauthKey::DA
            } else {
                macho::PtrauthKey::IA
            };
            let auth = Some(FixupAuth {
                key,
                diversity: ptr.diversity(),
                addr_div: ptr.addr_div(),
            });
            Fixup::Rebase(FixupRebase {
                target_offset: ptr.runtime_offset(),
                auth,
            })
        } else {
            let ptr = ptr.rebase();
            Fixup::Rebase(FixupRebase {
                target_offset: ptr.runtime_offset() | ptr.high8() << 56,
                auth: None,
            })
        };
        (ptr.next() * 8, fixup)
    }
}
