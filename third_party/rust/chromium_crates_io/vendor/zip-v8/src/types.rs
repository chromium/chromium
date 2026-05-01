//! Types that specify what is contained in a ZIP.

use crate::CompressionMethod;
use crate::cp437::FromCp437;
use crate::datetime::DateTime;
use crate::extra_fields::ExtraField;
use crate::path::{enclosed_name, file_name_sanitized};
use crate::read::readers::SeekableTake;
use crate::result::{ZipError, ZipResult, invalid};
use crate::spec::is_dir;
use crate::spec::{
    self, FixedSizeBlock, Magic, Zip64DataDescriptorBlock, ZipCentralEntryBlock,
    ZipDataDescriptorBlock, ZipFlags, ZipLocalEntryBlock,
};
use crate::write::FileOptionExtension;
use crate::zipcrypto::EncryptWith;
use core::fmt::Debug;
use core::fmt::Display;
use std::ffi::OsStr;
use std::io::{Read, Seek, SeekFrom, Take};
use std::path::{Path, PathBuf};
use std::sync::{Arc, OnceLock};

pub(crate) mod ffi {
    pub const S_IFDIR: u32 = 0o0_040_000;
    pub const S_IFREG: u32 = 0o0_100_000;
    pub const S_IFLNK: u32 = 0o0_120_000;
}

pub(crate) struct ZipRawValues {
    pub(crate) crc32: u32,
    pub(crate) compressed_size: u64,
    pub(crate) uncompressed_size: u64,
}

/// System inside `version made by` (upper byte)
/// Reference: 4.4.2.2
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
#[allow(clippy::upper_case_acronyms)]
#[repr(u8)]
pub enum System {
    /// `MS-DOS` and `OS/2` (`FAT` / `VFAT` / `FAT32` file systems; default on Windows)
    Dos = 0,
    /// `Amiga`
    Amiga = 1,
    /// `OpenVMS`
    OpenVMS = 2,
    /// Default on Unix; default for symlinks on all platforms
    Unix = 3,
    /// `VM/CMS`
    VmCms = 4,
    /// `Atari ST`
    AtariSt = 5,
    /// `OS/2 H.P.F.S.`
    Os2 = 6,
    /// Legacy `Mac OS`, pre `OS X`
    Macintosh = 7,
    /// `Z-System`
    ZSystemO = 8,
    /// `CP/M`
    CPM = 9,
    /// Windows NTFS (with extra attributes; not used by default)
    WindowsNTFS = 10,
    /// `MVS (OS/390 - Z/OS)`
    MVS = 11,
    /// `VSE`
    VSE = 12,
    /// `Acorn Risc`
    AcornRisc = 13,
    /// `VFAT`
    VFAT = 14,
    /// alternate MVS
    AlternateMVS = 15,
    /// `BeOS`
    BeOS = 16,
    /// `Tandem`
    Tandem = 17,
    /// `OS/400`
    Os400 = 18,
    /// `OS X` (Darwin) (with extra attributes; not used by default)
    OsDarwin = 19,
    /// unused
    #[default]
    Unknown = 255,
}

impl System {
    /// Parse `version_made_by` block in local entry block.
    #[must_use]
    pub fn from_version_made_by(version_made_by: u16) -> Self {
        // Extract upper byte from little-endian representation
        let upper_byte = version_made_by.to_le_bytes()[1];
        System::from(upper_byte) // from u8
    }

    /// Extract the system and version from a `version_made_by` field.
    /// The first byte (lower) is the version, and the second byte (upper) is the system.
    pub(crate) fn extract_bytes(version_made_by: u16) -> (u8, Self) {
        let bytes = version_made_by.to_le_bytes();
        (bytes[0], Self::from(bytes[1]))
    }
}

impl From<u8> for System {
    fn from(system: u8) -> Self {
        match system {
            0 => System::Dos,
            1 => System::Amiga,
            2 => System::OpenVMS,
            3 => System::Unix,
            4 => System::VmCms,
            5 => System::AtariSt,
            6 => System::Os2,
            7 => System::Macintosh,
            8 => System::ZSystemO,
            9 => System::CPM,
            10 => System::WindowsNTFS,
            11 => System::MVS,
            12 => System::VSE,
            13 => System::AcornRisc,
            14 => System::VFAT,
            15 => System::AlternateMVS,
            16 => System::BeOS,
            17 => System::Tandem,
            18 => System::Os400,
            19 => System::OsDarwin,
            _ => System::Unknown,
        }
    }
}

impl From<System> for u8 {
    fn from(system: System) -> Self {
        system as u8
    }
}

/// Metadata for a file to be written
#[non_exhaustive]
#[derive(Clone, Debug, Copy, Eq, PartialEq)]
pub struct FileOptions<'k, T: FileOptionExtension> {
    pub(crate) compression_method: CompressionMethod,
    pub(crate) compression_level: Option<i64>,
    pub(crate) last_modified_time: DateTime,
    pub(crate) permissions: Option<u32>,
    pub(crate) large_file: bool,
    pub(crate) encrypt_with: Option<EncryptWith<'k>>,
    pub(crate) extended_options: T,
    pub(crate) alignment: u16,
    #[cfg(feature = "deflate-zopfli")]
    pub(super) zopfli_buffer_size: Option<usize>,
    #[cfg(feature = "aes-crypto")]
    pub(crate) aes_mode: Option<crate::aes::AesModeOptions>,
    pub(crate) system: Option<System>,
}
/// Simple File Options. Can be copied and good for simple writing zip files
pub type SimpleFileOptions = FileOptions<'static, ()>;

impl FileOptions<'static, ()> {
    const DEFAULT_FILE_PERMISSION: u32 = 0o100_644;
}

pub const MIN_VERSION: u8 = 10;
pub const DEFAULT_VERSION: u8 = 45;

/// Structure representing a ZIP file.
#[derive(Debug, Clone, Default)]
pub struct ZipFileData {
    /// Compatibility of the file attribute information
    pub system: System,
    /// Specification version
    pub version_made_by: u8,
    /// ZIP flags
    pub flags: u16,
    /// True if the file is encrypted.
    pub encrypted: bool,
    /// True if `file_name` and `file_comment` are UTF8
    pub is_utf8: bool,
    /// True if the file uses a data-descriptor section
    pub using_data_descriptor: bool,
    /// Compression method used to store the file
    pub compression_method: crate::compression::CompressionMethod,
    /// Compression level to store the file
    pub compression_level: Option<i64>,
    /// Last modified time. This will only have a 2 second precision.
    pub last_modified_time: Option<DateTime>,
    /// CRC32 checksum
    pub crc32: u32,
    /// Size of the file in the ZIP
    pub compressed_size: u64,
    /// Size of the file when extracted
    pub uncompressed_size: u64,
    /// Name of the file
    pub file_name: Box<str>,
    /// Raw file name. To be used when `file_name` was incorrectly decoded.
    pub file_name_raw: Box<[u8]>,
    /// Extra field usually used for storage expansion
    pub extra_field: Option<Arc<[u8]>>,
    /// Extra field only written to central directory
    pub central_extra_field: Option<Arc<[u8]>>,
    /// File comment
    pub file_comment: Box<str>,
    /// Specifies where the local header of the file starts
    pub header_start: u64,
    /// Specifies where the extra data of the file starts
    pub extra_data_start: Option<u64>,
    /// Specifies where the central header of the file starts
    ///
    /// Note that when this is not known, it is set to 0
    pub central_header_start: u64,
    /// Specifies where the compressed data of the file starts
    pub data_start: OnceLock<u64>,
    /// External file attributes
    pub external_attributes: u32,
    /// Reserve local ZIP64 extra field
    pub large_file: bool,
    /// AES mode if applicable
    pub aes_mode: Option<(AesMode, AesVendorVersion, CompressionMethod)>,
    /// Specifies where in the extra data the AES metadata starts
    pub aes_extra_data_start: u64,

    /// extra fields, see <https://libzip.org/specifications/extrafld.txt>
    pub extra_fields: Vec<ExtraField>,
}

impl ZipFileData {
    /// Get the starting offset of the data of the compressed file
    pub fn data_start(&self, reader: &mut (impl Read + Seek + ?Sized)) -> ZipResult<u64> {
        match self.data_start.get() {
            Some(data_start) => Ok(*data_start),
            None => Ok(self.find_data_start(reader)?),
        }
    }

    pub(crate) fn find_data_start(
        &self,
        reader: &mut (impl Read + Seek + ?Sized),
    ) -> Result<u64, ZipError> {
        // Go to start of data.
        reader.seek(SeekFrom::Start(self.header_start))?;

        // Parse static-sized fields and check the magic value.
        let block = ZipLocalEntryBlock::parse(reader)?;

        // Calculate the end of the local header from the fields we just parsed.
        let variable_fields_len =
        // Each of these fields must be converted to u64 before adding, as the result may
        // easily overflow a u16.
        u64::from(block.file_name_length) + u64::from(block.extra_field_length);
        let data_start = self.header_start
            + (size_of::<Magic>() + size_of::<ZipLocalEntryBlock>()) as u64
            + variable_fields_len;

        // Set the value so we don't have to read it again.
        match self.data_start.set(data_start) {
            Ok(()) => (),
            // If the value was already set in the meantime, ensure it matches (this is probably
            // unnecessary).
            Err(existing_value) => {
                debug_assert_eq!(existing_value, data_start);
            }
        }

        Ok(data_start)
    }

    pub(crate) fn find_content<'a, R: Read + Seek + ?Sized>(
        &self,
        reader: &'a mut R,
    ) -> ZipResult<Take<&'a mut R>> {
        // TODO: use .get_or_try_init() once stabilized to provide a closure returning a Result!
        let data_start = self.data_start(reader)?;
        reader.seek(SeekFrom::Start(data_start))?;

        Ok(reader.take(self.compressed_size))
    }

    pub(crate) fn find_content_seek<'a, R: Read + Seek + ?Sized>(
        &self,
        reader: &'a mut R,
    ) -> ZipResult<SeekableTake<'a, R>> {
        // Parse local header
        let data_start = self.data_start(reader)?;
        reader.seek(SeekFrom::Start(data_start))?;

        // Explicit Ok and ? are needed to convert io::Error to ZipError
        Ok(SeekableTake::new(reader, self.compressed_size)?)
    }

    pub fn is_dir(&self) -> bool {
        is_dir(&self.file_name)
    }

    pub fn file_name_sanitized(&self) -> PathBuf {
        let no_null_filename = match self.file_name.find('\0') {
            Some(index) => &self.file_name[0..index],
            None => &self.file_name,
        };

        file_name_sanitized(no_null_filename)
    }

    /// Simplify the file name by removing the prefix and parent directories and only return normal components
    pub(crate) fn simplified_components(&self) -> Option<Vec<&OsStr>> {
        if self.file_name.contains('\0') {
            return None;
        }
        let input = Path::new(OsStr::new(&*self.file_name));
        crate::path::simplified_components(input)
    }

    pub(crate) fn enclosed_name(&self) -> Option<PathBuf> {
        if self.file_name.contains('\0') {
            return None;
        }
        enclosed_name(&self.file_name)
    }

    /// Get unix mode for the file
    pub(crate) const fn unix_mode(&self) -> Option<u32> {
        if self.external_attributes == 0 {
            return None;
        }
        let unix_mode = self.external_attributes >> 16;
        if unix_mode != 0 {
            // If the high 16 bits are non-zero, they probably contain Unix permissions.
            // This happens for archives created on Windows by this crate or other tools,
            // and is the only way to identify symlinks in such archives.
            return Some(unix_mode);
        }
        match self.system {
            System::Unix => Some(unix_mode),
            System::Dos => {
                // Interpret MS-DOS directory bit
                let mut mode = if 0x10 == (self.external_attributes & 0x10) {
                    ffi::S_IFDIR | 0o0775
                } else {
                    ffi::S_IFREG | 0o0664
                };
                if 0x01 == (self.external_attributes & 0x01) {
                    // Read-only bit; strip write permissions
                    mode &= !0o222;
                }
                Some(mode)
            }
            _ => None,
        }
    }

    /// PKZIP version needed to open this file (from APPNOTE 4.4.3.2).
    pub fn version_needed(&self) -> u16 {
        let compression_version: u16 = match self.compression_method {
            CompressionMethod::Stored => MIN_VERSION.into(),
            #[cfg(feature = "_deflate-any")]
            CompressionMethod::Deflated => 20,
            #[cfg(feature = "_bzip2_any")]
            CompressionMethod::Bzip2 => 46,
            #[cfg(feature = "deflate64")]
            CompressionMethod::Deflate64 => 21,
            #[cfg(feature = "lzma")]
            CompressionMethod::Lzma => 63,
            #[cfg(feature = "xz")]
            CompressionMethod::Xz => 63,
            // APPNOTE doesn't specify a version for Zstandard
            _ => u16::from(DEFAULT_VERSION),
        };
        let crypto_version: u16 = if self.aes_mode.is_some() {
            51
        } else if self.encrypted {
            20
        } else {
            10
        };
        let misc_feature_version: u16 = if self.large_file {
            45
        } else if self
            .unix_mode()
            .is_some_and(|mode| mode & ffi::S_IFDIR == ffi::S_IFDIR)
        {
            // file is directory
            20
        } else {
            10
        };
        compression_version
            .max(crypto_version)
            .max(misc_feature_version)
    }
    #[inline(always)]
    pub(crate) fn extra_field_len(&self) -> usize {
        self.extra_field
            .as_ref()
            .map(|v| v.len())
            .unwrap_or_default()
    }
    #[inline(always)]
    pub(crate) fn central_extra_field_len(&self) -> usize {
        self.central_extra_field
            .as_ref()
            .map(|v| v.len())
            .unwrap_or_default()
    }

    #[allow(clippy::too_many_arguments)]
    pub(crate) fn initialize_local_block<S, T: FileOptionExtension>(
        name: &S,
        options: &FileOptions<'_, T>,
        raw_values: &ZipRawValues,
        header_start: u64,
        extra_data_start: Option<u64>,
        aes_extra_data_start: u64,
        compression_method: crate::compression::CompressionMethod,
        aes_mode: Option<(AesMode, AesVendorVersion, CompressionMethod)>,
        extra_field: &[u8],
    ) -> Self
    where
        S: ToString,
    {
        let permissions = options
            .permissions
            .unwrap_or(FileOptions::DEFAULT_FILE_PERMISSION);
        let file_name: Box<str> = name.to_string().into_boxed_str();
        let file_name_raw: Box<[u8]> = file_name.as_bytes().into();
        let mut external_attributes = permissions << 16;
        let system = if (permissions & ffi::S_IFLNK) == ffi::S_IFLNK {
            System::Unix
        } else if let Some(system_option) = options.system {
            // user provided
            system_option
        } else if cfg!(windows) {
            System::Dos
        } else {
            System::Unix
        };
        if system == System::Dos {
            if is_dir(&file_name) {
                // DOS directory bit
                external_attributes |= 0x10;
            }
            if options
                .permissions
                .is_some_and(|permissions| permissions & 0o444 == 0)
            {
                // DOS read-only bit
                external_attributes |= 0x01;
            }
        }
        let encrypted = options.encrypt_with.is_some();
        #[cfg(feature = "aes-crypto")]
        let encrypted = encrypted || options.aes_mode.is_some();
        let mut local_block = ZipFileData {
            system,
            version_made_by: DEFAULT_VERSION,
            flags: 0,
            encrypted,
            using_data_descriptor: false,
            is_utf8: !file_name.is_ascii(),
            compression_method,
            compression_level: options.compression_level,
            last_modified_time: Some(options.last_modified_time),
            crc32: raw_values.crc32,
            compressed_size: raw_values.compressed_size,
            uncompressed_size: raw_values.uncompressed_size,
            file_name, // Never used for saving, but used as map key in insert_file_data()
            file_name_raw,
            extra_field: Some(Arc::from(extra_field)),
            central_extra_field: options
                .extended_options
                .central_extra_data()
                .map(|v| Arc::from(v.as_ref().as_slice())),
            file_comment: String::with_capacity(0).into_boxed_str(),
            header_start,
            data_start: OnceLock::new(),
            central_header_start: 0,
            external_attributes,
            large_file: options.large_file,
            aes_mode,
            extra_fields: Vec::new(),
            extra_data_start,
            aes_extra_data_start,
        };
        local_block.version_made_by = local_block.version_needed() as u8;
        local_block
    }

    pub(crate) fn from_local_block<R: std::io::Read + ?Sized>(
        block: ZipLocalEntryBlock,
        reader: &mut R,
    ) -> ZipResult<Self> {
        let ZipLocalEntryBlock {
            version_made_by,
            flags,
            compression_method,
            last_mod_time,
            last_mod_date,
            crc32,
            compressed_size,
            uncompressed_size,
            file_name_length,
            extra_field_length,
            ..
        } = block;

        let encrypted: bool = ZipFlags::matching(flags, ZipFlags::Encrypted);
        if encrypted {
            return Err(ZipError::UnsupportedArchive(
                "Encrypted files are not supported",
            ));
        }

        /* FIXME: these were previously incorrect: add testing! */
        let using_data_descriptor: bool = ZipFlags::matching(flags, ZipFlags::UsingDataDescriptor);
        if using_data_descriptor {
            return Err(ZipError::UnsupportedArchive(
                "The file length is not available in the local header",
            ));
        }

        let is_utf8: bool = ZipFlags::matching(flags, ZipFlags::LanguageEncoding);
        let compression_method = crate::CompressionMethod::parse_from_u16(compression_method);
        let file_name_length: usize = file_name_length.into();
        let extra_field_length: usize = extra_field_length.into();

        let mut file_name_raw = vec![0u8; file_name_length];
        if let Err(e) = reader.read_exact(&mut file_name_raw) {
            if e.kind() == std::io::ErrorKind::UnexpectedEof {
                return Err(invalid!("File name extends beyond file boundary"));
            }
            return Err(e.into());
        }
        let mut extra_field = vec![0u8; extra_field_length];
        if let Err(e) = reader.read_exact(&mut extra_field) {
            if e.kind() == std::io::ErrorKind::UnexpectedEof {
                return Err(invalid!("Extra field extends beyond file boundary"));
            }
            return Err(e.into());
        }

        let file_name: Box<str> = if is_utf8 {
            String::from_utf8_lossy(&file_name_raw).into()
        } else {
            file_name_raw
                .from_cp437()
                .map_err(std::io::Error::other)?
                .into()
        };

        let (version_made_by, system) = System::extract_bytes(version_made_by);
        Ok(ZipFileData {
            system,
            version_made_by,
            flags,
            encrypted,
            using_data_descriptor,
            is_utf8,
            compression_method,
            compression_level: None,
            last_modified_time: DateTime::try_from_msdos(last_mod_date, last_mod_time).ok(),
            crc32,
            compressed_size: compressed_size.into(),
            uncompressed_size: uncompressed_size.into(),
            file_name,
            file_name_raw: file_name_raw.into(),
            extra_field: Some(Arc::from(extra_field.into_boxed_slice())),
            central_extra_field: None,
            file_comment: String::with_capacity(0).into_boxed_str(), // file comment is only available in the central directory
            // header_start and data start are not available, but also don't matter, since seeking is
            // not available.
            header_start: 0,
            data_start: OnceLock::new(),
            central_header_start: 0,
            // The external_attributes field is only available in the central directory.
            // We set this to zero, which should be valid as the docs state 'If input came
            // from standard input, this field is set to zero.'
            external_attributes: 0,
            large_file: false,
            aes_mode: None,
            extra_fields: Vec::new(),
            extra_data_start: None,
            aes_extra_data_start: 0,
        })
    }

    fn is_utf8(&self) -> bool {
        std::str::from_utf8(&self.file_name_raw).is_ok()
    }

    fn is_ascii(&self) -> bool {
        self.file_name_raw.is_ascii() && self.file_comment.is_ascii()
    }

    fn flags(&self) -> u16 {
        let utf8_bit: u16 = if self.is_utf8() && !self.is_ascii() {
            ZipFlags::LanguageEncoding.as_u16()
        } else {
            0
        };

        let using_data_descriptor_bit = if self.using_data_descriptor {
            ZipFlags::UsingDataDescriptor.as_u16()
        } else {
            0
        };

        let encrypted_bit: u16 = if self.encrypted { 1u16 << 0 } else { 0 };

        utf8_bit | using_data_descriptor_bit | encrypted_bit
    }
    fn clamp_size_field(&self, field: u64) -> Result<u32, std::io::Error> {
        if self.large_file {
            Ok(spec::ZIP64_BYTES_THR as u32)
        } else {
            field.min(spec::ZIP64_BYTES_THR).try_into().map_err(|_| {
                std::io::Error::other(format!(
                    "File size {field} exceeds maximum size for non-ZIP64 files"
                ))
            })
        }
    }

    pub(crate) fn local_block(&self) -> ZipResult<ZipLocalEntryBlock> {
        let (compressed_size, uncompressed_size) = if self.using_data_descriptor {
            (0, 0)
        } else {
            (
                self.clamp_size_field(self.compressed_size)?,
                self.clamp_size_field(self.uncompressed_size)?,
            )
        };
        let extra_field_length: u16 = self
            .extra_field_len()
            .try_into()
            .map_err(|_| invalid!("Extra data field is too large"))?;

        let last_modified_time = self
            .last_modified_time
            .unwrap_or_else(DateTime::default_for_write);
        Ok(ZipLocalEntryBlock {
            version_made_by: self.version_needed(),
            flags: self.flags(),
            compression_method: self.compression_method.serialize_to_u16(),
            last_mod_time: last_modified_time.timepart(),
            last_mod_date: last_modified_time.datepart(),
            crc32: self.crc32,
            compressed_size,
            uncompressed_size,
            file_name_length: self
                .file_name_raw
                .len()
                .try_into()
                .map_err(std::io::Error::other)?,
            extra_field_length,
        })
    }

    pub(crate) fn block(&self) -> ZipResult<ZipCentralEntryBlock> {
        let compressed_size = if self.large_file {
            spec::ZIP64_BYTES_THR as u32
        } else {
            self.compressed_size
                .min(spec::ZIP64_BYTES_THR)
                .try_into()
                .map_err(std::io::Error::other)?
        };
        let uncompressed_size = if self.large_file {
            spec::ZIP64_BYTES_THR as u32
        } else {
            self.uncompressed_size
                .min(spec::ZIP64_BYTES_THR)
                .try_into()
                .map_err(std::io::Error::other)?
        };
        let offset = self
            .header_start
            .min(spec::ZIP64_BYTES_THR)
            .try_into()
            .map_err(std::io::Error::other)?;
        let extra_field_len: u16 = self
            .extra_field_len()
            .try_into()
            .map_err(std::io::Error::other)?;
        let central_extra_field_len: u16 = self
            .central_extra_field_len()
            .try_into()
            .map_err(std::io::Error::other)?;
        let last_modified_time = self
            .last_modified_time
            .unwrap_or_else(DateTime::default_for_write);
        let version_to_extract = self.version_needed();
        let version_made_by = u16::from(self.version_made_by).max(version_to_extract);
        Ok(ZipCentralEntryBlock {
            version_made_by: ((self.system as u16) << 8) | version_made_by,
            version_to_extract,
            flags: self.flags(),
            compression_method: self.compression_method.serialize_to_u16(),
            last_mod_time: last_modified_time.timepart(),
            last_mod_date: last_modified_time.datepart(),
            crc32: self.crc32,
            compressed_size,
            uncompressed_size,
            file_name_length: self
                .file_name_raw
                .len()
                .try_into()
                .map_err(std::io::Error::other)?,
            extra_field_length: extra_field_len.checked_add(central_extra_field_len).ok_or(
                invalid!("Extra field length in central directory exceeds 64KiB"),
            )?,
            file_comment_length: self
                .file_comment
                .len()
                .try_into()
                .map_err(std::io::Error::other)?,
            disk_number: 0,
            internal_file_attributes: 0,
            external_file_attributes: self.external_attributes,
            offset,
        })
    }

    pub(crate) fn write_data_descriptor<W: std::io::Write>(
        &self,
        writer: &mut W,
        auto_large_file: bool,
    ) -> Result<(), ZipError> {
        if self.large_file {
            return self.zip64_data_descriptor_block().write(writer);
        }
        if self.compressed_size > spec::ZIP64_BYTES_THR
            || self.uncompressed_size > spec::ZIP64_BYTES_THR
        {
            if auto_large_file {
                return self.zip64_data_descriptor_block().write(writer);
            }
            return Err(ZipError::Io(std::io::Error::other(
                "Large file option has not been set - use .large_file(true) in options",
            )));
        }
        self.data_descriptor_block().write(writer)
    }

    pub(crate) fn data_descriptor_block(&self) -> ZipDataDescriptorBlock {
        ZipDataDescriptorBlock {
            crc32: self.crc32,
            compressed_size: self.compressed_size as u32,
            uncompressed_size: self.uncompressed_size as u32,
        }
    }

    pub(crate) fn zip64_data_descriptor_block(&self) -> Zip64DataDescriptorBlock {
        Zip64DataDescriptorBlock {
            crc32: self.crc32,
            compressed_size: self.compressed_size,
            uncompressed_size: self.uncompressed_size,
        }
    }
}

/// The encryption specification used to encrypt a file with AES.
///
/// According to the [specification](https://www.winzip.com/win/en/aes_info.html#winzip11) AE-2
/// does not make use of the CRC check.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[repr(u16)]
pub enum AesVendorVersion {
    Ae1 = 0x0001,
    Ae2 = 0x0002,
}

impl AesVendorVersion {
    /// As u16
    #[must_use]
    pub const fn as_u16(self) -> u16 {
        self as u16
    }
}

impl TryFrom<u16> for AesVendorVersion {
    type Error = &'static str;

    fn try_from(value: u16) -> Result<Self, Self::Error> {
        let aes_vendor_version = match value {
            0x0001 => AesVendorVersion::Ae1,
            0x0002 => AesVendorVersion::Ae2,
            _ => return Err("Invalid AES vendor version"),
        };
        Ok(aes_vendor_version)
    }
}

impl From<AesVendorVersion> for u16 {
    fn from(value: AesVendorVersion) -> Self {
        value as u16
    }
}

/// AES variant used.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "_arbitrary", derive(arbitrary::Arbitrary))]
#[repr(u8)]
pub enum AesMode {
    /// 128-bit AES encryption.
    Aes128 = 0x01,
    /// 192-bit AES encryption.
    Aes192 = 0x02,
    /// 256-bit AES encryption.
    Aes256 = 0x03,
}

impl AesMode {
    /// As u8
    #[must_use]
    pub const fn as_u8(self) -> u8 {
        self as u8
    }
}

impl Display for AesMode {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Aes128 => write!(f, "AES-128"),
            Self::Aes192 => write!(f, "AES-192"),
            Self::Aes256 => write!(f, "AES-256"),
        }
    }
}

impl TryFrom<u8> for AesMode {
    type Error = &'static str;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        let mode = match value {
            0x01 => AesMode::Aes128,
            0x02 => AesMode::Aes192,
            0x03 => AesMode::Aes256,
            _ => return Err("Invalid AES encryption strength"),
        };
        Ok(mode)
    }
}

#[cfg(feature = "aes-crypto")]
impl AesMode {
    /// Length of the salt for the given AES mode.
    #[must_use]
    pub const fn salt_length(&self) -> usize {
        self.key_length() / 2
    }

    /// Length of the key for the given AES mode.
    #[must_use]
    pub const fn key_length(&self) -> usize {
        match self {
            Self::Aes128 => 16,
            Self::Aes192 => 24,
            Self::Aes256 => 32,
        }
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn system() {
        use super::System;
        assert_eq!(u8::from(System::Dos), 0u8);
        assert_eq!(System::Dos as u8, 0u8);
        assert_eq!(System::Unix as u8, 3u8);
        assert_eq!(u8::from(System::Unix), 3u8);
        assert_eq!(System::from(0), System::Dos);
        assert_eq!(System::from(3), System::Unix);
        assert_eq!(u8::from(System::Unknown), 255u8);
        assert_eq!(System::Unknown as u8, 255u8);
    }

    #[test]
    fn unix_mode_robustness() {
        use super::{System, ZipFileData};
        use crate::types::ffi;
        let mut data = ZipFileData {
            system: System::Dos,
            external_attributes: (ffi::S_IFLNK | 0o777) << 16,
            ..ZipFileData::default()
        };
        assert_eq!(data.unix_mode(), Some(ffi::S_IFLNK | 0o777));

        data.system = System::Unknown;
        assert_eq!(data.unix_mode(), Some(ffi::S_IFLNK | 0o777));

        data.external_attributes = 0x10; // DOS directory bit
        data.system = System::Dos;
        assert_eq!(data.unix_mode().unwrap() & 0o170000, ffi::S_IFDIR);
    }

    #[test]
    fn sanitize() {
        use super::{System, ZipFileData};
        use std::{path::PathBuf, sync::OnceLock};

        let file_name = "/path/../../../../etc/./passwd\0/etc/shadow".to_string();
        let data = ZipFileData {
            system: System::Dos,
            version_made_by: 0,
            flags: 0,
            encrypted: false,
            using_data_descriptor: false,
            is_utf8: true,
            compression_method: crate::compression::CompressionMethod::Stored,
            compression_level: None,
            last_modified_time: None,
            crc32: 0,
            compressed_size: 0,
            uncompressed_size: 0,
            file_name: file_name.clone().into_boxed_str(),
            file_name_raw: file_name.into_bytes().into_boxed_slice(),
            extra_field: None,
            central_extra_field: None,
            file_comment: String::with_capacity(0).into_boxed_str(),
            header_start: 0,
            extra_data_start: None,
            data_start: OnceLock::new(),
            central_header_start: 0,
            external_attributes: 0,
            large_file: false,
            aes_mode: None,
            aes_extra_data_start: 0,
            extra_fields: Vec::new(),
        };
        assert_eq!(data.file_name_sanitized(), PathBuf::from("path/etc/passwd"));
    }
}
