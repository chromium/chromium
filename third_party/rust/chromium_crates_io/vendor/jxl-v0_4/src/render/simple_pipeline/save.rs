// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::util::f16;

use crate::{
    api::{Endianness, JxlDataFormat, JxlOutputBuffer},
    error::Result,
    image::Image,
    render::save::SaveStage,
};

impl SaveStage {
    pub(super) fn save_simple(
        &self,
        data: &[Image<f64>],
        buffers: &mut [Option<JxlOutputBuffer>],
    ) -> Result<()> {
        for i in self.channels.iter().skip(1) {
            assert_eq!(data[self.channels[0]].size(), data[*i].size());
        }
        let Some(buf) = buffers[self.output_buffer_index].as_mut() else {
            return Ok(());
        };
        let size = data[0].size();

        self.check_buffer_size(size, Some(buf))?;

        let output_channels = self.output_channels();

        for (c, &chan) in self.channels.iter().enumerate() {
            for y in 0..size.1 {
                let src_row = data[chan].row(y);

                for (x, &px) in src_row.iter().enumerate() {
                    let (dx, dy) = self.orientation.display_pixel((x, y), size);
                    let dx = dx * output_channels + c;
                    let bps = self.data_format.bytes_per_sample();

                    macro_rules! write_pixel {
                        ($px: expr, $endianness: expr) => {
                            let px = $px;
                            let px_bytes = if $endianness == Endianness::LittleEndian {
                                px.to_le_bytes()
                            } else {
                                px.to_be_bytes()
                            };
                            buf.write_bytes(dy, dx * bps, &px_bytes);
                        };
                    }

                    match self.data_format {
                        JxlDataFormat::U8 { .. } => {
                            // Conversion stages already handle bit depth scaling
                            write_pixel!(px as u8, Endianness::LittleEndian);
                        }
                        JxlDataFormat::U16 { endianness, .. } => {
                            // Conversion stages already handle bit depth scaling
                            write_pixel!(px as u16, endianness);
                        }
                        JxlDataFormat::F32 { endianness } => {
                            write_pixel!(px as f32, endianness);
                        }
                        JxlDataFormat::F16 { endianness } => {
                            write_pixel!(f16::from_f64(px), endianness);
                        }
                    }
                }
            }
        }

        // Fill opaque alpha if needed (when RGBA requested but image has no alpha)
        if self.fill_opaque_alpha {
            let alpha_channel = self.channels.len(); // alpha is after the source channels
            let opaque_bytes = self.data_format.opaque_alpha_bytes();
            for y in 0..size.1 {
                for x in 0..size.0 {
                    let (dx, dy) = self.orientation.display_pixel((x, y), size);
                    let dx = dx * output_channels + alpha_channel;
                    let bps = self.data_format.bytes_per_sample();
                    buf.write_bytes(dy, dx * bps, &opaque_bytes);
                }
            }
        }

        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{
        api::JxlColorType, headers::Orientation, image::Rect, util::test::assert_almost_eq,
    };
    use rand::SeedableRng;
    use rand_xorshift::XorShiftRng;
    use test_log::test;

    #[test]
    fn save_stage() -> Result<()> {
        let save_stage = SaveStage::new(
            &[0],
            Orientation::Identity,
            0,
            JxlColorType::Grayscale,
            JxlDataFormat::U8 { bit_depth: 8 },
            false,
        );
        let mut rng = XorShiftRng::seed_from_u64(0);
        let src = [Image::<f64>::new_random((128, 128), &mut rng)?];
        let mut dst = Image::<u8>::new_random((128, 128), &mut rng)?;

        save_stage.save_simple(
            &src,
            &mut [Some(JxlOutputBuffer::from_image_rect_mut(
                dst.get_rect_mut(Rect {
                    size: (128, 128),
                    origin: (0, 0),
                })
                .into_raw(),
            ))],
        )?;

        for y in 0..128 {
            for x in 0..128 {
                // Conversion stages handle bit depth scaling, save stage just casts
                let expected = src[0].row(y)[x] as u8;
                assert_eq!(expected, dst.row(y)[x]);
            }
        }

        Ok(())
    }

    fn do_test_orientation(
        orientation: Orientation,
        transform: impl Fn(usize, usize, usize, usize) -> (usize, usize),
    ) -> Result<()> {
        let (w, h) = (32, 16);
        let mut rng = XorShiftRng::seed_from_u64(0);
        let src = [Image::<f64>::new_random((w, h), &mut rng)?];

        let (ow, oh) = if orientation.is_transposing() {
            (h, w)
        } else {
            (w, h)
        };

        let save_stage = SaveStage::new(
            &[0],
            orientation,
            0,
            JxlColorType::Grayscale,
            JxlDataFormat::f32(),
            false,
        );

        let mut rng = XorShiftRng::seed_from_u64(0);
        let mut dst = Image::<f32>::new_random((ow, oh), &mut rng)?;

        save_stage.save_simple(
            &src,
            &mut [Some(JxlOutputBuffer::from_image_rect_mut(
                dst.get_rect_mut(Rect {
                    origin: (0, 0),
                    size: (ow, oh),
                })
                .into_raw(),
            ))],
        )?;

        // Iterate over the DESTINATION image pixels.
        for y_dest in 0..oh {
            for x_dest in 0..ow {
                // For each destination pixel, find its corresponding source pixel.
                let (src_x, src_y) = transform(x_dest, y_dest, w, h);
                assert_almost_eq(
                    dst.row(y_dest)[x_dest],
                    src[0].row(src_y)[src_x] as f32,
                    1e-5,
                    1e-5,
                );
            }
        }

        Ok(())
    }

    #[test]
    fn orientation_identity() -> Result<()> {
        do_test_orientation(Orientation::Identity, |x, y, _, _| (x, y))
    }

    #[test]
    fn orientation_flip_horizontal() -> Result<()> {
        do_test_orientation(Orientation::FlipHorizontal, |x, y, w, _| (w - 1 - x, y))
    }

    #[test]
    fn orientation_flip_vertical() -> Result<()> {
        do_test_orientation(Orientation::FlipVertical, |x, y, _, h| (x, h - 1 - y))
    }

    #[test]
    fn orientation_rotate_180() -> Result<()> {
        do_test_orientation(Orientation::Rotate180, |x, y, w, h| (w - 1 - x, h - 1 - y))
    }

    // transposing orientations

    #[test]
    fn orientation_transpose() -> Result<()> {
        do_test_orientation(Orientation::Transpose, |x_dest, y_dest, _, _| {
            (y_dest, x_dest)
        })
    }

    #[test]
    fn orientation_rotate_90_cw() -> Result<()> {
        do_test_orientation(Orientation::Rotate90Cw, |x_dest, y_dest, _, h_src| {
            (y_dest, h_src - 1 - x_dest)
        })
    }

    #[test]
    fn orientation_anti_transpose() -> Result<()> {
        do_test_orientation(
            Orientation::AntiTranspose,
            |x_dest, y_dest, w_src, h_src| (w_src - 1 - y_dest, h_src - 1 - x_dest),
        )
    }

    #[test]
    fn orientation_rotate_90_ccw() -> Result<()> {
        do_test_orientation(Orientation::Rotate90Ccw, |x_dest, y_dest, w_src, _| {
            (w_src - 1 - y_dest, x_dest)
        })
    }
}
