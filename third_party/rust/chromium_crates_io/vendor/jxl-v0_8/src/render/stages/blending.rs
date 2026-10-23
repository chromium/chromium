// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::error::{Error, Result};
use crate::features::blending::perform_blending;
use crate::features::patches::{PatchBlendMode, PatchBlending};
use crate::frame::ReferenceFrame;
use crate::headers::FileHeader;
use crate::headers::extra_channels::ExtraChannelInfo;
use crate::headers::frame_header::*;
use crate::render::{ErasedLocalState, RenderPipelineInPlaceStage};
use crate::util::ChannelVec;
use crate::util::sync::Arc;

pub struct BlendingStage {
    pub frame_origin: (isize, isize),
    pub image_size: (isize, isize),
    pub blending_info: BlendingInfo,
    pub ec_blending_info: Vec<BlendingInfo>,
    /// Precomputed `PatchBlending` form of `ec_blending_info`.
    pub ec_patch_blending_info: Vec<PatchBlending>,
    pub extra_channels: Vec<ExtraChannelInfo>,
    pub reference_frames: Arc<[Option<ReferenceFrame>; 4]>,
    pub zeros: Vec<f32>,
}

struct BlendingScratch {
    scratch: Vec<f32>,
}

impl BlendingScratch {
    fn new() -> Self {
        Self {
            scratch: Vec::new(),
        }
    }
}

impl From<&BlendingInfo> for PatchBlending {
    fn from(info: &BlendingInfo) -> Self {
        let mode = match info.mode {
            BlendingMode::Replace => PatchBlendMode::None,
            BlendingMode::Add => PatchBlendMode::Add,
            BlendingMode::Mul => PatchBlendMode::Mul,
            BlendingMode::Blend => PatchBlendMode::BlendBelow,
            BlendingMode::AlphaWeightedAdd => PatchBlendMode::AlphaWeightedAddBelow,
        };
        PatchBlending {
            mode,
            alpha_channel: info.alpha_channel as usize,
            clamp: info.clamp,
        }
    }
}

impl BlendingStage {
    pub fn new(
        frame_header: &FrameHeader,
        file_header: &FileHeader,
        reference_frames: Arc<[Option<ReferenceFrame>; 4]>,
    ) -> Result<BlendingStage> {
        let image_size = (
            file_header.size.xsize() as usize,
            file_header.size.ysize() as usize,
        );
        for info in
            std::iter::once(&frame_header.blending_info).chain(frame_header.ec_blending_info.iter())
        {
            let source = info.source as usize;
            let Some(frame) = reference_frames[source].as_ref() else {
                continue;
            };
            if frame.saved_before_color_transform {
                return Err(Error::BlendingPreColorTransform(source));
            }
            assert_eq!(frame.frame[0].size(), image_size);
        }

        let ec_blending_info = frame_header.ec_blending_info.clone();
        let ec_patch_blending_info = ec_blending_info.iter().map(PatchBlending::from).collect();
        Ok(BlendingStage {
            frame_origin: (frame_header.x0 as isize, frame_header.y0 as isize),
            image_size: (image_size.0 as isize, image_size.1 as isize),
            blending_info: frame_header.blending_info.clone(),
            ec_blending_info,
            ec_patch_blending_info,
            extra_channels: file_header.image_metadata.extra_channel_info.clone(),
            reference_frames,
            zeros: vec![0f32; image_size.0],
        })
    }
}

impl std::fmt::Display for BlendingStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "blending")
    }
}

impl RenderPipelineInPlaceStage for BlendingStage {
    type Type = f32;

    fn uses_channel(&self, c: usize) -> bool {
        c < 3 + self.extra_channels.len()
    }

    fn init_local_state(&self) -> Result<Option<Box<ErasedLocalState>>> {
        Ok(Some(Box::new(BlendingScratch::new())))
    }

    fn process_row_chunk(
        &self,
        position: (usize, usize),
        xsize: usize,
        row: &mut [&mut [f32]],
        state: Option<&mut ErasedLocalState>,
        _previous_call_was_previous_row: bool,
    ) {
        let num_ec = self.extra_channels.len();
        let fg_y0 = self.frame_origin.1 + position.1 as isize;
        let mut fg_x0 = self.frame_origin.0 + position.0 as isize;
        let mut fg_x1 = fg_x0 + xsize as isize;
        let mut bg_x0: isize = 0;
        let mut bg_x1: isize = xsize as isize;

        if fg_x1 <= 0 || fg_x0 >= self.image_size.0 || fg_y0 < 0 || fg_y0 >= self.image_size.1 {
            return;
        }

        if fg_x0 < 0 {
            bg_x0 -= fg_x0;
            fg_x0 = 0;
        }
        if fg_x1 > self.image_size.0 {
            bg_x1 = bg_x0 + self.image_size.0 - fg_x0;
            fg_x1 = self.image_size.0;
        }

        let fg_x0: usize = fg_x0 as usize;
        let fg_x1: usize = fg_x1 as usize;
        let bg_x0: usize = bg_x0 as usize;
        let bg_x1: usize = bg_x1 as usize;
        let fg_y0: usize = fg_y0 as usize;

        let total_channels = 3 + num_ec;

        let zeros_slice: &[f32] = self.zeros.as_slice();
        let mut fg_buf: ChannelVec<&[f32]> = ChannelVec::new();
        for _ in 0..total_channels {
            fg_buf.push(zeros_slice);
        }
        if let Some(ref rf) = self.reference_frames[self.blending_info.source as usize] {
            for (c, fg) in fg_buf.iter_mut().enumerate().take(3) {
                *fg = &rf.frame[c].row(fg_y0)[fg_x0..fg_x1];
            }
        }
        for i in 0..num_ec {
            if let Some(ref rf) = self.reference_frames[self.ec_blending_info[i].source as usize] {
                fg_buf[3 + i] = &rf.frame[3 + i].row(fg_y0)[fg_x0..fg_x1];
            }
        }

        let blending_info = PatchBlending::from(&self.blending_info);

        // Per-channel mutable slices into the in-place row buffer.
        let mut bg_slices: ChannelVec<&mut [f32]> = ChannelVec::new();
        for r in row[..total_channels].iter_mut() {
            bg_slices.push(&mut r[bg_x0..bg_x1]);
        }

        let state = state.unwrap().downcast_mut::<BlendingScratch>().unwrap();

        perform_blending(
            &mut bg_slices,
            &fg_buf,
            &blending_info,
            &self.ec_patch_blending_info[..num_ec],
            &self.extra_channels,
            &mut state.scratch,
        );
    }
}

#[cfg(test)]
mod test {
    use rand::SeedableRng;
    use test_log::test;

    use super::*;
    use crate::error::Result;
    use crate::tests::decode::read_headers_and_toc;

    #[test]
    fn blending_consistency() -> Result<()> {
        let (file_header, frame_header, _) =
            read_headers_and_toc(include_bytes!("../../../resources/test/basic.jxl")).unwrap();
        let image_size = (
            file_header.size.xsize() as usize,
            file_header.size.ysize() as usize,
        );
        let mut rng = rand_xorshift::XorShiftRng::seed_from_u64(0);
        let mut reference_frame =
            || ReferenceFrame::random(&mut rng, image_size.0, image_size.1, 4, false);
        let reference_frames = Arc::new([
            Some(reference_frame()?),
            Some(reference_frame()?),
            Some(reference_frame()?),
            Some(reference_frame()?),
        ]);
        crate::render::test::test_stage_consistency(
            || BlendingStage::new(&frame_header, &file_header, reference_frames.clone()).unwrap(),
            (500, 500),
            4,
        )
    }

    #[test]
    fn reject_blending_with_pre_color_transform_reference() -> Result<()> {
        let (file_header, mut frame_header, _) =
            read_headers_and_toc(include_bytes!("../../../resources/test/basic.jxl")).unwrap();
        frame_header.blending_info.source = 3;
        let image_size = (
            file_header.size.xsize() as usize,
            file_header.size.ysize() as usize,
        );
        let reference_frames = Arc::new([
            None,
            None,
            None,
            Some(ReferenceFrame::blank(image_size.0, image_size.1, 4, true)?),
        ]);

        let result = BlendingStage::new(&frame_header, &file_header, reference_frames);
        assert!(matches!(result, Err(Error::BlendingPreColorTransform(3))));
        Ok(())
    }

    #[test]
    fn reject_extra_channel_blending_with_pre_color_transform_reference() -> Result<()> {
        let (file_header, mut frame_header, _) =
            read_headers_and_toc(include_bytes!("../../../resources/test/basic.jxl")).unwrap();
        let mut ec_blending_info = frame_header.blending_info.clone();
        ec_blending_info.source = 2;
        frame_header.ec_blending_info.push(ec_blending_info);
        let image_size = (
            file_header.size.xsize() as usize,
            file_header.size.ysize() as usize,
        );
        let reference_frames = Arc::new([
            None,
            None,
            Some(ReferenceFrame::blank(image_size.0, image_size.1, 4, true)?),
            None,
        ]);

        let result = BlendingStage::new(&frame_header, &file_header, reference_frames);
        assert!(matches!(result, Err(Error::BlendingPreColorTransform(2))));
        Ok(())
    }
}
