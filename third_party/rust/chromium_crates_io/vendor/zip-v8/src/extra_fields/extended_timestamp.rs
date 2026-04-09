//! Extended Timestamp extra field
//! Defined in <https://libzip.org/specifications/extrafld.txt>

use crate::result::invalid;
use crate::result::{ZipError, ZipResult};
use crate::unstable::LittleEndianReadExt;
use core::mem;
use std::io::Read;

/// `ExtendedTimestamp` Flags
#[rustfmt::skip]
#[repr(u8)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum ExtendedTimestampFlags {
    /// modification time is present
    Modified = 0b0000_0001_u8,
    /// access time is present
    Accessed = 0b0000_0010_u8,
    /// creation time is present
    Created  = 0b0000_0100_u8,
    // others bytes reversed
}

impl ExtendedTimestampFlags {
    #[inline(always)]
    fn matching(flags: u8, matching_flag: Self) -> bool {
        flags & u8::from(matching_flag) != 0
    }
}

impl From<ExtendedTimestampFlags> for u8 {
    fn from(extended_timestamp: ExtendedTimestampFlags) -> u8 {
        extended_timestamp as u8
    }
}

/// Extended timestamp, as described in <https://libzip.org/specifications/extrafld.txt>
#[derive(Debug, Clone)]
pub struct ExtendedTimestamp {
    modified: Option<u32>,
    accessed: Option<u32>,
    created: Option<u32>,
}

impl ExtendedTimestamp {
    const MAX_LENGTH: u16 = (mem::size_of::<u8>()
        + mem::size_of::<u32>()
        + mem::size_of::<u32>()
        + mem::size_of::<u32>()) as u16;
    /// creates an extended timestamp struct by reading the required bytes from the reader.
    ///
    /// This method assumes that the length has already been read, therefore
    /// it must be passed as an argument
    ///
    /// # Errors
    ///
    /// Fails if the len is empty or a read fails
    ///
    pub fn try_from_reader<R>(reader: &mut R, len: u16) -> ZipResult<Self>
    where
        R: Read,
    {
        if len == 0 {
            return Err(invalid!("Extended timestamp field is empty"));
        }
        let mut flags = [0u8];
        let mut bytes_to_read = len as usize;
        reader.read_exact(&mut flags)?;
        bytes_to_read = bytes_to_read
            .checked_sub(flags.len())
            .ok_or(invalid!("Extended timestamp field too short for flags"))?;
        let flags = flags[0];

        // the `flags` field refers to the local headers and might not correspond
        // to the len field. If the length field is 1+4, we assume that only
        // the modification time has been set

        // > Those times that are present will appear in the order indicated, but
        // > any combination of times may be omitted.  (Creation time may be
        // > present without access time, for example.)  TSize should equal
        // > (1 + 4*(number of set bits in Flags)), as the block is currently
        // > defined.
        if len != 5 && u32::from(len) != 1 + 4 * flags.count_ones() {
            return Err(ZipError::Io(std::io::Error::other(format!(
                "flags and len don't match in extended timestamp field len={len} flags={flags:08b}"
            ))));
        }

        let modified =
            if (ExtendedTimestampFlags::matching(flags, ExtendedTimestampFlags::Modified)
                && bytes_to_read >= mem::size_of::<u32>())
                || len == Self::MAX_LENGTH
            {
                bytes_to_read = bytes_to_read.checked_sub(mem::size_of::<u32>()).ok_or(
                    invalid!(
                        "Extended timestamp field too short for mod_time len={} flags={flags:08b}",
                        len
                    ),
                )?;
                Some(reader.read_u32_le()?)
            } else {
                None
            };

        let accessed =
            if (ExtendedTimestampFlags::matching(flags, ExtendedTimestampFlags::Accessed)
                && bytes_to_read >= mem::size_of::<u32>())
                || len == Self::MAX_LENGTH
            {
                bytes_to_read = bytes_to_read.checked_sub(mem::size_of::<u32>()).ok_or(
                    invalid!(
                        "Extended timestamp field too short for ac_time len={} flags={flags:08b}",
                        len
                    ),
                )?;
                Some(reader.read_u32_le()?)
            } else {
                None
            };

        let created = if (ExtendedTimestampFlags::matching(flags, ExtendedTimestampFlags::Created)
            && bytes_to_read >= mem::size_of::<u32>())
            || len == Self::MAX_LENGTH
        {
            bytes_to_read = bytes_to_read
                .checked_sub(mem::size_of::<u32>())
                .ok_or(invalid!(
                    "Extended timestamp field too short for cr_time len={} flags={flags:08b}",
                    len
                ))?;
            Some(reader.read_u32_le()?)
        } else {
            None
        };

        if bytes_to_read > 0 {
            // ignore undocumented bytes
            reader.read_exact(&mut vec![0; bytes_to_read])?;
        }

        Ok(Self {
            modified,
            accessed,
            created,
        })
    }

    /// returns the last modification timestamp, if defined, as UNIX epoch seconds
    #[must_use]
    pub fn mod_time(&self) -> Option<u32> {
        self.modified
    }

    /// returns the last access timestamp, if defined, as UNIX epoch seconds
    #[must_use]
    pub fn ac_time(&self) -> Option<u32> {
        self.accessed
    }

    /// returns the creation timestamp, if defined, as UNIX epoch seconds
    #[must_use]
    pub fn cr_time(&self) -> Option<u32> {
        self.created
    }
}

#[cfg(test)]
mod tests {
    use super::ExtendedTimestamp;

    use std::io::Cursor;

    /// Ensure we don't panic or read garbage data if the field body is empty
    #[test]
    pub fn test_bad_extended_timestamp() {
        use crate::ZipArchive;

        assert!(
            ZipArchive::new(Cursor::new(include_bytes!(
                "../../tests/data/extended_timestamp_bad.zip"
            )))
            .is_err()
        );
    }

    /// Ensure that a truncated extended timestamp (len too short for flags)
    /// returns an error instead of panicking from a subtraction overflow.
    #[test]
    fn test_extended_timestamp_overflow() {
        let tests_args = [
            (vec![0b0000_0001_u8, 0x00, 0x00, 0x00], 0), // len is 0
            (vec![0b0000_0001_u8, 0x00, 0x00, 0x00], 2), // len is 2 (not enough)
            (vec![0b0000_0010_u8, 0x00, 0x00, 0x00], 3), // len is 3 (not enough)
            (vec![0b0000_0100_u8, 0x00, 0x00, 0x00], 4), // len is 4 (not enough)
            // len is 8 (not enough)
            (
                vec![
                    0b0000_0011_u8,
                    0x00,
                    0x00,
                    0x00,
                    0x00,
                    0x00,
                    0x00,
                    0x00,
                    0x00,
                ],
                8,
            ),
            // len is too long and missing 4 bytes
            (vec![0b0000_0011_u8, 0x00, 0x00, 0x00, 0x00], 8),
            (vec![0b0000_0000_u8], 5), // too long len
        ];
        for (data, len) in tests_args {
            let mut cursor = Cursor::new(data);
            let result = ExtendedTimestamp::try_from_reader(&mut cursor, len);
            assert!(result.is_err());
        }
    }

    #[test]
    fn check_extended_timestamp_value() {
        let mut cursor = Cursor::new(&[0b0000_0001_u8, 0x00, 0x00, 0x00, 0x01]);
        let result = ExtendedTimestamp::try_from_reader(&mut cursor, 5).unwrap();
        assert_eq!(result.mod_time(), Some(16777216));
        assert_eq!(result.ac_time(), None);
        assert_eq!(result.cr_time(), None);

        let mut cursor = Cursor::new(&[0b0000_0010_u8, 0x00, 0x00, 0x00, 0x02]);
        let result = ExtendedTimestamp::try_from_reader(&mut cursor, 5).unwrap();
        assert_eq!(result.mod_time(), None);
        assert_eq!(result.ac_time(), Some(33554432));
        assert_eq!(result.cr_time(), None);

        let mut cursor = Cursor::new(&[0b0000_0100_u8, 0x00, 0x00, 0x00, 0x03]);
        let result = ExtendedTimestamp::try_from_reader(&mut cursor, 5).unwrap();
        assert_eq!(result.mod_time(), None);
        assert_eq!(result.ac_time(), None);
        assert_eq!(result.cr_time(), Some(50331648));

        let mut cursor = Cursor::new(&[
            0b0000_0011_u8,
            0x00,
            0x00,
            0x00,
            0x01,
            0x00,
            0x00,
            0x00,
            0x02,
        ]);
        let result = ExtendedTimestamp::try_from_reader(&mut cursor, 9).unwrap();
        assert_eq!(result.mod_time(), Some(16777216));
        assert_eq!(result.ac_time(), Some(33554432));
        assert_eq!(result.cr_time(), None);

        let mut cursor = Cursor::new(&[
            0b0000_0111_u8,
            0x00,
            0x00,
            0x00,
            0x01,
            0x00,
            0x00,
            0x00,
            0x02,
            0x00,
            0x00,
            0x00,
            0x03,
        ]);
        let result = ExtendedTimestamp::try_from_reader(&mut cursor, 13).unwrap();
        assert_eq!(result.mod_time(), Some(16777216));
        assert_eq!(result.ac_time(), Some(33554432));
        assert_eq!(result.cr_time(), Some(50331648));
    }

    #[test]
    fn test_extended_timestamp() {
        // in the central header
        let mut cursor = Cursor::new(&[0b0000_0111_u8, 0x00, 0x00, 0x00, 0x01]);
        let result = ExtendedTimestamp::try_from_reader(&mut cursor, 5).unwrap();
        assert_eq!(result.mod_time(), Some(16777216));
        assert_eq!(result.ac_time(), None);
        assert_eq!(result.cr_time(), None);

        // in the local header
        let mut cursor = Cursor::new(&[
            0b0000_0111_u8,
            0x00,
            0x00,
            0x00,
            0x01,
            0x00,
            0x00,
            0x00,
            0x02,
            0x00,
            0x00,
            0x00,
            0x03,
        ]);
        let result = ExtendedTimestamp::try_from_reader(&mut cursor, 13).unwrap();
        assert_eq!(result.mod_time(), Some(16777216));
        assert_eq!(result.ac_time(), Some(33554432));
        assert_eq!(result.cr_time(), Some(50331648));
    }
}
