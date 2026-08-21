// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::channel::decode_modular_channel;
use crate::{
    bit_reader::BitReader,
    entropy_coding::decode::{Codes, SymbolReader, unpack_signed},
    error::{Error, Result},
    frame::modular::{
        IMAGE_OFFSET, ModularChannel, Predictor, Tree, predict::clamped_gradient,
        transforms::apply_local::meta_apply_local_transforms, tree::TreeNode,
    },
    headers::{JxlHeader, modular::GroupHeader},
};

// If we have at least this many bits still available to read,
// we can be sure that none of the reads up to this point read garbage.
const DECODE_SAFETY_MARGIN: usize = 32;

fn can_decode_fast_lossless(tree: &Tree) -> bool {
    if !tree.nodes.iter().all(|n| {
        matches!(
            n,
            TreeNode::Split { property: 0, .. }
                | TreeNode::Leaf {
                    predictor: Predictor::Gradient,
                    offset: 0,
                    multiplier: 1,
                    ..
                }
        )
    }) {
        return false;
    }

    if !tree.histograms.is_rle() {
        return false;
    }

    matches!(tree.histograms.codes(), Codes::Huffman(_))
}

#[inline(never)]
fn decode_fast_lossless(
    buffers: Vec<&mut ModularChannel>,
    tree: &Tree,
    br: &mut BitReader,
    partial_decoded_buffers: Option<&mut usize>,
) -> Result<()> {
    let mut rle_len: usize = 0;
    let mut rle_sym = 0;

    let mut last_safe_buf = 0;
    for (c, buf) in buffers.into_iter().enumerate() {
        let (w, h) = buf.data.size();
        if w == 0 || h == 0 {
            continue;
        }
        let TreeNode::Leaf { id, .. } = tree.walk(&[c as i32]) else {
            unreachable!();
        };
        let Codes::Huffman(codes) = tree.histograms.codes() else {
            unreachable!();
        };

        if br.total_bits_available() >= DECODE_SAFETY_MARGIN {
            last_safe_buf = c;
        }

        let table = codes.table(tree.histograms.map_context_to_cluster(id as usize));
        let min_sym = tree.histograms.lz77_params().min_symbol.unwrap();
        let min_len = tree.histograms.lz77_params().min_length.unwrap() as usize;
        let sym_conf = tree
            .histograms
            .uint(tree.histograms.map_context_to_cluster(id as usize));
        let lz_conf = tree.histograms.lz77_length_uint();

        let buf = &mut buf.data;

        let mut decode = {
            #[inline(always)]
            || {
                if rle_len > 0 {
                    rle_len -= 1;
                } else {
                    let sym = table.read(br);
                    if let Some(token) = sym.checked_sub(min_sym) {
                        let count = lz_conf.read(token, br) as usize;
                        // If this calculation overflows, the bitstream is invalid (it would be rejected
                        // on the LZ77 path), but we don't report an error.
                        rle_len = count.wrapping_add(min_len - 1);
                    } else {
                        rle_sym = unpack_signed(sym_conf.read(sym, br));
                    }
                };
                rle_sym
            }
        };

        let mut last = 0i32;
        for p in buf.row_mut(0) {
            // clamped gradient == left on the first row.
            *p = last.wrapping_add(decode());
            last = *p;
        }

        for y in 1..h {
            let [row, row_top] =
                buf.distinct_full_rows_mut([y + IMAGE_OFFSET.1, y + IMAGE_OFFSET.1 - 1]);

            let mut left = row_top[0];
            let mut topleft = left;
            for (top, p) in row_top
                .iter()
                .copied()
                .zip(row.iter_mut())
                .skip(IMAGE_OFFSET.0)
                .take(w)
            {
                let pred = clamped_gradient(left as i64, top as i64, topleft as i64);
                *p = (pred + decode() as i64) as i32;
                left = *p;
                topleft = top;
            }
        }

        if let Err(e) = br.check_for_error() {
            if let Some(p) = partial_decoded_buffers {
                *p = last_safe_buf;
            }
            return Err(e);
        }
    }

    Ok(())
}

// This function will decode a header and apply local transforms if a header is not given.
// The intended use of passing a header is for the DcGlobal section.
pub(in crate::frame::modular) fn decode_modular_subbitstream(
    buffers: Vec<&mut ModularChannel>,
    stream_id: usize,
    header: Option<GroupHeader>,
    global_tree: &Option<Tree>,
    br: &mut BitReader,
    partial_decoded_buffers: Option<&mut usize>,
) -> Result<()> {
    // Skip decoding if all grids are zero-sized.
    let is_empty = buffers
        .iter()
        .all(|buffer| matches!(buffer.data.size(), (0, _) | (_, 0)));
    if is_empty {
        return Ok(());
    }
    let mut transform_steps = vec![];
    let mut buffer_storage = vec![];

    let buffers = buffers.into_iter().collect::<Vec<_>>();
    let (header, mut buffers) = match header {
        Some(h) => (h, buffers),
        None => {
            let h = GroupHeader::read(br)?;
            if !h.transforms.is_empty() {
                // Note: reassigning to `buffers` here convinces the borrow checker that the borrow of
                // `buffer_storage` ought to outlive `buffers[..]`'s lifetime, which obviously breaks
                // applying transforms later.
                let new_bufs;
                (new_bufs, transform_steps) =
                    meta_apply_local_transforms(buffers, &mut buffer_storage, &h)?;
                (h, new_bufs)
            } else {
                (h, buffers)
            }
        }
    };

    if header.use_global_tree && global_tree.is_none() {
        return Err(Error::NoGlobalTree);
    }
    let local_tree = if !header.use_global_tree {
        let num_local_samples = buffers
            .iter()
            .map(|buf| {
                let (width, height) = buf.channel_info().size;
                width * height
            })
            .sum::<usize>();
        let size_limit = (1024 + num_local_samples).min(1 << 20);
        Some(Tree::read(br, size_limit)?)
    } else {
        None
    };
    let tree = if header.use_global_tree {
        global_tree.as_ref().unwrap()
    } else {
        local_tree.as_ref().unwrap()
    };

    let image_width = buffers
        .iter()
        .map(|info| info.channel_info().size.0)
        .max()
        .unwrap_or(0);

    if can_decode_fast_lossless(tree) {
        decode_fast_lossless(buffers, tree, br, partial_decoded_buffers)?
    } else {
        let mut reader = SymbolReader::new(&tree.histograms, br, Some(image_width))?;

        let mut last_safe_buf = 0;
        for i in 0..buffers.len() {
            // Keep channel numbering stable, but skip actually decoding empty channels.
            // This matches libjxl, which continues the loop without renumbering.
            let (w, h) = buffers[i].data.size();
            if w == 0 || h == 0 {
                continue;
            }
            if br.total_bits_available() >= DECODE_SAFETY_MARGIN {
                last_safe_buf = i;
            }
            if let Err(e) =
                decode_modular_channel(&mut buffers, i, stream_id, &header, tree, &mut reader, br)
            {
                if let Some(p) = partial_decoded_buffers {
                    *p = last_safe_buf;
                }
                return Err(e);
            }
        }

        reader.check_final_state(&tree.histograms, br)?;

        drop(buffers);
    }

    for step in transform_steps.iter().rev() {
        step.local_apply(&mut buffer_storage)?;
    }

    Ok(())
}
