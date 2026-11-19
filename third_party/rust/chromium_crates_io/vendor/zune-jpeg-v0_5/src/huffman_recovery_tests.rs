use zune_core::bytestream::ZCursor;
use zune_core::colorspace::ColorSpace;
use zune_core::options::DecoderOptions;

use crate::JpegDecoder;

#[test]
fn missing_mcus_do_not_execute_idct() {
    // Embed the authored upstream control so this test is self-contained.
    let mut data = vec![
        0xff, 0xd8, 0xff, 0xee, 0x00, 0x0e, 0x41, 0x64, 0x6f, 0x62, 0x65, 0x00, 0x64, 0x00, 0x00,
        0x00, 0x00, 0x01, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0xff, 0xc4, 0x00,
        0x15, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xc4, 0x00, 0x15, 0x11, 0x01, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xc0,
        0x00, 0x11, 0x08, 0x00, 0x18, 0x00, 0x20, 0x03, 0x01, 0x11, 0x00, 0x02, 0x11, 0x00, 0x03,
        0x11, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x01, 0x02, 0x01, 0x03, 0x01, 0x00, 0x3f,
        0x00, 0xa2, 0x8a, 0x28, 0xff, 0xd9,
    ];
    let sof = data
        .windows(2)
        .position(|bytes| bytes == [255, 192])
        .unwrap();
    data[sof + 5..sof + 7].copy_from_slice(&60_000u16.to_be_bytes());
    let options = DecoderOptions::default()
        .set_max_height(65_535)
        .set_strict_mode(false)
        .jpeg_set_out_colorspace(ColorSpace::RGBA);
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(data), options);
    decoder.decode_headers().unwrap();
    let output = decoder.decode().unwrap();
    assert_eq!(output.len(), 32 * 60_000 * 4);
    assert_eq!(decoder.idct_blocks_for_test, 6);
    assert!(output[32 * 8 * 4..]
        .chunks_exact(4)
        .all(|pixel| pixel == [128, 128, 128, 255]));
}

#[test]
fn unknown_entropy_markers_remain_fatal() {
    // PR518's authored 9x11-block grayscale control, with 99 valid DC/EOB pairs.
    let mut valid = vec![255, 216, 255, 219, 0, 67, 0];
    valid.extend([8; 64]);
    for (selector, values) in [(0, [0, 3]), (0x11, [0, 1])] {
        valid.extend([255, 196, 0, 21, selector, 1, 1]);
        valid.extend([0; 14]);
        valid.extend(values);
    }
    valid.extend([
        255, 192, 0, 11, 8, 0, 88, 0, 72, 1, 1, 0x11, 0, 255, 218, 0, 8, 1, 1, 1, 0, 63, 0
    ]);
    let entropy = valid.len();
    for _ in 0..24 {
        valid.extend([0xa2, 0x8a, 0x28]);
    }
    valid.extend([0xa2, 0x8a, 0x3f, 255, 217]);
    let options = DecoderOptions::default()
        .set_strict_mode(false)
        .jpeg_set_out_colorspace(ColorSpace::Luma);
    assert_eq!(
        JpegDecoder::new_with_options(ZCursor::new(&valid), options)
            .decode()
            .unwrap()
            .len(),
        72 * 88
    );
    for offset in [0, 3, 12] {
        let mut malformed = valid[..entropy + offset].to_vec();
        malformed.extend([255, 0x4b, 0, 2, 255, 217]);
        for simd in [false, true] {
            for incremental in [false, true] {
                let mut decoder = JpegDecoder::new_with_options(
                    ZCursor::new(&malformed),
                    options.set_use_unsafe(simd)
                );
                decoder.set_incremental_mode(incremental);
                let error = decoder.decode().unwrap_err();
                assert!(matches!(error, crate::errors::DecodeErrors::Format(_)));
                assert!(!error.is_recoverable_eof());
            }
        }
    }
}
