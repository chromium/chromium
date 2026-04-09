//! AE-x encryption structure extra field

use std::io::{ErrorKind, Read};

use crate::AesMode;
use crate::CompressionMethod;
use crate::extra_fields::UsedExtraField;
use crate::result::{ZipError, ZipResult, invalid, invalid_archive_const};
use crate::spec::Pod;
use crate::types::AesVendorVersion;
use crate::unstable::LittleEndianReadExt;

#[derive(Copy, Clone)]
#[repr(packed, C)]
pub(crate) struct AexEncryption {
    header_id: u16,
    data_size: u16,
    pub(crate) version: u16,
    vendor_id: u16,
    aes_mode: u8,
    compression_method: u16,
}

unsafe impl Pod for AexEncryption {}

impl AexEncryption {
    #[inline(always)]
    pub(crate) fn to_le(mut self) -> Self {
        self.header_id = u16::to_le(self.header_id);
        self.data_size = u16::to_le(self.data_size);
        self.version = u16::to_le(self.version);
        self.vendor_id = u16::to_le(self.vendor_id);
        self.aes_mode = u8::to_le(self.aes_mode);
        self.compression_method = u16::to_le(self.compression_method);
        self
    }

    #[inline]
    pub(crate) fn new(
        version: AesVendorVersion,
        aes_mode: AesMode,
        compression_method: CompressionMethod,
    ) -> Self {
        Self {
            header_id: UsedExtraField::AeXEncryption.as_u16(),
            data_size: (size_of::<u16>() + size_of::<u16>() + size_of::<u8>() + size_of::<u16>())
                as u16,
            version: version.as_u16(),
            vendor_id: u16::from_le_bytes(*b"AE"),
            aes_mode: aes_mode.as_u8(),
            compression_method: compression_method.serialize_to_u16(),
        }
        .to_le()
    }

    #[inline]
    pub(crate) fn parse<R: Read>(
        reader: &mut R,
        len: u16,
        aes_mode_options: &mut Option<(AesMode, AesVendorVersion, CompressionMethod)>,
        compression_method: &mut CompressionMethod,
    ) -> ZipResult<()> {
        if len != 7 {
            return Err(ZipError::UnsupportedArchive(
                "AES extra data field has an unsupported length",
            ));
        }
        let vendor_version = reader.read_u16_le()?;
        let vendor_id = reader.read_u16_le()?;
        let mut buff = [0u8];
        if let Err(e) = reader.read_exact(&mut buff) {
            if e.kind() == ErrorKind::UnexpectedEof {
                return Err(invalid!("AES extra field truncated"));
            }
            return Err(e.into());
        }
        if vendor_id != 0x4541 {
            return Err(invalid!("Invalid AES vendor"));
        }
        let vendor_version = vendor_version.try_into().map_err(invalid_archive_const)?;
        let aes_mode = buff[0].try_into().map_err(invalid_archive_const)?;
        let comp_method = CompressionMethod::parse_from_u16(reader.read_u16_le()?);
        *aes_mode_options = Some((aes_mode, vendor_version, comp_method));
        *compression_method = comp_method;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_create_aex() {
        use super::AexEncryption;
        use crate::AesMode;
        use crate::CompressionMethod;
        use crate::spec::Pod;
        use crate::types::AesVendorVersion;

        let aex_encryption = AexEncryption::new(
            AesVendorVersion::Ae2,
            AesMode::Aes256,
            CompressionMethod::Stored,
        );

        let buf = aex_encryption.as_bytes();
        assert_eq!(buf.len(), 11);
        assert_eq!(buf[0..2], [1, 153]);
        assert_eq!(buf[2..4], [7, 0]);
        assert_eq!(buf[4..6], [2, 0]);
        assert_eq!(buf[6..8], [65, 69]);
        assert_eq!(buf[8], 0x03);
        assert_eq!(buf[9..], [0, 0]);

        // test length used in write.rs
        assert_eq!(buf[std::mem::offset_of!(AexEncryption, version)..].len(), 7);
    }

    #[test]
    fn test_too_long_length() {
        use super::AexEncryption;
        use crate::CompressionMethod;
        use std::io::Cursor;

        let data = &[0, 1, 2, 3, 4, 5, 6, 7];
        let len = data.len() as u16;
        let mut cursor = Cursor::new(data);
        let mut aes_mode_options = None;
        let mut compression_method = CompressionMethod::Stored;

        let res = AexEncryption::parse(
            &mut cursor,
            len,
            &mut aes_mode_options,
            &mut compression_method,
        );
        assert!(res.is_err());
    }

    #[test]
    fn test_serialize_parse() {
        use super::AexEncryption;
        use crate::AesMode;
        use crate::CompressionMethod;
        use crate::spec::Pod;
        use crate::types::AesVendorVersion;
        use std::io::Cursor;

        let aex_encryption = AexEncryption::new(
            AesVendorVersion::Ae2,
            AesMode::Aes256,
            CompressionMethod::Stored,
        );

        let data = aex_encryption.as_bytes();
        let len_data = u16::from_le_bytes([data[2], data[3]]);
        let data = &data[4..]; // remove the signature
        let len = data.len() as u16;
        assert_eq!(len_data, len);
        assert_eq!(len, 7);
        let mut cursor = Cursor::new(data);
        let mut aes_mode_options = None;
        let mut compression_method = CompressionMethod::Stored;

        let res = AexEncryption::parse(
            &mut cursor,
            len,
            &mut aes_mode_options,
            &mut compression_method,
        );
        assert!(res.is_ok());
        assert_eq!(
            aes_mode_options,
            Some((
                AesMode::Aes256,
                AesVendorVersion::Ae2,
                CompressionMethod::Stored
            ))
        );
    }
}
