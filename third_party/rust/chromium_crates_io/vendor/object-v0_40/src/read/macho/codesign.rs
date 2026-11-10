use crate::endian::{BigEndian, Endian};
use crate::macho;
use crate::read::{Bytes, Error, ReadError, ReadRef, Result};

impl<E: Endian> macho::LinkeditDataCommand<E> {
    /// Parse the data referenced by an `LC_CODE_SIGNATURE` load command.
    pub fn code_signature<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<CodeSignature<'data>> {
        let signature_data = data
            .read_bytes_at(
                self.dataoff.get(endian).into(),
                self.datasize.get(endian).into(),
            )
            .read_error("Invalid Mach-O code signature offset or size")?;
        CodeSignature::parse(signature_data)
    }
}

/// The parsed data of an `LC_CODE_SIGNATURE` load command.
///
/// This is a [`macho::CsSuperBlob`] header and an index table for the blobs.
#[derive(Debug, Clone, Copy)]
pub struct CodeSignature<'data> {
    data: Bytes<'data>,
    header: &'data macho::CsSuperBlob,
    index: &'data [macho::CsBlobIndex],
}

impl<'data> CodeSignature<'data> {
    /// Parse the code signature data.
    ///
    /// `data` should be the data referenced by the `LC_CODE_SIGNATURE` load command.
    pub fn parse(data: &'data [u8]) -> Result<Self> {
        let mut bytes = Bytes(data);
        let header = bytes
            .read::<macho::CsSuperBlob>()
            .read_error("Invalid Mach-O code signature super blob")?;
        if header.magic.get(BigEndian) != macho::CSMAGIC_EMBEDDED_SIGNATURE {
            return Err(Error("Unsupported Mach-O code signature magic"));
        }
        let index = bytes
            .read_slice::<macho::CsBlobIndex>(header.count.get(BigEndian) as usize)
            .read_error("Invalid Mach-O code signature blob count")?;
        Ok(CodeSignature {
            data: Bytes(data),
            header,
            index,
        })
    }

    /// Return the super blob header.
    pub fn header(&self) -> &'data macho::CsSuperBlob {
        self.header
    }

    /// Return the blob index.
    pub fn index(&self) -> &'data [macho::CsBlobIndex] {
        self.index
    }

    /// Return an iterator over the blobs in the code signature.
    pub fn blobs(&self) -> CodeSignatureBlobIterator<'data> {
        CodeSignatureBlobIterator {
            data: self.data,
            index: self.index.iter(),
        }
    }
}

/// An iterator over the blobs in a [`CodeSignature`].
///
/// Returned by [`CodeSignature::blobs`].
#[derive(Debug, Clone)]
pub struct CodeSignatureBlobIterator<'data> {
    data: Bytes<'data>,
    index: core::slice::Iter<'data, macho::CsBlobIndex>,
}

impl<'data> CodeSignatureBlobIterator<'data> {
    /// Return the next blob.
    pub fn next(&mut self) -> Result<Option<CodeSignatureBlob<'data>>> {
        let Some(index) = self.index.next() else {
            return Ok(None);
        };
        let slot = index.slot.get(BigEndian);
        let offset = index.offset.get(BigEndian);
        let header = self
            .data
            .read_at::<macho::CsGenericBlob>(offset as usize)
            .read_error("Invalid Mach-O code signature blob offset")?;
        let data = self
            .data
            .read_bytes_at(offset as usize, header.length.get(BigEndian) as usize)
            .read_error("Invalid Mach-O code signature blob length")?;
        Ok(Some(CodeSignatureBlob {
            slot,
            offset,
            magic: header.magic.get(BigEndian),
            data,
        }))
    }
}

impl<'data> Iterator for CodeSignatureBlobIterator<'data> {
    type Item = Result<CodeSignatureBlob<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// A single blob in a [`CodeSignature`].
///
/// Returned by [`CodeSignature::blobs`].
#[derive(Debug, Clone, Copy)]
pub struct CodeSignatureBlob<'data> {
    slot: macho::CsSlot,
    offset: u32,
    magic: u32,
    data: Bytes<'data>,
}

impl<'data> CodeSignatureBlob<'data> {
    /// Return the slot type from the super blob index entry.
    pub fn slot(&self) -> macho::CsSlot {
        self.slot
    }

    /// Return the offset from the super blob index entry.
    pub fn offset(&self) -> u32 {
        self.offset
    }

    /// Return the magic number at the start of the blob.
    ///
    /// This is one of the `CSMAGIC_*` constants.
    pub fn magic(&self) -> u32 {
        self.magic
    }

    /// Return the blob data, including the magic number and length.
    pub fn data(&self) -> &'data [u8] {
        self.data.0
    }

    /// Return the blob data, excluding the magic number and length.
    pub fn contents(&self) -> &'data [u8] {
        self.data
            .0
            .get(core::mem::size_of::<macho::CsGenericBlob>()..)
            .unwrap_or(&[])
    }

    /// Parse this blob as a code directory.
    ///
    /// Returns `None` if `magic` is not [`macho::CSMAGIC_CODEDIRECTORY`].
    pub fn code_directory(&self) -> Result<Option<CodeDirectory<'data>>> {
        if self.magic != macho::CSMAGIC_CODEDIRECTORY {
            return Ok(None);
        }
        CodeDirectory::parse(self.data.0).map(Some)
    }
}

/// A code directory blob in a [`CodeSignature`].
///
/// Returned by [`CodeSignatureBlob::code_directory`].
#[derive(Debug, Clone, Copy)]
pub struct CodeDirectory<'data> {
    data: Bytes<'data>,
    header: &'data macho::CsCodeDirectoryV0,
    v1: Option<&'data macho::CsCodeDirectoryV1>,
    v2: Option<&'data macho::CsCodeDirectoryV2>,
    v3: Option<&'data macho::CsCodeDirectoryV3>,
    v4: Option<&'data macho::CsCodeDirectoryV4>,
}

impl<'data> CodeDirectory<'data> {
    /// Parse a code directory blob.
    ///
    /// `data` should be the entire blob, including the magic number and length.
    pub fn parse(data: &'data [u8]) -> Result<Self> {
        let mut header_data = Bytes(data);
        let header = header_data
            .read::<macho::CsCodeDirectoryV0>()
            .read_error("Invalid Mach-O code directory")?;
        if header.magic.get(BigEndian) != macho::CSMAGIC_CODEDIRECTORY {
            return Err(Error("Unsupported Mach-O code directory magic"));
        }
        let version = header.version.get(BigEndian);

        let v1 = if version >= macho::CS_SUPPORTSSCATTER {
            Some(
                header_data
                    .read::<macho::CsCodeDirectoryV1>()
                    .read_error("Invalid Mach-O code directory length")?,
            )
        } else {
            None
        };
        let v2 = if version >= macho::CS_SUPPORTSTEAMID {
            Some(
                header_data
                    .read::<macho::CsCodeDirectoryV2>()
                    .read_error("Invalid Mach-O code directory length")?,
            )
        } else {
            None
        };
        let v3 = if version >= macho::CS_SUPPORTSCODELIMIT64 {
            Some(
                header_data
                    .read::<macho::CsCodeDirectoryV3>()
                    .read_error("Invalid Mach-O code directory length")?,
            )
        } else {
            None
        };
        let v4 = if version >= macho::CS_SUPPORTSEXECSEG {
            Some(
                header_data
                    .read::<macho::CsCodeDirectoryV4>()
                    .read_error("Invalid Mach-O code directory length")?,
            )
        } else {
            None
        };

        Ok(CodeDirectory {
            data: Bytes(data),
            header,
            v1,
            v2,
            v3,
            v4,
        })
    }

    /// Return the code directory header.
    pub fn header(&self) -> &'data macho::CsCodeDirectoryV0 {
        self.header
    }

    /// Return the version of the code directory.
    pub fn version(&self) -> macho::CsVersion {
        self.header.version.get(BigEndian)
    }

    /// Return the offset of the optional scatter vector.
    ///
    /// Returns `None` if the version does not support this field.
    pub fn scatter_offset(&self) -> Option<u32> {
        Some(self.v1?.scatter_offset.get(BigEndian))
    }

    /// Return the offset of the optional team identifier.
    ///
    /// Returns `None` if the version does not support this field.
    pub fn team_offset(&self) -> Option<u32> {
        Some(self.v2?.team_offset.get(BigEndian))
    }

    /// Return the limit to the main image signature range.
    ///
    /// This handles both the 32-bit and 64-bit fields.
    pub fn code_limit(&self) -> u64 {
        match self.code_limit64() {
            Some(code_limit64) if code_limit64 != 0 => code_limit64,
            _ => self.header.code_limit.get(BigEndian).into(),
        }
    }

    /// Return the 64-bit limit to the main image signature range.
    ///
    /// This is zero for images that are smaller than 4GB. Use [`Self::code_limit`]
    /// to obtain the limit for any image.
    ///
    /// Returns `None` if the version does not support this field.
    pub fn code_limit64(&self) -> Option<u64> {
        Some(self.v3?.code_limit64.get(BigEndian))
    }

    /// Return the executable segment fields.
    ///
    /// Returns `None` if the version does not support these fields.
    pub fn exec_seg(&self) -> Option<&'data macho::CsCodeDirectoryV4> {
        self.v4
    }

    /// Return the identifier string.
    pub fn ident(&self) -> Result<&'data [u8]> {
        self.data
            .read_string_at(self.header.ident_offset.get(BigEndian) as usize)
            .read_error("Invalid Mach-O code directory identifier offset")
    }

    /// Return the team identifier string, if present.
    pub fn team_id(&self) -> Result<Option<&'data [u8]>> {
        let team_offset = self.team_offset().unwrap_or(0);
        if team_offset == 0 {
            return Ok(None);
        }
        let team_id = self
            .data
            .read_string_at(team_offset as usize)
            .read_error("Invalid Mach-O code directory team identifier offset")?;
        Ok(Some(team_id))
    }

    /// Return the hash for a special slot.
    ///
    /// Special slots are identified by a [`macho::CsSlot`] value in the range
    /// `1..=header.n_special_slots`.
    ///
    /// Returns an error if slot is not present.
    pub fn special_hash(&self, slot: macho::CsSlot) -> Result<&'data [u8]> {
        if slot.0 < 1 || slot.0 > self.header.n_special_slots.get(BigEndian) {
            return Err(Error("Invalid Mach-O code directory special hash index"));
        }
        let offset = slot
            .0
            .checked_mul(u32::from(self.header.hash_size))
            .and_then(|offset| self.header.hash_offset.get(BigEndian).checked_sub(offset))
            .read_error("Invalid Mach-O code directory hash size")?;
        Ok(self
            .data
            .read_bytes_at(offset as usize, self.header.hash_size as usize)
            .read_error("Invalid Mach-O code directory hash offset")?
            .0)
    }

    /// Return the code hash for the given page index.
    ///
    /// Returns an error if index is greater than or equal to `n_code_slots`.
    pub fn code_hash(&self, index: u32) -> Result<&'data [u8]> {
        if self.scatter_offset().unwrap_or(0) != 0 {
            return Err(Error("Unsupported Mach-O code directory scatter"));
        }
        if index >= self.header.n_code_slots.get(BigEndian) {
            return Err(Error("Invalid Mach-O code directory hash index"));
        }
        let offset = index
            .checked_mul(u32::from(self.header.hash_size))
            .and_then(|offset| offset.checked_add(self.header.hash_offset.get(BigEndian)))
            .read_error("Invalid Mach-O code directory hash size")?;
        Ok(self
            .data
            .read_bytes_at(offset as usize, self.header.hash_size as usize)
            .read_error("Invalid Mach-O code directory hash offset")?
            .0)
    }
}
