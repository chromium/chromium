use crate::result::{ZipResult, invalid};
use crate::unstable::LittleEndianReadExt;
use core::mem::size_of;
use std::io::Read;

/// Info-ZIP Unicode Path Extra Field (0x7075) or Unicode Comment Extra Field (0x6375), as
/// specified in APPNOTE 4.6.8 and 4.6.9
#[derive(Clone, Debug)]
pub struct UnicodeExtraField {
    crc32: u32,
    content: Box<[u8]>,
}

impl UnicodeExtraField {
    /// Verifies the checksum and returns the content.
    ///
    /// # Errors
    ///
    /// Returns an error when the crc32 from the extra field does not match the crc32 of the
    /// `ascii_field`.
    pub fn unwrap_valid(self, ascii_field: &[u8]) -> ZipResult<Box<[u8]>> {
        let computed_crc32 = crc32fast::hash(ascii_field);
        if self.crc32 != computed_crc32 {
            return Err(invalid!(
                "CRC32 checksum failed on Unicode extra field, it is '{:#08X}' and it should be '{:#08X}'",
                self.crc32,
                computed_crc32
            ));
        }
        Ok(self.content)
    }
}

impl UnicodeExtraField {
    pub(crate) fn try_from_reader<R: Read>(reader: &mut R, len: u16) -> ZipResult<Self> {
        // Read and discard version byte
        reader.read_exact(&mut [0u8])?;

        let crc32 = reader.read_u32_le()?;
        let content_len = (len as usize)
            .checked_sub(size_of::<u8>() + size_of::<u32>())
            .ok_or(invalid!("Unicode extra field is too small"))?;
        let mut content = vec![0u8; content_len].into_boxed_slice();
        reader.read_exact(&mut content)?;
        Ok(Self { crc32, content })
    }
}

#[cfg(test)]
mod tests {
    use crate::extra_fields::UnicodeExtraField;
    #[test]
    fn unicode_extra_field_crc32_correct() {
        let data = [
            0x01, 0xef, 0x39, 0x8e, 0x4b, 'u' as u8, 't' as u8, 'f' as u8, '-' as u8, '8' as u8,
        ];
        let extra =
            UnicodeExtraField::try_from_reader(&mut std::io::Cursor::new(data), 10).unwrap();
        let res = extra.unwrap_valid(b"abcdef");
        assert!(res.is_ok());
        let content = res.unwrap();
        assert_eq!(content.as_ref(), b"utf-8");
    }

    #[test]
    fn unicode_extra_field_crc32_incorrect() {
        let data = [
            0x01, 0x00, 0x00, 0x00, 0x00, 'u' as u8, 't' as u8, 'f' as u8, '-' as u8, '8' as u8,
        ];
        let extra =
            UnicodeExtraField::try_from_reader(&mut std::io::Cursor::new(data), 10).unwrap();
        let res = extra.unwrap_valid(b"abcdef");
        assert!(res.is_err());
    }
}
