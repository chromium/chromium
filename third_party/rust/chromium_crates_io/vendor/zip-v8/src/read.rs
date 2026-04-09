//! Types for reading ZIP archives

use crate::compression::CompressionMethod;
use crate::cp437::FromCp437;
use crate::datetime::DateTime;
use crate::extra_fields::AexEncryption;
use crate::extra_fields::UnicodeExtraField;
use crate::extra_fields::Zip64ExtendedInformation;
use crate::extra_fields::{ExtendedTimestamp, ExtraField, Ntfs, UsedExtraField};
use crate::read::readers::{ZipFileReader, ZipFileSeekReader};
use crate::result::{ZipError, ZipResult, invalid};
use crate::spec::is_dir;
use crate::spec::{
    CentralDirectoryEndInfo, DataAndPosition, FixedSizeBlock, ZIP64_BYTES_THR,
    ZipCentralEntryBlock, ZipFlags,
};
use crate::types::{SimpleFileOptions, System, ZipFileData, ffi};
use crate::unstable::LittleEndianReadExt;
use core::mem::replace;
use indexmap::IndexMap;
use std::borrow::Cow;
use std::ffi::OsStr;
use std::io::{self, Read, Seek, SeekFrom, Write, copy, sink};
use std::path::{Component, Path, PathBuf};
use std::sync::{Arc, OnceLock};

mod config;
pub use config::{ArchiveOffset, Config};

/// Provides high level API for reading from a stream.
pub(crate) mod stream;
pub use stream::{read_zipfile_from_stream, read_zipfile_from_stream_with_compressed_size};

pub(crate) mod magic_finder;
pub(crate) mod readers;

pub(crate) mod zip_archive;
pub use zip_archive::{ZipArchive, ZipArchiveMetadata};

#[cfg(feature = "aes-crypto")]
pub use crate::aes::AesInfo;

/// A struct for reading a zip file
///
/// When reading from a `ZipFile` using [`Self::read()`], keep in mind that `read()` **does not guarantee** the buffer will be fully filled in a single call.
///
/// If your logic depends on the buffer being completely populated, use [`Self::read_exact()`] instead. It will continue reading until the entire buffer is filled or an error occurs.
#[derive(Debug)]
pub struct ZipFile<'a, R: Read + ?Sized> {
    pub(crate) data: Cow<'a, ZipFileData>,
    pub(crate) reader: ZipFileReader<'a, R>,
}

/// A struct for reading and seeking a zip file
pub struct ZipFileSeek<'a, R> {
    data: Cow<'a, ZipFileData>,
    reader: ZipFileSeekReader<'a, R>,
}

pub(crate) fn make_writable_dir_all<T: AsRef<Path>>(outpath: T) -> Result<(), ZipError> {
    use std::fs;
    fs::create_dir_all(outpath.as_ref())?;
    #[cfg(unix)]
    {
        // Dirs must be writable until all normal files are extracted
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(
            outpath.as_ref(),
            std::fs::Permissions::from_mode(
                0o700 | std::fs::metadata(outpath.as_ref())?.permissions().mode(),
            ),
        )?;
    }
    Ok(())
}

#[cfg(unix)]
pub(crate) fn make_symlink_impl<T>(
    outpath: &Path,
    target_str: &str,
    _existing_files: &IndexMap<Box<str>, T>,
) -> ZipResult<()> {
    std::os::unix::fs::symlink(Path::new(&target_str), outpath)?;
    Ok(())
}

#[cfg(windows)]
pub(crate) fn make_symlink_impl<T>(
    outpath: &Path,
    target_str: &str,
    existing_files: &IndexMap<Box<str>, T>,
) -> ZipResult<()> {
    let target = Path::new(OsStr::new(&target_str));
    let target_is_dir_from_archive = existing_files.contains_key(target_str) && is_dir(target_str);
    let target_is_dir = if target_is_dir_from_archive {
        true
    } else if let Ok(meta) = std::fs::metadata(target) {
        meta.is_dir()
    } else {
        false
    };
    if target_is_dir {
        std::os::windows::fs::symlink_dir(target, outpath)?;
    } else {
        std::os::windows::fs::symlink_file(target, outpath)?;
    }
    Ok(())
}

#[cfg(any(windows, unix))]
pub(crate) fn make_symlink<T>(
    outpath: &Path,
    target: &[u8],
    #[cfg_attr(not(any(windows, unix)), allow(unused))] existing_files: &IndexMap<Box<str>, T>,
) -> ZipResult<()> {
    let Ok(target_str) = std::str::from_utf8(target) else {
        return Err(invalid!("Invalid UTF-8 as symlink target"));
    };
    make_symlink_impl(outpath, target_str, existing_files)
}

#[cfg(not(any(windows, unix)))]
pub(crate) fn make_symlink<T>(
    outpath: &Path,
    target: &[u8],
    #[cfg_attr(not(any(windows, unix)), allow(unused))] existing_files: &IndexMap<Box<str>, T>,
) -> ZipResult<()> {
    let Ok(_) = std::str::from_utf8(target) else {
        return Err(invalid!("Invalid UTF-8 as symlink target"));
    };
    use std::fs::File;
    let output = File::create(outpath);
    output?.write_all(target)?;
    Ok(())
}

#[derive(Debug)]
pub(crate) struct CentralDirectoryInfo {
    pub(crate) archive_offset: u64,
    pub(crate) directory_start: u64,
    pub(crate) number_of_files: usize,
    pub(crate) disk_number: u32,
    pub(crate) disk_with_central_directory: u32,
}

impl<'a> TryFrom<&'a CentralDirectoryEndInfo> for CentralDirectoryInfo {
    type Error = ZipError;

    fn try_from(value: &'a CentralDirectoryEndInfo) -> Result<Self, Self::Error> {
        let (relative_cd_offset, number_of_files, disk_number, disk_with_central_directory) =
            match &value.eocd64 {
                Some(DataAndPosition { data: eocd64, .. }) => {
                    if eocd64.number_of_files_on_this_disk > eocd64.number_of_files {
                        return Err(invalid!(
                            "ZIP64 footer indicates more files on this disk than in the whole archive"
                        ));
                    }
                    (
                        eocd64.central_directory_offset,
                        eocd64.number_of_files as usize,
                        eocd64.disk_number,
                        eocd64.disk_with_central_directory,
                    )
                }
                _ => (
                    u64::from(value.eocd.data.central_directory_offset),
                    value.eocd.data.number_of_files_on_this_disk as usize,
                    u32::from(value.eocd.data.disk_number),
                    u32::from(value.eocd.data.disk_with_central_directory),
                ),
            };

        let directory_start = relative_cd_offset
            .checked_add(value.archive_offset)
            .ok_or(invalid!("Invalid central directory size or offset"))?;

        Ok(Self {
            archive_offset: value.archive_offset,
            directory_start,
            number_of_files,
            disk_number,
            disk_with_central_directory,
        })
    }
}

/// Store all entries which specify a numeric "mode" which is familiar to POSIX operating systems.
#[cfg(unix)]
#[derive(Default, Debug)]
struct UnixFileModes {
    map: std::collections::BTreeMap<PathBuf, u32>,
}

#[cfg(unix)]
impl UnixFileModes {
    #[cfg_attr(not(debug_assertions), allow(unused))]
    pub fn add_mode(&mut self, path: PathBuf, mode: u32) {
        // We don't print a warning or consider it remotely out of the ordinary to receive two
        // separate modes for the same path: just take the later one.
        let old_entry = self.map.insert(path, mode);
        debug_assert_eq!(old_entry, None);
    }

    // Child nodes will be sorted later lexicographically, so reversing the order puts them first.
    pub fn all_perms_with_children_first(
        self,
    ) -> impl IntoIterator<Item = (PathBuf, std::fs::Permissions)> {
        use std::os::unix::fs::PermissionsExt;
        self.map
            .into_iter()
            .rev()
            .map(|(p, m)| (p, std::fs::Permissions::from_mode(m)))
    }
}

impl<R: Read + Seek> ZipArchive<R> {
    pub(crate) fn merge_contents<W: Write + Seek>(
        &mut self,
        mut w: W,
    ) -> ZipResult<IndexMap<Box<str>, ZipFileData>> {
        if self.shared.files.is_empty() {
            return Ok(IndexMap::new());
        }
        let mut new_files = self.shared.files.clone();
        /* The first file header will probably start at the beginning of the file, but zip doesn't
         * enforce that, and executable zips like PEX files will have a shebang line so will
         * definitely be greater than 0.
         *
         * assert_eq!(0, new_files[0].header_start); // Avoid this.
         */

        let first_new_file_header_start = w.stream_position()?;

        /* Push back file header starts for all entries in the covered files. */
        new_files.values_mut().try_for_each(|f| {
            /* This is probably the only really important thing to change. */
            f.header_start = f
                .header_start
                .checked_add(first_new_file_header_start)
                .ok_or(invalid!(
                    "new header start from merge would have been too large"
                ))?;
            /* This is only ever used internally to cache metadata lookups (it's not part of the
             * zip spec), and 0 is the sentinel value. */
            f.central_header_start = 0;
            /* This is an atomic variable so it can be updated from another thread in the
             * implementation (which is good!). */
            if let Some(old_data_start) = f.data_start.take() {
                let new_data_start = old_data_start
                    .checked_add(first_new_file_header_start)
                    .ok_or(invalid!(
                        "new data start from merge would have been too large"
                    ))?;
                f.data_start.get_or_init(|| new_data_start);
            }
            Ok::<_, ZipError>(())
        })?;

        /* Rewind to the beginning of the file.
         *
         * NB: we *could* decide to start copying from new_files[0].header_start instead, which
         * would avoid copying over e.g. any pex shebangs or other file contents that start before
         * the first zip file entry. However, zip files actually shouldn't care about garbage data
         * in *between* real entries, since the central directory header records the correct start
         * location of each, and keeping track of that math is more complicated logic that will only
         * rarely be used, since most zips that get merged together are likely to be produced
         * specifically for that purpose (and therefore are unlikely to have a shebang or other
         * preface). Finally, this preserves any data that might actually be useful.
         */
        self.reader.rewind()?;
        /* Find the end of the file data. */
        let length_to_read = self.shared.dir_start;
        /* Produce a Read that reads bytes up until the start of the central directory header.
         * This "as &mut dyn Read" trick is used elsewhere to avoid having to clone the underlying
         * handle, which it really shouldn't need to anyway. */
        let mut limited_raw = (&mut self.reader as &mut dyn Read).take(length_to_read);
        /* Copy over file data from source archive directly. */
        io::copy(&mut limited_raw, &mut w)?;

        /* Return the files we've just written to the data stream. */
        Ok(new_files)
    }

    /// Extract a Zip archive into a directory, overwriting files if they
    /// already exist. Paths are sanitized with [`ZipFile::enclosed_name`]. Symbolic links are only
    /// created and followed if the target is within the destination directory (this is checked
    /// conservatively using [`std::fs::canonicalize`]).
    ///
    /// Extraction is not atomic. If an error is encountered, some of the files
    /// may be left on disk. However, on Unix targets, no newly-created directories with part but
    /// not all of their contents extracted will be readable, writable or usable as process working
    /// directories by any non-root user except you.
    ///
    /// On Unix and Windows, symbolic links are extracted correctly. On other platforms such as
    /// WebAssembly, symbolic links aren't supported, so they're extracted as normal files
    /// containing the target path in UTF-8.
    pub fn extract<P: AsRef<Path>>(&mut self, directory: P) -> ZipResult<()> {
        self.extract_internal(directory, None::<fn(&Path) -> bool>)
    }

    /// Extracts a Zip archive into a directory in the same fashion as
    /// [`ZipArchive::extract`], but detects a "root" directory in the archive
    /// (a single top-level directory that contains the rest of the archive's
    /// entries) and extracts its contents directly.
    ///
    /// For a sensible default `filter`, you can use [`root_dir_common_filter`].
    /// For a custom `filter`, see [`RootDirFilter`].
    ///
    /// See [`ZipArchive::root_dir`] for more information on how the root
    /// directory is detected and the meaning of the `filter` parameter.
    ///
    /// ## Example
    ///
    /// Imagine a Zip archive with the following structure:
    ///
    /// ```text
    /// root/file1.txt
    /// root/file2.txt
    /// root/sub/file3.txt
    /// root/sub/subsub/file4.txt
    /// ```
    ///
    /// If the archive is extracted to `foo` using [`ZipArchive::extract`],
    /// the resulting directory structure will be:
    ///
    /// ```text
    /// foo/root/file1.txt
    /// foo/root/file2.txt
    /// foo/root/sub/file3.txt
    /// foo/root/sub/subsub/file4.txt
    /// ```
    ///
    /// If the archive is extracted to `foo` using
    /// [`ZipArchive::extract_unwrapped_root_dir`], the resulting directory
    /// structure will be:
    ///
    /// ```text
    /// foo/file1.txt
    /// foo/file2.txt
    /// foo/sub/file3.txt
    /// foo/sub/subsub/file4.txt
    /// ```
    ///
    /// ## Example - No Root Directory
    ///
    /// Imagine a Zip archive with the following structure:
    ///
    /// ```text
    /// root/file1.txt
    /// root/file2.txt
    /// root/sub/file3.txt
    /// root/sub/subsub/file4.txt
    /// other/file5.txt
    /// ```
    ///
    /// Due to the presence of the `other` directory,
    /// [`ZipArchive::extract_unwrapped_root_dir`] will extract this in the same
    /// fashion as [`ZipArchive::extract`] as there is now no "root directory."
    pub fn extract_unwrapped_root_dir<P: AsRef<Path>>(
        &mut self,
        directory: P,
        root_dir_filter: impl RootDirFilter,
    ) -> ZipResult<()> {
        self.extract_internal(directory, Some(root_dir_filter))
    }

    fn extract_internal<P: AsRef<Path>>(
        &mut self,
        directory: P,
        root_dir_filter: Option<impl RootDirFilter>,
    ) -> ZipResult<()> {
        use std::fs;

        fs::create_dir_all(&directory)?;
        let directory = directory.as_ref().canonicalize()?;

        let root_dir = root_dir_filter
            .and_then(|filter| {
                self.root_dir(&filter)
                    .transpose()
                    .map(|root_dir| root_dir.map(|root_dir| (root_dir, filter)))
            })
            .transpose()?;

        // If we have a root dir, simplify the path components to be more
        // appropriate for passing to `safe_prepare_path`
        let root_dir = root_dir
            .as_ref()
            .map(|(root_dir, filter)| {
                crate::path::simplified_components(root_dir)
                    .ok_or_else(|| {
                        // Should be unreachable
                        debug_assert!(false, "Invalid root dir path");

                        invalid!("Invalid root dir path")
                    })
                    .map(|root_dir| (root_dir, filter))
            })
            .transpose()?;

        #[cfg(unix)]
        let mut files_by_unix_mode = UnixFileModes::default();

        for i in 0..self.len() {
            let mut file = self.by_index(i)?;

            let mut outpath = directory.clone();
            /* TODO: the control flow of this method call and subsequent expectations about the
             *       values in this loop is extremely difficult to follow. It also appears to
             *       perform a nested loop upon extracting every single file entry? Why does it
             *       accept two arguments that point to the same directory path, one mutable? */
            file.safe_prepare_path(directory.as_ref(), &mut outpath, root_dir.as_ref())?;

            #[cfg(any(unix, windows))]
            if file.is_symlink() {
                let mut target = Vec::with_capacity(file.size() as usize);
                file.read_to_end(&mut target)?;
                drop(file);
                make_symlink(&outpath, &target, &self.shared.files)?;
                continue;
            } else if file.is_dir() {
                crate::read::make_writable_dir_all(&outpath)?;
                continue;
            }
            let mut outfile = fs::File::create(&outpath)?;
            io::copy(&mut file, &mut outfile)?;

            // Check for real permissions, which we'll set in a second pass.
            #[cfg(unix)]
            if let Some(mode) = file.unix_mode() {
                files_by_unix_mode.add_mode(outpath, mode);
            }

            // Set original timestamp.
            #[cfg(feature = "chrono")]
            if let Some(last_modified) = file.last_modified()
                && let Some(t) = last_modified.datetime_to_systemtime()
            {
                outfile.set_modified(t)?;
            }
        }

        // Ensure we update children's permissions before making a parent unwritable.
        #[cfg(unix)]
        for (path, perms) in files_by_unix_mode.all_perms_with_children_first() {
            std::fs::set_permissions(path, perms)?;
        }

        Ok(())
    }
}

/// Parse a central directory entry to collect the information for the file.
pub(crate) fn central_header_to_zip_file<R: Read + Seek>(
    reader: &mut R,
    central_directory: &CentralDirectoryInfo,
) -> ZipResult<ZipFileData> {
    let central_header_start = reader.stream_position()?;

    // Parse central header
    let block = ZipCentralEntryBlock::parse(reader)?;

    let file = central_header_to_zip_file_inner(
        reader,
        central_directory.archive_offset,
        central_header_start,
        block,
    )?;

    let central_header_end = reader.stream_position()?;

    reader.seek(SeekFrom::Start(central_header_end))?;
    Ok(file)
}

#[inline]
fn read_variable_length_byte_field<R: Read>(reader: &mut R, len: usize) -> ZipResult<Box<[u8]>> {
    let mut data = vec![0; len].into_boxed_slice();
    if let Err(e) = reader.read_exact(&mut data) {
        if e.kind() == io::ErrorKind::UnexpectedEof {
            return Err(invalid!(
                "Variable-length field extends beyond file boundary"
            ));
        }
        return Err(e.into());
    }
    Ok(data)
}

/// Parse a central directory entry to collect the information for the file.
fn central_header_to_zip_file_inner<R: Read>(
    reader: &mut R,
    archive_offset: u64,
    central_header_start: u64,
    block: ZipCentralEntryBlock,
) -> ZipResult<ZipFileData> {
    let ZipCentralEntryBlock {
        // magic,
        version_made_by,
        // version_to_extract,
        flags,
        compression_method,
        last_mod_time,
        last_mod_date,
        crc32,
        compressed_size,
        uncompressed_size,
        file_name_length,
        extra_field_length,
        file_comment_length,
        // disk_number,
        // internal_file_attributes,
        external_file_attributes,
        offset,
        ..
    } = block;

    let encrypted = ZipFlags::matching(flags, ZipFlags::Encrypted);
    let is_utf8 = ZipFlags::matching(flags, ZipFlags::LanguageEncoding);
    let using_data_descriptor = ZipFlags::matching(flags, ZipFlags::UsingDataDescriptor);

    let file_name_raw = read_variable_length_byte_field(reader, file_name_length as usize)?;
    let extra_field = read_variable_length_byte_field(reader, extra_field_length as usize)?;
    let file_comment_raw = read_variable_length_byte_field(reader, file_comment_length as usize)?;
    let file_name: Box<str> = if is_utf8 {
        String::from_utf8_lossy(&file_name_raw).into()
    } else {
        file_name_raw.from_cp437()?.into()
    };
    let file_comment: Box<str> = if is_utf8 {
        String::from_utf8_lossy(&file_comment_raw).into()
    } else {
        file_comment_raw.from_cp437()?.into()
    };

    let (version_made_by, system) = System::extract_bytes(version_made_by);
    // Construct the result
    let mut result = ZipFileData {
        system,
        version_made_by,
        encrypted,
        using_data_descriptor,
        is_utf8,
        compression_method: CompressionMethod::parse_from_u16(compression_method),
        compression_level: None,
        last_modified_time: DateTime::try_from_msdos(last_mod_date, last_mod_time).ok(),
        crc32,
        compressed_size: compressed_size.into(),
        uncompressed_size: uncompressed_size.into(),
        flags,
        file_name,
        file_name_raw,
        extra_field: Some(Arc::from(extra_field)),
        central_extra_field: None,
        file_comment,
        header_start: offset.into(),
        extra_data_start: None,
        central_header_start,
        data_start: OnceLock::new(),
        external_attributes: external_file_attributes,
        large_file: false,
        aes_mode: None,
        aes_extra_data_start: 0,
        extra_fields: Vec::new(),
    };
    parse_extra_field(&mut result)?;

    let aes_enabled = result.compression_method == CompressionMethod::AES;
    if aes_enabled && result.aes_mode.is_none() {
        return Err(invalid!("AES encryption without AES extra data field"));
    }

    // Account for shifted zip offsets.
    result.header_start = result
        .header_start
        .checked_add(archive_offset)
        .ok_or(invalid!("Archive header is too large"))?;

    Ok(result)
}

pub(crate) fn parse_extra_field(file: &mut ZipFileData) -> ZipResult<()> {
    let mut extra_field = file.extra_field.clone();
    let mut central_extra_field = file.central_extra_field.clone();
    for field_group in [&mut extra_field, &mut central_extra_field] {
        let Some(extra_field) = field_group else {
            continue;
        };
        let mut modified = false;
        let mut processed_extra_field = vec![];
        let len = extra_field.len();
        let mut reader = io::Cursor::new(&**extra_field);

        let mut position = reader.position();
        while position < len as u64 {
            let old_position = position;
            let remove = parse_single_extra_field(file, &mut reader, position, false)?;
            position = reader.position();
            if remove {
                modified = true;
            } else {
                let field_len = (position - old_position) as usize;
                let write_start = processed_extra_field.len();
                reader.seek(SeekFrom::Start(old_position))?;
                processed_extra_field.extend_from_slice(&vec![0u8; field_len]);
                if let Err(e) = reader
                    .read_exact(&mut processed_extra_field[write_start..(write_start + field_len)])
                {
                    if e.kind() == io::ErrorKind::UnexpectedEof {
                        return Err(invalid!("Extra field content exceeds declared length"));
                    }
                    return Err(e.into());
                }
            }
        }
        if modified {
            *field_group = Some(Arc::from(processed_extra_field.into_boxed_slice()));
        }
    }
    file.extra_field = extra_field;
    file.central_extra_field = central_extra_field;
    Ok(())
}

pub(crate) fn parse_single_extra_field<R: Read>(
    file: &mut ZipFileData,
    reader: &mut R,
    bytes_already_read: u64,
    disallow_zip64: bool,
) -> ZipResult<bool> {
    let kind = match reader.read_u16_le() {
        Ok(kind) => kind,
        Err(e) if e.kind() == io::ErrorKind::UnexpectedEof => return Ok(false),
        Err(e) => return Err(e.into()),
    };
    let decoded_extra_field = UsedExtraField::try_from(kind);
    let len = match decoded_extra_field {
        Ok(known_field) => match reader.read_u16_le() {
            Ok(len) => len,
            Err(e) if e.kind() == io::ErrorKind::UnexpectedEof => {
                return Err(invalid!("Extra field {} header truncated", known_field));
            }
            Err(e) => return Err(e.into()),
        },
        Err(()) => {
            match reader.read_u16_le() {
                Ok(len) => len,
                Err(e) if e.kind() == io::ErrorKind::UnexpectedEof => return Ok(false), // early return, most likely a padding
                Err(_e) => {
                    // Consume remaining bytes to avoid infinite loop in caller
                    let mut buf = Vec::new();
                    let _ = reader.read_to_end(&mut buf);
                    return Ok(false);
                }
            }
        }
    };
    match decoded_extra_field {
        // Zip64 extended information extra field
        Ok(UsedExtraField::Zip64ExtendedInfo) => {
            if disallow_zip64 {
                return Err(invalid!("Can't write a custom field using the ZIP64 ID"));
            }
            file.large_file = true;
            Zip64ExtendedInformation::parse(
                reader,
                len,
                &mut file.uncompressed_size,
                &mut file.compressed_size,
                &mut file.header_start,
            )?;
            return Ok(true);
        }
        Ok(UsedExtraField::Ntfs) => {
            // NTFS extra field
            file.extra_fields
                .push(ExtraField::Ntfs(Ntfs::try_from_reader(reader, len)?));
        }
        Ok(UsedExtraField::AeXEncryption) => {
            // AES
            AexEncryption::parse(
                reader,
                len,
                &mut file.aes_mode,
                &mut file.compression_method,
            )?;
            file.aes_extra_data_start = bytes_already_read;
        }
        Ok(UsedExtraField::ExtendedTimestamp) => {
            file.extra_fields.push(ExtraField::ExtendedTimestamp(
                ExtendedTimestamp::try_from_reader(reader, len)?,
            ));
        }
        Ok(UsedExtraField::UnicodeComment) => {
            // Info-ZIP Unicode Comment Extra Field
            // APPNOTE 4.6.8 and https://libzip.org/specifications/extrafld.txt
            file.file_comment = String::from_utf8(
                UnicodeExtraField::try_from_reader(reader, len)?
                    .unwrap_valid(file.file_comment.as_bytes())?
                    .into_vec(),
            )?
            .into();
        }
        Ok(UsedExtraField::UnicodePath) => {
            // Info-ZIP Unicode Path Extra Field
            // APPNOTE 4.6.9 and https://libzip.org/specifications/extrafld.txt
            file.file_name_raw = UnicodeExtraField::try_from_reader(reader, len)?
                .unwrap_valid(&file.file_name_raw)?;
            file.file_name =
                String::from_utf8(file.file_name_raw.clone().into_vec())?.into_boxed_str();
            file.is_utf8 = true;
        }
        _ => {
            if let Err(e) = reader.read_exact(&mut vec![0u8; len as usize]) {
                if e.kind() == io::ErrorKind::UnexpectedEof {
                    return Err(invalid!("Extra field content truncated"));
                }
                return Err(e.into());
            }
            // Other fields are ignored
        }
    }
    Ok(false)
}

/// A trait for exposing file metadata inside the zip.
pub trait HasZipMetadata {
    /// Get the file metadata
    fn get_metadata(&self) -> &ZipFileData;
}

/// Options for reading a file from an archive.
#[derive(Default)]
pub struct ZipReadOptions<'a> {
    /// The password to use when decrypting the file.  This is ignored if not required.
    password: Option<&'a [u8]>,

    /// Ignore the value of the encryption flag and proceed as if the file were plaintext.
    ignore_encryption_flag: bool,

    /// Ignore the crc32 of the file
    ignore_crc: bool,
}

impl<'a> ZipReadOptions<'a> {
    /// Create a new set of options with the default values.
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Set the password, if any, to use.  Return for chaining.
    #[must_use]
    pub fn password(mut self, password: Option<&'a [u8]>) -> Self {
        self.password = password;
        self
    }

    /// Set the ignore encryption flag.  Return for chaining.
    #[must_use]
    pub fn ignore_encryption_flag(mut self, ignore: bool) -> Self {
        self.ignore_encryption_flag = ignore;
        self
    }

    /// Ignore the CRC32 of the file
    #[must_use]
    pub fn ignore_crc32(mut self, should_ignore: bool) -> Self {
        self.ignore_crc = should_ignore;
        self
    }
}

/// Methods for retrieving information on zip files
impl<'a, R: Read + ?Sized> ZipFile<'a, R> {
    pub(crate) fn take_raw_reader(&mut self) -> io::Result<io::Take<&'a mut R>> {
        replace(&mut self.reader, ZipFileReader::NoReader).into_inner()
    }

    /// Get the version of the file
    pub fn version_made_by(&self) -> (u8, u8) {
        (
            self.get_metadata().version_made_by / 10,
            self.get_metadata().version_made_by % 10,
        )
    }

    /// Get the name of the file
    ///
    /// # Warnings
    ///
    /// It is dangerous to use this name directly when extracting an archive.
    /// It may contain an absolute path (`/etc/shadow`), or break out of the
    /// current directory (`../runtime`). Carelessly writing to these paths
    /// allows an attacker to craft a ZIP archive that will overwrite critical
    /// files.
    ///
    /// You can use the [`ZipFile::enclosed_name`] method to validate the name
    /// as a safe path.
    pub fn name(&self) -> &str {
        &self.get_metadata().file_name
    }

    /// Get the name of the file, in the raw (internal) byte representation.
    ///
    /// The encoding of this data is currently undefined.
    pub fn name_raw(&self) -> &[u8] {
        &self.get_metadata().file_name_raw
    }

    /// Get the name of the file in a sanitized form. It truncates the name to the first NULL byte,
    /// removes a leading '/' and removes '..' parts.
    #[deprecated(
        since = "0.5.7",
        note = "by stripping `..`s from the path, the meaning of paths can change.
                `mangled_name` can be used if this behaviour is desirable"
    )]
    pub fn sanitized_name(&self) -> PathBuf {
        self.mangled_name()
    }

    /// Rewrite the path, ignoring any path components with special meaning.
    ///
    /// - Absolute paths are made relative
    /// - [`ParentDir`]s are ignored
    /// - Truncates the filename at a NULL byte
    ///
    /// This is appropriate if you need to be able to extract *something* from
    /// any archive, but will easily misrepresent trivial paths like
    /// `foo/../bar` as `foo/bar` (instead of `bar`). Because of this,
    /// [`ZipFile::enclosed_name`] is the better option in most scenarios.
    ///
    /// [`ParentDir`]: `Component::ParentDir`
    pub fn mangled_name(&self) -> PathBuf {
        self.get_metadata().file_name_sanitized()
    }

    /// Ensure the file path is safe to use as a [`Path`].
    ///
    /// - It can't contain NULL bytes
    /// - It can't resolve to a path outside the current directory
    ///   > `foo/../bar` is fine, `foo/../../bar` is not.
    /// - It can't be an absolute path
    ///
    /// This will read well-formed ZIP files correctly, and is resistant
    /// to path-based exploits. It is recommended over
    /// [`ZipFile::mangled_name`].
    pub fn enclosed_name(&self) -> Option<PathBuf> {
        self.get_metadata().enclosed_name()
    }

    pub(crate) fn simplified_components(&self) -> Option<Vec<&OsStr>> {
        self.get_metadata().simplified_components()
    }

    /// Prepare the path for extraction by creating necessary missing directories and checking for symlinks to be contained within the base path.
    ///
    /// `base_path` parameter is assumed to be canonicalized.
    pub(crate) fn safe_prepare_path(
        &self,
        base_path: &Path,
        outpath: &mut PathBuf,
        root_dir: Option<&(Vec<&OsStr>, impl RootDirFilter)>,
    ) -> ZipResult<()> {
        let components = self
            .simplified_components()
            .ok_or(invalid!("Invalid file path"))?;

        let components = match root_dir {
            Some((root_dir, filter)) => match components.strip_prefix(&**root_dir) {
                Some(components) => components,

                // In this case, we expect that the file was not in the root
                // directory, but was filtered out when searching for the
                // root directory.
                None => {
                    // We could technically find ourselves at this code
                    // path if the user provides an unstable or
                    // non-deterministic `filter` function.
                    //
                    // If debug assertions are on, we should panic here.
                    // Otherwise, the safest thing to do here is to just
                    // extract as-is.
                    debug_assert!(
                        !filter(&PathBuf::from_iter(components.iter())),
                        "Root directory filter should not match at this point"
                    );

                    // Extract as-is.
                    &components[..]
                }
            },

            None => &components[..],
        };

        let components_len = components.len();

        for (is_last, component) in components
            .iter()
            .copied()
            .enumerate()
            .map(|(i, c)| (i == components_len - 1, c))
        {
            // we can skip the target directory itself because the base path is assumed to be "trusted" (if the user say extract to a symlink we can follow it)
            outpath.push(component);

            // check if the path is a symlink, the target must be _inherently_ within the directory
            for limit in (0..5u8).rev() {
                let meta = match std::fs::symlink_metadata(&outpath) {
                    Ok(meta) => meta,
                    Err(e) if e.kind() == io::ErrorKind::NotFound => {
                        if !is_last {
                            crate::read::make_writable_dir_all(&outpath)?;
                        }
                        break;
                    }
                    Err(e) => return Err(e.into()),
                };

                if !meta.is_symlink() {
                    break;
                }

                if limit == 0 {
                    return Err(invalid!("Extraction followed a symlink too deep"));
                }

                // note that we cannot accept links that do not inherently resolve to a path inside the directory to prevent:
                // - disclosure of unrelated path exists (no check for a path exist and then ../ out)
                // - issues with file-system specific path resolution (case sensitivity, etc)
                let target = std::fs::read_link(&outpath)?;

                if !crate::path::simplified_components(&target)
                    .ok_or(invalid!("Invalid symlink target path"))?
                    .starts_with(
                        &crate::path::simplified_components(base_path)
                            .ok_or(invalid!("Invalid base path"))?,
                    )
                {
                    let is_absolute_enclosed = base_path
                        .components()
                        .map(Some)
                        .chain(std::iter::once(None))
                        .zip(target.components().map(Some).chain(std::iter::repeat(None)))
                        .all(|(a, b)| match (a, b) {
                            // both components are normal
                            (Some(Component::Normal(a)), Some(Component::Normal(b))) => a == b,
                            // both components consumed fully
                            (None, None) => true,
                            // target consumed fully but base path is not
                            (Some(_), None) => false,
                            // base path consumed fully but target is not (and normal)
                            (None, Some(Component::CurDir | Component::Normal(_))) => true,
                            _ => false,
                        });

                    if !is_absolute_enclosed {
                        return Err(invalid!("Symlink is not inherently safe"));
                    }
                }

                outpath.push(target);
            }
        }
        Ok(())
    }

    /// Get the comment of the file
    pub fn comment(&self) -> &str {
        &self.get_metadata().file_comment
    }

    /// Get the compression method used to store the file
    pub fn compression(&self) -> CompressionMethod {
        self.get_metadata().compression_method
    }

    /// Get if the files is encrypted or not
    pub fn encrypted(&self) -> bool {
        self.data.encrypted
    }

    /// Get the size of the file, in bytes, in the archive
    pub fn compressed_size(&self) -> u64 {
        self.get_metadata().compressed_size
    }

    /// Get the size of the file, in bytes, when uncompressed
    pub fn size(&self) -> u64 {
        self.get_metadata().uncompressed_size
    }

    /// Get the time the file was last modified
    pub fn last_modified(&self) -> Option<DateTime> {
        self.data.last_modified_time
    }
    /// Returns whether the file is actually a directory
    pub fn is_dir(&self) -> bool {
        is_dir(self.name())
    }

    /// Returns whether the file is actually a symbolic link
    pub fn is_symlink(&self) -> bool {
        self.unix_mode()
            .is_some_and(|mode| mode & ffi::S_IFLNK == ffi::S_IFLNK)
    }

    /// Returns whether the file is a normal file (i.e. not a directory or symlink)
    pub fn is_file(&self) -> bool {
        !self.is_dir() && !self.is_symlink()
    }

    /// Get unix mode for the file
    pub fn unix_mode(&self) -> Option<u32> {
        self.get_metadata().unix_mode()
    }

    /// Get the CRC32 hash of the original file
    pub fn crc32(&self) -> u32 {
        self.get_metadata().crc32
    }

    /// Get the extra data of the zip header for this file
    pub fn extra_data(&self) -> Option<&[u8]> {
        self.get_metadata().extra_field.as_deref()
    }

    /// Get the starting offset of the data of the compressed file
    pub fn data_start(&self) -> Option<u64> {
        self.data.data_start.get().copied()
    }

    /// Get the starting offset of the zip header for this file
    pub fn header_start(&self) -> u64 {
        self.get_metadata().header_start
    }
    /// Get the starting offset of the zip header in the central directory for this file
    pub fn central_header_start(&self) -> u64 {
        self.get_metadata().central_header_start
    }

    /// Get the [`SimpleFileOptions`] that would be used to write this file to
    /// a new zip archive.
    pub fn options(&self) -> SimpleFileOptions {
        let mut options = SimpleFileOptions::default()
            .large_file(self.compressed_size().max(self.size()) > ZIP64_BYTES_THR)
            .compression_method(self.compression())
            .unix_permissions(self.unix_mode().unwrap_or(0o644) | ffi::S_IFREG)
            .last_modified_time(
                self.last_modified()
                    .filter(DateTime::is_valid)
                    .unwrap_or_else(DateTime::default_for_write),
            );

        options.normalize();
        #[cfg(feature = "aes-crypto")]
        if let Some((mode, vendor_version, compression_method)) = self.get_metadata().aes_mode {
            // Preserve AES metadata in options for downstream writers.
            // This is metadata-only and does not trigger encryption.
            options.aes_mode = Some(crate::aes::AesModeOptions::new(
                mode,
                vendor_version,
                compression_method,
                None,
            ));
        }
        options
    }
}

/// Methods for retrieving information on zip files
impl<R: Read> ZipFile<'_, R> {
    /// iterate through all extra fields
    pub fn extra_data_fields(&self) -> impl Iterator<Item = &ExtraField> {
        self.data.extra_fields.iter()
    }
}

impl<R: Read + ?Sized> HasZipMetadata for ZipFile<'_, R> {
    fn get_metadata(&self) -> &ZipFileData {
        self.data.as_ref()
    }
}

impl<R: Read + ?Sized> Read for ZipFile<'_, R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.reader.read(buf)
    }

    fn read_exact(&mut self, buf: &mut [u8]) -> io::Result<()> {
        self.reader.read_exact(buf)
    }

    fn read_to_end(&mut self, buf: &mut Vec<u8>) -> io::Result<usize> {
        self.reader.read_to_end(buf)
    }

    fn read_to_string(&mut self, buf: &mut String) -> io::Result<usize> {
        self.reader.read_to_string(buf)
    }
}

impl<R: Read> Read for ZipFileSeek<'_, R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        match &mut self.reader {
            ZipFileSeekReader::Raw(r) => r.read(buf),
        }
    }
}

impl<R: Seek> Seek for ZipFileSeek<'_, R> {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {
        match &mut self.reader {
            ZipFileSeekReader::Raw(r) => r.seek(pos),
        }
    }
}

impl<R> HasZipMetadata for ZipFileSeek<'_, R> {
    fn get_metadata(&self) -> &ZipFileData {
        self.data.as_ref()
    }
}

impl<R: Read + ?Sized> Drop for ZipFile<'_, R> {
    fn drop(&mut self) {
        // self.data is Owned, this reader is constructed by a streaming reader.
        // In this case, we want to exhaust the reader so that the next file is accessible.
        if let Cow::Owned(_) = self.data {
            // Get the inner `Take` reader so all decryption, decompression and CRC calculation is skipped.
            if let Ok(mut inner) = self.take_raw_reader() {
                let _ = copy(&mut inner, &mut sink());
            }
        }
    }
}

/// A filter that determines whether an entry should be ignored when searching
/// for the root directory of a Zip archive.
///
/// Returns `true` if the entry should be considered, and `false` if it should
/// be ignored.
///
/// See [`root_dir_common_filter`] for a sensible default filter.
pub trait RootDirFilter: Fn(&Path) -> bool {}
impl<F: Fn(&Path) -> bool> RootDirFilter for F {}

/// Common filters when finding the root directory of a Zip archive.
///
/// This filter is a sensible default for most use cases and filters out common
/// system files that are usually irrelevant to the contents of the archive.
///
/// Currently, the filter ignores:
/// - `/__MACOSX/`
/// - `/.DS_Store`
/// - `/Thumbs.db`
///
/// **This function is not guaranteed to be stable and may change in future versions.**
///
/// # Example
///
/// ```rust
/// # use std::path::Path;
/// assert!(zip::read::root_dir_common_filter(Path::new("foo.txt")));
/// assert!(!zip::read::root_dir_common_filter(Path::new(".DS_Store")));
/// assert!(!zip::read::root_dir_common_filter(Path::new("Thumbs.db")));
/// assert!(!zip::read::root_dir_common_filter(Path::new("__MACOSX")));
/// assert!(!zip::read::root_dir_common_filter(Path::new("__MACOSX/foo.txt")));
/// ```
#[must_use]
pub fn root_dir_common_filter(path: &Path) -> bool {
    const COMMON_FILTER_ROOT_FILES: &[&str] = &[".DS_Store", "Thumbs.db"];

    if path.starts_with("__MACOSX") {
        return false;
    }

    if path.components().count() == 1
        && path.file_name().is_some_and(|file_name| {
            COMMON_FILTER_ROOT_FILES
                .iter()
                .map(OsStr::new)
                .any(|cmp| cmp == file_name)
        })
    {
        return false;
    }

    true
}

#[cfg(test)]
mod tests {
    use std::io::Cursor;

    /// Only on little endian because we cannot use fs with miri CI
    #[cfg(all(target_endian = "little", not(miri)))]
    #[test]
    fn test_is_symlink() -> std::io::Result<()> {
        use super::ZipArchive;
        use tempfile::TempDir;

        let mut reader = ZipArchive::new(Cursor::new(include_bytes!("../tests/data/symlink.zip")))?;
        assert!(reader.by_index(0)?.is_symlink());
        let tempdir = TempDir::with_prefix("test_is_symlink")?;
        reader.extract(&tempdir)?;
        assert!(tempdir.path().join("bar").is_symlink());
        Ok(())
    }

    #[test]
    #[cfg(feature = "deflate-flate2")]
    fn test_utf8_extra_field() {
        use super::ZipArchive;

        let mut reader =
            ZipArchive::new(Cursor::new(include_bytes!("../tests/data/chinese.zip"))).unwrap();
        reader.by_name("七个房间.txt").unwrap();
    }

    #[test]
    fn test_utf8() {
        use super::ZipArchive;

        let mut reader =
            ZipArchive::new(Cursor::new(include_bytes!("../tests/data/linux-7z.zip"))).unwrap();
        reader.by_name("你好.txt").unwrap();
    }

    #[test]
    fn test_utf8_2() {
        use super::ZipArchive;

        let mut reader = ZipArchive::new(Cursor::new(include_bytes!(
            "../tests/data/windows-7zip.zip"
        )))
        .unwrap();
        reader.by_name("你好.txt").unwrap();
    }

    /// Only on little endian because it runs too long with Miri CI
    #[cfg(all(target_endian = "little", not(miri)))]
    #[test]
    fn test_64k_files() -> crate::result::ZipResult<()> {
        use super::ZipArchive;
        use crate::CompressionMethod::Stored;
        use crate::ZipWriter;
        use crate::types::SimpleFileOptions;
        use std::io::{Read, Write};

        let mut writer = ZipWriter::new(Cursor::new(Vec::new()));
        let options = SimpleFileOptions {
            compression_method: Stored,
            ..Default::default()
        };
        for i in 0..=u16::MAX {
            let file_name = format!("{i}.txt");
            writer.start_file(&*file_name, options)?;
            writer.write_all(i.to_string().as_bytes())?;
        }

        let mut reader = ZipArchive::new(writer.finish()?)?;
        for i in 0..=u16::MAX {
            let expected_name = format!("{i}.txt");
            let expected_contents = i.to_string();
            let expected_contents = expected_contents.as_bytes();
            let mut file = reader.by_name(&expected_name)?;
            let mut contents = Vec::with_capacity(expected_contents.len());
            file.read_to_end(&mut contents)?;
            assert_eq!(contents, expected_contents);
            drop(file);
            contents.clear();
            let mut file = reader.by_index(i as usize)?;
            file.read_to_end(&mut contents)?;
            assert_eq!(contents, expected_contents);
        }
        Ok(())
    }

    /// Symlinks being extracted shouldn't be followed out of the destination directory.
    /// Only on little endian because we cannot use fs with miri CI
    #[cfg(all(target_endian = "little", not(miri)))]
    #[test]
    fn test_cannot_symlink_outside_destination() -> crate::result::ZipResult<()> {
        use crate::ZipWriter;
        use crate::types::SimpleFileOptions;
        use std::fs::create_dir;
        use tempfile::TempDir;

        let mut writer = ZipWriter::new(Cursor::new(Vec::new()));
        writer.add_symlink("symlink/", "../dest-sibling/", SimpleFileOptions::default())?;
        writer.start_file("symlink/dest-file", SimpleFileOptions::default())?;
        let mut reader = writer.finish_into_readable()?;
        let dest_parent = TempDir::with_prefix("read__test_cannot_symlink_outside_destination")?;
        let dest_sibling = dest_parent.path().join("dest-sibling");
        create_dir(&dest_sibling)?;
        let dest = dest_parent.path().join("dest");
        create_dir(&dest)?;
        assert!(reader.extract(dest).is_err());
        assert!(!dest_sibling.join("dest-file").exists());
        Ok(())
    }

    /// Only on little endian because we cannot use fs with miri CI
    #[cfg(all(target_endian = "little", not(miri)))]
    #[test]
    fn test_can_create_destination() -> crate::result::ZipResult<()> {
        use super::ZipArchive;
        use tempfile::TempDir;

        let mut reader =
            ZipArchive::new(Cursor::new(include_bytes!("../tests/data/mimetype.zip")))?;
        let dest = TempDir::with_prefix("read__test_can_create_destination")?;
        reader.extract(&dest)?;
        assert!(dest.path().join("mimetype").exists());
        Ok(())
    }
}
