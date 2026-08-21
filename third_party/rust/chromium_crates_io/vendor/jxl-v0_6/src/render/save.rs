// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::{JxlColorType, JxlDataFormat, JxlOutputBuffer},
    error::{Error, Result},
    headers::Orientation,
    image::DataTypeTag,
};

#[derive(Debug)]
pub struct SaveStage {
    pub(super) channels: Vec<usize>,
    pub(super) orientation: Orientation,
    pub(super) output_buffer_index: usize,
    pub(super) color_type: JxlColorType,
    pub(super) data_format: JxlDataFormat,
    /// When true, fill alpha channel with opaque (1.0) values.
    /// Used when RGBA output is requested but image has no alpha channel.
    pub(super) fill_opaque_alpha: bool,
}

impl SaveStage {
    pub fn new(
        channels: &[usize],
        orientation: Orientation,
        output_buffer_index: usize,
        mut color_type: JxlColorType,
        data_format: JxlDataFormat,
        fill_opaque_alpha: bool,
    ) -> SaveStage {
        let mut channels = channels.to_vec();
        if color_type == JxlColorType::Bgr {
            color_type = JxlColorType::Rgb;
            channels.swap(0, 2);
        }
        if color_type == JxlColorType::Bgra {
            color_type = JxlColorType::Rgba;
            channels.swap(0, 2);
        }
        Self {
            channels,
            orientation,
            output_buffer_index,
            color_type,
            data_format,
            fill_opaque_alpha,
        }
    }

    /// Returns the number of output channels (including filled alpha if applicable)
    pub fn output_channels(&self) -> usize {
        self.color_type.samples_per_pixel()
    }

    pub fn uses_channel(&self, c: usize) -> bool {
        self.channels.contains(&c)
    }

    pub fn input_type(&self) -> DataTypeTag {
        self.data_format.data_type()
    }

    pub fn check_buffer_size(
        &self,
        size: (usize, usize),
        buffer: Option<&JxlOutputBuffer>,
    ) -> Result<()> {
        let Some(buf) = buffer else {
            return Ok(());
        };
        let osize = self.orientation.map_size(size);

        let expected_w = self.output_channels() * self.data_format.bytes_per_sample() * osize.0;

        if buf.byte_size() != (expected_w, osize.1) {
            return Err(Error::InvalidOutputBufferSize(
                buf.byte_size().0,
                buf.byte_size().1,
                osize.0,
                osize.1,
                self.color_type,
                self.data_format,
            ));
        }
        Ok(())
    }
}

impl std::fmt::Display for SaveStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "save channels {:?} (type {:?} {:?})",
            self.channels, self.color_type, self.data_format
        )
    }
}
