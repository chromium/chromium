//! Utility methods to support structured append (multiple QR codes)

use crate::bits::{Bits, ExtendedMode};
use crate::structured::StructuredQrError::*;
use crate::types::QrError::Structured;
use crate::{bits, EcLevel, QrCode, QrResult, Version};
use std::convert::TryFrom;
use std::fmt::{Display, Formatter};

/// Error variants regarding structured append
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum StructuredQrError {
    /// Structured append mode should contain at least two qrcodes
    AtLeast2Pieces,
    /// Encoded number of pieces does not match real number of pieces
    TotalMismatch(usize),
    /// Not all the QR codes parts are present
    MissingParts,
    /// Computed parity byte does not match
    Parity,
    /// QR code data shorter than 5 bytes
    TooShort,
    /// QR code mode is not structured append
    StructuredWrongMode,
    /// QR code encoding is not the one supported in structured append
    StructuredWrongEnc,
    /// QR code sequence number is greater than total
    SeqGreaterThanTotal(u8, u8),
    /// QR code length mismatch
    LengthMismatch(usize, usize),
    /// Unsupported version
    UnsupportedVersion(i16),
    /// Maximum supported pieces in structured append is 16
    SplitMax16(usize),
}

impl Display for StructuredQrError {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            AtLeast2Pieces => write!(f, "Need at least 2 different pieces to merge structured QR"),
            TotalMismatch(i) => write!(f, "Total pieces in input {} does not match the encoded total, or different encoded totals", i),
            MissingParts => write!(f, "Not all the part are present"),
            Parity => write!(f, "Invalid parities while merging"),
            TooShort => write!(f, "QR data shorter than 5 bytes"),
            StructuredWrongMode => write!(f, "Structured append QR must have mode 3"),
            StructuredWrongEnc => write!(f, "Structured append QR must have encoding 4"),
            SeqGreaterThanTotal(s, t) => write!(f, "QR sequence {} greater than total {}", s, t),
            LengthMismatch(calc, exp) => write!(f, "calculated end {} greater than effective length {}", calc, exp),
            UnsupportedVersion(ver) => write!(f, "Unsupported version {}", ver),
            SplitMax16(req) => write!(f, "Could split into max 16 qr, requested {}", req),
        }
    }
}

/// Merge multiple structured QR codes content into the original content
pub fn merge_qrs(mut bytes: Vec<Vec<u8>>) -> QrResult<Vec<u8>> {
    use std::collections::HashSet;
    use std::convert::TryInto;

    let mut vec_structured = vec![];

    bytes.sort();
    bytes.dedup();

    if bytes.len() < 2 {
        return Err(Structured(AtLeast2Pieces));
    }

    for vec in bytes {
        let current: StructuredQr = vec.try_into()?;
        vec_structured.push(current);
    }

    let total = (vec_structured.len() - 1) as u8;
    let totals_same = vec_structured.iter().map(|q| q.total).all(|t| t == total);
    if !totals_same {
        return Err(Structured(TotalMismatch(vec_structured.len())));
    }

    let sequences: HashSet<u8> = vec_structured.iter().map(|q| q.seq).collect();
    let all_sequence = sequences.len() == vec_structured.len();
    if !all_sequence {
        return Err(Structured(MissingParts));
    }

    vec_structured.sort_by(|a, b| a.seq.cmp(&b.seq)); // allows to merge out of order by reordering here
    let result: Vec<u8> = vec_structured
        .iter()
        .flat_map(|q| q.content.clone())
        .collect();

    let final_parity = result.iter().fold(0u8, |acc, &x| acc ^ x);
    if vec_structured
        .iter()
        .map(|q| q.parity)
        .all(|p| p == final_parity)
    {
        Ok(result)
    } else {
        Err(Structured(Parity))
    }
}

struct StructuredQr {
    pub seq: u8,   // u4
    pub total: u8, // u4
    pub parity: u8,
    pub content: Vec<u8>,
}

impl TryFrom<Vec<u8>> for StructuredQr {
    type Error = crate::types::QrError;

    fn try_from(value: Vec<u8>) -> QrResult<Self> {
        if value.len() < 5 {
            return Err(Structured(TooShort));
        }
        let qr_mode = value[0] >> 4;
        if qr_mode != 3 {
            return Err(Structured(StructuredWrongMode));
        }
        let seq = value[0] & 0x0f;
        let total = value[1] >> 4;
        if seq > total {
            return Err(Structured(SeqGreaterThanTotal(seq, total)));
        }
        let parity = ((value[1] & 0x0f) << 4) + (value[2] >> 4);
        let enc_mode = value[2] & 0x0f;
        if enc_mode != 4 {
            return Err(Structured(StructuredWrongEnc));
        }

        let (length, from) = if value.len() < u8::max_value() as usize + 4 {
            // 4 is header size, TODO recheck boundary
            (value[3] as u16, 4usize)
        } else {
            (((value[3] as u16) << 8) + (value[4] as u16), 5usize)
        };
        let end = from + length as usize;
        if value.len() < end {
            return Err(Structured(LengthMismatch(end, value.len())));
        }
        let content = value[from..end].to_vec();
        //TODO check padding

        Ok(StructuredQr {
            seq,
            total,
            parity,
            content,
        })
    }
}

/// Represent a QR code that could be splitted in `total_qr` QR codes
pub struct SplittedQr {
    /// QR code version
    pub version: i16,
    /// QR code parity (will be the same in any QR code of the sequence)
    pub parity: u8,
    /// Total QR codes necessary to encode `bytes` with `version` QR codes
    pub total_qr: usize,
    /// QR code content
    pub bytes: Vec<u8>,
}

impl SplittedQr {
    /// Creates a new `SplittedQr` if possible given `bytes` len and max QR code `version` to use
    pub fn new(bytes: Vec<u8>, version: i16) -> QrResult<Self> {
        let parity = bytes.iter().fold(0u8, |acc, &x| acc ^ x);
        if version == 0 {
            return Err(Structured(UnsupportedVersion(version)));
        }
        let max_bytes = *MAX_BYTES
            .get(version as usize)
            .ok_or(Structured(UnsupportedVersion(version)))?;
        let extra = if bytes.len() % max_bytes == 0 { 0 } else { 1 };
        let total_qr = bytes.len() / max_bytes + extra;
        if total_qr > 16 {
            return Err(Structured(SplitMax16(total_qr)));
        }

        Ok(SplittedQr {
            bytes,
            version,
            parity,
            total_qr,
        })
    }

    fn split_to_bits(&self) -> QrResult<Vec<Bits>> {
        let max_bytes = MAX_BYTES[self.version as usize];
        if self.total_qr == 1 {
            let bits = bits::encode_auto(&self.bytes, LEVEL)?;
            Ok(vec![bits])
        } else {
            let mut result = vec![];
            for (i, chunk) in self.bytes.chunks(max_bytes).enumerate() {
                let bits = self.make_chunk(i, chunk)?;
                result.push(bits);
            }
            Ok(result)
        }
    }

    /// Creates the sequence of QR codes that merged back would produce `self.bytes`
    pub fn split(&self) -> QrResult<Vec<QrCode>> {
        self.split_to_bits()?
            .into_iter()
            .map(|bits| QrCode::with_bits(bits, LEVEL))
            .collect()
    }

    fn make_chunk(&self, i: usize, chunk: &[u8]) -> QrResult<Bits> {
        //println!("chunk len : {} version: {}", chunk.len(), self.version);
        //println!("chunk : {}", hex::encode(chunk) );
        let mut bits = Bits::new(Version::Normal(self.version));
        bits.push_mode_indicator(ExtendedMode::StructuredAppend)?;
        bits.push_number_checked(4, i)?;
        bits.push_number_checked(4, self.total_qr - 1)?;
        bits.push_number_checked(8, self.parity as usize)?;
        bits.push_byte_data(chunk)?;
        bits.push_terminator(LEVEL)?;

        //println!("bits: {}\n", hex::encode(bits.clone().into_bytes()));

        Ok(bits)
    }
}

const LEVEL: crate::types::EcLevel = EcLevel::L;

/// Max bytes encodable in a structured append qr code, given Qr code version as array index
const MAX_BYTES: [usize; 33] = [
    0, 15, 30, 51, 76, 104, 132, 152, 190, 228, 269, 319, 365, 423, 456, 518, 584, 642, 716, 790,
    856, 927, 1001, 1089, 1169, 1271, 1365, 1463, 1526, 1626, 1730, 1838, 1950,
];

#[cfg(test)]
mod tests {
    use crate::bits::{Bits, ExtendedMode};
    use crate::structured::StructuredQrError::*;
    use crate::structured::{merge_qrs, SplittedQr, StructuredQr, LEVEL};
    use crate::{QrCode, Version};
    use rand::Rng;
    use std::convert::TryInto;

    // from example https://segno.readthedocs.io/en/stable/structured-append.html#structured-append
    /*
    I read the news today oh boy
    4 1c 49207265616420746865206e6577 7320746f646179206f6820626f79 000ec11ec

    I read the new
    3 0 1 39 4 0e 49207265616420746865206e6577 00

    s today oh boy
    3 1 1 39 4 0e 7320746f646179206f6820626f79 00

    MODE SEQ TOTAL PARITY MODE LENGTH
    */

    const _FULL: &str = "41c49207265616420746865206e65777320746f646179206f6820626f79000ec11ec";
    const FULL_CONTENT: &str = "49207265616420746865206e65777320746f646179206f6820626f79";
    const FIRST: &str = "3013940e49207265616420746865206e657700";
    const FIRST_CONTENT: &str = "49207265616420746865206e6577";
    const SECOND: &str = "3113940e7320746f646179206f6820626f7900";
    const SECOND_CONTENT: &str = "7320746f646179206f6820626f79";

    #[test]
    fn test_try_into_structured() {
        let bytes = hex::decode(FIRST).unwrap();
        let content = hex::decode(FIRST_CONTENT).unwrap();
        let structured: StructuredQr = bytes.try_into().unwrap();
        assert_eq!(structured.seq, 0);
        assert_eq!(structured.total, 1);
        assert_eq!(structured.parity, 57);
        assert_eq!(structured.content, content);

        let bytes = hex::decode(SECOND).unwrap();
        let content = hex::decode(SECOND_CONTENT).unwrap();
        let structured_2: StructuredQr = bytes.try_into().unwrap();
        assert_eq!(structured_2.seq, 1);
        assert_eq!(structured_2.total, 1);
        assert_eq!(structured_2.parity, 57);
        assert_eq!(structured_2.content, content);
    }

    #[test]
    fn test_merge() {
        let first = hex::decode(FIRST).unwrap();
        let second = hex::decode(SECOND).unwrap();
        let full_content = hex::decode(FULL_CONTENT).unwrap();
        let vec = vec![first.clone(), second.clone()];
        let result = merge_qrs(vec).unwrap();
        assert_eq!(hex::encode(result), FULL_CONTENT);

        let vec = vec![second.clone(), first.clone()];
        let result = merge_qrs(vec).unwrap(); //merge out of order
        assert_eq!(hex::encode(result), FULL_CONTENT);

        let vec = vec![second.clone(), first.clone(), second.clone()];
        let result = merge_qrs(vec).unwrap(); //merge duplicates
        assert_eq!(hex::encode(result), FULL_CONTENT);

        let vec = vec![first.clone(), first.clone()];
        let result = merge_qrs(vec);
        assert_eq!(result.unwrap_err().to_string(), AtLeast2Pieces.to_string());

        let vec = vec![first.clone(), full_content];
        let result = merge_qrs(vec);
        assert_eq!(
            result.unwrap_err().to_string(),
            StructuredWrongMode.to_string()
        );

        let mut first_mut = first.clone();
        first_mut[15] = 14u8;
        let vec = vec![first.clone(), first_mut.clone()];
        let result = merge_qrs(vec);
        assert_eq!(result.unwrap_err().to_string(), MissingParts.to_string());

        let vec = vec![first, first_mut.clone(), second];
        let result = merge_qrs(vec);
        assert_eq!(
            result.unwrap_err().to_string(),
            TotalMismatch(3).to_string(),
        );
    }

    #[test]
    fn test_structured_append() {
        let data = "I read the news today oh boy".as_bytes();
        let data_half = "I read the new".as_bytes();
        let parity = data.iter().fold(0u8, |acc, &x| acc ^ x);
        let mut bits = Bits::new(Version::Normal(1));
        bits.push_mode_indicator(ExtendedMode::StructuredAppend)
            .unwrap();
        bits.push_number_checked(4, 0).unwrap(); // first element of the sequence
        bits.push_number_checked(4, 1).unwrap(); // total length of the sequence (means 2)
        bits.push_number_checked(8, parity as usize).unwrap(); //parity of the complete data
        bits.push_byte_data(data_half).unwrap();
        bits.push_terminator(LEVEL).unwrap();
        assert_eq!(
            hex::encode(bits.into_bytes()),
            "3013940e49207265616420746865206e657700"
        ); // raw bytes of the first qr code of the example
    }

    #[test]
    fn test_split_merge_qr() {
        // consider using https://rust-fuzz.github.io/book/introduction.html
        let mut rng = rand::thread_rng();
        let random_bytes: Vec<u8> = (0..4000).map(|_| rand::random::<u8>()).collect();
        for _ in 0..1_000 {
            let len = rng.gen_range(100, 4000);
            let ver = rng.gen_range(10, 20);
            let data = random_bytes[0..len].to_vec();
            let split_qr = SplittedQr::new(data.clone(), ver).unwrap();
            let bits = split_qr.split_to_bits().unwrap();
            if bits.len() > 1 {
                let bytes: Vec<Vec<u8>> = bits.into_iter().map(|b| b.into_bytes()).collect();
                let result = merge_qrs(bytes).unwrap();
                assert_eq!(result, data);
            }
        }
    }

    #[test]
    fn test_print_qr() {
        let qr = QrCode::new(b"01234567").unwrap();
        let printed = qr.to_string(false, 3);
        assert_eq!(hex::encode(printed.as_bytes()),"2020202020202020202020202020202020202020202020202020200a2020202020202020202020202020202020202020202020202020200a202020e29688e29680e29680e29680e29680e29680e296882020e29688e29684e29688e2968820e29688e29680e29680e29680e29680e29680e296882020200a202020e2968820e29688e29688e2968820e2968820e29688e2968420202020e2968820e29688e29688e2968820e296882020200a202020e2968820e29680e29680e2968020e2968820e2968820e29680e29680e2968820e2968820e29680e29680e2968020e296882020200a202020e29680e29680e29680e29680e29680e29680e2968020e2968820e29680e29684e2968820e29680e29680e29680e29680e29680e29680e296802020200a202020e2968020e29680e29688e29680e29688e29680e29684e29684e29680e2968420e2968820e29680e29688e29680e29688e2968820202020200a2020202020e2968020e2968420e29680e2968020e2968820e2968020e2968020e29684e29688e29688e29688e29680e296802020200a202020202020e29680e29680e29680e29680e29680e2968820e29684e29688e29684e29688e2968420e29680e29684e2968420202020200a202020e29688e29680e29680e29680e29680e29680e2968820e29684e29680e29688e29684e29688e29684e29688e296802020e2968420e296842020200a202020e2968820e29688e29688e2968820e2968820e29688e296842020e296882020e2968820e29680e2968020202020200a202020e2968820e29680e29680e2968020e2968820e2968020e29680e2968020e2968020e29684e2968820e29688e29684202020200a202020e29680e29680e29680e29680e29680e29680e2968020e29680e29680e29680e2968020e296802020e2968020e2968020202020200a2020202020202020202020202020202020202020202020202020200a0a");
    }
}
