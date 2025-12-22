// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    fmt::Debug,
    io::{BufRead, BufReader, Cursor, Read, Write},
    num::{ParseFloatError, ParseIntError},
};

use crate::{
    bit_reader::BitReader,
    container::ContainerParser,
    error::Error as JXLError,
    headers::{
        FileHeader, JxlHeader,
        encodings::*,
        frame_header::FrameHeader,
        toc::{Toc, TocNonserialized},
    },
    image::{Image, ImageDataType},
};

use num_traits::AsPrimitive;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum Error {
    #[error("Invalid PFM: {0}")]
    InvalidPFM(String),
}

impl From<ParseFloatError> for Error {
    fn from(value: ParseFloatError) -> Self {
        Error::InvalidPFM(value.to_string())
    }
}

impl From<ParseIntError> for Error {
    fn from(value: ParseIntError) -> Self {
        Error::InvalidPFM(value.to_string())
    }
}

impl From<std::io::Error> for Error {
    fn from(value: std::io::Error) -> Self {
        Error::InvalidPFM(value.to_string())
    }
}

impl From<JXLError> for Error {
    fn from(value: JXLError) -> Self {
        Error::InvalidPFM(value.to_string())
    }
}

fn rel_error_gt<T: AsPrimitive<f64>>(left: T, right: T, max_rel_error: T) -> bool {
    let left_f64: f64 = left.as_();
    let right_f64: f64 = right.as_();
    let error = (left_f64 - right_f64).abs();
    matches!(
        (2.0 * error / (left_f64.abs() + right_f64.abs() + 1e-16))
            .partial_cmp(&max_rel_error.as_()),
        Some(std::cmp::Ordering::Greater) | None
    )
}

fn abs_error_gt<T: AsPrimitive<f64>>(left: T, right: T, max_abs_error: T) -> bool {
    let left_f64: f64 = left.as_();
    let right_f64: f64 = right.as_();
    matches!(
        (left_f64 - right_f64)
            .abs()
            .partial_cmp(&max_abs_error.as_()),
        Some(std::cmp::Ordering::Greater) | None
    )
}

pub fn assert_almost_eq<T: AsPrimitive<f64> + Debug + Copy>(
    left: T,
    right: T,
    max_abs_error: T,
    max_rel_error: T,
) {
    if abs_error_gt(left, right, max_abs_error) || rel_error_gt(left, right, max_rel_error) {
        panic!(
            "assertion failed: `(left ≈ right)`\n  left: `{left:?}`,\n right: `{right:?}`,\n max_abs_error: `{max_abs_error:?}`,\n max_rel_error: `{max_rel_error:?}`"
        );
    }
}

pub fn assert_almost_rel_eq<T: AsPrimitive<f64> + Debug + Copy>(
    left: T,
    right: T,
    max_rel_error: T,
) {
    if rel_error_gt(left, right, max_rel_error) {
        panic!(
            "assertion failed: `(left ≈ right)`\n  left: `{left:?}`,\n right: `{right:?}`,\n max_rel_error: `{max_rel_error:?}`"
        );
    }
}

pub fn assert_almost_abs_eq<T: AsPrimitive<f64> + Debug + Copy>(
    left: T,
    right: T,
    max_abs_error: T,
) {
    if abs_error_gt(left, right, max_abs_error) {
        panic!(
            "assertion failed: `(left ≈ right)`\n  left: `{left:?}`,\n right: `{right:?}`,\n max_abs_error: `{max_abs_error:?}`"
        );
    }
}

pub fn assert_almost_abs_eq_coords<T: AsPrimitive<f64> + Debug + Copy>(
    left: T,
    right: T,
    max_abs_error: T,
    pos: (usize, usize),
    c: usize,
) {
    if abs_error_gt(left, right, max_abs_error) {
        panic!(
            "assertion failed @{pos:?}, c {c}: `(left ≈ right)`\n  left: `{left:?}`,\n right: `{right:?}`,\n abs error: `{:?}`\n max_abs_error: `{max_abs_error:?}`",
            (left.as_() - right.as_()).abs()
        );
    }
}

fn assert_same_len<T: AsPrimitive<f64> + Debug + Copy>(left: &[T], right: &[T]) {
    if left.as_ref().len() != right.as_ref().len() {
        panic!(
            "assertion failed: `(left ≈ right)`\n left.len(): `{}`,\n right.len(): `{}`",
            left.as_ref().len(),
            right.as_ref().len()
        );
    }
}

pub fn assert_all_almost_eq<T: AsPrimitive<f64> + Debug + Copy, V: AsRef<[T]> + Debug>(
    left: V,
    right: V,
    max_abs_error: T,
    max_rel_error: T,
) {
    assert_same_len(left.as_ref(), right.as_ref());
    for (idx, (left_val, right_val)) in left
        .as_ref()
        .iter()
        .copied()
        .zip(right.as_ref().iter().copied())
        .enumerate()
    {
        if abs_error_gt(left_val, right_val, max_abs_error)
            || rel_error_gt(left_val, right_val, max_rel_error)
        {
            panic!(
                "assertion failed: `(left ≈ right)`\n left: `{left:?}`,\n right: `{right:?}`,\n max_abs_error: `{max_abs_error:?}`,\n max_rel_error: `{max_rel_error:?}`,\n left[{idx}]: `{left_val:?}`,\n right[{idx}]: `{right_val:?}`",
            );
        }
    }
}

pub fn assert_all_almost_rel_eq<T: AsPrimitive<f64> + Debug + Copy, V: AsRef<[T]> + Debug>(
    left: V,
    right: V,
    max_rel_error: T,
) {
    assert_same_len(left.as_ref(), right.as_ref());
    for (idx, (left_val, right_val)) in left
        .as_ref()
        .iter()
        .copied()
        .zip(right.as_ref().iter().copied())
        .enumerate()
    {
        if rel_error_gt(left_val, right_val, max_rel_error) {
            panic!(
                "assertion failed: `(left ≈ right)`\n left: `{left:?}`,\n right: `{right:?}`,\n max_rel_error: `{max_rel_error:?}`,\n left[{idx}]: `{left_val:?}`,\n right[{idx}]: `{right_val:?}`",
            );
        }
    }
}

pub fn assert_all_almost_abs_eq<T: AsPrimitive<f64> + Debug + Copy, V: AsRef<[T]> + Debug>(
    left: V,
    right: V,
    max_abs_error: T,
) {
    assert_same_len(left.as_ref(), right.as_ref());
    for (idx, (left_val, right_val)) in left
        .as_ref()
        .iter()
        .copied()
        .zip(right.as_ref().iter().copied())
        .enumerate()
    {
        if abs_error_gt(left_val, right_val, max_abs_error) {
            panic!(
                "assertion failed: `(left ≈ right)`\n left: `{left:?}`,\n right: `{right:?}`,\n max_abs_error: `{max_abs_error:?}`,\n left[{idx}]: `{left_val:?}`,\n right[{idx}]: `{right_val:?}`",
            );
        }
    }
}

pub fn check_equal_images<T: ImageDataType>(a: &Image<T>, b: &Image<T>) {
    assert_eq!(a.size(), b.size());
    let mismatch_info = |x: usize, y: usize| -> String {
        let msg = format!(
            "mismatch at position {x}x{y}, values {:?} and {:?}",
            a.row(y)[x],
            b.row(y)[x]
        );
        msg
    };
    for y in 0..a.size().1 {
        for x in 0..a.size().0 {
            assert_eq!(a.row(y)[x], b.row(y)[x], "{}", mismatch_info(x, y));
        }
    }
}

pub fn read_headers_and_toc(image: &[u8]) -> Result<(FileHeader, FrameHeader, Toc), JXLError> {
    let codestream = ContainerParser::collect_codestream(image).unwrap();
    let mut br = BitReader::new(&codestream);
    let file_header = FileHeader::read(&mut br)?;

    let frame_header =
        FrameHeader::read_unconditional(&(), &mut br, &file_header.frame_header_nonserialized())?;
    let num_toc_entries = frame_header.num_toc_entries();
    let toc = Toc::read_unconditional(
        &(),
        &mut br,
        &TocNonserialized {
            num_entries: num_toc_entries as u32,
        },
    )?;
    Ok((file_header, frame_header, toc))
}

pub fn write_pfm(image: Vec<Image<f32>>, mut buf: impl Write) -> Result<(), Error> {
    if image.len() == 1 {
        buf.write_all(b"Pf\n")?;
    } else if image.len() == 3 {
        buf.write_all(b"PF\n")?;
    } else {
        return Err(Error::InvalidPFM(format!(
            "invalid number of channels: {}",
            image.len()
        )));
    }
    let size = image[0].size();
    for c in image.iter().skip(1) {
        assert_eq!(size, c.size());
    }
    buf.write_fmt(format_args!("{} {}\n", size.0, size.1))?;
    buf.write_all(b"1.0\n")?;
    let mut b: [u8; 4];
    for row in 0..size.1 {
        for col in 0..size.0 {
            for c in image.iter() {
                b = c.row(size.1 - row - 1)[col].to_be_bytes();
                buf.write_all(&b)?;
            }
        }
    }
    buf.flush()?;
    Ok(())
}

pub fn read_pfm(b: &[u8]) -> Result<Vec<Image<f32>>, Error> {
    let mut bf = BufReader::new(Cursor::new(b));
    let mut line = String::new();
    bf.read_line(&mut line)?;
    let channels = match line.trim() {
        "Pf" => 1,
        "PF" => 3,
        &_ => return Err(Error::InvalidPFM(format!("invalid PFM type header {line}"))),
    };
    line.clear();
    bf.read_line(&mut line)?;
    let mut dims = line.split_whitespace();
    let xres = if let Some(xres_str) = dims.next() {
        xres_str.trim().parse()?
    } else {
        return Err(Error::InvalidPFM(format!(
            "invalid PFM resolution header {line}",
        )));
    };
    let yres = if let Some(yres_str) = dims.next() {
        yres_str.trim().parse()?
    } else {
        return Err(Error::InvalidPFM(format!(
            "invalid PFM resolution header {line}",
        )));
    };
    line.clear();
    bf.read_line(&mut line)?;
    let endianness: f32 = line.trim().parse()?;

    let mut res = Vec::<Image<f32>>::new();
    for _ in 0..channels {
        let img = Image::new((xres, yres))?;
        res.push(img);
    }

    let mut buf = [0u8; 4];
    for row in 0..yres {
        for col in 0..xres {
            for chan in res.iter_mut() {
                bf.read_exact(&mut buf)?;
                chan.row_mut(yres - row - 1)[col] = if endianness < 0.0 {
                    f32::from_le_bytes(buf)
                } else {
                    f32::from_be_bytes(buf)
                }
            }
        }
    }

    Ok(res)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_with_floats() {
        assert_almost_abs_eq(1.0000001f64, 1.0000002, 0.000001);
        assert_almost_abs_eq(1.0, 1.1, 0.2);
    }

    #[test]
    fn test_with_integers() {
        assert_almost_abs_eq(100, 101, 2);
        assert_almost_abs_eq(777u32, 770, 7);
        assert_almost_abs_eq(500i64, 498, 3);
    }

    #[test]
    #[should_panic]
    fn test_panic_float() {
        assert_almost_abs_eq(1.0, 1.2, 0.1);
    }
    #[test]
    #[should_panic]
    fn test_panic_integer() {
        assert_almost_abs_eq(100, 105, 2);
    }

    #[test]
    #[should_panic]
    fn test_nan_comparison() {
        assert_almost_abs_eq(f64::NAN, f64::NAN, 0.1);
    }

    #[test]
    #[should_panic]
    fn test_nan_tolerance() {
        assert_almost_abs_eq(1.0, 1.0, f64::NAN);
    }

    #[test]
    fn test_infinity_tolerance() {
        assert_almost_abs_eq(1.0, 1.0, f64::INFINITY);
    }

    #[test]
    #[should_panic]
    fn test_nan_comparison_with_infinity_tolerance() {
        assert_almost_abs_eq(f32::NAN, f32::NAN, f32::INFINITY);
    }

    #[test]
    #[should_panic]
    fn test_infinity_comparison_with_infinity_tolerance() {
        assert_almost_abs_eq(f32::INFINITY, f32::INFINITY, f32::INFINITY);
    }
}
