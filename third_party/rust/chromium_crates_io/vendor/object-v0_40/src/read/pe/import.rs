use core::fmt;
use core::marker::PhantomData;
use core::mem;

use crate::endian::{LittleEndian as LE, U16};
use crate::pe;
use crate::pod::Pod;
use crate::read::{self, Bytes, Error, ReadError, ReadRef, Result};

use super::{ImageNtHeaders, PeFile, SectionTable};

/// Information for parsing a PE import table.
///
/// Returned by [`DataDirectories::import_table`](super::DataDirectories::import_table).
#[derive(Debug, Clone)]
pub struct ImportTable<'data> {
    import_address: u32,
    import_section_data: Bytes<'data>,
    import_section_address: u32,
    name_section_data: Bytes<'data>,
    name_section_address: u32,
}

impl<'data> ImportTable<'data> {
    /// Create a new import table parser.
    ///
    /// The import descriptors start at `import_address`.
    /// The size declared in the `IMAGE_DIRECTORY_ENTRY_IMPORT` data directory is
    /// ignored by the Windows loader, and so descriptors will be parsed until a null entry.
    ///
    /// `section_data` should be from the section containing `import_address`, and
    /// `section_address` should be the address of that section. Pointers within the
    /// descriptors and thunks may point to anywhere within the section data.
    pub fn new(section_data: &'data [u8], section_address: u32, import_address: u32) -> Self {
        ImportTable {
            import_address,
            import_section_data: Bytes(section_data),
            import_section_address: section_address,
            name_section_data: Bytes(section_data),
            name_section_address: section_address,
        }
    }

    /// Create a new import table parser.
    ///
    /// The import descriptors start at `import_address`.
    /// The size declared in the `IMAGE_DIRECTORY_ENTRY_IMPORT` data directory is
    /// ignored by the Windows loader, and so descriptors will be parsed until a null entry.
    ///
    /// `data` should be the data for the entire file. The section table is used to
    /// find the data for the section containing `import_address`.
    ///
    /// We also support names in a separate section by parsing the first descriptor
    /// to get the name address, and finding the section containing that address.
    pub fn from_sections<R: ReadRef<'data>>(
        data: R,
        sections: &SectionTable<'data>,
        import_address: u32,
    ) -> Result<Self> {
        let (section_data, section_address) = sections
            .pe_data_containing(data, import_address)
            .read_error("Invalid import data dir virtual address")?;
        let mut imports = Self::new(section_data, section_address, import_address);

        // Usually imports, names and thunks are all in the same section. However,
        // this isn't required, and use of different sections has been seen in older
        // files. We don't know which other section to use without parsing first though,
        // so do that for the first descriptor and ignore any errors (the user will get
        // those errors when they parse again later).
        //
        // This still won't support all possible layouts (e.g. names split across
        // multiple sections).
        if let Ok(mut descriptors) = imports.descriptors() {
            if let Ok(Some(descriptor)) = descriptors.next() {
                if let Some((name_section_data, name_section_address)) =
                    sections.pe_data_containing(data, descriptor.name.get(LE))
                {
                    imports.name_section_data = Bytes(name_section_data);
                    imports.name_section_address = name_section_address;
                }
            }
        }
        Ok(imports)
    }

    /// Return an iterator for the import descriptors.
    pub fn descriptors(&self) -> Result<ImportDescriptorIterator<'data>> {
        let offset = self
            .import_address
            .wrapping_sub(self.import_section_address);
        let mut data = self.import_section_data;
        data.skip(offset as usize)
            .read_error("Invalid PE import descriptor address")?;
        Ok(ImportDescriptorIterator { data, null: false })
    }

    /// Return a library name given its address.
    ///
    /// This address may be from [`pe::ImageImportDescriptor::name`].
    pub fn name(&self, address: u32) -> Result<&'data [u8]> {
        self.name_section_data
            .read_string_at(address.wrapping_sub(self.name_section_address) as usize)
            .read_error("Invalid PE import descriptor name")
    }

    /// Return a list of thunks given its address.
    ///
    /// This address may be from [`pe::ImageImportDescriptor::original_first_thunk`]
    /// or [`pe::ImageImportDescriptor::first_thunk`].
    pub fn thunks(&self, address: u32) -> Result<ImportThunkList<'data>> {
        let offset = address.wrapping_sub(self.import_section_address);
        let mut data = self.import_section_data;
        data.skip(offset as usize)
            .read_error("Invalid PE import thunk table address")?;
        Ok(ImportThunkList { data, null: false })
    }

    /// Parse a thunk.
    pub fn import<Pe: ImageNtHeaders>(&self, thunk: Pe::ImageThunkData) -> Result<Import<'data>> {
        if thunk.is_ordinal() {
            Ok(Import::Ordinal(thunk.ordinal()))
        } else {
            let (hint, name) = self.hint_name(thunk.address())?;
            Ok(Import::Name(hint, name))
        }
    }

    /// Return the hint and name at the given address.
    ///
    /// This address may be from [`pe::ImageThunkData32`] or [`pe::ImageThunkData64`].
    ///
    /// The hint is an index into the export name pointer table in the target library.
    pub fn hint_name(&self, address: u32) -> Result<(u16, &'data [u8])> {
        let offset = address.wrapping_sub(self.name_section_address);
        let mut data = self.name_section_data;
        data.skip(offset as usize)
            .read_error("Invalid PE import thunk address")?;
        let hint = data
            .read::<U16<LE>>()
            .read_error("Missing PE import thunk hint")?
            .get(LE);
        let name = data
            .read_string()
            .ok()
            .filter(|s| !s.is_empty())
            .read_error("Missing PE import thunk name")?;
        Ok((hint, name))
    }
}

/// A fallible iterator for the descriptors in the import data directory.
#[derive(Debug, Clone)]
pub struct ImportDescriptorIterator<'data> {
    data: Bytes<'data>,
    null: bool,
}

impl<'data> ImportDescriptorIterator<'data> {
    /// Return the next descriptor.
    ///
    /// Returns `Ok(None)` when a null descriptor is found.
    ///
    /// Once this returns `Ok(None)` or an error, it will always return `Ok(None)`.
    pub fn next(&mut self) -> Result<Option<&'data pe::ImageImportDescriptor>> {
        if self.null {
            return Ok(None);
        }
        let Ok(import_desc) = self.data.read::<pe::ImageImportDescriptor>() else {
            self.null = true;
            return Err(Error("Missing PE null import descriptor"));
        };
        if import_desc.is_null() {
            self.null = true;
            Ok(None)
        } else {
            Ok(Some(import_desc))
        }
    }
}

impl<'data> Iterator for ImportDescriptorIterator<'data> {
    type Item = Result<&'data pe::ImageImportDescriptor>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// A list of import thunks.
///
/// These may be in the import lookup table, or the import address table.
#[derive(Debug, Clone)]
pub struct ImportThunkList<'data> {
    data: Bytes<'data>,
    null: bool,
}

impl<'data> ImportThunkList<'data> {
    /// Get the thunk at the given index.
    pub fn get<Pe: ImageNtHeaders>(&self, index: usize) -> Result<Pe::ImageThunkData> {
        let thunk = index
            .checked_mul(mem::size_of::<Pe::ImageThunkData>())
            .and_then(|offset| self.data.read_at(offset).ok())
            .read_error("Invalid PE import thunk index")?;
        Ok(*thunk)
    }

    /// Return the first thunk in the list, and update `self` to point after it.
    ///
    /// Returns `Ok(None)` when a null thunk is found.
    ///
    /// Once this returns `Ok(None)` or an error, it will always return `Ok(None)`.
    pub fn next<Pe: ImageNtHeaders>(&mut self) -> Result<Option<Pe::ImageThunkData>> {
        if self.null {
            return Ok(None);
        }
        let Ok(thunk) = self.data.read::<Pe::ImageThunkData>() else {
            self.null = true;
            return Err(Error("Missing PE null import thunk"));
        };
        if thunk.address() == 0 {
            self.null = true;
            Ok(None)
        } else {
            Ok(Some(*thunk))
        }
    }
}

/// A parsed import thunk.
#[derive(Debug, Clone, Copy)]
pub enum Import<'data> {
    /// Import by ordinal.
    Ordinal(u16),
    /// Import by name.
    ///
    /// Includes a hint for the index into the export name pointer table in the target library.
    Name(u16, &'data [u8]),
}

/// A trait for generic access to [`pe::ImageThunkData32`] and [`pe::ImageThunkData64`].
#[allow(missing_docs)]
pub trait ImageThunkData: fmt::Debug + Pod + read::private::Sealed {
    /// Return the raw thunk value.
    fn raw(self) -> u64;

    /// Returns true if the ordinal flag is set.
    fn is_ordinal(self) -> bool;

    /// Return the ordinal portion of the thunk.
    ///
    /// Does not check the ordinal flag.
    fn ordinal(self) -> u16;

    /// Return the RVA portion of the thunk.
    ///
    /// Does not check the ordinal flag.
    fn address(self) -> u32;
}

impl read::private::Sealed for pe::ImageThunkData64 {}

impl ImageThunkData for pe::ImageThunkData64 {
    fn raw(self) -> u64 {
        self.0.get(LE)
    }

    fn is_ordinal(self) -> bool {
        self.0.get(LE) & pe::IMAGE_ORDINAL_FLAG64 != 0
    }

    fn ordinal(self) -> u16 {
        self.0.get(LE) as u16
    }

    fn address(self) -> u32 {
        self.0.get(LE) as u32 & 0x7fff_ffff
    }
}

impl read::private::Sealed for pe::ImageThunkData32 {}

impl ImageThunkData for pe::ImageThunkData32 {
    fn raw(self) -> u64 {
        self.0.get(LE).into()
    }

    fn is_ordinal(self) -> bool {
        self.0.get(LE) & pe::IMAGE_ORDINAL_FLAG32 != 0
    }

    fn ordinal(self) -> u16 {
        self.0.get(LE) as u16
    }

    fn address(self) -> u32 {
        self.0.get(LE) & 0x7fff_ffff
    }
}

/// Information for parsing a PE delay-load import table.
///
/// Returned by
/// [`DataDirectories::delay_load_import_table`](super::DataDirectories::delay_load_import_table).
#[derive(Debug, Clone)]
pub struct DelayLoadImportTable<'data> {
    section_data: Bytes<'data>,
    section_address: u32,
    import_address: u32,
}

impl<'data> DelayLoadImportTable<'data> {
    /// Create a new delay load import table parser.
    ///
    /// The import descriptors start at `import_address`.
    /// This table works in the same way the import table does: descriptors will be
    /// parsed until a null entry.
    ///
    /// `section_data` should be from the section containing `import_address`, and
    /// `section_address` should be the address of that section. Pointers within the
    /// descriptors and thunks may point to anywhere within the section data.
    pub fn new(section_data: &'data [u8], section_address: u32, import_address: u32) -> Self {
        DelayLoadImportTable {
            section_data: Bytes(section_data),
            section_address,
            import_address,
        }
    }

    /// Return an iterator for the import descriptors.
    pub fn descriptors(&self) -> Result<DelayLoadDescriptorIterator<'data>> {
        let offset = self.import_address.wrapping_sub(self.section_address);
        let mut data = self.section_data;
        data.skip(offset as usize)
            .read_error("Invalid PE delay-load import descriptor address")?;
        Ok(DelayLoadDescriptorIterator { data, null: false })
    }

    /// Return a library name given its address.
    ///
    /// This address may be from [`pe::ImageDelayloadDescriptor::dll_name_rva`].
    pub fn name(&self, address: u32) -> Result<&'data [u8]> {
        self.section_data
            .read_string_at(address.wrapping_sub(self.section_address) as usize)
            .read_error("Invalid PE import descriptor name")
    }

    /// Return a list of thunks given its address.
    ///
    /// This address may be from the INT, i.e. from
    /// [`pe::ImageDelayloadDescriptor::import_name_table_rva`].
    ///
    /// Please note that others RVA values from [`pe::ImageDelayloadDescriptor`] are used
    /// by the delay loader at runtime to store values, and thus do not point inside the same
    /// section as the INT. Calling this function on those addresses will fail.
    pub fn thunks(&self, address: u32) -> Result<ImportThunkList<'data>> {
        let offset = address.wrapping_sub(self.section_address);
        let mut data = self.section_data;
        data.skip(offset as usize)
            .read_error("Invalid PE delay load import thunk table address")?;
        Ok(ImportThunkList { data, null: false })
    }

    /// Parse a thunk.
    pub fn import<Pe: ImageNtHeaders>(&self, thunk: Pe::ImageThunkData) -> Result<Import<'data>> {
        if thunk.is_ordinal() {
            Ok(Import::Ordinal(thunk.ordinal()))
        } else {
            let (hint, name) = self.hint_name(thunk.address())?;
            Ok(Import::Name(hint, name))
        }
    }

    /// Return the hint and name at the given address.
    ///
    /// This address may be from [`pe::ImageThunkData32`] or [`pe::ImageThunkData64`].
    ///
    /// The hint is an index into the export name pointer table in the target library.
    pub fn hint_name(&self, address: u32) -> Result<(u16, &'data [u8])> {
        let offset = address.wrapping_sub(self.section_address);
        let mut data = self.section_data;
        data.skip(offset as usize)
            .read_error("Invalid PE delay load import thunk address")?;
        let hint = data
            .read::<U16<LE>>()
            .read_error("Missing PE delay load import thunk hint")?
            .get(LE);
        let name = data
            .read_string()
            .ok()
            .filter(|s| !s.is_empty())
            .read_error("Missing PE delay load import thunk name")?;
        Ok((hint, name))
    }
}

/// A fallible iterator for the descriptors in the delay-load data directory.
#[derive(Debug, Clone)]
pub struct DelayLoadDescriptorIterator<'data> {
    data: Bytes<'data>,
    null: bool,
}

impl<'data> DelayLoadDescriptorIterator<'data> {
    /// Return the next descriptor.
    ///
    /// Returns `Ok(None)` when a null descriptor is found.
    ///
    /// Once this returns `Ok(None)` or an error, it will always return `Ok(None)`.
    pub fn next(&mut self) -> Result<Option<&'data pe::ImageDelayloadDescriptor>> {
        if self.null {
            return Ok(None);
        }
        let Ok(import_desc) = self.data.read::<pe::ImageDelayloadDescriptor>() else {
            self.null = true;
            return Err(Error("Missing PE null delay-load import descriptor"));
        };
        if import_desc.is_null() {
            self.null = true;
            Ok(None)
        } else {
            Ok(Some(import_desc))
        }
    }
}

impl<'data> Iterator for DelayLoadDescriptorIterator<'data> {
    type Item = Result<&'data pe::ImageDelayloadDescriptor>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// An iterator for the import libraries in a [`PeFile32`](super::PeFile32).
pub type PeImportLibraryIterator32<'data, 'file, R = &'data [u8]> =
    PeImportLibraryIterator<'data, 'file, pe::ImageNtHeaders32, R>;
/// An iterator for the import libraries in a [`PeFile64`](super::PeFile64).
pub type PeImportLibraryIterator64<'data, 'file, R = &'data [u8]> =
    PeImportLibraryIterator<'data, 'file, pe::ImageNtHeaders64, R>;

/// An iterator for the import libraries in a [`PeFile`].
pub struct PeImportLibraryIterator<'data, 'file, Pe, R = &'data [u8]>
where
    Pe: ImageNtHeaders,
    R: ReadRef<'data>,
{
    table: Option<ImportTable<'data>>,
    descs: Option<ImportDescriptorIterator<'data>>,
    delay_table: Option<DelayLoadImportTable<'data>>,
    delay_descs: Option<DelayLoadDescriptorIterator<'data>>,
    marker: PhantomData<(&'file (), Pe, R)>,
}

impl<'data, 'file, Pe, R> PeImportLibraryIterator<'data, 'file, Pe, R>
where
    Pe: ImageNtHeaders,
    R: ReadRef<'data>,
{
    pub(super) fn new(file: &'file PeFile<'data, Pe, R>) -> Result<Self> {
        let table = file.import_table()?;
        let descs = table
            .as_ref()
            .map(|table| table.descriptors())
            .transpose()?;
        let delay_table = file.delay_load_import_table()?;
        let delay_descs = delay_table
            .as_ref()
            .map(|table| table.descriptors())
            .transpose()?;
        Ok(PeImportLibraryIterator {
            table,
            descs,
            delay_table,
            delay_descs,
            marker: PhantomData,
        })
    }

    fn next(&mut self) -> read::Result<Option<read::ImportLibrary<'data>>> {
        // All iterators used in this method fuse after error, and any other errors only
        // occur after an iterator has made progress, so this method doesn't need to fuse
        // itself.
        if let Some(table) = self.table.as_ref() {
            if let Some(descs) = self.descs.as_mut() {
                if let Some(desc) = descs.next()? {
                    return Ok(Some(read::ImportLibrary {
                        name: table.name(desc.name.get(LE))?,
                        flags: read::ImportLibraryFlags::Pe { delay: false },
                    }));
                }
                self.descs = None;
            }
            self.table = None;
        }
        if let Some(table) = self.delay_table.as_ref() {
            if let Some(descs) = self.delay_descs.as_mut() {
                if let Some(desc) = descs.next()? {
                    if desc.attributes.get(LE) & pe::IMAGE_DELAYLOAD_RVA_BASED == 0 {
                        return Err(Error("Unsupported PE delay-load non-RVA based descriptor"));
                    }
                    return Ok(Some(read::ImportLibrary {
                        name: table.name(desc.dll_name_rva.get(LE))?,
                        flags: read::ImportLibraryFlags::Pe { delay: true },
                    }));
                }
                self.delay_descs = None;
            }
            self.delay_table = None;
        }
        Ok(None)
    }
}

impl<'data, 'file, Pe, R> fmt::Debug for PeImportLibraryIterator<'data, 'file, Pe, R>
where
    Pe: ImageNtHeaders,
    R: ReadRef<'data>,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PeImportLibraryIterator").finish()
    }
}

impl<'data, 'file, Pe, R> Iterator for PeImportLibraryIterator<'data, 'file, Pe, R>
where
    Pe: ImageNtHeaders,
    R: ReadRef<'data>,
{
    type Item = Result<read::ImportLibrary<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// An iterator for the imports in a [`PeFile32`](super::PeFile32).
pub type PeImportIterator32<'data, 'file, R = &'data [u8]> =
    PeImportIterator<'data, 'file, pe::ImageNtHeaders32, R>;
/// An iterator for the imports in a [`PeFile64`](super::PeFile64).
pub type PeImportIterator64<'data, 'file, R = &'data [u8]> =
    PeImportIterator<'data, 'file, pe::ImageNtHeaders64, R>;

/// An iterator for the imports in a [`PeFile`].
pub struct PeImportIterator<'data, 'file, Pe, R = &'data [u8]>
where
    Pe: ImageNtHeaders,
    R: ReadRef<'data>,
{
    table: Option<ImportTable<'data>>,
    descs: Option<ImportDescriptorIterator<'data>>,
    delay_table: Option<DelayLoadImportTable<'data>>,
    delay_descs: Option<DelayLoadDescriptorIterator<'data>>,
    thunks: Option<ImportThunkList<'data>>,
    library: &'data [u8],
    marker: PhantomData<(&'file (), Pe, R)>,
}

impl<'data, 'file, Pe, R> PeImportIterator<'data, 'file, Pe, R>
where
    Pe: ImageNtHeaders,
    R: ReadRef<'data>,
{
    pub(super) fn new(file: &'file PeFile<'data, Pe, R>) -> Result<Self> {
        let table = file.import_table()?;
        let descs = table
            .as_ref()
            .map(|table| table.descriptors())
            .transpose()?;
        let delay_table = file.delay_load_import_table()?;
        let delay_descs = delay_table
            .as_ref()
            .map(|table| table.descriptors())
            .transpose()?;
        Ok(PeImportIterator {
            table,
            descs,
            delay_table,
            delay_descs,
            thunks: None,
            library: &[],
            marker: PhantomData,
        })
    }

    fn next(&mut self) -> read::Result<Option<read::Import<'data>>> {
        // All iterators used in this method fuse after error, and any other errors only
        // occur after an iterator has made progress, so this method doesn't need to fuse
        // itself.
        while let Some(table) = self.table.as_ref() {
            if let Some(thunks) = self.thunks.as_mut() {
                if let Some(thunk) = thunks.next::<Pe>()? {
                    return Ok(Some(self.import(table.import::<Pe>(thunk)?, false)));
                }
                self.thunks = None;
            }
            if let Some(descs) = self.descs.as_mut() {
                if let Some(desc) = descs.next()? {
                    self.library = table.name(desc.name.get(LE))?;
                    let mut first_thunk = desc.original_first_thunk.get(LE);
                    if first_thunk == 0 {
                        first_thunk = desc.first_thunk.get(LE);
                    }
                    self.thunks = Some(table.thunks(first_thunk)?);
                    continue;
                }
                self.descs = None;
            }
            self.table = None;
        }
        while let Some(table) = self.delay_table.as_ref() {
            if let Some(thunks) = self.thunks.as_mut() {
                if let Some(thunk) = thunks.next::<Pe>()? {
                    return Ok(Some(self.import(table.import::<Pe>(thunk)?, true)));
                }
                self.thunks = None;
            }
            if let Some(descs) = self.delay_descs.as_mut() {
                if let Some(desc) = descs.next()? {
                    if desc.attributes.get(LE) & pe::IMAGE_DELAYLOAD_RVA_BASED == 0 {
                        return Err(Error("Unsupported PE delay-load non-RVA based descriptor"));
                    }
                    self.library = table.name(desc.dll_name_rva.get(LE))?;
                    self.thunks = Some(table.thunks(desc.import_name_table_rva.get(LE))?);
                    continue;
                }
                self.delay_descs = None;
            }
            self.delay_table = None;
        }
        Ok(None)
    }

    fn import(&self, import: Import<'data>, delay: bool) -> read::Import<'data> {
        let name = match import {
            Import::Ordinal(ordinal) => read::NameOrOrdinal::Ordinal(ordinal),
            Import::Name(_hint, name) => read::NameOrOrdinal::Name(name),
        };
        read::Import {
            library: self.library,
            name,
            weak: false,
            flags: read::ImportFlags::Pe { delay },
        }
    }
}

impl<'data, 'file, Pe, R> fmt::Debug for PeImportIterator<'data, 'file, Pe, R>
where
    Pe: ImageNtHeaders,
    R: ReadRef<'data>,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PeImportIterator").finish()
    }
}

impl<'data, 'file, Pe, R> Iterator for PeImportIterator<'data, 'file, Pe, R>
where
    Pe: ImageNtHeaders,
    R: ReadRef<'data>,
{
    type Item = Result<read::Import<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}
