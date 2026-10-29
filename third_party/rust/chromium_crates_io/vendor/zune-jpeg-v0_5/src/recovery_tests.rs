/*
 * Copyright (c) 2026.
 *
 * This software is free software;
 *
 * You can redistribute it or modify it under terms of the MIT, Apache
 * License or Zlib license
 */

use alloc::vec;
use alloc::vec::Vec;

use zune_core::bytestream::ZCursor;
use zune_core::colorspace::ColorSpace;
use zune_core::options::DecoderOptions;

use crate::{JpegDecoder, RawImcuRowStatus};

// Small generated sequential streams with a unit DC step in spatial samples.
fn frame(width: u16, height: u16, components: &[(u8, u8)]) -> Vec<u8> {
    let mut data = vec![0xFF, 0xD8, 0xFF, 0xDB, 0, 67, 0];
    data.extend_from_slice(&[8; 64]);
    data.extend_from_slice(&[
        0xFF,
        0xC0,
        0,
        8 + 3 * u8::try_from(components.len()).unwrap(),
        8
    ]);
    data.extend_from_slice(&height.to_be_bytes());
    data.extend_from_slice(&width.to_be_bytes());
    data.push(u8::try_from(components.len()).unwrap());
    for &(id, sampling) in components {
        data.extend_from_slice(&[id, sampling, 0]);
    }
    data.extend_from_slice(&[0xFF, 0xC4, 0, 31, 0]);
    data.extend_from_slice(&[0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    data.extend(0..12);
    data.extend_from_slice(&[0xFF, 0xC4, 0, 21, 0x10]);
    data.extend_from_slice(&[0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    data.extend_from_slice(&[0, 1]);
    data
}

fn scan(data: &mut Vec<u8>, components: &[u8], blocks: &[(i32, bool)]) {
    scan_with_codes(data, components, blocks, false);
}

fn scan_with_codes(
    data: &mut Vec<u8>, components: &[u8], blocks: &[(i32, bool)], zero_is_ac: bool
) {
    data.extend_from_slice(&[
        0xFF,
        0xDA,
        0,
        6 + 2 * u8::try_from(components.len()).unwrap()
    ]);
    data.push(u8::try_from(components.len()).unwrap());
    for &id in components {
        data.extend_from_slice(&[id, 0]);
    }
    data.extend_from_slice(&[0, 63, 0]);
    let mut bits = Vec::new();
    let mut emit = |value: u32, size: u32| {
        for bit in (0..size).rev() {
            bits.push(u8::try_from((value >> bit) & 1).unwrap());
        }
    };
    for &(difference, ac) in blocks {
        let magnitude = difference.unsigned_abs();
        let size = 32 - magnitude.leading_zeros();
        emit(size, 4);
        if size != 0 {
            let value = if difference < 0 { (1 << size) - 1 - magnitude } else { magnitude };
            emit(value, size);
        }
        if ac {
            emit(u32::from(!zero_is_ac), 2);
            emit(1, 1);
        }
        emit(u32::from(zero_is_ac), 2);
    }
    while bits.len() % 8 != 0 {
        bits.push(1);
    }
    for byte in bits.chunks_exact(8) {
        let value = byte.iter().fold(0, |value, bit| (value << 1) | bit);
        data.push(value);
        if value == 255 {
            data.push(0);
        }
    }
}

fn options(strict: bool) -> DecoderOptions {
    DecoderOptions::default()
        .set_strict_mode(strict)
        .jpeg_set_out_colorspace(ColorSpace::RGB)
}

#[test]
fn bad_huffman_code_consumes_seventeen_bits_only_in_lenient_mode() {
    let mut data = frame(8, 8, &[(1, 0x11)]);
    data.extend_from_slice(&[
        0xFF, 0xDA, 0, 8, 1, 1, 0, 0, 63, 0, 0xFF, 0, 0xFF, 0, 0x9F, 0xFF, 0xD9
    ]);
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
    assert_eq!(decoder.decode().unwrap(), [128; 8 * 8 * 3]);
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(true));
    assert!(decoder.decode().is_err());
}

#[test]
fn buffered_publication_tracks_eoi_after_trailing_metadata() {
    for eoi in [false, true] {
        let mut data = frame(8, 8, &[(1, 0x11), (2, 0x11), (3, 0x11)]);
        for component in 1..=3 {
            scan(&mut data, &[component], &[(0, false)]);
        }
        data.extend_from_slice(&[0xFF, 0xE3, 0, 2, 0xFF, 0xFE, 0, 6, b't', b'e', b's', b't']);
        if eoi {
            data.extend_from_slice(&[0xFF, 0xD9]);
        }
        let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
        assert_eq!(decoder.buffered_output_rows(), None);
        assert_eq!(decoder.decode().unwrap(), [128; 8 * 8 * 3]);
        assert_eq!(
            decoder.buffered_output_rows(),
            Some(if eoi { 8 } else { 0 })
        );
    }
}

#[test]
fn invalid_ac_run_does_not_wrap_and_overwrite_dc() {
    let mut data = frame(8, 8, &[(1, 0x11), (2, 0x11), (3, 0x11)]);
    *data.last_mut().unwrap() = 0xF1;
    data.extend_from_slice(&[0xFF, 0xDA, 0, 8, 1, 1, 0, 0, 63, 0, 0x06, 0xDB]);
    scan(&mut data, &[2, 3], &[(0, false); 2]);
    data.extend_from_slice(&[0xFF, 0xD9]);
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
    decoder.decode().unwrap();
    assert_eq!(decoder.baseline_coefficients[0][0], 0);
    assert_eq!(decoder.baseline_coefficients[0][63], 8);
    assert_eq!(decoder.buffered_output_rows(), Some(8));
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(true));
    assert!(decoder.decode().is_err());
}

#[test]
fn subset_scan_uses_frame_mcu_geometry_and_neutral_missing_components() {
    let mut data = frame(32, 32, &[(1, 0x22), (2, 0x12), (3, 0x12)]);
    // Two columns and two rows of frame MCUs, each with two vertical
    // blocks per scan component. The missing Y component still determines
    // the frame's horizontal MCU count.
    scan(&mut data, &[2, 3], &[(1, false); 16]);
    data.extend_from_slice(&[0xFF, 0xD9]);
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
    decoder.decode().unwrap();
    assert!(decoder.baseline_coefficients[0]
        .iter()
        .all(|&coefficient| coefficient == 0));
    for component in [1, 2] {
        let blocks = &decoder.baseline_coefficients[component];
        assert_eq!(blocks.len(), 2 * 4 * 64);
        // Scan order is MCU first, then the component's vertical blocks.
        let expected_dc = [8, 24, 16, 32, 40, 56, 48, 64];
        for (block, expected) in blocks.chunks_exact(64).zip(expected_dc) {
            assert_eq!(block[0], expected);
            assert!(block[1..].iter().all(|&coefficient| coefficient == 0));
        }
    }
}

#[test]
fn repeated_sequential_scan_preserves_ac_and_resets_dc_prediction() {
    let mut data = frame(8, 8, &[(1, 0x11), (2, 0x11), (3, 0x11)]);
    scan(&mut data, &[1], &[(4, true)]);
    scan(&mut data, &[2, 3], &[(0, false); 2]);
    // Redefine the AC table between repeated scans.
    data.extend_from_slice(&[0xFF, 0xC4, 0, 20, 0x10]);
    data.extend_from_slice(&[1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    data.push(0);
    // This table's EOB is one bit, so encode the scan explicitly.
    data.extend_from_slice(&[0xFF, 0xDA, 0, 8, 1, 1, 0, 0, 63, 0, 0x1B]);
    data.extend_from_slice(&[0xFF, 0xD9]);
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
    decoder.decode().unwrap();
    assert_eq!(&decoder.baseline_coefficients[0][..2], &[8, 8]);
}

#[test]
fn buffered_sequential_publication_tracks_native_input_completion() {
    for case in [
        "clean",
        "incomplete",
        "bad_huffman",
        "discarded_entropy",
        "discarded_before_eoi"
    ] {
        let mut data = frame(8, 8, &[(1, 0x11), (2, 0x11), (3, 0x11)]);
        if case == "bad_huffman" {
            data.extend_from_slice(&[0xFF, 0xDA, 0, 8, 1, 1, 0, 0, 63, 0, 0xFF, 0, 0xFF, 0, 0x9F]);
        } else {
            scan(&mut data, &[1], &[(0, false)]);
        }
        if case == "discarded_entropy" {
            data.extend_from_slice(&[0x12; 12]);
        }
        scan(&mut data, &[2, 3], &[(0, false); 2]);
        if case == "discarded_before_eoi" {
            data.extend_from_slice(&[0x12; 12]);
        }
        if case != "incomplete" {
            data.extend_from_slice(&[0xFF, 0xD9]);
        }
        let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
        assert_eq!(decoder.decode().unwrap(), [128; 8 * 8 * 3], "{case}");
        assert_eq!(
            decoder.buffered_output_rows(),
            Some(if matches!(case, "clean" | "discarded_before_eoi") { 8 } else { 0 }),
            "{case}"
        );
    }

    let mut data = frame(8, 8, &[(1, 0x11), (2, 0x11), (3, 0x11)]);
    scan(&mut data, &[1, 2, 3], &[(0, false); 3]);
    data.extend_from_slice(&[0xFF, 0xD9]);
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
    decoder.decode().unwrap();
    assert_eq!(decoder.buffered_output_rows(), None);
}

#[test]
fn premature_scan_marker_is_recoverable_only_in_lenient_mode() {
    let mut data = frame(16, 16, &[(1, 0x11)]);
    scan(&mut data, &[1], &[(0, false)]);
    data.extend_from_slice(&[0xFF, 0xD9]);
    let mut strict = JpegDecoder::new_with_options(ZCursor::new(&data), options(true));
    assert!(strict.decode().is_err());
    let mut lenient = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
    assert_eq!(lenient.decode().unwrap().len(), 16 * 16 * 3);
}

#[test]
fn buffered_fast_huffman_recovery_preserves_publication() {
    for bad_last in [false, true] {
        let mut data = frame(8, 8, &[(1, 0x11), (2, 0x11), (3, 0x11)]);
        if bad_last {
            scan(&mut data, &[1], &[(0, false)]);
            scan(&mut data, &[2], &[(0, false)]);
        }
        let component = if bad_last { 3 } else { 1 };
        data.extend_from_slice(&[
            0xff, 0xda, 0, 8, 1, component, 0, 0, 63, 0, 0xff, 0, 0xff, 0, 0x9f
        ]);
        if !bad_last {
            scan(&mut data, &[2, 3], &[(0, false); 2]);
        }
        data.extend_from_slice(&[0; 2048]);
        data.extend_from_slice(&[0xff, 0xd9]);
        let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
        assert_eq!(decoder.decode().unwrap(), [128; 8 * 8 * 3]);
        assert_eq!(
            decoder.buffered_output_rows(),
            Some(if bad_last { 8 } else { 0 })
        );
    }
}

#[test]
fn malformed_entropy_is_bounded_with_and_without_end_markers() {
    let mut random = 1_u32;
    for length in 0..32 {
        for with_eoi in [false, true] {
            for strict in [false, true] {
                let mut data = frame(16, 16, &[(1, 0x11)]);
                data.extend_from_slice(&[0xFF, 0xDA, 0, 8, 1, 1, 0, 0, 63, 0]);
                for _ in 0..length {
                    random = random.wrapping_mul(1664525).wrapping_add(1013904223);
                    let byte = random.to_be_bytes()[0];
                    data.push(byte);
                    if byte == 255 {
                        data.push(0);
                    }
                }
                if with_eoi {
                    data.extend_from_slice(&[0xFF, 0xD9]);
                }
                let mut decoder =
                    JpegDecoder::new_with_options(ZCursor::new(&data), options(strict));
                let mut output = [0xA5; 16 * 16 * 3 + 17];
                let _ = decoder.decode_into(&mut output);
                assert_eq!(&output[16 * 16 * 3..], &[0xA5; 17]);
            }
        }
    }
}

#[test]
fn raw_rows_preserve_caller_padding() {
    let mut data = frame(9, 9, &[(1, 0x11)]);
    scan(&mut data, &[1], &[(0, false); 4]);
    data.extend_from_slice(&[0xFF, 0xD9]);
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
    decoder.decode_headers().unwrap();
    let mut raw = decoder.raw_output();
    let mut plane = [0xA5; 8 * 13];
    for expected_rows in [8, 1] {
        plane.fill(0xA5);
        assert!(matches!(
            raw.decode_next_imcu_row(&mut [&mut plane], &[13]).unwrap(),
            RawImcuRowStatus::RowReady { rows_written } if rows_written[0] == expected_rows
        ));
        for row in plane.chunks_exact(13).take(expected_rows) {
            assert_eq!(&row[..9], &[128; 9]);
            assert_eq!(&row[9..], &[0xA5; 4]);
        }
        assert!(plane[expected_rows * 13..]
            .iter()
            .all(|&value| value == 0xA5));
    }
}

#[test]
fn raw_subset_scan_does_not_publish_never_seen_components() {
    let mut data = frame(8, 8, &[(1, 0x11), (2, 0x11), (3, 0x11)]);
    scan(&mut data, &[2, 3], &[(1, false); 2]);
    data.extend_from_slice(&[0xFF, 0xD9]);
    let mut decoder = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
    decoder.decode_headers().unwrap();
    let mut raw = decoder.raw_output();
    raw.set_input_complete();
    let mut y = [0xA5; 64];
    let mut cb = y;
    let mut cr = y;
    assert!(raw
        .decode_next_imcu_row(&mut [&mut y, &mut cb, &mut cr], &[8; 3])
        .unwrap_err()
        .is_recoverable_eof());
    assert!(y
        .iter()
        .chain(cb.iter())
        .chain(cr.iter())
        .all(|&value| value == 0xA5));
}

#[cfg(feature = "std")]
mod growing {
    use std::cell::Cell;
    use std::io::{self, BufRead, Read, Seek, SeekFrom};
    use std::rc::Rc;

    use super::*;

    struct GrowingInput {
        data:     Vec<u8>,
        visible:  Rc<Cell<usize>>,
        position: usize
    }

    impl Read for GrowingInput {
        fn read(&mut self, destination: &mut [u8]) -> io::Result<usize> {
            let source = self.fill_buf()?;
            let len = source.len().min(destination.len());
            destination[..len].copy_from_slice(&source[..len]);
            self.consume(len);
            Ok(len)
        }
    }

    impl BufRead for GrowingInput {
        fn fill_buf(&mut self) -> io::Result<&[u8]> {
            let end = self.visible.get().min(self.data.len());
            Ok(&self.data[self.position.min(end)..end])
        }

        fn consume(&mut self, count: usize) {
            self.position += count;
        }
    }

    impl Seek for GrowingInput {
        fn seek(&mut self, from: SeekFrom) -> io::Result<u64> {
            let position = match from {
                SeekFrom::Start(position) => i64::try_from(position),
                SeekFrom::End(offset) => i64::try_from(self.visible.get()).map(|end| end + offset),
                SeekFrom::Current(offset) => i64::try_from(self.position).map(|end| end + offset)
            }
            .map_err(|_| io::Error::from(io::ErrorKind::InvalidInput))?;
            self.position = usize::try_from(position)
                .map_err(|_| io::Error::from(io::ErrorKind::InvalidInput))?;
            u64::try_from(self.position).map_err(|_| io::Error::from(io::ErrorKind::InvalidInput))
        }
    }

    #[test]
    fn byte_by_byte_repeated_scans_match_one_shot_without_early_publication() {
        for zero_is_ac in [false, true] {
            let mut data = frame(32, 32, &[(1, 0x22), (2, 0x12), (3, 0x12)]);
            if zero_is_ac {
                let len = data.len();
                data[len - 2..].copy_from_slice(&[1, 0]);
            }
            scan_with_codes(&mut data, &[1], &[(1, true); 16], zero_is_ac);
            scan_with_codes(&mut data, &[2, 3], &[(1, true); 16], zero_is_ac);
            // Resuming inside an all-component scan must not change the first
            // scan's buffered reconstruction strategy.
            scan_with_codes(&mut data, &[1, 2, 3], &[(0, false); 32], zero_is_ac);
            scan_with_codes(&mut data, &[1], &[(0, false); 16], zero_is_ac);
            data.extend_from_slice(&[0xFF, 0xD9]);
            assert_retry(data);
        }
    }

    fn assert_retry(data: Vec<u8>) {
        let mut complete = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
        let expected = complete.decode().unwrap();
        let visible = Rc::new(Cell::new(0));
        let input = GrowingInput {
            data:     data.clone(),
            visible:  visible.clone(),
            position: 0
        };
        let mut decoder = JpegDecoder::new_with_options(input, options(false));
        decoder.set_incremental_mode(true);
        let mut output = vec![0xA5; expected.len()];
        let mut suspensions = 0;
        for end in 0..=data.len() {
            visible.set(end);
            match decoder.decode_into(&mut output) {
                Ok(()) => {
                    assert_eq!(output, expected, "visible bytes: {end}");
                    assert_eq!(end, data.len());
                }
                Err(error) => {
                    assert!(
                        error.is_recoverable_eof(),
                        "visible bytes: {end}: {error:?}"
                    );
                    assert!(output.iter().all(|&value| value == 0xA5));
                    suspensions += 1;
                }
            }
        }
        assert!(suspensions > 100);
        assert_eq!(output, expected);
        assert_eq!(
            decoder.baseline_coefficients,
            complete.baseline_coefficients
        );
    }

    #[test]
    fn restart_checkpoints_do_not_outlive_rolled_back_coefficient_rows() {
        fn restarted_scan(
            data: &mut Vec<u8>, ids: &[u8], blocks_per_mcu: usize, blocks: &[(i32, bool)]
        ) {
            let header_size = 8 + 2 * ids.len();
            for (mcu, blocks) in blocks.chunks_exact(blocks_per_mcu).enumerate() {
                let mut encoded = Vec::new();
                scan(&mut encoded, ids, blocks);
                if mcu == 0 {
                    data.extend_from_slice(&encoded[..header_size]);
                } else {
                    data.extend_from_slice(&[0xFF, 0xD0 + u8::try_from((mcu - 1) % 8).unwrap()]);
                }
                data.extend_from_slice(&encoded[header_size..]);
            }
        }
        let mut data = frame(32, 16, &[(1, 0x11), (2, 0x11), (3, 0x11)]);
        data.extend_from_slice(&[0xFF, 0xDD, 0, 4, 0, 1]);
        restarted_scan(&mut data, &[1], 1, &[(1, true); 8]);
        restarted_scan(&mut data, &[2, 3], 2, &[(1, true); 16]);
        restarted_scan(&mut data, &[1], 1, &[(0, false); 8]);
        data.extend_from_slice(&[0xFF, 0xD9]);
        assert_retry(data);
    }

    #[test]
    fn final_and_growing_baseline_input_have_distinct_recovery_contracts() {
        let mut data = frame(8, 16, &[(1, 0x11)]);
        scan(&mut data, &[1], &[(0, false)]);
        let visible = Rc::new(Cell::new(data.len()));
        let input = GrowingInput {
            data: data.clone(),
            visible,
            position: 0
        };
        let mut decoder = JpegDecoder::new_with_options(input, options(false));
        decoder.decode_headers().unwrap();
        let mut raw = decoder.raw_output();
        let mut plane = [0xA5; 64];
        assert_eq!(
            raw.decode_next_imcu_row(&mut [&mut plane], &[8]).unwrap(),
            RawImcuRowStatus::NeedMoreInput,
        );
        assert!(plane.iter().all(|&value| value == 0xA5));
        raw.set_input_complete();
        assert!(raw
            .decode_next_imcu_row(&mut [&mut plane], &[8])
            .unwrap_err()
            .is_recoverable_eof());
        assert!(plane.iter().all(|&value| value == 0xA5));
        let mut packed = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
        assert_eq!(packed.decode().unwrap().len(), 8 * 16 * 3);
    }

    #[test]
    fn growing_baseline_does_not_publish_zero_padded_rows_as_stable_pixels() {
        let mut data = frame(32, 32, &[(1, 0x11)]);
        let len = data.len();
        data[len - 2..].copy_from_slice(&[1, 0]);
        scan_with_codes(&mut data, &[1], &[(1, true); 16], true);
        data.extend_from_slice(&[0xFF, 0xD9]);
        let mut complete = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
        let expected = complete.decode().unwrap();
        let visible = Rc::new(Cell::new(0));
        let input = GrowingInput {
            data:     data.clone(),
            visible:  visible.clone(),
            position: 0
        };
        let mut decoder = JpegDecoder::new_with_options(input, options(false));
        decoder.set_incremental_mode(true);
        let mut output = vec![0xA5; expected.len()];
        for end in 0..=data.len() {
            visible.set(end);
            match decoder.decode_into(&mut output) {
                Ok(()) => assert_eq!(output, expected),
                Err(error) => {
                    assert!(error.is_recoverable_eof(), "{end}: {error:?}");
                    let stable = decoder.decoded_output_bytes().unwrap_or(0);
                    assert_eq!(
                        &output[..stable],
                        &expected[..stable],
                        "visible bytes: {end}"
                    );
                }
            }
        }
        assert_eq!(output, expected);
    }

    #[test]
    fn final_multiscan_recovery_keeps_completed_blocks_in_a_partial_row() {
        let mut data = frame(32, 16, &[(1, 0x11), (2, 0x11), (3, 0x11)]);
        scan(&mut data, &[1], &[(1, true); 8]);
        scan(&mut data, &[2, 3], &[(1, true); 16]);
        data.truncate(data.len() - 5);
        let mut complete = JpegDecoder::new_with_options(ZCursor::new(&data), options(false));
        let expected = complete.decode().unwrap();
        let visible = Rc::new(Cell::new(data.len()));
        let input = GrowingInput {
            data,
            visible,
            position: 0
        };
        let mut decoder = JpegDecoder::new_with_options(input, options(false));
        decoder.set_incremental_mode(true);
        let mut output = vec![0xA5; expected.len()];
        assert!(decoder
            .decode_into(&mut output)
            .unwrap_err()
            .is_recoverable_eof());
        assert!(output.iter().all(|&value| value == 0xA5));
        decoder.set_incremental_mode(false);
        decoder.decode_into(&mut output).unwrap();
        assert_eq!(output, expected);
    }
}
