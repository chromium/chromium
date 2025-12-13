// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use num_derive::FromPrimitive;
use num_traits::FromPrimitive;

use crate::{
    bit_reader::BitReader,
    entropy_coding::decode::Histograms,
    entropy_coding::decode::SymbolReader,
    error::{Error, Result},
    features::blending::perform_blending,
    frame::{DecoderState, ReferenceFrame},
    headers::extra_channels::ExtraChannelInfo,
    util::{NewWithCapacity, slice, tracing_wrappers::*},
};

// Context numbers as specified in Section C.4.5, Listing C.2:
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
#[repr(usize)]
pub enum PatchContext {
    NumRefPatch = 0,
    ReferenceFrame = 1,
    PatchSize = 2,
    PatchReferencePosition = 3,
    PatchPosition = 4,
    PatchBlendMode = 5,
    PatchOffset = 6,
    PatchCount = 7,
    PatchAlphaChannel = 8,
    PatchClamp = 9,
}

impl PatchContext {
    const NUM: usize = 10;
}

/// Blend modes
#[derive(Debug, PartialEq, Eq, Clone, Copy, FromPrimitive)]
#[repr(u8)]
pub enum PatchBlendMode {
    // The new values are the old ones. Useful to skip some channels.
    None = 0,
    // The new values (in the crop) replace the old ones: sample = new
    Replace = 1,
    // The new values (in the crop) get added to the old ones: sample = old + new
    Add = 2,
    // The new values (in the crop) get multiplied by the old ones:
    // sample = old * new
    // This blend mode is only supported if BlendColorSpace is kEncoded. The
    // range of the new value matters for multiplication purposes, and its
    // nominal range of 0..1 is computed the same way as this is done for the
    // alpha values in kBlend and kAlphaWeightedAdd.
    Mul = 3,
    // The new values (in the crop) replace the old ones if alpha>0:
    // For first alpha channel:
    // alpha = old + new * (1 - old)
    // For other channels if !alpha_associated:
    // sample = ((1 - new_alpha) * old * old_alpha + new_alpha * new) / alpha
    // For other channels if alpha_associated:
    // sample = (1 - new_alpha) * old + new
    // The alpha formula applies to the alpha used for the division in the other
    // channels formula, and applies to the alpha channel itself if its
    // blend_channel value matches itself.
    // If using kBlendAbove, new is the patch and old is the original image; if
    // using kBlendBelow, the meaning is inverted.
    BlendAbove = 4,
    BlendBelow = 5,
    // The new values (in the crop) are added to the old ones if alpha>0:
    // For first alpha channel: sample = sample = old + new * (1 - old)
    // For other channels: sample = old + alpha * new
    AlphaWeightedAddAbove = 6,
    AlphaWeightedAddBelow = 7,
}

impl PatchBlendMode {
    pub const NUM_BLEND_MODES: u8 = 8;

    #[cfg(test)]
    fn try_from(i: u8) -> Result<PatchBlendMode> {
        match i {
            0 => Ok(PatchBlendMode::None),
            1 => Ok(PatchBlendMode::Replace),
            2 => Ok(PatchBlendMode::Add),
            3 => Ok(PatchBlendMode::Mul),
            4 => Ok(PatchBlendMode::BlendAbove),
            5 => Ok(PatchBlendMode::BlendBelow),
            6 => Ok(PatchBlendMode::AlphaWeightedAddAbove),
            7 => Ok(PatchBlendMode::AlphaWeightedAddBelow),
            _ => Err(Error::PatchesInvalidBlendMode(
                i,
                PatchBlendMode::NUM_BLEND_MODES,
            )),
        }
    }

    #[cfg(test)]
    fn random<R: rand::Rng>(rng: &mut R) -> Self {
        use rand::distr::{Distribution, Uniform};
        Self::try_from(
            Uniform::new_inclusive(
                PatchBlendMode::None as u8,
                PatchBlendMode::AlphaWeightedAddBelow as u8,
            )
            .unwrap()
            .sample(rng),
        )
        .unwrap()
    }

    pub fn uses_alpha(self) -> bool {
        matches!(
            self,
            Self::BlendAbove
                | Self::BlendBelow
                | Self::AlphaWeightedAddAbove
                | Self::AlphaWeightedAddBelow
        )
    }

    pub fn uses_clamp(self) -> bool {
        self.uses_alpha() || self == Self::Mul
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PatchBlending {
    pub mode: PatchBlendMode,
    pub alpha_channel: usize,
    pub clamp: bool,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PatchReferencePosition {
    // Not using `ref` like in the spec here, because it is a keyword.
    reference: usize,
    x0: usize,
    y0: usize,
    xsize: usize,
    ysize: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PatchPosition {
    x: usize,
    y: usize,
    ref_pos_idx: usize,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
struct PatchTreeNode {
    left_child: isize,
    right_child: isize,
    y_center: usize,
    start: usize,
    num: usize,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct PatchesDictionary {
    pub positions: Vec<PatchPosition>,
    pub ref_positions: Vec<PatchReferencePosition>,
    blendings: Vec<PatchBlending>,
    blendings_stride: usize,
    patch_tree: Vec<PatchTreeNode>,
    // Number of patches for each row.
    num_patches: Vec<usize>,
    sorted_patches_y0: Vec<(usize, usize)>,
    sorted_patches_y1: Vec<(usize, usize)>,
}

impl PatchesDictionary {
    #[cfg(test)]
    pub fn random<R: rand::Rng>(
        size: (usize, usize),
        num_extra_channels: usize,
        alpha_channel: usize,
        reference_frames: usize,
        rng: &mut R,
    ) -> Self {
        use rand::distr::{Distribution, Uniform};
        let width_dist = Uniform::new_inclusive(0, size.0 - 1).unwrap();
        let height_dist = Uniform::new_inclusive(0, size.1 - 1).unwrap();
        let num_refs = Uniform::new_inclusive(1, 5).unwrap().sample(rng);
        let ref_dist = Uniform::new_inclusive(0, num_refs - 1).unwrap();
        let ref_frame_dist = Uniform::new_inclusive(0, reference_frames - 1).unwrap();
        let num_patches = Uniform::new_inclusive(num_refs, 10).unwrap().sample(rng);
        let mut result = PatchesDictionary {
            positions: (0..num_patches)
                .map(|_| PatchPosition {
                    x: width_dist.sample(rng),
                    y: height_dist.sample(rng),
                    ref_pos_idx: ref_dist.sample(rng),
                })
                .collect(),
            ref_positions: (0..num_refs)
                .map(|_| {
                    let mut result = PatchReferencePosition {
                        reference: ref_frame_dist.sample(rng),
                        x0: width_dist.sample(rng),
                        y0: height_dist.sample(rng),
                        xsize: 0,
                        ysize: 0,
                    };
                    result.xsize = Uniform::new_inclusive(1, size.0 - result.x0)
                        .unwrap()
                        .sample(rng);
                    result.ysize = Uniform::new_inclusive(1, size.1 - result.y0)
                        .unwrap()
                        .sample(rng);
                    result
                })
                .collect(),
            blendings: (0..num_patches)
                .map(|_| PatchBlending {
                    mode: PatchBlendMode::random(rng),
                    alpha_channel,
                    clamp: Uniform::new_inclusive(0, 1).unwrap().sample(rng) == 0,
                })
                .collect(),
            blendings_stride: num_extra_channels + 1,
            patch_tree: vec![],
            num_patches: vec![],
            sorted_patches_y0: vec![],
            sorted_patches_y1: vec![],
        };
        result.compute_patch_tree().unwrap();
        result
    }

    fn compute_patch_tree(&mut self) -> Result<()> {
        #[derive(Debug, Clone, Copy)]
        struct PatchInterval {
            idx: usize,
            y0: usize,
            y1: usize,
        }

        self.patch_tree.clear();
        self.num_patches.clear();
        self.sorted_patches_y0.clear();
        self.sorted_patches_y1.clear();

        if self.positions.is_empty() {
            return Ok(());
        }

        // Create a y-interval for each patch.
        let mut intervals: Vec<PatchInterval> = Vec::new_with_capacity(self.positions.len())?;
        for (i, pos) in self.positions.iter().enumerate() {
            let ref_pos = self.ref_positions[pos.ref_pos_idx];
            if ref_pos.xsize > 0 && ref_pos.ysize > 0 {
                intervals.push(PatchInterval {
                    idx: i,
                    y0: pos.y,
                    y1: pos.y + self.ref_positions[pos.ref_pos_idx].ysize,
                });
            }
        }

        let intervals_len = intervals.len();
        let sort_by_y0 = |intervals: &mut Vec<PatchInterval>, start: usize, end: usize| {
            intervals[start..end].sort_unstable_by_key(|i| i.y0);
        };
        let sort_by_y1 = |intervals: &mut Vec<PatchInterval>, start: usize, end: usize| {
            intervals[start..end].sort_unstable_by_key(|i| i.y1);
        };

        // Count the number of patches for each row.
        sort_by_y1(&mut intervals, 0, intervals_len);
        self.num_patches
            .resize(intervals.last().map_or(0, |iv| iv.y1), 0); //Safe last()
        for iv in &intervals {
            for y in iv.y0..iv.y1 {
                self.num_patches[y] += 1;
            }
        }

        let root = PatchTreeNode {
            start: 0,
            num: intervals.len(),
            ..Default::default()
        };
        self.patch_tree.push(root);

        let mut next = 0;
        while next < self.patch_tree.len() {
            let node = &mut self.patch_tree[next]; // Borrow mutably *before* accessing fields
            let start = node.start;
            let end = node.start + node.num;

            // Choose the y_center for this node to be the median of interval starts.
            sort_by_y0(&mut intervals, start, end);
            let middle_idx = start + node.num / 2;
            node.y_center = intervals[middle_idx].y0;

            // Divide the intervals in [start, end) into three groups:
            let mut right_start = middle_idx;
            while right_start < end && intervals[right_start].y0 == node.y_center {
                right_start += 1;
            }

            sort_by_y1(&mut intervals, start, right_start);
            let mut left_end = right_start;
            while left_end > start && intervals[left_end - 1].y1 > node.y_center {
                left_end -= 1;
            }

            // Fill in sorted_patches_y0_ and sorted_patches_y1_ for the current node.
            node.num = right_start - left_end;
            node.start = self.sorted_patches_y0.len();

            self.sorted_patches_y1
                .try_reserve(right_start.saturating_sub(left_end))?;
            self.sorted_patches_y0
                .try_reserve(right_start.saturating_sub(left_end))?;
            for i in (left_end..right_start).rev() {
                self.sorted_patches_y1
                    .push((intervals[i].y1, intervals[i].idx));
            }
            sort_by_y0(&mut intervals, left_end, right_start);
            for interval in intervals.iter().take(right_start).skip(left_end) {
                self.sorted_patches_y0.push((interval.y0, interval.idx));
            }

            // Create the left and right nodes (if not empty).
            // We modify left_child/right_child on the *original* node in patch_tree,
            // so we have to do the assignment *before* we push the new nodes.
            self.patch_tree[next].left_child = -1;
            self.patch_tree[next].right_child = -1;

            if left_end > start {
                let mut left = PatchTreeNode::default();
                left.start = start;
                left.num = left_end - left.start;
                self.patch_tree[next].left_child = self.patch_tree.len() as isize;
                self.patch_tree.try_reserve(1)?;
                self.patch_tree.push(left);
            }
            if right_start < end {
                let mut right = PatchTreeNode::default();
                right.start = right_start;
                right.num = end - right.start;
                self.patch_tree[next].right_child = self.patch_tree.len() as isize;
                self.patch_tree.try_reserve(1)?;
                self.patch_tree.push(right);
            }

            next += 1;
        }
        Ok(())
    }

    #[instrument(level = "debug", skip(br), ret, err)]
    pub fn read(
        br: &mut BitReader,
        xsize: usize,
        ysize: usize,
        num_extra_channels: usize,
        reference_frames: &[Option<ReferenceFrame>],
    ) -> Result<PatchesDictionary> {
        let blendings_stride = num_extra_channels + 1;
        let patches_histograms = Histograms::decode(PatchContext::NUM, br, true)?;
        let mut patches_reader = SymbolReader::new(&patches_histograms, br, None)?;
        let num_ref_patch = patches_reader.read_unsigned(
            &patches_histograms,
            br,
            PatchContext::NumRefPatch as usize,
        ) as usize;
        let num_pixels = xsize * ysize;
        let max_ref_patches = 1024 + num_pixels / 4;
        let max_patches = max_ref_patches * 4;
        let max_blending_infos = max_patches * 4;
        if num_ref_patch > max_ref_patches {
            return Err(Error::PatchesTooMany(
                "reference patches".to_string(),
                num_ref_patch,
                max_ref_patches,
            ));
        }
        let mut total_patches = 0;
        let mut next_size = 1;
        let mut positions: Vec<PatchPosition> = Vec::new();
        let mut blendings = Vec::new();
        let mut ref_positions = Vec::new_with_capacity(num_ref_patch)?;
        for _ in 0..num_ref_patch {
            let reference = patches_reader.read_unsigned(
                &patches_histograms,
                br,
                PatchContext::ReferenceFrame as usize,
            ) as usize;
            if reference >= DecoderState::MAX_STORED_FRAMES {
                return Err(Error::PatchesRefTooLarge(
                    reference,
                    DecoderState::MAX_STORED_FRAMES,
                ));
            }

            let x0 = patches_reader.read_unsigned(
                &patches_histograms,
                br,
                PatchContext::PatchReferencePosition as usize,
            ) as usize;
            let y0 = patches_reader.read_unsigned(
                &patches_histograms,
                br,
                PatchContext::PatchReferencePosition as usize,
            ) as usize;
            let ref_pos_xsize = patches_reader.read_unsigned(
                &patches_histograms,
                br,
                PatchContext::PatchSize as usize,
            ) as usize
                + 1;
            let ref_pos_ysize = patches_reader.read_unsigned(
                &patches_histograms,
                br,
                PatchContext::PatchSize as usize,
            ) as usize
                + 1;
            let reference_frame = &reference_frames[reference];
            // TODO(firsching): make sure this check is correct in the presence of downsampled extra channels (also in libjxl).
            match reference_frame {
                None => return Err(Error::PatchesInvalidReference(reference)),
                Some(reference) => {
                    if !reference.saved_before_color_transform {
                        return Err(Error::PatchesPostColorTransform());
                    }
                    if x0 + ref_pos_xsize > reference.frame[0].size().0 {
                        return Err(Error::PatchesInvalidPosition(
                            "x".to_string(),
                            x0,
                            ref_pos_xsize,
                            reference.frame[0].size().0,
                        ));
                    }
                    if y0 + ref_pos_ysize > reference.frame[0].size().1 {
                        return Err(Error::PatchesInvalidPosition(
                            "y".to_string(),
                            y0,
                            ref_pos_ysize,
                            reference.frame[0].size().1,
                        ));
                    }
                }
            }

            let id_count = patches_reader.read_unsigned(
                &patches_histograms,
                br,
                PatchContext::PatchCount as usize,
            ) as usize
                + 1;
            if id_count > max_patches + 1 {
                return Err(Error::PatchesTooMany(
                    "patches".to_string(),
                    id_count,
                    max_patches,
                ));
            }
            total_patches += id_count;

            if total_patches > max_patches {
                return Err(Error::PatchesTooMany(
                    "patches".to_string(),
                    total_patches,
                    max_patches,
                ));
            }

            if next_size < total_patches {
                next_size *= 2;
                next_size = std::cmp::min(next_size, max_patches);
            }
            if next_size * blendings_stride > max_blending_infos {
                return Err(Error::PatchesTooMany(
                    "blending_info".to_string(),
                    total_patches,
                    max_patches,
                ));
            }
            positions.try_reserve(next_size.saturating_sub(positions.len()))?;
            blendings.try_reserve(
                (next_size * PatchBlendMode::NUM_BLEND_MODES as usize)
                    .saturating_sub(blendings.len()),
            )?;

            for i in 0..id_count {
                let mut pos = PatchPosition {
                    x: 0,
                    y: 0,
                    ref_pos_idx: ref_positions.len(),
                };
                if i == 0 {
                    // Read initial position
                    pos.x = patches_reader.read_unsigned(
                        &patches_histograms,
                        br,
                        PatchContext::PatchPosition as usize,
                    ) as usize;
                    pos.y = patches_reader.read_unsigned(
                        &patches_histograms,
                        br,
                        PatchContext::PatchPosition as usize,
                    ) as usize;
                } else {
                    // Read offsets and calculate new position
                    let delta_x = patches_reader.read_signed(
                        &patches_histograms,
                        br,
                        PatchContext::PatchOffset as usize,
                    );
                    if delta_x < 0 && (-delta_x as usize) > positions.last().unwrap().x {
                        return Err(Error::PatchesInvalidDelta(
                            "x".to_string(),
                            positions.last().unwrap().x,
                            delta_x,
                        ));
                    }
                    pos.x = (positions.last().unwrap().x as i32 + delta_x) as usize;

                    let delta_y = patches_reader.read_signed(
                        &patches_histograms,
                        br,
                        PatchContext::PatchOffset as usize,
                    );
                    if delta_y < 0 && (-delta_y as usize) > positions.last().unwrap().y {
                        return Err(Error::PatchesInvalidDelta(
                            "y".to_string(),
                            positions.last().unwrap().y,
                            delta_y,
                        ));
                    }
                    pos.y = (positions.last().unwrap().y as i32 + delta_y) as usize;
                }

                if pos.x + ref_pos_xsize > xsize {
                    return Err(Error::PatchesOutOfBounds(
                        "x".to_string(),
                        pos.x,
                        ref_pos_xsize,
                        xsize,
                    ));
                }
                if pos.y + ref_pos_ysize > ysize {
                    return Err(Error::PatchesOutOfBounds(
                        "y".to_string(),
                        pos.y,
                        ref_pos_ysize,
                        ysize,
                    ));
                }

                for _ in 0..blendings_stride {
                    let mut alpha_channel = 0;
                    let mut clamp = false;
                    let maybe_blend_mode = patches_reader.read_unsigned(
                        &patches_histograms,
                        br,
                        PatchContext::PatchBlendMode as usize,
                    ) as u8;
                    let blend_mode = match PatchBlendMode::from_u8(maybe_blend_mode) {
                        None => {
                            return Err(Error::PatchesInvalidBlendMode(
                                maybe_blend_mode,
                                PatchBlendMode::NUM_BLEND_MODES,
                            ));
                        }
                        Some(blend_mode) => blend_mode,
                    };

                    if PatchBlendMode::uses_alpha(blend_mode) && blendings_stride > 2 {
                        alpha_channel = patches_reader.read_unsigned(
                            &patches_histograms,
                            br,
                            PatchContext::PatchAlphaChannel as usize,
                        ) as usize;
                        if alpha_channel >= num_extra_channels {
                            return Err(Error::PatchesInvalidAlphaChannel(
                                alpha_channel,
                                num_extra_channels,
                            ));
                        }
                    }

                    if PatchBlendMode::uses_clamp(blend_mode) {
                        clamp = patches_reader.read_unsigned(
                            &patches_histograms,
                            br,
                            PatchContext::PatchClamp as usize,
                        ) != 0;
                    }
                    blendings.push(PatchBlending {
                        mode: blend_mode,
                        alpha_channel,
                        clamp,
                    });
                }
                positions.push(pos);
            }

            ref_positions.push(PatchReferencePosition {
                reference,
                x0,
                y0,
                xsize: ref_pos_xsize,
                ysize: ref_pos_ysize,
            })
        }

        let mut patches_dict = PatchesDictionary {
            positions,
            blendings,
            ref_positions,
            blendings_stride,
            num_patches: vec![],
            sorted_patches_y0: vec![],
            sorted_patches_y1: vec![],
            patch_tree: vec![],
        };
        patches_dict.compute_patch_tree()?;
        Ok(patches_dict)
    }

    pub fn set_patches_for_row(&self, y: usize, patches_for_row_result: &mut Vec<usize>) {
        patches_for_row_result.clear();
        if self.num_patches.len() <= y || self.num_patches[y] == 0 {
            return;
        }

        let mut tree_idx: isize = 0;
        loop {
            if tree_idx == -1 {
                break;
            }

            // Safe access using get() and unwrap_or().  No need for the assert.
            let node = self.patch_tree.get(tree_idx as usize).unwrap_or_else(|| {
                // TODO(firsching): Handle panic differently?
                panic!("Invalid tree_idx: {tree_idx}");
            });

            if y <= node.y_center {
                for i in 0..node.num {
                    let p = self.sorted_patches_y0[node.start + i];
                    if y < p.0 {
                        break;
                    }
                    patches_for_row_result.push(p.1);
                }
                tree_idx = if y < node.y_center {
                    node.left_child
                } else {
                    -1
                };
            } else {
                for i in 0..node.num {
                    let p = self.sorted_patches_y1[node.start + i];
                    if y >= p.0 {
                        break;
                    }
                    patches_for_row_result.push(p.1);
                }
                tree_idx = node.right_child;
            }
        }

        // Ensure that the relative order of patches is preserved.
        patches_for_row_result.sort();
    }

    pub fn add_one_row(
        &self,
        row: &mut [&mut [f32]],
        row_pos: (usize, usize),
        xsize: usize,
        extra_channel_info: &[ExtraChannelInfo],
        reference_frames: &[Option<ReferenceFrame>],
        patches_for_row_result: &mut Vec<usize>,
    ) {
        // TODO(zond): Allocate a buffer for this when building the stage instead of when executing it.
        let mut out = row
            .iter_mut()
            .map(|s| &mut s[..xsize])
            .collect::<Vec<&mut [f32]>>();
        let num_ec = extra_channel_info.len();
        assert!(num_ec + 1 == self.blendings_stride);
        let dummy_fg = vec![0f32];
        let mut fg = vec![dummy_fg.as_slice(); 3 + num_ec];
        self.set_patches_for_row(row_pos.1, &mut *patches_for_row_result);
        for pos_idx in patches_for_row_result.iter() {
            let pos = &self.positions[*pos_idx];
            assert!(row_pos.1 >= pos.y); // assert patch starts at or before current row
            if pos.x >= row_pos.0 + out[0].len() {
                // if patch starts before end of current chunk, continue
                continue;
            }

            let ref_pos = &self.ref_positions[pos.ref_pos_idx];
            assert!(pos.y + ref_pos.ysize > row_pos.1); // assert patch ends after current row
            if pos.x + ref_pos.xsize < row_pos.0 {
                // if patch ends before current chunk, continue
                continue;
            }

            let (ref_x0, out_x0, ref_xsize) = if pos.x < row_pos.0 {
                // if patch starts before current chunk
                // crop the first part of the patch and use the first part of the chunk
                (
                    ref_pos.x0 + row_pos.0 - pos.x,
                    0,
                    ref_pos.xsize + pos.x - row_pos.0,
                )
            } else {
                // otherwise
                // use the first part of the patch and crop the first part of the chunk
                (ref_pos.x0, pos.x - row_pos.0, ref_pos.xsize)
            };
            let (ref_x1, out_x1) = if out[0].len() - out_x0 < ref_xsize {
                // if rest of chunk is smaller than patch
                // crop the last part of the patch and use the last part of the chunk
                (ref_x0 + out[0].len() - out_x0, out[0].len())
            } else {
                // otherwise
                // use the last part of the patch and crop the last part of the chunk
                (ref_x0 + ref_xsize, out_x0 + ref_xsize)
            };
            let ref_pos_y = ref_pos.y0 + row_pos.1 - pos.y;

            for (c, fg_ptr) in fg.iter_mut().enumerate().take(3) {
                *fg_ptr = &(reference_frames[ref_pos.reference].as_ref().unwrap().frame[c]
                    .row(ref_pos_y)[ref_x0..ref_x1]);
            }
            for i in 0..num_ec {
                fg[3 + i] = &(reference_frames[ref_pos.reference].as_ref().unwrap().frame[3 + i]
                    .row(ref_pos_y)[ref_x0..ref_x1]);
            }

            let blending_idx = pos_idx * self.blendings_stride;
            perform_blending(
                &mut slice!(&mut out, .., out_x0..out_x1),
                &fg,
                &self.blendings[blending_idx],
                &self.blendings[blending_idx + 1..],
                extra_channel_info,
            );
        }
    }
}

#[cfg(test)]
mod tests {

    mod read_patches_tests {
        use super::super::*;
        use test_log::test;

        #[test]
        fn read_single_patch_dict() -> Result<()> {
            let mut br = BitReader::new(&[0x12, 0x4a, 0x8c, 0x63, 0x13, 0x01, 0xa6, 0x53, 0x01]);
            let got_dict = PatchesDictionary::read(
                &mut br,
                1024,
                1024,
                0,
                &[Some(ReferenceFrame::blank(1024, 1024, 1, true).unwrap())],
            )?;
            let want_dict = PatchesDictionary {
                positions: vec![PatchPosition {
                    x: 10,
                    y: 20,
                    ref_pos_idx: 0,
                }],
                ref_positions: vec![PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 1,
                    ysize: 1,
                }],
                blendings: vec![PatchBlending {
                    mode: PatchBlendMode::Add,
                    alpha_channel: 0,
                    clamp: false,
                }],
                blendings_stride: 1,
                num_patches: vec![
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                ],
                patch_tree: vec![PatchTreeNode {
                    left_child: -1,
                    right_child: -1,
                    y_center: 20,
                    start: 0,
                    num: 1,
                }],
                sorted_patches_y0: vec![(20, 0)],
                sorted_patches_y1: vec![(21, 0)],
            };
            assert_eq!(got_dict, want_dict);
            Ok(())
        }

        #[test]
        fn read_multi_patch_dict() -> Result<()> {
            let mut br = BitReader::new(&[
                0x12, 0xc6, 0x26, 0x3f, 0x08, 0x4e, 0xb6, 0x0d, 0xf2, 0xde, 0xb6, 0x6d,
            ]);
            let got_dict = PatchesDictionary::read(
                &mut br,
                1024,
                1024,
                2,
                &[Some(ReferenceFrame::blank(1024, 1024, 1, true).unwrap())],
            )?;
            let want_dict = PatchesDictionary {
                positions: vec![
                    PatchPosition {
                        x: 0,
                        y: 0,
                        ref_pos_idx: 0,
                    },
                    PatchPosition {
                        x: 5,
                        y: 5,
                        ref_pos_idx: 1,
                    },
                ],
                ref_positions: vec![
                    PatchReferencePosition {
                        reference: 0,
                        x0: 0,
                        y0: 0,
                        xsize: 2,
                        ysize: 1,
                    },
                    PatchReferencePosition {
                        reference: 0,
                        x0: 0,
                        y0: 0,
                        xsize: 1,
                        ysize: 2,
                    },
                ],
                blendings: vec![
                    PatchBlending {
                        mode: PatchBlendMode::BlendAbove,
                        alpha_channel: 1,
                        clamp: false,
                    },
                    PatchBlending {
                        mode: PatchBlendMode::Mul,
                        alpha_channel: 0,
                        clamp: true,
                    },
                    PatchBlending {
                        mode: PatchBlendMode::Mul,
                        alpha_channel: 0,
                        clamp: true,
                    },
                    PatchBlending {
                        mode: PatchBlendMode::Mul,
                        alpha_channel: 0,
                        clamp: true,
                    },
                    PatchBlending {
                        mode: PatchBlendMode::Mul,
                        alpha_channel: 0,
                        clamp: true,
                    },
                    PatchBlending {
                        mode: PatchBlendMode::Mul,
                        alpha_channel: 0,
                        clamp: true,
                    },
                ],
                blendings_stride: 3,
                num_patches: vec![1, 0, 0, 0, 0, 1, 1],
                patch_tree: vec![
                    PatchTreeNode {
                        left_child: 1,
                        right_child: -1,
                        y_center: 5,
                        start: 0,
                        num: 1,
                    },
                    PatchTreeNode {
                        left_child: -1,
                        right_child: -1,
                        y_center: 0,
                        start: 1,
                        num: 1,
                    },
                ],
                sorted_patches_y0: vec![(5, 1), (0, 0)],
                sorted_patches_y1: vec![(7, 1), (1, 0)],
            };
            assert_eq!(got_dict, want_dict);
            Ok(())
        }

        #[test]
        fn read_large_patch_dict() -> Result<()> {
            let mut br = BitReader::new(&[
                0x12, 0x4e, 0x50, 0x76, 0xeb, 0x41, 0x0d, 0x7e, 0xe5, 0x8e, 0xd2, 0x5d, 0x01,
            ]);
            let got_dict = PatchesDictionary::read(
                &mut br,
                1024,
                1024,
                1,
                &[Some(ReferenceFrame::blank(1024, 1024, 1, true).unwrap())],
            )?;
            let want_dict = PatchesDictionary {
                positions: vec![PatchPosition {
                    x: 2,
                    y: 3,
                    ref_pos_idx: 0,
                }],
                ref_positions: vec![PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 300,
                    ysize: 200,
                }],
                blendings: vec![
                    PatchBlending {
                        mode: PatchBlendMode::AlphaWeightedAddBelow,
                        alpha_channel: 0,
                        clamp: false,
                    },
                    PatchBlending {
                        mode: PatchBlendMode::Mul,
                        alpha_channel: 0,
                        clamp: false,
                    },
                ],
                blendings_stride: 2,
                num_patches: vec![
                    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                ],
                patch_tree: vec![PatchTreeNode {
                    left_child: -1,
                    right_child: -1,
                    y_center: 3,
                    start: 0,
                    num: 1,
                }],
                sorted_patches_y0: vec![(3, 0)],
                sorted_patches_y1: vec![(203, 0)],
            };
            assert_eq!(got_dict, want_dict);
            Ok(())
        }

        #[test]
        fn read_clamped_patch_dict() -> Result<()> {
            let mut br = BitReader::new(&[0x12, 0xc6, 0x26, 0x1f, 0x70, 0xce, 0x06]);
            let got_dict = PatchesDictionary::read(
                &mut br,
                1024,
                1024,
                0,
                &[Some(ReferenceFrame::blank(1024, 1024, 1, true).unwrap())],
            )?;
            let want_dict = PatchesDictionary {
                positions: vec![PatchPosition {
                    x: 4,
                    y: 4,
                    ref_pos_idx: 0,
                }],
                ref_positions: vec![PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 1,
                    ysize: 1,
                }],
                blendings: vec![PatchBlending {
                    mode: PatchBlendMode::Mul,
                    alpha_channel: 0,
                    clamp: true,
                }],
                blendings_stride: 1,
                num_patches: vec![0, 0, 0, 0, 1],
                patch_tree: vec![PatchTreeNode {
                    left_child: -1,
                    right_child: -1,
                    y_center: 4,
                    start: 0,
                    num: 1,
                }],
                sorted_patches_y0: vec![(4, 0)],
                sorted_patches_y1: vec![(5, 0)],
            };
            assert_eq!(got_dict, want_dict);
            Ok(())
        }

        #[test]
        fn read_dup_patch_dict() -> Result<()> {
            let mut br = BitReader::new(&[0x12, 0x0a, 0x8d, 0x88, 0x03, 0x31, 0xd7, 0x35]);
            let got_dict = PatchesDictionary::read(
                &mut br,
                1024,
                1024,
                0,
                &[Some(ReferenceFrame::blank(1024, 1024, 1, true).unwrap())],
            )?;
            let want_dict = PatchesDictionary {
                positions: vec![
                    PatchPosition {
                        x: 0,
                        y: 0,
                        ref_pos_idx: 0,
                    },
                    PatchPosition {
                        x: 5,
                        y: 5,
                        ref_pos_idx: 0,
                    },
                ],
                ref_positions: vec![PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 1,
                    ysize: 1,
                }],
                blendings: vec![
                    PatchBlending {
                        mode: PatchBlendMode::Add,
                        alpha_channel: 0,
                        clamp: false,
                    },
                    PatchBlending {
                        mode: PatchBlendMode::Add,
                        alpha_channel: 0,
                        clamp: false,
                    },
                ],
                blendings_stride: 1,
                num_patches: vec![1, 0, 0, 0, 0, 1],
                patch_tree: vec![
                    PatchTreeNode {
                        left_child: 1,
                        right_child: -1,
                        y_center: 5,
                        start: 0,
                        num: 1,
                    },
                    PatchTreeNode {
                        left_child: -1,
                        right_child: -1,
                        y_center: 0,
                        start: 1,
                        num: 1,
                    },
                ],
                sorted_patches_y0: vec![(5, 1), (0, 0)],
                sorted_patches_y1: vec![(6, 1), (1, 0)],
            };
            assert_eq!(got_dict, want_dict);
            Ok(())
        }
    }

    mod set_patches_for_row_tests {
        use super::super::*;
        use test_log::test;

        // Helper to create a PatchesDictionary for tests
        fn create_dictionary(
            positions: Vec<PatchPosition>,
            ref_positions: Vec<PatchReferencePosition>,
        ) -> PatchesDictionary {
            // Using default/empty blendings for these tests as they don't affect get_patches_for_row
            let mut dict = PatchesDictionary {
                positions,
                ref_positions,
                ..Default::default()
            };
            dict.compute_patch_tree().unwrap();
            dict
        }

        #[test]
        fn test_no_patches() {
            let dict = create_dictionary(vec![], vec![]);
            let mut patches_for_row_result = vec![];
            dict.set_patches_for_row(0, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
            dict.set_patches_for_row(10, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
        }

        #[test]
        fn test_single_patch_hit() {
            let ref_positions = vec![PatchReferencePosition {
                reference: 0,
                x0: 0,
                y0: 0,
                xsize: 10,
                ysize: 5,
            }];
            let positions = vec![PatchPosition {
                x: 0,
                y: 10,
                ref_pos_idx: 0,
            }];
            let dict = create_dictionary(positions, ref_positions);
            let mut patches_for_row_result = vec![];

            // Patch covers rows 10, 11, 12, 13, 14
            dict.set_patches_for_row(10, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]); // First row of patch
            dict.set_patches_for_row(12, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]); // Middle row of patch
            dict.set_patches_for_row(14, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]); // Last row of patch
        }

        #[test]
        fn test_single_patch_miss() {
            let ref_positions = vec![PatchReferencePosition {
                reference: 0,
                x0: 0,
                y0: 0,
                xsize: 10,
                ysize: 5,
            }]; // Covers y=10 to y=14
            let positions = vec![PatchPosition {
                x: 0,
                y: 10,
                ref_pos_idx: 0,
            }];
            let dict = create_dictionary(positions, ref_positions);
            let mut patches_for_row_result = vec![];

            dict.set_patches_for_row(9, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>); // Row before patch
            dict.set_patches_for_row(15, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>); // Row after patch
        }

        #[test]
        fn test_single_patch_height_one() {
            let ref_positions = vec![PatchReferencePosition {
                reference: 0,
                x0: 0,
                y0: 0,
                xsize: 10,
                ysize: 1,
            }]; // Covers y=5 only
            let positions = vec![PatchPosition {
                x: 0,
                y: 5,
                ref_pos_idx: 0,
            }];
            let dict = create_dictionary(positions, ref_positions);
            let mut patches_for_row_result = vec![];

            dict.set_patches_for_row(4, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
            dict.set_patches_for_row(5, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]);
            dict.set_patches_for_row(6, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
        }

        #[test]
        fn test_multiple_patches_non_overlapping() {
            let ref_positions = vec![
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 10,
                    ysize: 3,
                }, // Patch 0: rows 5,6,7
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 10,
                    ysize: 2,
                }, // Patch 1: rows 10,11
            ];
            let positions = vec![
                PatchPosition {
                    x: 0,
                    y: 5,
                    ref_pos_idx: 0,
                },
                PatchPosition {
                    x: 0,
                    y: 10,
                    ref_pos_idx: 1,
                },
            ];
            let dict = create_dictionary(positions, ref_positions);
            let mut patches_for_row_result = vec![];

            dict.set_patches_for_row(4, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
            dict.set_patches_for_row(5, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]);
            dict.set_patches_for_row(7, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]);
            dict.set_patches_for_row(8, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>); // Between patches
            dict.set_patches_for_row(9, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>); // Between patches
            dict.set_patches_for_row(10, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]);
            dict.set_patches_for_row(11, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]);
            dict.set_patches_for_row(12, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
        }

        #[test]
        fn test_multiple_patches_overlapping() {
            let ref_positions = vec![
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 10,
                    ysize: 5,
                }, // Patch 0: rows 10-14
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 10,
                    ysize: 4,
                }, // Patch 1: rows 12-15
            ];
            let positions = vec![
                PatchPosition {
                    x: 0,
                    y: 10,
                    ref_pos_idx: 0,
                }, // idx 0
                PatchPosition {
                    x: 0,
                    y: 12,
                    ref_pos_idx: 1,
                }, // idx 1
            ];
            let dict = create_dictionary(positions, ref_positions);
            let mut patches_for_row_result = vec![];

            dict.set_patches_for_row(10, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]); // Only patch 0
            dict.set_patches_for_row(11, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]); // Only patch 0
            dict.set_patches_for_row(12, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0, 1]); // Both patches (sorted indices)
            dict.set_patches_for_row(13, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0, 1]); // Both patches
            dict.set_patches_for_row(14, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0, 1]); // Patch 0 ends, Patch 1 continues
            dict.set_patches_for_row(15, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]); // Only patch 1
            dict.set_patches_for_row(16, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
        }

        #[test]
        fn test_multiple_patches_adjacent() {
            let ref_positions = vec![
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 10,
                    ysize: 2,
                }, // Patch 0: rows 5,6
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 10,
                    ysize: 3,
                }, // Patch 1: rows 7,8,9
            ];
            let positions = vec![
                PatchPosition {
                    x: 0,
                    y: 5,
                    ref_pos_idx: 0,
                },
                PatchPosition {
                    x: 0,
                    y: 7,
                    ref_pos_idx: 1,
                },
            ];
            let dict = create_dictionary(positions, ref_positions);
            let mut patches_for_row_result = vec![];

            dict.set_patches_for_row(4, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
            dict.set_patches_for_row(5, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]);
            dict.set_patches_for_row(6, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0]);
            dict.set_patches_for_row(7, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]); // Patch 0 ends, Patch 1 starts
            dict.set_patches_for_row(8, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]);
            dict.set_patches_for_row(9, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]);
            dict.set_patches_for_row(10, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
        }

        #[test]
        fn test_multiple_patches_same_start_different_heights() {
            let ref_positions = vec![
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 10,
                    ysize: 2,
                }, // Patch 0: rows 3,4
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 10,
                    ysize: 4,
                }, // Patch 1: rows 3,4,5,6
            ];
            let positions = vec![
                PatchPosition {
                    x: 0,
                    y: 3,
                    ref_pos_idx: 0,
                }, // idx 0
                PatchPosition {
                    x: 0,
                    y: 3,
                    ref_pos_idx: 1,
                }, // idx 1
            ];
            let dict = create_dictionary(positions, ref_positions);
            let mut patches_for_row_result = vec![];

            dict.set_patches_for_row(2, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
            dict.set_patches_for_row(3, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0, 1]); // Both cover
            dict.set_patches_for_row(4, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0, 1]); // Both cover
            dict.set_patches_for_row(5, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]); // Only patch 1 (longer)
            dict.set_patches_for_row(6, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]); // Only patch 1
            dict.set_patches_for_row(7, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
        }

        #[test]
        fn test_patches_out_of_order_definition() {
            // Define patches in a non-sorted order of their y positions
            // get_patches_for_row should still return sorted indices if multiple apply.
            let ref_positions = vec![
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 5,
                    ysize: 3,
                }, // Patch 0 (idx 0): rows 10,11,12
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 5,
                    ysize: 3,
                }, // Patch 1 (idx 1): rows 5,6,7
                PatchReferencePosition {
                    reference: 0,
                    x0: 0,
                    y0: 0,
                    xsize: 5,
                    ysize: 3,
                }, // Patch 2 (idx 2): rows 10,11,12 (overlaps with 0)
            ];
            let positions = vec![
                PatchPosition {
                    x: 0,
                    y: 10,
                    ref_pos_idx: 0,
                }, // Patch 0
                PatchPosition {
                    x: 0,
                    y: 5,
                    ref_pos_idx: 1,
                }, // Patch 1
                PatchPosition {
                    x: 0,
                    y: 10,
                    ref_pos_idx: 2,
                }, // Patch 2
            ];
            let dict = create_dictionary(positions, ref_positions);
            let mut patches_for_row_result = vec![];

            dict.set_patches_for_row(4, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
            dict.set_patches_for_row(5, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]);
            dict.set_patches_for_row(6, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]);
            dict.set_patches_for_row(7, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![1]);
            dict.set_patches_for_row(8, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
            dict.set_patches_for_row(9, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
            dict.set_patches_for_row(10, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0, 2]); // Patches 0 and 2, indices sorted
            dict.set_patches_for_row(11, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0, 2]);
            dict.set_patches_for_row(12, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![0, 2]);
            dict.set_patches_for_row(13, &mut patches_for_row_result);
            assert_eq!(patches_for_row_result, vec![] as Vec<usize>);
        }
    }

    mod add_one_row_tests {
        use super::super::*;
        use crate::{
            headers::{bit_depth::BitDepth, extra_channels::ExtraChannel},
            image::Image,
            util::test::assert_all_almost_abs_eq,
        };

        const MAX_ABS_DELTA: f32 = 1e-6; // Adjusted for typical f32 comparisons

        fn create_reference_frame(
            width: usize,
            height: usize,
            channel_values: &[f32],
        ) -> Result<Option<ReferenceFrame>> {
            let mut frame_channels = Vec::new();
            for v in channel_values.iter() {
                let img = Image::new_with_value((width, height), *v)?;
                frame_channels.push(img);
            }
            Ok(Some(ReferenceFrame {
                frame: frame_channels,
                saved_before_color_transform: true,
            }))
        }

        fn create_reference_frame_single_row<const N: usize>(
            rows: &[[f32; N]],
        ) -> Result<Option<ReferenceFrame>> {
            let mut frame_channels = Vec::new();
            for v in rows.iter() {
                let mut img = Image::new((N, 1))?;
                img.row_mut(0).copy_from_slice(v);
                frame_channels.push(img);
            }
            Ok(Some(ReferenceFrame {
                frame: frame_channels,
                saved_before_color_transform: true,
            }))
        }

        #[test]
        fn test_add_one_row_simple_replace() -> Result<()> {
            let xsize = 10;

            let ref_frames = vec![create_reference_frame(xsize, 1, &[1.0; 3])?];
            let extra_channel_info: Vec<ExtraChannelInfo> = Vec::new();

            let ref_positions = vec![PatchReferencePosition {
                reference: 0, // Points to main_ref_frame
                x0: 2,
                y0: 0,
                xsize: 3,
                ysize: 1,
            }];
            let positions = vec![PatchPosition {
                x: 2,
                y: 0,
                ref_pos_idx: 0,
            }];
            let blendings = vec![PatchBlending {
                mode: PatchBlendMode::Replace,
                alpha_channel: 0,
                clamp: false, // Clamping set to false
            }];
            let mut patches_dict = PatchesDictionary {
                positions,
                ref_positions,
                blendings,
                blendings_stride: 1 + extra_channel_info.len(),
                patch_tree: Vec::new(),
                num_patches: Vec::new(),
                sorted_patches_y0: Vec::new(),
                sorted_patches_y1: Vec::new(),
            };
            patches_dict.compute_patch_tree()?;

            let mut r_data: Vec<f32> = vec![0.0; xsize];
            let mut g_data: Vec<f32> = vec![0.0; xsize];
            let mut b_data: Vec<f32> = vec![0.0; xsize];
            let mut row_slices: Vec<&mut [f32]> = vec![&mut r_data, &mut g_data, &mut b_data];

            let expected_r = vec![0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0];

            patches_dict.add_one_row(
                &mut row_slices,
                (0, 0),
                xsize,
                &extra_channel_info,
                &ref_frames, // Pass the Vec<ReferenceFrame>
                &mut vec![],
            );

            assert_all_almost_abs_eq(&r_data, &expected_r, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&g_data, &expected_r, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&b_data, &expected_r, MAX_ABS_DELTA);
            Ok(())
        }

        #[test]
        fn test_add_one_row_simple_add() -> Result<()> {
            let xsize = 10;
            let y_coord = 0;

            let ref_frames = vec![create_reference_frame(xsize, 1, &[0.2; 3])?];
            let extra_channel_info: Vec<ExtraChannelInfo> = Vec::new();

            let ref_positions = vec![PatchReferencePosition {
                reference: 0,
                x0: 2,
                y0: 0,
                xsize: 3,
                ysize: 1,
            }];
            let positions = vec![PatchPosition {
                x: 2,
                y: 0,
                ref_pos_idx: 0,
            }];
            let blendings = vec![PatchBlending {
                mode: PatchBlendMode::Add,
                alpha_channel: 0,
                clamp: false, // Clamping set to false
            }];
            let mut patches_dict = PatchesDictionary {
                positions,
                ref_positions,
                blendings,
                blendings_stride: 1 + extra_channel_info.len(),
                patch_tree: Vec::new(),
                num_patches: Vec::new(),
                sorted_patches_y0: Vec::new(),
                sorted_patches_y1: Vec::new(),
            };
            patches_dict.compute_patch_tree()?;

            let mut r_data: Vec<f32> = vec![0.5; xsize];
            let mut g_data: Vec<f32> = vec![0.5; xsize];
            let mut b_data: Vec<f32> = vec![0.5; xsize];
            let mut row_slices: Vec<&mut [f32]> = vec![&mut r_data, &mut g_data, &mut b_data];

            let mut expected_r: Vec<f32> = vec![0.5; xsize];
            for r in expected_r.iter_mut().take(5).skip(2) {
                *r = 0.5 + 0.2
            }

            patches_dict.add_one_row(
                &mut row_slices,
                (0, y_coord),
                xsize,
                &extra_channel_info,
                &ref_frames,
                &mut vec![],
            );

            assert_all_almost_abs_eq(&r_data, &expected_r, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&g_data, &expected_r, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&b_data, &expected_r, MAX_ABS_DELTA);
            Ok(())
        }

        #[test]
        fn test_add_one_row_overlapping_replace() -> Result<()> {
            let xsize = 10;
            let y_coord = 0;

            let main_ref_frame1 = create_reference_frame(xsize, 1, &[1.0; 3])?;
            let main_ref_frame2 = create_reference_frame(xsize, 1, &[2.0; 3])?;

            let ref_frames = vec![main_ref_frame1, main_ref_frame2];
            let extra_channel_info: Vec<ExtraChannelInfo> = Vec::new();

            let ref_positions = vec![
                PatchReferencePosition {
                    reference: 0, // Points to main_ref_frame1
                    x0: 0,
                    y0: 0,
                    xsize: 4,
                    ysize: 1,
                },
                PatchReferencePosition {
                    reference: 1, // Points to main_ref_frame2
                    x0: 0,
                    y0: 0,
                    xsize: 3,
                    ysize: 1,
                },
            ];
            let positions = vec![
                PatchPosition {
                    x: 2,
                    y: 0,
                    ref_pos_idx: 0,
                }, // P1: canvas [2..6] with 1.0
                PatchPosition {
                    x: 4,
                    y: 0,
                    ref_pos_idx: 1,
                }, // P2: canvas [4..7] with 2.0
            ];
            let blendings = vec![
                PatchBlending {
                    mode: PatchBlendMode::Replace,
                    alpha_channel: 0,
                    clamp: false, // Clamping set to false
                }, // For P1
                PatchBlending {
                    mode: PatchBlendMode::Replace,
                    alpha_channel: 0,
                    clamp: false, // Clamping set to false
                }, // For P2
            ];
            let mut patches_dict = PatchesDictionary {
                positions,
                ref_positions,
                blendings,
                blendings_stride: 1 + extra_channel_info.len(),
                patch_tree: Vec::new(),
                num_patches: Vec::new(),
                sorted_patches_y0: Vec::new(),
                sorted_patches_y1: Vec::new(),
            };
            patches_dict.compute_patch_tree()?;

            let mut r_data: Vec<f32> = vec![0.0; xsize];
            let mut g_data: Vec<f32> = vec![0.0; xsize];
            let mut b_data: Vec<f32> = vec![0.0; xsize];
            let mut row_slices: Vec<&mut [f32]> = vec![&mut r_data, &mut g_data, &mut b_data];

            let expected_r: Vec<f32> = vec![0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 2.0, 0.0, 0.0, 0.0];

            patches_dict.add_one_row(
                &mut row_slices,
                (0, y_coord),
                xsize,
                &extra_channel_info,
                &ref_frames,
                &mut vec![],
            );

            assert_all_almost_abs_eq(&r_data, &expected_r, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&g_data, &expected_r, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&b_data, &expected_r, MAX_ABS_DELTA);
            Ok(())
        }

        #[test]
        fn test_add_one_row_blend_above_ec_alpha_non_associated() -> Result<()> {
            let xsize = 1;
            let y_coord = 0;

            let initial_color_val = 0.1;
            let initial_ec0_alpha = 0.4;
            let ref_color_val = 0.8;
            let ref_ec0_alpha_val = 0.5;

            let ec_info = vec![ExtraChannelInfo::new(
                true,
                ExtraChannel::Alpha,
                BitDepth::f32(),
                0,
                "AlphaEC".to_string(),
                false, // alpha_associated = false
                None,
                None,
            )];

            let main_ref_frame = create_reference_frame(
                xsize,
                1,
                &[
                    ref_color_val,
                    ref_color_val,
                    ref_color_val,
                    ref_ec0_alpha_val,
                ],
            )?;

            let ref_frames = vec![main_ref_frame];

            let ref_positions = vec![PatchReferencePosition {
                reference: 0,
                x0: 0,
                y0: 0,
                xsize: 1,
                ysize: 1,
            }];
            let positions = vec![PatchPosition {
                x: 0,
                y: 0,
                ref_pos_idx: 0,
            }];
            let blendings = vec![
                PatchBlending {
                    mode: PatchBlendMode::BlendAbove,
                    alpha_channel: 0, // Alpha for color is EC0
                    clamp: false,     // Clamping set to false
                }, // Color
                PatchBlending {
                    mode: PatchBlendMode::BlendAbove,
                    alpha_channel: 0, // Alpha for EC0 is EC0 itself
                    clamp: false,     // Clamping set to false
                }, // EC0
            ];
            let mut patches_dict = PatchesDictionary {
                positions,
                ref_positions,
                blendings,
                blendings_stride: 1 + ec_info.len(), // Color + 1 EC
                patch_tree: Vec::new(),
                num_patches: Vec::new(),
                sorted_patches_y0: Vec::new(),
                sorted_patches_y1: Vec::new(),
            };
            patches_dict.compute_patch_tree()?;

            let mut r_data = vec![initial_color_val; xsize];
            let mut g_data = vec![initial_color_val; xsize];
            let mut b_data = vec![initial_color_val; xsize];
            let mut ec0_data = vec![initial_ec0_alpha; xsize];
            let mut row_slices: Vec<&mut [f32]> =
                vec![&mut r_data, &mut g_data, &mut b_data, &mut ec0_data];

            // Calculations based on C++ logic for non-associated alpha:
            // OutputAlpha = OldAlpha + PatchAlpha * (1 - OldAlpha)
            // OutputColor = (OldColor * OldAlpha * (1 - PatchAlpha) + PatchColor * PatchAlpha) / OutputAlpha
            // (If OutputAlpha is very small, OutputColor is 0)
            let canvas_alpha_val = initial_ec0_alpha; // old_alpha
            let patch_alpha_val = ref_ec0_alpha_val; // ref_alpha

            let expected_ec0 = canvas_alpha_val + patch_alpha_val * (1.0 - canvas_alpha_val);

            let canvas_color_val = initial_color_val; // old_color
            let patch_color_val = ref_color_val; // ref_color (straight)

            let expected_color = if expected_ec0.abs() < 1e-5 {
                // Threshold similar to kSmallAlpha
                0.0
            } else {
                (canvas_color_val * canvas_alpha_val * (1.0 - patch_alpha_val)
                    + patch_color_val * patch_alpha_val)
                    / expected_ec0
            };

            patches_dict.add_one_row(
                &mut row_slices,
                (0, y_coord),
                xsize,
                &ec_info,
                &ref_frames,
                &mut vec![],
            );

            assert_all_almost_abs_eq(&r_data, &vec![expected_color], MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&g_data, &vec![expected_color], MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&b_data, &vec![expected_color], MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&ec0_data, &vec![expected_ec0], MAX_ABS_DELTA);
            Ok(())
        }

        #[test]
        fn test_add_one_row_blend_above_ec_alpha_associated() -> Result<()> {
            let xsize = 1;
            let y_coord = 0;

            let initial_color_val = 0.1; // Canvas color, assumed associated
            let initial_ec0_alpha = 0.4; // Canvas alpha
            let ref_color_val = 0.8; // Patch color, straight (non-associated)
            let ref_ec0_alpha_val = 0.5; // Patch alpha

            let ec_info = vec![ExtraChannelInfo::new(
                true,
                ExtraChannel::Alpha,
                BitDepth::f32(),
                0,
                "AlphaEC".to_string(),
                true, // alpha_associated = true
                None,
                None,
            )];

            let main_ref_frame = create_reference_frame(
                xsize,
                1,
                &[
                    ref_color_val,
                    ref_color_val,
                    ref_color_val,
                    ref_ec0_alpha_val,
                ],
            )?;

            let ref_frames = vec![main_ref_frame];

            let ref_positions = vec![PatchReferencePosition {
                reference: 0,
                x0: 0,
                y0: 0,
                xsize: 1,
                ysize: 1,
            }];
            let positions = vec![PatchPosition {
                x: 0,
                y: 0,
                ref_pos_idx: 0,
            }];
            let blendings = vec![
                PatchBlending {
                    mode: PatchBlendMode::BlendAbove,
                    alpha_channel: 0,
                    clamp: false, // Clamping set to false
                }, // Color
                PatchBlending {
                    mode: PatchBlendMode::BlendAbove,
                    alpha_channel: 0,
                    clamp: false, // Clamping set to false
                }, // EC0
            ];
            let mut patches_dict = PatchesDictionary {
                positions,
                ref_positions,
                blendings,
                blendings_stride: 1 + ec_info.len(),
                patch_tree: Vec::new(),
                num_patches: Vec::new(),
                sorted_patches_y0: Vec::new(),
                sorted_patches_y1: Vec::new(),
            };
            patches_dict.compute_patch_tree()?;

            let mut r_data = vec![initial_color_val; xsize];
            let mut g_data = vec![initial_color_val; xsize];
            let mut b_data = vec![initial_color_val; xsize];
            let mut ec0_data = vec![initial_ec0_alpha; xsize];
            let mut row_slices: Vec<&mut [f32]> =
                vec![&mut r_data, &mut g_data, &mut b_data, &mut ec0_data];

            let expected_ec0 = ref_ec0_alpha_val + initial_ec0_alpha * (1.0 - ref_ec0_alpha_val);

            let expected_color = ref_color_val + initial_color_val * (1.0 - ref_ec0_alpha_val);

            patches_dict.add_one_row(
                &mut row_slices,
                (0, y_coord),
                xsize,
                &ec_info,
                &ref_frames,
                &mut vec![],
            );

            assert_all_almost_abs_eq(&ec0_data, &vec![expected_ec0], MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&r_data, &vec![expected_color], MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&g_data, &vec![expected_color], MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&b_data, &vec![expected_color], MAX_ABS_DELTA);
            Ok(())
        }

        #[test]
        fn test_add_one_row_mul_blend() -> Result<()> {
            let xsize = 2;
            let y_coord = 0;

            let initial_vals = [0.5, 2.0];
            let ref_vals = [0.8, 0.7];

            let main_ref_frame = create_reference_frame_single_row(&[ref_vals; 3])?;
            let dummy_ref_frame1 = create_reference_frame_single_row(&[[0.0; 2]; 3])?;
            let dummy_ref_frame2 = create_reference_frame_single_row(&[[0.0; 2]; 3])?;

            let ref_frames = vec![main_ref_frame, dummy_ref_frame1, dummy_ref_frame2];
            let extra_channel_info: Vec<ExtraChannelInfo> = Vec::new();

            let ref_positions = vec![PatchReferencePosition {
                reference: 0,
                x0: 0,
                y0: 0,
                xsize,
                ysize: 1,
            }];
            let positions = vec![PatchPosition {
                x: 0,
                y: 0,
                ref_pos_idx: 0,
            }];

            // Test Mul (always without clamp as per new instruction)
            let blendings = vec![PatchBlending {
                mode: PatchBlendMode::Mul,
                alpha_channel: 0,
                clamp: false, // Clamping set to false
            }];
            let mut dict = PatchesDictionary {
                positions: positions.clone(),
                ref_positions: ref_positions.clone(),
                blendings,
                blendings_stride: 1, // Only color channels
                patch_tree: Vec::new(),
                num_patches: Vec::new(),
                sorted_patches_y0: Vec::new(),
                sorted_patches_y1: Vec::new(),
            };
            dict.compute_patch_tree()?;

            let mut r_data = initial_vals;
            let mut g_data = initial_vals;
            let mut b_data = initial_vals;
            let mut slices: Vec<&mut [f32]> = vec![&mut r_data, &mut g_data, &mut b_data];
            dict.add_one_row(
                &mut slices,
                (0, y_coord),
                xsize,
                &extra_channel_info,
                &ref_frames,
                &mut vec![],
            );

            let expected_vals = [0.5 * 0.8, 2.0 * 0.7]; // [0.4, 1.4]
            assert_all_almost_abs_eq(&r_data, &expected_vals, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&g_data, &expected_vals, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&b_data, &expected_vals, MAX_ABS_DELTA);

            Ok(())
        }

        #[test]
        fn test_add_one_row_none_blend() -> Result<()> {
            let xsize = 5;
            let y_coord = 0;

            let main_ref_frame = create_reference_frame(xsize, 1, &[100.0; 3])?;
            let dummy_ref_frame1 = create_reference_frame(xsize, 1, &[0.0; 3])?;
            let dummy_ref_frame2 = create_reference_frame(xsize, 1, &[0.0; 3])?;

            let ref_frames = vec![main_ref_frame, dummy_ref_frame1, dummy_ref_frame2];
            let extra_channel_info: Vec<ExtraChannelInfo> = Vec::new();

            let ref_positions = vec![PatchReferencePosition {
                reference: 0,
                x0: 0,
                y0: 0,
                xsize: 3,
                ysize: 1,
            }];
            let positions = vec![PatchPosition {
                x: 1,
                y: 0,
                ref_pos_idx: 0,
            }];
            let blendings = vec![PatchBlending {
                mode: PatchBlendMode::None,
                alpha_channel: 0,
                clamp: false, // Clamping set to false
            }];

            let mut patches_dict = PatchesDictionary {
                positions,
                ref_positions,
                blendings,
                blendings_stride: 1, // Only color channels
                patch_tree: Vec::new(),
                num_patches: Vec::new(),
                sorted_patches_y0: Vec::new(),
                sorted_patches_y1: Vec::new(),
            };
            patches_dict.compute_patch_tree()?;

            let initial_data: Vec<f32> = (0..xsize).map(|i| i as f32 * 0.1 + 0.05).collect();
            let mut r_data = initial_data.clone();
            let mut g_data = initial_data.clone();
            let mut b_data = initial_data.clone();
            let mut row_slices: Vec<&mut [f32]> = vec![&mut r_data, &mut g_data, &mut b_data];

            patches_dict.add_one_row(
                &mut row_slices,
                (0, y_coord),
                xsize,
                &extra_channel_info,
                &ref_frames,
                &mut vec![],
            );

            assert_all_almost_abs_eq(&r_data, &initial_data, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&g_data, &initial_data, MAX_ABS_DELTA);
            assert_all_almost_abs_eq(&b_data, &initial_data, MAX_ABS_DELTA);
            Ok(())
        }
    }
}
