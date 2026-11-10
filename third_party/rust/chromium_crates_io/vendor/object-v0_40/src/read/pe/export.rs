use alloc::borrow::Cow;
use alloc::vec::Vec;
use core::fmt;
use core::marker::PhantomData;

use crate::endian::{LittleEndian as LE, U16, U32};
use crate::pe;
use crate::read::{self, ByteString, Bytes, Error, NameOrOrdinal, ReadError, ReadRef, Result};

use super::{ImageNtHeaders, PeFile};

/// The ordinal for an address entry in an [`ExportTable`].
#[derive(Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ExportOrdinal(pub u16);

wrap!(ExportOrdinal, u16);

impl fmt::Display for ExportOrdinal {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl fmt::Debug for ExportOrdinal {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

/// The index of an address entry in an [`ExportTable`].
#[derive(Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ExportAddressIndex(pub u16);

wrap!(ExportAddressIndex, u16);

impl fmt::Display for ExportAddressIndex {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl fmt::Debug for ExportAddressIndex {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

/// Where an export is pointing to.
#[derive(Clone, Copy)]
pub enum ExportTarget<'data> {
    /// The address of the export, relative to the image base.
    Address(u32),
    /// Forwarded to an export ordinal in another DLL.
    ///
    /// This gives the name of the DLL, and the ordinal.
    ForwardByOrdinal(&'data [u8], ExportOrdinal),
    /// Forwarded to an export name in another DLL.
    ///
    /// This gives the name of the DLL, and the export name.
    ForwardByName(&'data [u8], &'data [u8]),
}

impl<'data> ExportTarget<'data> {
    /// Returns true if the target is an address.
    pub fn is_address(&self) -> bool {
        match self {
            ExportTarget::Address(_) => true,
            _ => false,
        }
    }

    /// Returns true if the export is forwarded to another DLL.
    pub fn is_forward(&self) -> bool {
        !self.is_address()
    }
}

/// An export from a PE file.
///
/// There are multiple kinds of PE exports (with or without a name, and local or forwarded).
#[derive(Clone, Copy)]
pub struct Export<'data> {
    /// The ordinal of the export.
    ///
    /// These are sequential, starting at a base specified in the DLL.
    pub ordinal: ExportOrdinal,
    /// The name of the export, if known.
    pub name: Option<&'data [u8]>,
    /// The target of this export.
    pub target: ExportTarget<'data>,
}

impl<'a> fmt::Debug for Export<'a> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::result::Result<(), core::fmt::Error> {
        f.debug_struct("Export")
            .field("ordinal", &self.ordinal)
            .field("name", &self.name.map(ByteString))
            .field("target", &self.target)
            .finish()
    }
}

impl<'a> fmt::Debug for ExportTarget<'a> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::result::Result<(), core::fmt::Error> {
        match self {
            ExportTarget::Address(address) => write!(f, "Address({:#x})", address),
            ExportTarget::ForwardByOrdinal(library, ordinal) => write!(
                f,
                "ForwardByOrdinal({:?}.#{})",
                ByteString(library),
                ordinal
            ),
            ExportTarget::ForwardByName(library, name) => write!(
                f,
                "ForwardByName({:?}.{:?})",
                ByteString(library),
                ByteString(name)
            ),
        }
    }
}

/// A partially parsed PE export table.
///
/// Returned by [`DataDirectories::export_table`](super::DataDirectories::export_table).
#[derive(Debug, Clone)]
pub struct ExportTable<'data> {
    data: Bytes<'data>,
    virtual_address: u32,
    directory: &'data pe::ImageExportDirectory,
    ordinal_base: u16,
    addresses: &'data [U32<LE>],
    names: &'data [U32<LE>],
    name_ordinals: &'data [U16<LE, ExportAddressIndex>],
}

impl<'data> ExportTable<'data> {
    /// Parse the export table given its section data and address.
    pub fn parse(data: &'data [u8], virtual_address: u32) -> Result<Self> {
        let directory = Self::parse_directory(data)?;
        let data = Bytes(data);

        let Ok(ordinal_base) = u16::try_from(directory.base.get(LE)) else {
            return Err(Error("Invalid PE export ordinal base"));
        };

        let mut addresses = &[][..];
        let address_of_functions = directory.address_of_functions.get(LE);
        if address_of_functions != 0 {
            let number = directory.number_of_functions.get(LE) as usize;
            // Ordinals must fit in a u16, so this bounds every valid address index too.
            let max_index = usize::from(u16::MAX - ordinal_base);
            if number > max_index + 1 {
                return Err(Error("Invalid PE export number of functions"));
            }
            addresses = data
                .read_slice_at::<U32<_>>(
                    address_of_functions.wrapping_sub(virtual_address) as usize,
                    number,
                )
                .read_error("Invalid PE export address table")?;
        }

        let mut names = &[][..];
        let mut name_ordinals = &[][..];
        let address_of_names = directory.address_of_names.get(LE);
        let address_of_name_ordinals = directory.address_of_name_ordinals.get(LE);
        if address_of_names != 0 {
            if address_of_name_ordinals == 0 {
                return Err(Error("Missing PE export ordinal table"));
            }

            let number = directory.number_of_names.get(LE) as usize;
            names = data
                .read_slice_at::<U32<_>>(
                    address_of_names.wrapping_sub(virtual_address) as usize,
                    number,
                )
                .read_error("Invalid PE export name pointer table")?;
            name_ordinals = data
                .read_slice_at::<U16<_, _>>(
                    address_of_name_ordinals.wrapping_sub(virtual_address) as usize,
                    number,
                )
                .read_error("Invalid PE export ordinal table")?;
        }

        Ok(ExportTable {
            data,
            virtual_address,
            directory,
            ordinal_base,
            addresses,
            names,
            name_ordinals,
        })
    }

    /// Parse the export directory given its section data.
    pub fn parse_directory(data: &'data [u8]) -> Result<&'data pe::ImageExportDirectory> {
        data.read_at::<pe::ImageExportDirectory>(0)
            .read_error("Invalid PE export dir size")
    }

    /// Returns the header of the export table.
    pub fn directory(&self) -> &'data pe::ImageExportDirectory {
        self.directory
    }

    /// Returns the base value of ordinals.
    ///
    /// Adding this to an address index will give an ordinal.
    pub fn ordinal_base(&self) -> u16 {
        self.ordinal_base
    }

    /// Add the ordinal base to an address index.
    ///
    /// Returns an error if the index is out of bounds for the address table.
    pub fn ordinal_from_index(&self, index: ExportAddressIndex) -> Result<ExportOrdinal> {
        if usize::from(index.0) >= self.addresses.len() {
            return Err(Error("Invalid PE export address index"));
        }
        Ok(ExportOrdinal(self.ordinal_base + index.0))
    }

    /// Subtract the ordinal base from an ordinal to obtain an address index.
    ///
    /// Returns an error if the resulting index is out of bounds for the address table.
    pub fn index_from_ordinal(&self, ordinal: ExportOrdinal) -> Result<ExportAddressIndex> {
        let index = ordinal.0.wrapping_sub(self.ordinal_base);
        if usize::from(index) >= self.addresses.len() {
            return Err(Error("Invalid PE export ordinal"));
        }
        Ok(ExportAddressIndex(index))
    }

    /// Returns the unparsed address table.
    ///
    /// An address table entry may be a local address, or the address of a forwarded export entry.
    /// See [`Self::is_forward`] and [`Self::target_from_address`].
    pub fn addresses(&self) -> &'data [U32<LE>] {
        self.addresses
    }

    /// Returns an iterator for the entries in the address table.
    pub fn address_iter(
        &self,
    ) -> impl Iterator<Item = (ExportAddressIndex, ExportOrdinal, u32)> + use<'data> {
        let ordinal_base = self.ordinal_base;
        self.addresses.iter().enumerate().map(move |(i, x)| {
            (
                ExportAddressIndex(i as u16),
                ExportOrdinal(ordinal_base + i as u16),
                x.get(LE),
            )
        })
    }

    /// Returns the unparsed name pointer table.
    ///
    /// A name pointer table entry can be used with [`Self::name_from_pointer`].
    pub fn name_pointers(&self) -> &'data [U32<LE>] {
        self.names
    }

    /// Returns the unparsed ordinal table.
    ///
    /// An ordinal table entry is a 0-based index into the address table.
    /// See [`Self::address_by_index`] and [`Self::target_by_index`].
    pub fn name_ordinals(&self) -> &'data [U16<LE, ExportAddressIndex>] {
        self.name_ordinals
    }

    /// Returns an iterator for the entries in the name pointer table and ordinal table.
    ///
    /// A name pointer table entry can be used with [`Self::name_from_pointer`].
    ///
    /// An ordinal table entry is a 0-based index into the address table.
    /// See [`Self::address_by_index`] and [`Self::target_by_index`].
    pub fn name_iter(&self) -> impl Iterator<Item = (u32, ExportAddressIndex)> + use<'data> {
        self.names
            .iter()
            .map(|x| x.get(LE))
            .zip(self.name_ordinals.iter().map(|x| x.get(LE)))
    }

    /// Returns the export address table entry at the given address index.
    ///
    /// This may be a local address, or the address of a forwarded export entry.
    /// See [`Self::is_forward`] and [`Self::target_from_address`].
    ///
    /// `index` is a 0-based index into the export address table.
    pub fn address_by_index(&self, index: ExportAddressIndex) -> Result<u32> {
        Ok(self
            .addresses
            .get(index.0 as usize)
            .read_error("Invalid PE export address index")?
            .get(LE))
    }

    /// Returns the export address table entry at the given ordinal.
    ///
    /// This may be a local address, or the address of a forwarded export entry.
    /// See [`Self::is_forward`] and [`Self::target_from_address`].
    pub fn address_by_ordinal(&self, ordinal: ExportOrdinal) -> Result<u32> {
        let index = self.index_from_ordinal(ordinal)?;
        self.address_by_index(index)
    }

    /// Returns the target of the export at the given address index.
    ///
    /// `index` is a 0-based index into the export address table.
    pub fn target_by_index(&self, index: ExportAddressIndex) -> Result<ExportTarget<'data>> {
        self.target_from_address(self.address_by_index(index)?)
    }

    /// Returns the target of the export at the given ordinal.
    pub fn target_by_ordinal(&self, ordinal: ExportOrdinal) -> Result<ExportTarget<'data>> {
        self.target_from_address(self.address_by_ordinal(ordinal)?)
    }

    /// Convert an export address table entry into a target.
    pub fn target_from_address(&self, address: u32) -> Result<ExportTarget<'data>> {
        Ok(if let Some(forward) = self.forward_string(address)? {
            let i = forward
                .iter()
                .rposition(|x| *x == b'.')
                .read_error("Missing PE forwarded export separator")?;
            let library = &forward[..i];
            match &forward[i + 1..] {
                [b'#', digits @ ..] => {
                    let ordinal =
                        parse_ordinal(digits).read_error("Invalid PE forwarded export ordinal")?;
                    ExportTarget::ForwardByOrdinal(library, ordinal)
                }
                [] => {
                    return Err(Error("Missing PE forwarded export name"));
                }
                name => ExportTarget::ForwardByName(library, name),
            }
        } else {
            ExportTarget::Address(address)
        })
    }

    fn forward_offset(&self, address: u32) -> Option<usize> {
        let offset = address.wrapping_sub(self.virtual_address) as usize;
        if offset < self.data.len() {
            Some(offset)
        } else {
            None
        }
    }

    /// Return true if the export address table entry is a forward.
    pub fn is_forward(&self, address: u32) -> bool {
        self.forward_offset(address).is_some()
    }

    /// Return the forward string if the export address table entry is a forward.
    pub fn forward_string(&self, address: u32) -> Result<Option<&'data [u8]>> {
        if let Some(offset) = self.forward_offset(address) {
            self.data
                .read_string_at(offset)
                .read_error("Invalid PE forwarded export address")
                .map(Some)
        } else {
            Ok(None)
        }
    }

    /// Convert an export name pointer table entry into a name.
    pub fn name_from_pointer(&self, name_pointer: u32) -> Result<&'data [u8]> {
        let offset = name_pointer.wrapping_sub(self.virtual_address);
        self.data
            .read_string_at(offset as usize)
            .read_error("Invalid PE export name pointer")
    }

    /// Returns the parsed exports in this table.
    pub fn exports(&self) -> Result<Vec<Export<'data>>> {
        // First, let's list all exports.
        let mut exports = Vec::new();
        for (_index, ordinal, address) in self.address_iter() {
            let target = self.target_from_address(address)?;
            exports.push(Export {
                ordinal,
                target,
                // Might be populated later.
                name: None,
            });
        }

        // Now, check whether some (or all) of them have an associated name.
        // `ordinal_index` is a 0-based index into `addresses`.
        for (name_pointer, ordinal_index) in self.name_iter() {
            let name = self.name_from_pointer(name_pointer)?;
            exports
                .get_mut(ordinal_index.0 as usize)
                .read_error("Invalid PE export ordinal")?
                .name = Some(name);
        }

        Ok(exports)
    }
}

fn parse_ordinal(digits: &[u8]) -> Option<ExportOrdinal> {
    if digits.is_empty() {
        return None;
    }
    let mut result: u16 = 0;
    for &c in digits {
        let x = (c as char).to_digit(10)? as u16;
        result = result.checked_mul(10)?.checked_add(x)?;
    }
    Some(ExportOrdinal(result))
}

/// An iterator for the exports in a [`PeFile`].
pub struct PeExportIterator<'data, 'file, R = &'data [u8]>
where
    R: ReadRef<'data>,
{
    image_base: u64,
    table: Option<ExportTable<'data>>,
    index: usize,
    // false: iterating names, true: iterating addresses
    addresses: bool,
    seen: Vec<bool>,
    marker: PhantomData<(&'file (), R)>,
}

impl<'data, 'file, R> PeExportIterator<'data, 'file, R>
where
    R: ReadRef<'data>,
{
    pub(super) fn new<Pe: ImageNtHeaders>(file: &'file PeFile<'data, Pe, R>) -> Result<Self> {
        let table = file.export_table()?;
        let seen = match &table {
            Some(table) => vec![false; table.addresses().len()],
            None => Vec::new(),
        };
        Ok(PeExportIterator {
            image_base: file.common.image_base,
            table,
            index: 0,
            addresses: false,
            seen,
            marker: PhantomData,
        })
    }

    fn next(&mut self) -> read::Result<Option<read::Export<'data>>> {
        let Some(table) = &self.table else {
            return Ok(None);
        };
        if !self.addresses {
            let index = self.index;
            if let (Some(name_pointer), Some(address_index)) = (
                table.name_pointers().get(index),
                table.name_ordinals().get(index),
            ) {
                // Ensure progress is made, so errors after here don't need to terminate iteration.
                self.index += 1;

                let address_index = address_index.get(LE);
                if let Some(seen) = self.seen.get_mut(usize::from(address_index.0)) {
                    *seen = true;
                }

                let address = table.address_by_index(address_index)?;
                let ordinal = table.ordinal_from_index(address_index)?;
                let name = table.name_from_pointer(name_pointer.get(LE))?;
                let name = NameOrOrdinal::Name(Cow::Borrowed(name));
                return self.export(table, address, ordinal, name);
            } else {
                self.index = 0;
                self.addresses = true;
            }
        }
        loop {
            let index = self.index;
            let Some(address) = table.addresses().get(index).map(|x| x.get(LE)) else {
                return Ok(None);
            };
            // Ensure progress is made, so errors after here don't need to terminate iteration.
            self.index += 1;

            if self.seen[index] || address == 0 {
                continue;
            }

            let ordinal = table.ordinal_from_index(ExportAddressIndex(index as u16))?;
            return self.export(table, address, ordinal, NameOrOrdinal::Ordinal(ordinal.0));
        }
    }

    fn export(
        &self,
        table: &ExportTable<'data>,
        address: u32,
        ordinal: ExportOrdinal,
        name: NameOrOrdinal<Cow<'data, [u8]>>,
    ) -> read::Result<Option<read::Export<'data>>> {
        let target = match table.target_from_address(address)? {
            ExportTarget::Address(address) => read::ExportTarget::Address {
                address: self.image_base.wrapping_add(address.into()),
            },
            ExportTarget::ForwardByName(library, name) => read::ExportTarget::Reexport {
                library,
                name: NameOrOrdinal::Name(name),
            },
            ExportTarget::ForwardByOrdinal(library, ordinal) => read::ExportTarget::Reexport {
                library,
                name: NameOrOrdinal::Ordinal(ordinal.0),
            },
        };
        Ok(Some(read::Export {
            name,
            target,
            weak: false,
            flags: read::ExportFlags::Pe { ordinal },
        }))
    }
}

impl<'data, 'file, R: ReadRef<'data>> fmt::Debug for PeExportIterator<'data, 'file, R> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PeExportIterator").finish()
    }
}

impl<'data, 'file, R: ReadRef<'data>> Iterator for PeExportIterator<'data, 'file, R> {
    type Item = Result<read::Export<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}
