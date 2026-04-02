use super::{varint_encode_u128, varint_encode_u16, varint_encode_u32, varint_encode_u64};
use crate::{config::Endianness, enc::write::Writer, error::EncodeError};

pub fn varint_encode_i16<W: Writer>(
    writer: &mut W,
    endian: Endianness,
    val: i16,
) -> Result<(), EncodeError> {
    varint_encode_u16(
        writer,
        endian,
        if val < 0 {
            // let's avoid the edge case of i16::min_value()
            // !n is equal to `-n - 1`, so this is:
            // !n * 2 + 1 = 2(-n - 1) + 1 = -2n - 2 + 1 = -2n - 1
            !(val as u16) * 2 + 1
        } else {
            (val as u16) * 2
        },
    )
}

pub fn varint_encode_i32<W: Writer>(
    writer: &mut W,
    endian: Endianness,
    val: i32,
) -> Result<(), EncodeError> {
    varint_encode_u32(
        writer,
        endian,
        if val < 0 {
            // let's avoid the edge case of i32::min_value()
            // !n is equal to `-n - 1`, so this is:
            // !n * 2 + 1 = 2(-n - 1) + 1 = -2n - 2 + 1 = -2n - 1
            !(val as u32) * 2 + 1
        } else {
            (val as u32) * 2
        },
    )
}

pub fn varint_encode_i64<W: Writer>(
    writer: &mut W,
    endian: Endianness,
    val: i64,
) -> Result<(), EncodeError> {
    varint_encode_u64(
        writer,
        endian,
        if val < 0 {
            // let's avoid the edge case of i64::min_value()
            // !n is equal to `-n - 1`, so this is:
            // !n * 2 + 1 = 2(-n - 1) + 1 = -2n - 2 + 1 = -2n - 1
            !(val as u64) * 2 + 1
        } else {
            (val as u64) * 2
        },
    )
}

pub fn varint_encode_i128<W: Writer>(
    writer: &mut W,
    endian: Endianness,
    val: i128,
) -> Result<(), EncodeError> {
    varint_encode_u128(
        writer,
        endian,
        if val < 0 {
            // let's avoid the edge case of i128::min_value()
            // !n is equal to `-n - 1`, so this is:
            // !n * 2 + 1 = 2(-n - 1) + 1 = -2n - 2 + 1 = -2n - 1
            !(val as u128) * 2 + 1
        } else {
            (val as u128) * 2
        },
    )
}

pub fn varint_encode_isize<W: Writer>(
    writer: &mut W,
    endian: Endianness,
    val: isize,
) -> Result<(), EncodeError> {
    // isize is being encoded as a i64
    varint_encode_i64(writer, endian, val as i64)
}

#[test]
fn test_encode_i16() {
    let cases: &[(i16, &[u8], &[u8])] = &[
        (0, &[0], &[0]),
        (2, &[4], &[4]),
        (256, &[super::U16_BYTE, 0, 2], &[super::U16_BYTE, 2, 0]),
        (
            16_000,
            &[super::U16_BYTE, 0, 125],
            &[super::U16_BYTE, 125, 0],
        ),
        (
            i16::MAX - 1,
            &[super::U16_BYTE, 252, 255],
            &[super::U16_BYTE, 255, 252],
        ),
        (
            i16::MAX,
            &[super::U16_BYTE, 254, 255],
            &[super::U16_BYTE, 255, 254],
        ),
    ];

    use crate::enc::write::SliceWriter;
    let mut buffer = [0u8; 20];
    for &(value, expected_le, expected_be) in cases {
        std::dbg!(value);

        // Little endian
        let mut writer = SliceWriter::new(&mut buffer);
        varint_encode_i16(&mut writer, Endianness::Little, value).unwrap();

        assert_eq!(writer.bytes_written(), expected_le.len());
        assert_eq!(&buffer[..expected_le.len()], expected_le);

        // Big endian
        let mut writer = SliceWriter::new(&mut buffer);
        varint_encode_i16(&mut writer, Endianness::Big, value).unwrap();

        assert_eq!(writer.bytes_written(), expected_be.len());
        assert_eq!(&buffer[..expected_be.len()], expected_be);
    }
}

#[test]
fn test_encode_i32() {
    let cases: &[(i32, &[u8], &[u8])] = &[
        (0, &[0], &[0]),
        (2, &[4], &[4]),
        (256, &[super::U16_BYTE, 0, 2], &[super::U16_BYTE, 2, 0]),
        (
            16_000,
            &[super::U16_BYTE, 0, 125],
            &[super::U16_BYTE, 125, 0],
        ),
        (
            40_000,
            &[super::U32_BYTE, 128, 56, 1, 0],
            &[super::U32_BYTE, 0, 1, 56, 128],
        ),
        (
            i32::MAX - 1,
            &[super::U32_BYTE, 252, 255, 255, 255],
            &[super::U32_BYTE, 255, 255, 255, 252],
        ),
        (
            i32::MAX,
            &[super::U32_BYTE, 254, 255, 255, 255],
            &[super::U32_BYTE, 255, 255, 255, 254],
        ),
    ];

    use crate::enc::write::SliceWriter;
    let mut buffer = [0u8; 20];
    for &(value, expected_le, expected_be) in cases {
        std::dbg!(value);

        // Little endian
        let mut writer = SliceWriter::new(&mut buffer);
        varint_encode_i32(&mut writer, Endianness::Little, value).unwrap();

        assert_eq!(writer.bytes_written(), expected_le.len());
        assert_eq!(&buffer[..expected_le.len()], expected_le);

        // Big endian
        let mut writer = SliceWriter::new(&mut buffer);
        varint_encode_i32(&mut writer, Endianness::Big, value).unwrap();

        assert_eq!(writer.bytes_written(), expected_be.len());
        assert_eq!(&buffer[..expected_be.len()], expected_be);
    }
}

#[test]
fn test_encode_i64() {
    let cases: &[(i64, &[u8], &[u8])] = &[
        (0, &[0], &[0]),
        (2, &[4], &[4]),
        (256, &[super::U16_BYTE, 0, 2], &[super::U16_BYTE, 2, 0]),
        (
            16_000,
            &[super::U16_BYTE, 0, 125],
            &[super::U16_BYTE, 125, 0],
        ),
        (
            40_000,
            &[super::U32_BYTE, 128, 56, 1, 0],
            &[super::U32_BYTE, 0, 1, 56, 128],
        ),
        (
            3_000_000_000,
            &[super::U64_BYTE, 0, 188, 160, 101, 1, 0, 0, 0],
            &[super::U64_BYTE, 0, 0, 0, 1, 101, 160, 188, 0],
        ),
        (
            i64::MAX - 1,
            &[super::U64_BYTE, 252, 255, 255, 255, 255, 255, 255, 255],
            &[super::U64_BYTE, 255, 255, 255, 255, 255, 255, 255, 252],
        ),
        (
            i64::MAX,
            &[super::U64_BYTE, 254, 255, 255, 255, 255, 255, 255, 255],
            &[super::U64_BYTE, 255, 255, 255, 255, 255, 255, 255, 254],
        ),
    ];

    use crate::enc::write::SliceWriter;
    let mut buffer = [0u8; 20];
    for &(value, expected_le, expected_be) in cases {
        std::dbg!(value);

        // Little endian
        let mut writer = SliceWriter::new(&mut buffer);
        varint_encode_i64(&mut writer, Endianness::Little, value).unwrap();

        assert_eq!(writer.bytes_written(), expected_le.len());
        assert_eq!(&buffer[..expected_le.len()], expected_le);

        // Big endian
        let mut writer = SliceWriter::new(&mut buffer);
        varint_encode_i64(&mut writer, Endianness::Big, value).unwrap();

        assert_eq!(writer.bytes_written(), expected_be.len());
        assert_eq!(&buffer[..expected_be.len()], expected_be);
    }
}

#[test]
fn test_encode_i128() {
    #[rustfmt::skip]
    let cases: &[(i128, &[u8], &[u8])] = &[
        (0, &[0], &[0]),
        (2, &[4], &[4]),
        (256, &[super::U16_BYTE, 0, 2], &[super::U16_BYTE, 2, 0]),
        (
            16_000,
            &[super::U16_BYTE, 0, 125],
            &[super::U16_BYTE, 125, 0],
        ),
        (
            40_000,
            &[super::U32_BYTE, 128, 56, 1, 0],
            &[super::U32_BYTE, 0, 1, 56, 128],
        ),
        (
            3_000_000_000,
            &[super::U64_BYTE, 0, 188, 160, 101, 1, 0, 0, 0],
            &[super::U64_BYTE, 0, 0, 0, 1, 101, 160, 188, 0],
        ),
        (
            11_000_000_000_000_000_000,
            &[
                super::U128_BYTE,
                0, 0, 152, 98, 112, 179, 79, 49,
                1, 0, 0, 0, 0, 0, 0, 0,
            ],
            &[
                super::U128_BYTE,
                0, 0, 0, 0, 0, 0, 0, 1,
                49, 79, 179, 112, 98, 152, 0, 0,
            ],
        ),
        (
            i128::MAX - 1,
            &[
                super::U128_BYTE,
                252, 255, 255, 255, 255, 255, 255, 255,
                255, 255, 255, 255, 255, 255, 255, 255,
            ],
            &[
                super::U128_BYTE,
                255, 255, 255, 255, 255, 255, 255, 255,
                255, 255, 255, 255, 255, 255, 255, 252,
            ],
        ),
        (
            i128::MAX,
            &[
                super::U128_BYTE,
                254, 255, 255, 255, 255, 255, 255, 255,
                255, 255, 255, 255, 255, 255, 255, 255,
            ],
            &[
                super::U128_BYTE,
                255, 255, 255, 255, 255, 255, 255, 255,
                255, 255, 255, 255, 255, 255, 255, 254,
            ],
        ),
    ];

    use crate::enc::write::SliceWriter;
    let mut buffer = [0u8; 20];
    for &(value, expected_le, expected_be) in cases {
        std::dbg!(value);

        // Little endian
        let mut writer = SliceWriter::new(&mut buffer);
        varint_encode_i128(&mut writer, Endianness::Little, value).unwrap();

        assert_eq!(writer.bytes_written(), expected_le.len());
        assert_eq!(&buffer[..expected_le.len()], expected_le);

        // Big endian
        let mut writer = SliceWriter::new(&mut buffer);
        varint_encode_i128(&mut writer, Endianness::Big, value).unwrap();

        assert_eq!(writer.bytes_written(), expected_be.len());
        assert_eq!(&buffer[..expected_be.len()], expected_be);
    }
}
