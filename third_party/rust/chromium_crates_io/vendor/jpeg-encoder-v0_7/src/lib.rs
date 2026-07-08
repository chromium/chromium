//! # JPEG encoder
//!
//! ## Using the encoder
//! ```no_run
//! # use jpeg_encoder::EncodingError;
//! # pub fn main() -> Result<(), EncodingError> {
//! use jpeg_encoder::{Encoder, ColorType};
//!
//! // An array with 4 pixels in RGB format.
//! let data = [
//!     255,0,0,
//!     0,255,0,
//!     0,0,255,
//!     255,255,255,
//! ];
//!
//! // Create new encoder that writes to a file with maximum quality (100)
//! let mut encoder = Encoder::new_file("some.jpeg", 100)?;
//!
//! // Encode the data with dimension 2x2
//! encoder.encode(&data, 2, 2, ColorType::Rgb)?;
//! # Ok(())
//! # }

#![no_std]
#![cfg_attr(not(feature = "simd"), forbid(unsafe_code))]

#[cfg(feature = "std")]
extern crate std;

extern crate alloc;
extern crate core;

#[cfg(all(feature = "simd", any(target_arch = "x86", target_arch = "x86_64")))]
mod avx2;
mod encoder;
mod error;
mod fdct;
mod huffman;
mod image_buffer;
mod marker;
mod quantization;
mod writer;

pub use encoder::{ColorType, Encoder, JpegColorType, SamplingFactor};
pub use error::EncodingError;
pub use image_buffer::{ImageBuffer, cmyk_to_ycck, rgb_to_ycbcr};
pub use quantization::QuantizationTableType;
pub use writer::{JfifWrite, PixelDensity, PixelDensityUnit};

#[cfg(feature = "benchmark")]
pub use fdct::fdct;

#[cfg(feature = "benchmark")]
pub use image_buffer::RgbImage;

#[cfg(all(
    feature = "benchmark",
    feature = "simd",
    any(target_arch = "x86", target_arch = "x86_64")
))]
pub use avx2::fdct_avx2;

#[cfg(all(
    feature = "benchmark",
    feature = "simd",
    any(target_arch = "x86", target_arch = "x86_64")
))]
pub use avx2::RgbImageAVX2;

#[cfg(test)]
mod tests {
    use crate::image_buffer::rgb_to_ycbcr;
    use crate::{ColorType, Encoder, QuantizationTableType, SamplingFactor};
    use jpeg_decoder::{Decoder, ImageInfo, PixelFormat};

    use alloc::boxed::Box;
    use alloc::vec;
    use alloc::vec::Vec;

    fn create_test_img_rgb() -> (Vec<u8>, u16, u16) {
        // Ensure size which which ensures an odd MCU count per row to test chroma subsampling
        let width = 258;
        let height = 128;

        let mut data = Vec::with_capacity(width * height * 3);

        for y in 0..height {
            for x in 0..width {
                let x = x.min(255);
                data.push(x as u8);
                data.push((y * 2) as u8);
                data.push(((x + y * 2) / 2) as u8);
            }
        }

        (data, width as u16, height as u16)
    }

    fn create_test_img_rgba() -> (Vec<u8>, u16, u16) {
        // Ensure size which which ensures an odd MCU count per row to test chroma subsampling
        let width = 258;
        let height = 128;

        let mut data = Vec::with_capacity(width * height * 3);

        for y in 0..height {
            for x in 0..width {
                let x = x.min(255);
                data.push(x as u8);
                data.push((y * 2) as u8);
                data.push(((x + y * 2) / 2) as u8);
                data.push(x as u8);
            }
        }

        (data, width as u16, height as u16)
    }

    fn create_test_img_gray() -> (Vec<u8>, u16, u16) {
        let width = 258;
        let height = 128;

        let mut data = Vec::with_capacity(width * height);

        for y in 0..height {
            for x in 0..width {
                let x = x.min(255);
                let (y, _, _) = rgb_to_ycbcr(x as u8, (y * 2) as u8, ((x + y * 2) / 2) as u8);
                data.push(y);
            }
        }

        (data, width as u16, height as u16)
    }

    fn create_test_img_cmyk() -> (Vec<u8>, u16, u16) {
        let width = 258;
        let height = 192;

        let mut data = Vec::with_capacity(width * height * 4);

        for y in 0..height {
            for x in 0..width {
                let x = x.min(255);
                data.push(x as u8);
                data.push((y * 3 / 2) as u8);
                data.push(((x + y * 3 / 2) / 2) as u8);
                data.push((255 - (x + y) / 2) as u8);
            }
        }

        (data, width as u16, height as u16)
    }

    fn decode(data: &[u8]) -> (Vec<u8>, ImageInfo) {
        let mut decoder = Decoder::new(data);

        (decoder.decode().unwrap(), decoder.info().unwrap())
    }

    fn check_result(
        data: Vec<u8>,
        width: u16,
        height: u16,
        result: &mut Vec<u8>,
        pixel_format: PixelFormat,
    ) {
        let (img, info) = decode(&result);

        assert_eq!(info.pixel_format, pixel_format);
        assert_eq!(info.width, width);
        assert_eq!(info.height, height);
        assert_eq!(img.len(), data.len());

        for (i, (&v1, &v2)) in data.iter().zip(img.iter()).enumerate() {
            let diff = (v1 as i16 - v2 as i16).abs();
            assert!(
                diff < 20,
                "Large color diff at index: {}: {} vs {}",
                i,
                v1,
                v2
            );
        }
    }

    #[test]
    fn test_gray_100() {
        let (data, width, height) = create_test_img_gray();

        let mut result = Vec::new();
        let encoder = Encoder::new(&mut result, 100);
        encoder
            .encode(&data, width, height, ColorType::Luma)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::L8);
    }

    #[test]
    fn test_rgb_100() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let encoder = Encoder::new(&mut result, 100);
        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_80() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let encoder = Encoder::new(&mut result, 80);
        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgba_80() {
        let (data, width, height) = create_test_img_rgba();

        let mut result = Vec::new();
        let encoder = Encoder::new(&mut result, 80);
        encoder
            .encode(&data, width, height, ColorType::Rgba)
            .unwrap();

        let (data, width, height) = create_test_img_rgb();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_custom_q_table() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);

        let table = QuantizationTableType::Custom(Box::new([
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1,
        ]));

        encoder.set_quantization_tables(table.clone(), table);

        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_2_2() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_2_2);
        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_2_1() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_2_1);
        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_4_1() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_4_1);
        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_1_1() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_1_1);
        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_1_4() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_1_4);
        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_progressive() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_2_1);
        encoder.set_progressive(true);

        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_optimized() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_2_2);
        encoder.set_optimized_huffman_tables(true);

        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_rgb_optimized_progressive() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_2_1);
        encoder.set_progressive(true);
        encoder.set_optimized_huffman_tables(true);

        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_cmyk() {
        let (data, width, height) = create_test_img_cmyk();

        let mut result = Vec::new();
        let encoder = Encoder::new(&mut result, 100);
        encoder
            .encode(&data, width, height, ColorType::Cmyk)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::CMYK32);
    }

    #[test]
    fn test_ycck() {
        let (data, width, height) = create_test_img_cmyk();

        let mut result = Vec::new();
        let encoder = Encoder::new(&mut result, 100);
        encoder
            .encode(&data, width, height, ColorType::CmykAsYcck)
            .unwrap();

        check_result(data, width, height, &mut result, PixelFormat::CMYK32);
    }

    #[test]
    fn test_restart_interval() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);

        encoder.set_restart_interval(32);
        const DRI_DATA: &[u8; 6] = b"\xFF\xDD\0\x04\0\x20";

        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        assert!(
            result
                .as_slice()
                .windows(DRI_DATA.len())
                .any(|w| w == DRI_DATA)
        );

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_restart_interval_4_1() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_4_1);

        encoder.set_restart_interval(32);
        const DRI_DATA: &[u8; 6] = b"\xFF\xDD\0\x04\0\x20";

        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        assert!(
            result
                .as_slice()
                .windows(DRI_DATA.len())
                .any(|w| w == DRI_DATA)
        );

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_restart_interval_progressive() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 85);
        encoder.set_progressive(true);

        encoder.set_restart_interval(32);
        const DRI_DATA: &[u8; 6] = b"\xFF\xDD\0\x04\0\x20";

        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        assert!(
            result
                .as_slice()
                .windows(DRI_DATA.len())
                .any(|w| w == DRI_DATA)
        );

        check_result(data, width, height, &mut result, PixelFormat::RGB24);
    }

    #[test]
    fn test_app_segment() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);

        encoder.add_app_segment(15, b"HOHOHO\0".to_vec()).unwrap();

        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        let segment_data = b"\xEF\0\x09HOHOHO\0";

        assert!(
            result
                .as_slice()
                .windows(segment_data.len())
                .any(|w| w == segment_data)
        );
    }

    #[test]
    fn test_icc_profile() {
        let (data, width, height) = create_test_img_rgb();

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);

        let mut icc = Vec::with_capacity(128 * 1024);

        for i in 0..128 * 1024 {
            icc.push((i % 255) as u8);
        }

        encoder.add_icc_profile(&icc).unwrap();

        encoder
            .encode(&data, width, height, ColorType::Rgb)
            .unwrap();

        const MARKER: &[u8; 12] = b"ICC_PROFILE\0";

        assert!(result.as_slice().windows(MARKER.len()).any(|w| w == MARKER));

        let mut decoder = Decoder::new(result.as_slice());

        decoder.decode().unwrap();

        let icc_out = match decoder.icc_profile() {
            Some(icc) => icc,
            None => panic!("Missing icc profile"),
        };

        assert_eq!(icc, icc_out);
    }

    #[test]
    fn test_rgb_optimized_missing_table_frequency() {
        let data = vec![0xfb, 0x15, 0x15];

        let mut result = Vec::new();
        let mut encoder = Encoder::new(&mut result, 100);
        encoder.set_sampling_factor(SamplingFactor::F_2_2);
        encoder.set_optimized_huffman_tables(true);

        encoder.encode(&data, 1, 1, ColorType::Rgb).unwrap();

        check_result(data, 1, 1, &mut result, PixelFormat::RGB24);
    }
}
