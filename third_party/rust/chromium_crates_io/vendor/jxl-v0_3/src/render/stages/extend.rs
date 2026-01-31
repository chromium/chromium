// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::sync::Arc;

use crate::{
    error::Result,
    frame::ReferenceFrame,
    headers::{FileHeader, extra_channels::ExtraChannelInfo, frame_header::*},
};

/// Does not directly modify the current image pixels, but extends the current image with
/// additional data.
///
/// `uses_channel` must always return true, and stages of this type should override
/// `new_size` and `original_data_origin`.
/// `process_row_chunk` will be called with the *new* image coordinates, and will only be called
/// on row chunks outside of the original image data.
/// After stages of this type, no stage can have a non-0 SHIFT_X, SHIFT_Y, BORDER_X or BORDER_Y.
/// There can be at most one extend stage per image.
pub struct ExtendToImageDimensionsStage {
    pub frame_origin: (isize, isize),
    pub image_size: (usize, usize),
    pub blending_info: BlendingInfo,
    pub ec_blending_info: Vec<BlendingInfo>,
    pub extra_channels: Vec<ExtraChannelInfo>,
    pub reference_frames: Arc<[Option<ReferenceFrame>; 4]>,
    pub zeros: Vec<f32>,
}

impl ExtendToImageDimensionsStage {
    // TODO(veluca): should this return a Result?
    pub fn new(
        frame_header: &FrameHeader,
        file_header: &FileHeader,
        reference_frames: Arc<[Option<ReferenceFrame>; 4]>,
    ) -> Result<ExtendToImageDimensionsStage> {
        let xsize = file_header.size.xsize() as usize;
        Ok(ExtendToImageDimensionsStage {
            frame_origin: (frame_header.x0 as isize, frame_header.y0 as isize),
            image_size: (xsize, file_header.size.ysize() as usize),
            blending_info: frame_header.blending_info.clone(),
            ec_blending_info: frame_header.ec_blending_info.clone(),
            extra_channels: file_header.image_metadata.extra_channel_info.clone(),
            reference_frames,
            zeros: vec![0f32; xsize],
        })
    }
}

impl std::fmt::Display for ExtendToImageDimensionsStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "extend-to-image-dims")
    }
}

impl ExtendToImageDimensionsStage {
    pub(in crate::render) fn process_row_chunk(
        &self,
        position: (usize, usize),
        xsize: usize,
        c: usize,
        row: &mut [f32],
    ) {
        let x0 = position.0;
        let x1 = x0 + xsize;
        let y0 = position.1;
        let source = if c < 3 {
            self.blending_info.source as usize
        } else if c - 3 < self.ec_blending_info.len() {
            self.ec_blending_info[c - 3].source as usize
        } else {
            // This is a synthetic channel such as the noise channels, which can't be extended
            // and can't be used after extending, so nothing to do.
            return;
        };
        let bg = if let Some(bg) = self.reference_frames[source].as_ref() {
            bg.frame[c].row(y0)
        } else {
            self.zeros.as_slice()
        };
        row[0..xsize].copy_from_slice(&bg[x0..x1]);
    }
}

#[cfg(test)]
mod test {
    use std::sync::Arc;

    use test_log::test;

    use super::*;
    use crate::error::Result;
    use crate::util::test::read_headers_and_toc;

    #[test]
    fn extend_consistency() -> Result<()> {
        let (file_header, frame_header, _) =
            read_headers_and_toc(include_bytes!("../../../resources/test/basic.jxl")).unwrap();
        let reference_frames = Arc::new([None, None, None, None]);
        crate::render::test::test_stage_consistency(
            || {
                ExtendToImageDimensionsStage::new(
                    &frame_header,
                    &file_header,
                    reference_frames.clone(),
                )
                .unwrap()
            },
            (500, 500),
            4,
        )
    }
}
