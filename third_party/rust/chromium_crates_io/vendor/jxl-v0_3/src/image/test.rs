// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use arbtest::arbitrary::Arbitrary;

use crate::error::Result;

use super::{Image, ImageDataType, Rect};

impl<T: ImageDataType> Image<T> {
    #[cfg(test)]
    pub fn new_random<R: rand::Rng>(size: (usize, usize), rng: &mut R) -> Result<Image<T>> {
        let mut img = Self::new(size)?;
        for y in 0..size.1 {
            img.row_mut(y).iter_mut().for_each(|x| *x = T::random(rng));
        }
        Ok(img)
    }

    #[cfg(test)]
    pub fn new_range(size: (usize, usize), start: f32, step: f32) -> Result<Image<T>> {
        let mut img = Self::new(size)?;
        for y in 0..size.1 {
            img.row_mut(y).iter_mut().enumerate().for_each(|(x, val)| {
                *val = T::from_f64((start + step * (y * size.0 + x) as f32) as f64)
            });
        }
        Ok(img)
    }
}

#[test]
fn huge_image() {
    assert!(Image::<u8>::new((1 << 28, 1 << 28)).is_err());
}

#[test]
fn rect_basic() -> Result<()> {
    let mut image = Image::<u8>::new((32, 42))?;
    assert_eq!(
        image
            .get_rect_mut(Rect {
                origin: (31, 40),
                size: (1, 1)
            })
            .size(),
        (1, 1)
    );
    assert_eq!(
        image
            .get_rect_mut(Rect {
                origin: (0, 0),
                size: (1, 1)
            })
            .size(),
        (1, 1)
    );
    image
        .get_rect_mut(Rect {
            origin: (30, 30),
            size: (1, 1),
        })
        .row(0)[0] = 1;
    assert_eq!(image.row(30)[30], 1);
    Ok(())
}

fn f64_conversions<T: ImageDataType + Eq + for<'a> Arbitrary<'a>>() {
    arbtest::arbtest(|u| {
        let t = T::arbitrary(u)?;
        assert_eq!(t, T::from_f64(t.to_f64()));
        Ok(())
    });
}

#[test]
fn u8_f64_conv() {
    f64_conversions::<u8>();
}

#[test]
fn u16_f64_conv() {
    f64_conversions::<u16>();
}

#[test]
fn u32_f64_conv() {
    f64_conversions::<u32>();
}

#[test]
fn i8_f64_conv() {
    f64_conversions::<i8>();
}

#[test]
fn i16_f64_conv() {
    f64_conversions::<i16>();
}

#[test]
fn i32_f64_conv() {
    f64_conversions::<i32>();
}

#[test]
fn f32_f64_conv() {
    arbtest::arbtest(|u| {
        let t = f32::arbitrary(u)?;
        if !t.is_nan() {
            assert_eq!(t, f32::from_f64(t.to_f64()));
        }
        Ok(())
    });
}
