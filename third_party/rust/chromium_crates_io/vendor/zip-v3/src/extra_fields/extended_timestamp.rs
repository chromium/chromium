use crate::result::{ZipError, ZipResult};
use crate::unstable::LittleEndianReadExt;
use std::io::Read;

/// extended timestamp, as described in <https://libzip.org/specifications/extrafld.txt>

#[derive(Debug, Clone)]
pub struct ExtendedTimestamp {
    mod_time: Option<u32>,
    ac_time: Option<u32>,
    cr_time: Option<u32>,
}

impl ExtendedTimestamp {
    /// creates an extended timestamp struct by reading the required bytes from the reader.
    ///
    /// This method assumes that the length has already been read, therefore
    /// it must be passed as an argument
    pub fn try_from_reader<R>(reader: &mut R, len: u16) -> ZipResult<Self>
    where
        R: Read,
    {
        let mut flags = [0u8];
        reader.read_exact(&mut flags)?;
        let flags = flags[0];

        // the `flags` field refers to the local headers and might not correspond
        // to the len field. If the length field is 1+4, we assume that only
        // the modification time has been set

        // > Those times that are present will appear in the order indicated, but
        // > any combination of times may be omitted.  (Creation time may be
        // > present without access time, for example.)  TSize should equal
        // > (1 + 4*(number of set bits in Flags)), as the block is currently
        // > defined.
        if len != 5 && len as u32 != 1 + 4 * flags.count_ones() {
            //panic!("found len {len} and flags {flags:08b}");
            return Err(ZipError::UnsupportedArchive(
                "flags and len don't match in extended timestamp field",
            ));
        }

        if flags & 0b11111000 != 0 {
            return Err(ZipError::UnsupportedArchive(
                "found unsupported timestamps in the extended timestamp header",
            ));
        }

        let mod_time = if (flags & 0b00000001u8 == 0b00000001u8) || len == 5 {
            Some(reader.read_u32_le()?)
        } else {
            None
        };

        let ac_time = if flags & 0b00000010u8 == 0b00000010u8 && len > 5 {
            Some(reader.read_u32_le()?)
        } else {
            None
        };

        let cr_time = if flags & 0b00000100u8 == 0b00000100u8 && len > 5 {
            Some(reader.read_u32_le()?)
        } else {
            None
        };
        Ok(Self {
            mod_time,
            ac_time,
            cr_time,
        })
    }

    /// returns the last modification timestamp, if defined, as UNIX epoch seconds
    pub fn mod_time(&self) -> Option<u32> {
        self.mod_time
    }

    /// returns the last access timestamp, if defined, as UNIX epoch seconds
    pub fn ac_time(&self) -> Option<u32> {
        self.ac_time
    }

    /// returns the creation timestamp, if defined, as UNIX epoch seconds
    pub fn cr_time(&self) -> Option<u32> {
        self.cr_time
    }
}
