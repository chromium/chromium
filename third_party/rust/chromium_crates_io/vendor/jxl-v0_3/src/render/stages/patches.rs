// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{any::Any, sync::Arc};

use crate::{
    features::patches::PatchesDictionary, frame::ReferenceFrame,
    headers::extra_channels::ExtraChannelInfo, render::RenderPipelineInPlaceStage,
    util::NewWithCapacity as _,
};

pub struct PatchesStage {
    pub patches: Arc<PatchesDictionary>,
    pub extra_channels: Vec<ExtraChannelInfo>,
    pub decoder_state: Arc<[Option<ReferenceFrame>; 4]>,
}

impl std::fmt::Display for PatchesStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "patches")
    }
}

impl RenderPipelineInPlaceStage for PatchesStage {
    type Type = f32;

    fn uses_channel(&self, c: usize) -> bool {
        c < 3 + self.extra_channels.len()
    }

    fn process_row_chunk(
        &self,
        position: (usize, usize),
        xsize: usize,
        row: &mut [&mut [f32]],
        state: Option<&mut dyn Any>,
    ) {
        let state: &mut Vec<usize> = state.unwrap().downcast_mut().unwrap();
        self.patches.add_one_row(
            row,
            position,
            xsize,
            &self.extra_channels,
            &self.decoder_state[..],
            state,
        );
    }

    fn init_local_state(&self, _thread_index: usize) -> crate::error::Result<Option<Box<dyn Any>>> {
        let patches_for_row_result = Vec::<usize>::new_with_capacity(self.patches.positions.len())?;
        Ok(Some(Box::new(patches_for_row_result) as Box<dyn Any>))
    }
}

#[cfg(test)]
mod test {
    use std::sync::Arc;

    use rand::SeedableRng;
    use test_log::test;

    use super::*;
    use crate::error::Result;
    use crate::util::test::read_headers_and_toc;

    #[test]
    fn patches_consistency() -> Result<()> {
        let (file_header, _, _) =
            read_headers_and_toc(include_bytes!("../../../resources/test/basic.jxl")).unwrap();
        let mut rng = rand_xorshift::XorShiftRng::seed_from_u64(0);
        let patch_dict = Arc::new(PatchesDictionary::random(
            (500, 500),
            file_header.image_metadata.extra_channel_info.len(),
            0,
            4,
            &mut rng,
        ));
        let reference_frames = Arc::new([
            Some(ReferenceFrame::random(&mut rng, 500, 500, 4, false)?),
            Some(ReferenceFrame::random(&mut rng, 500, 500, 4, false)?),
            Some(ReferenceFrame::random(&mut rng, 500, 500, 4, false)?),
            Some(ReferenceFrame::random(&mut rng, 500, 500, 4, false)?),
        ]);
        crate::render::test::test_stage_consistency(
            || PatchesStage {
                patches: patch_dict.clone(),
                extra_channels: file_header.image_metadata.extra_channel_info.clone(),
                decoder_state: reference_frames.clone(),
            },
            (500, 500),
            4,
        )
    }
}
