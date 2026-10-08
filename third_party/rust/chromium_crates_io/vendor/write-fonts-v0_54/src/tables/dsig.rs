//! The [DSIG](https://learn.microsoft.com/en-us/typography/opentype/spec/dsig) table

include!("../../generated/generated_dsig.rs");

impl SignatureRecord {
    fn compute_signature_block_len(&self) -> u32 {
        self.signature_block.compute_len()
    }
}

impl SignatureBlockFormat1 {
    const HEADER_LENGTH: usize = 2 + 2 + 4;

    fn compute_len(&self) -> u32 {
        (Self::HEADER_LENGTH + self.signature.len())
            .try_into()
            .expect("DSIG signature block overflow")
    }
}

#[cfg(test)]
mod tests {
    use font_test_data::bebuffer::BeBuffer;

    use super::{Dsig, PermissionFlags, SignatureBlockFormat1, SignatureRecord};
    use crate::dump_table;

    /// An empty, dummy DSIG, as inserted by fonttools.
    /// See <https://github.com/fonttools/fonttools/blob/ec716f11851f8d5a04e3f535b53219d97001482a/Lib/fontTools/fontBuilder.py#L823-L833>.
    #[test]
    fn test_empty() {
        let empty = Dsig {
            flags: PermissionFlags::empty(),
            signature_records: Vec::new(),
        };

        let actual = dump_table(&empty).unwrap();

        let expected = BeBuffer::new()
            .push(0x1u32) // version
            .push(0x0u16) // numSignatures
            .push(0x0u16); // flags

        assert_eq!(expected.data(), actual);
    }

    // A DSIG with a single entry. For ease-of-testing, we use 0xDEADBEEF
    // instead of a full PKCS#7 packet, as it would not be this crate's
    // responsibility to validate it anyway.
    #[test]
    fn test_beef() {
        let beef = Dsig {
            flags: PermissionFlags::CANNOT_BE_RESIGNED,
            signature_records: vec![SignatureRecord {
                signature_block: SignatureBlockFormat1 {
                    signature: vec![0xDE, 0xAD, 0xBE, 0xEF],
                }
                .into(),
            }],
        };

        let actual = dump_table(&beef).unwrap();

        let expected = BeBuffer::new()
            .push(0x1_u32) // DsigHeader.version
            .push(0x1_u16) // DsigHeader.numSignatures
            .push(0x1_u16) // DsigHeader.flags
            .push(0x1_u32) // SignatureRecord.format
            .push(0xC_u32) // SignatureRecord.length
            .push(0x14_u32) // SignatureRecord.signatureBlockOffset
            .push(0x0_u16) // SignatureBlockFormat1.reserved1
            .push(0x0_u16) // SignatureBlockFormat1.reserved2
            .push(0x4_u32) // SignatureBlockFormat1.signatureLength
            .push(0xDEADBEEF_u32); // SignatureBlockFormat1.signature

        assert_eq!(expected.data(), actual);
    }
}
