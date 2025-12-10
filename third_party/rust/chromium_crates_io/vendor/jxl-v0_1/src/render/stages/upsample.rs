// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::needless_range_loop)]

use crate::{
    headers::CustomTransformData,
    render::{Channels, ChannelsMut, RenderPipelineInOutStage},
};

pub struct Upsample<const N: usize, const SHIFT: u8> {
    kernel: [[[[f32; 5]; 5]; N]; N],
    channel: usize,
}

impl<const N: usize, const SHIFT: u8> Upsample<N, SHIFT> {
    pub fn new(ups_factors: &CustomTransformData, channel: usize) -> Self {
        const { assert!(SHIFT >= 1 && SHIFT <= 3) }
        const { assert!(1 << SHIFT == N) }

        let weights: &[f32] = match N {
            2 => &ups_factors.weights2,
            4 => &ups_factors.weights4,
            8 => &ups_factors.weights8,
            _ => unreachable!(),
        };

        let mut kernel = [[[[0.0; 5]; 5]; N]; N];
        let n = N / 2;
        for i in 0..5 * n {
            for j in 0..5 * n {
                let y = i.min(j);
                let x = i.max(j);
                let y = y as isize;
                let x = x as isize;
                let n = n as isize;
                let index = (5 * n * y - y * (y - 1) / 2 + x - y) as usize;
                // Filling in the top left corner from the weights
                kernel[j / 5][i / 5][j % 5][i % 5] = weights[index];
                // Mirroring to get the rest of the kernel.
                kernel[(2 * n as usize - 1) - j / 5][i / 5][4 - (j % 5)][i % 5] = weights[index];
                kernel[j / 5][(2 * n as usize - 1) - i / 5][j % 5][4 - (i % 5)] = weights[index];
                kernel[(2 * n as usize - 1) - j / 5][(2 * n as usize - 1) - i / 5][4 - (j % 5)]
                    [4 - (i % 5)] = weights[index];
            }
        }

        Self { kernel, channel }
    }
}

impl<const N: usize, const SHIFT: u8> std::fmt::Display for Upsample<N, SHIFT> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{N}x{N} upsampling of channel {}", self.channel)
    }
}

impl<const N: usize, const SHIFT: u8> RenderPipelineInOutStage for Upsample<N, SHIFT> {
    type InputT = f32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (SHIFT, SHIFT);
    const BORDER: (u8, u8) = (2, 2);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }
    /// Processes a chunk of a row, applying NxN upsampling using a 5x5 kernel.
    /// Each input value expands into a NxN region in the output, based on neighboring inputs.
    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let input = &input_rows[0];

        for x in 0..xsize {
            // Upsample this input value into a NxN region in the output
            let mut minval = input[0][x];
            let mut maxval = minval;
            for di in 0..N {
                for dj in 0..N {
                    // Iterate over the input rows and columns
                    let mut output_val = 0.0;
                    #[allow(clippy::needless_range_loop)]
                    for i in 0..5 {
                        for j in 0..5 {
                            let input_value = input[i][j + x];
                            output_val += input_value * self.kernel[di][dj][i % 5][j % 5];
                            minval = input_value.min(minval);
                            maxval = input_value.max(maxval);
                        }
                    }
                    output_rows[0][di][dj + N * x] = output_val.clamp(minval, maxval);
                }
            }
        }
    }
}

pub type Upsample2x = Upsample<2, 1>;
pub type Upsample4x = Upsample<4, 2>;
pub type Upsample8x = Upsample<8, 3>;

#[cfg(test)]
mod test {
    use super::*;
    use crate::{
        error::Result, headers::CustomTransformDataNonserialized, image::Image,
        render::test::make_and_run_simple_pipeline, util::test::assert_almost_abs_eq,
    };
    use test_log::test;

    fn ups_factors() -> CustomTransformData {
        CustomTransformData::default(&CustomTransformDataNonserialized { xyb_encoded: true })
    }

    #[test]
    fn upsample2x_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || Upsample2x::new(&ups_factors(), 0),
            (500, 500),
            1,
        )
    }

    #[test]
    fn upsample4x_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || Upsample4x::new(&ups_factors(), 0),
            (500, 500),
            1,
        )
    }

    #[test]
    fn upsample8x_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || Upsample8x::new(&ups_factors(), 0),
            (504, 504),
            1,
        )
    }

    #[test]
    fn upsample2x_constant() -> Result<()> {
        let image_size = (238, 412);
        let input_size = (image_size.0 / 2, image_size.1 / 2);
        let val = 0.777f32;
        let input = Image::new_with_value(input_size, val)?;
        let stage = Upsample2x::new(&ups_factors(), 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], image_size, 0, 123)?;
        for x in 0..image_size.0 {
            for y in 0..image_size.1 {
                assert_almost_abs_eq(output[0].row(y)[x], val, 0.0000001);
            }
        }
        Ok(())
    }

    #[test]
    fn upsample4x_constant() -> Result<()> {
        let image_size = (240, 412);
        let input_size = (image_size.0 / 4, image_size.1 / 4);
        let val = 0.777f32;
        let input = Image::new_with_value(input_size, val)?;
        let stage = Upsample4x::new(&ups_factors(), 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], image_size, 0, 123)?;
        for x in 0..image_size.0 {
            for y in 0..image_size.1 {
                assert_almost_abs_eq(output[0].row(y)[x], val, 0.00001);
            }
        }
        Ok(())
    }

    #[test]
    fn upsample8x_constant() -> Result<()> {
        let image_size = (240, 416);
        let input_size = (image_size.0 / 8, image_size.1 / 8);
        let val = 0.777f32;
        let input = Image::new_with_value(input_size, val)?;
        let stage = Upsample8x::new(&ups_factors(), 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], image_size, 0, 123)?;
        for x in 0..image_size.0 {
            for y in 0..image_size.1 {
                assert_almost_abs_eq(output[0].row(y)[x], val, 0.00001);
            }
        }
        Ok(())
    }

    #[test]
    fn test_upsample2() -> Result<()> {
        let eps = 0.0000001;
        let mut input = Image::new((7, 7))?;
        // Put a single "1.0" in the middle of the image.
        input.row_mut(3)[3] = 1.0f32;
        let ups_factors = ups_factors();
        let stage = Upsample2x::new(&ups_factors, 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], (14, 14), 0, 77)?;
        assert_eq!(output[0].size(), (14, 14));
        // Check we have a border with zeros
        for i in 0..14 {
            for j in 0..2 {
                assert_almost_abs_eq(output[0].row(j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[j], 0.0, eps);
                assert_almost_abs_eq(output[0].row(13 - j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[13 - j], 0.0, eps);
            }
        }
        // Define the mapping for the symmetric top-left kernel
        let index_map = [
            [0, 1, 2, 3, 4],
            [1, 5, 6, 7, 8],
            [2, 6, 9, 10, 11],
            [3, 7, 10, 12, 13],
            [4, 8, 11, 13, 14],
        ];

        // Validate weights from the kernel
        let kernel_size = 5;
        let kernel_offset = 2;
        let weights = &ups_factors.weights2;
        for di in 0..2 {
            for dj in 0..2 {
                for i in 0..kernel_size {
                    for j in 0..kernel_size {
                        let output_value =
                            output[0].row(kernel_offset + di + 2 * i)[kernel_offset + dj + 2 * j];
                        let mapped_i = if di == 0 { kernel_size - 1 - i } else { i };
                        let mapped_j = if dj == 0 { kernel_size - 1 - j } else { j };
                        let weight_index = index_map[mapped_i][mapped_j];
                        assert_almost_abs_eq(
                            output_value,
                            weights[weight_index].clamp(0.0, 1.0),
                            eps,
                        );
                    }
                }
            }
        }

        Ok(())
    }

    #[test]
    fn test_upsample4() -> Result<()> {
        let eps = 0.0000001;
        let mut input = Image::new((7, 7))?;
        // Put a single "1.0" in the middle of the image.
        input.row_mut(3)[3] = 1.0f32;
        let ups_factors = ups_factors();
        let stage = Upsample4x::new(&ups_factors, 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], (28, 28), 0, 1024)?;

        assert_eq!(output[0].size(), (28, 28));

        // Check we have a border with zeros
        for i in 0..28 {
            for j in 0..4 {
                assert_almost_abs_eq(output[0].row(j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[j], 0.0, eps);
                assert_almost_abs_eq(output[0].row(27 - j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[27 - j], 0.0, eps);
            }
        }

        // Define the mapping for the symmetric top-left kernel
        let index_map = [
            [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
            [1, 10, 11, 12, 13, 14, 15, 16, 17, 18],
            [2, 11, 19, 20, 21, 22, 23, 24, 25, 26],
            [3, 12, 20, 27, 28, 29, 30, 31, 32, 33],
            [4, 13, 21, 28, 34, 35, 36, 37, 38, 39],
            [5, 14, 22, 29, 35, 40, 41, 42, 43, 44],
            [6, 15, 23, 30, 36, 41, 45, 46, 47, 48],
            [7, 16, 24, 31, 37, 42, 46, 49, 50, 51],
            [8, 17, 25, 32, 38, 43, 47, 50, 52, 53],
            [9, 18, 26, 33, 39, 44, 48, 51, 53, 54],
        ];

        // Validate weights from the kernel
        let kernel_size = 5;
        let kernel_offset = 4;
        let weights = &ups_factors.weights4;
        let row_size = output[0].size().0;
        let column_size = row_size;
        for di in 0..4 {
            for dj in 0..4 {
                for ki in 0..kernel_size {
                    for kj in 0..kernel_size {
                        let i = kernel_size * di + ki;
                        let j = kernel_size * dj + kj;
                        let offset_i = kernel_offset + i;
                        let offset_j = kernel_offset + j;
                        // Testing symmetry
                        let output_value = output[0].row(offset_i)[offset_j];
                        let output_value_mirrored_right =
                            output[0].row(row_size - offset_i - 1)[offset_j];
                        let output_value_mirrored_down =
                            output[0].row(row_size - offset_i - 1)[column_size - offset_j - 1];
                        let output_value_mirrored_down_right =
                            output[0].row(row_size - offset_i - 1)[column_size - offset_j - 1];

                        assert_almost_abs_eq(output_value, output_value_mirrored_right, eps);
                        assert_almost_abs_eq(output_value, output_value_mirrored_down, eps);
                        assert_almost_abs_eq(output_value, output_value_mirrored_down_right, eps);

                        // Testing if we get the expected weights, appropriately mapped.
                        let mapped_i = if (i % 4) < 2 {
                            4 - (i / 4) + (i % 2) * 5
                        } else {
                            i / 4 + (1 - (i % 2)) * 5
                        };
                        let mapped_j = if (j % 4) < 2 {
                            4 - (j / 4) + (j % 2) * 5
                        } else {
                            j / 4 + (1 - (j % 2)) * 5
                        };
                        let weight_index = index_map[mapped_i][mapped_j];
                        assert_almost_abs_eq(
                            output_value,
                            weights[weight_index].clamp(0.0, 1.0),
                            eps,
                        );
                    }
                }
            }
        }

        Ok(())
    }

    #[test]
    fn test_upsample8() -> Result<()> {
        let eps = 0.0000001;
        let mut input = Image::new((7, 7))?;
        // Put a single "1.0" in the middle of the image.
        input.row_mut(3)[3] = 1.0f32;
        let ups_factors = ups_factors();
        let stage = Upsample8x::new(&ups_factors, 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], (56, 56), 0, 1024)?;

        assert_eq!(output[0].size(), (56, 56));

        // Check we have a border with zeros
        for i in 0..56 {
            for j in 0..8 {
                assert_almost_abs_eq(output[0].row(j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[j], 0.0, eps);
                assert_almost_abs_eq(output[0].row(55 - j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[55 - j], 0.0, eps);
            }
        }

        // Define the mapping for the symmetric top-left kernel
        let index_map = [
            [
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
                0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
            ],
            [
                0x01, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
            ],
            [
                0x02, 0x15, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
                0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
            ],
            [
                0x03, 0x16, 0x28, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43,
                0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
            ],
            [
                0x04, 0x17, 0x29, 0x3a, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
                0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
            ],
            [
                0x05, 0x18, 0x2a, 0x3b, 0x4b, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62,
                0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
            ],
            [
                0x06, 0x19, 0x2b, 0x3c, 0x4c, 0x5b, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
                0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
            ],
            [
                0x07, 0x1a, 0x2c, 0x3d, 0x4d, 0x5c, 0x6a, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d,
                0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
            ],
            [
                0x08, 0x1b, 0x2d, 0x3e, 0x4e, 0x5d, 0x6b, 0x78, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
                0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
            ],
            [
                0x09, 0x1c, 0x2e, 0x3f, 0x4f, 0x5e, 0x6c, 0x79, 0x85, 0x90, 0x91, 0x92, 0x93, 0x94,
                0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
            ],
            [
                0x0a, 0x1d, 0x2f, 0x40, 0x50, 0x5f, 0x6d, 0x7a, 0x86, 0x91, 0x9b, 0x9c, 0x9d, 0x9e,
                0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
            ],
            [
                0x0b, 0x1e, 0x30, 0x41, 0x51, 0x60, 0x6e, 0x7b, 0x87, 0x92, 0x9c, 0xa5, 0xa6, 0xa7,
                0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad,
            ],
            [
                0x0c, 0x1f, 0x31, 0x42, 0x52, 0x61, 0x6f, 0x7c, 0x88, 0x93, 0x9d, 0xa6, 0xae, 0xaf,
                0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5,
            ],
            [
                0x0d, 0x20, 0x32, 0x43, 0x53, 0x62, 0x70, 0x7d, 0x89, 0x94, 0x9e, 0xa7, 0xaf, 0xb6,
                0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc,
            ],
            [
                0x0e, 0x21, 0x33, 0x44, 0x54, 0x63, 0x71, 0x7e, 0x8a, 0x95, 0x9f, 0xa8, 0xb0, 0xb7,
                0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2,
            ],
            [
                0x0f, 0x22, 0x34, 0x45, 0x55, 0x64, 0x72, 0x7f, 0x8b, 0x96, 0xa0, 0xa9, 0xb1, 0xb8,
                0xbe, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
            ],
            [
                0x10, 0x23, 0x35, 0x46, 0x56, 0x65, 0x73, 0x80, 0x8c, 0x97, 0xa1, 0xaa, 0xb2, 0xb9,
                0xbf, 0xc4, 0xc8, 0xc9, 0xca, 0xcb,
            ],
            [
                0x11, 0x24, 0x36, 0x47, 0x57, 0x66, 0x74, 0x81, 0x8d, 0x98, 0xa2, 0xab, 0xb3, 0xba,
                0xc0, 0xc5, 0xc9, 0xcc, 0xcd, 0xce,
            ],
            [
                0x12, 0x25, 0x37, 0x48, 0x58, 0x67, 0x75, 0x82, 0x8e, 0x99, 0xa3, 0xac, 0xb4, 0xbb,
                0xc1, 0xc6, 0xca, 0xcd, 0xcf, 0xd0,
            ],
            [
                0x13, 0x26, 0x38, 0x49, 0x59, 0x68, 0x76, 0x83, 0x8f, 0x9a, 0xa4, 0xad, 0xb5, 0xbc,
                0xc2, 0xc7, 0xcb, 0xce, 0xd0, 0xd1,
            ],
        ];

        // Validate weights from the kernel
        let kernel_size = 5;
        let kernel_offset = 8;
        let weights = &ups_factors.weights8;
        let row_size = output[0].size().0;
        let column_size = row_size;
        for di in 0..8 {
            for dj in 0..8 {
                for ki in 0..kernel_size {
                    for kj in 0..kernel_size {
                        let i = kernel_size * di + ki;
                        let j = kernel_size * dj + kj;
                        let offset_i = kernel_offset + i;
                        let offset_j = kernel_offset + j;
                        // Testing symmetry
                        let output_value = output[0].row(offset_i)[offset_j];
                        let output_value_mirrored_right =
                            output[0].row(row_size - offset_i - 1)[offset_j];
                        let output_value_mirrored_down =
                            output[0].row(row_size - offset_i - 1)[column_size - offset_j - 1];
                        let output_value_mirrored_down_right =
                            output[0].row(row_size - offset_i - 1)[column_size - offset_j - 1];

                        assert_almost_abs_eq(output_value, output_value_mirrored_right, eps);
                        assert_almost_abs_eq(output_value, output_value_mirrored_down, eps);
                        assert_almost_abs_eq(output_value, output_value_mirrored_down_right, eps);

                        // Testing if we get the expected weights, appropriately mapped.
                        let mapped_i = if (i % 8) < 4 {
                            4 - (i / 8) + (i % 4) * 5
                        } else {
                            i / 8 + (3 - (i % 4)) * 5
                        };
                        let mapped_j = if (j % 8) < 4 {
                            4 - (j / 8) + (j % 4) * 5
                        } else {
                            j / 8 + (3 - (j % 4)) * 5
                        };
                        let weight_index = index_map[mapped_i][mapped_j];
                        assert_almost_abs_eq(
                            output_value,
                            weights[weight_index].clamp(0.0, 1.0),
                            eps,
                        );
                    }
                }
            }
        }

        Ok(())
    }
}
