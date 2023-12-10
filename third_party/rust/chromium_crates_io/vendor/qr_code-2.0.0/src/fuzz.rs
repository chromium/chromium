use crate::decode::BmpDecode;
use crate::structured::{merge_qrs, SplittedQr};
use crate::{QrCode, Version};

impl arbitrary::Arbitrary for QrCode {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let level = crate::EcLevel::arbitrary(u)?;
        let version = crate::Version::arbitrary(u)?;
        let data = <Vec<u8>>::arbitrary(u)?;
        let qr_code = QrCode::with_version(data, version, level)
            .map_err(|_| arbitrary::Error::IncorrectFormat)?;
        Ok(qr_code)
    }
}

#[derive(Debug)]
/// doc
pub struct QrCodeData {
    /// qr
    pub qr_code: QrCode,
    /// data
    pub data: Vec<u8>,
    /// mul
    pub mul_border: Option<(u8, u8)>,
}

impl QrCodeData {
    /// used for fuzz tests
    pub fn check(self) {
        let crate::QrCodeData {
            qr_code,
            data,
            mul_border,
        } = self;
        let base_bmp = qr_code.to_bmp();
        let bmp = match mul_border {
            None => base_bmp,
            Some((mul, border)) => base_bmp
                .mul(mul)
                .and_then(|mul_bmp| mul_bmp.add_white_border(border))
                .unwrap_or(base_bmp)
                .normalize(),
        };
        let decoded = bmp.decode().unwrap();
        assert_eq!(data, decoded);
    }
}

impl arbitrary::Arbitrary for QrCodeData {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let level = crate::EcLevel::arbitrary(u)?;
        let version = crate::Version::arbitrary(u)?;
        let data = <Vec<u8>>::arbitrary(u)?;
        let qr_code = QrCode::with_version(&data, version, level)
            .map_err(|_| arbitrary::Error::IncorrectFormat)?;
        let mul_border = u8::arbitrary(u)?;
        let mul_border = if mul_border % 2 == 0 {
            None
        } else {
            Some(((mul_border / 64) + 2, (mul_border % 64) + 1))
        };

        Ok(QrCodeData {
            qr_code,
            data,
            mul_border,
        })
    }
}

impl arbitrary::Arbitrary for Version {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let v = u8::arbitrary(u)?;
        match v {
            1..=40 => Ok(Version::Normal(v as i16)),
            //41..=44 => Ok(Version::Micro((v-40u8) as i16)),  not supported for now
            _ => Err(arbitrary::Error::IncorrectFormat),
        }
    }
}

/// split merge round-trip for fuzz testing
pub fn split_merge_rt(version: i16, bytes: Vec<u8>) {
    if let Ok(splitted_qr) = SplittedQr::new(bytes.clone(), version) {
        if let Ok(qrs) = splitted_qr.split() {
            //println!("qrs len:{}", qrs.len());
            let mut vec = vec![];
            for qr in qrs {
                let decoded = qr.to_bmp().decode().unwrap();
                vec.push(decoded);
            }
            if let Ok(merged) = merge_qrs(vec) {
                assert_eq!(merged, bytes);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::fuzz::split_merge_rt;

    #[test]
    fn test_fuzz_decode_check() {
        use arbitrary::Arbitrary;
        let data = include_bytes!(
            "../fuzz/artifacts/decode_check/crash-045dc09e6d00cfd6d3b3e9a4cfe5835e410c5fd1"
        );
        let unstructured = arbitrary::Unstructured::new(&data[..]);
        let qr_code_data = crate::QrCodeData::arbitrary_take_rest(unstructured).unwrap();
        qr_code_data.check();
    }

    #[test]
    fn test_fuzz_encode() {
        let data = include_bytes!(
            "../fuzz/artifacts/encode/crash-9ffe42701f80d18e9c114f1134b9d64045f7d5ce"
        );
        assert!(crate::QrCode::new(data).is_ok());
    }

    #[test]
    fn test_fuzz_split_merge_rt() {
        use arbitrary::Arbitrary;
        let data = include_bytes!(
            "../fuzz/artifacts/split_merge_rt/crash-1e272488adbe45a04a14b1a2b848997ff64e1b74"
        );
        let unstructured = arbitrary::Unstructured::new(&data[..]);
        let (version, bytes) = <(i16, Vec<u8>)>::arbitrary_take_rest(unstructured).unwrap();
        split_merge_rt(version, bytes);
    }
}
