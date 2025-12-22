// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::render::low_memory_pipeline::render_group::ChannelVec;

/// Returns a vector of &mut vals[idx[i].0][idx[i].1], in order of idx[i].2.
/// Panics if any of the indices are out of bounds or
/// (idx[i].0, idx[i].1) == (idx[j].0, idx[j].1) for i != j or indices are not
/// sorted lexicographically.
pub(super) fn get_distinct_indices<'a, T>(
    vals: &'a mut [impl AsMut<[T]>],
    idx: &[(usize, usize, usize)],
) -> ChannelVec<&'a mut T> {
    let mut answer_buffer = ChannelVec::new();
    for _ in 0..idx.len() {
        answer_buffer.push(None);
    }

    // TODO(veluca): in theory, we don't really need to first create a vector of
    // `Option`s that then get `unwrap`-ed separately. Currently, this function
    // uses somewhere between 0.5 and 1.5% of the total runtime; if that number
    // increases, it might be worth investigating how to speed this up.
    let mut targets = idx.iter();
    let mut target = targets.next().unwrap();
    'outer: for (aa, bufs) in vals.iter_mut().enumerate() {
        for (bb, buf) in bufs.as_mut().iter_mut().enumerate() {
            let (a, b, pos) = target;
            if aa == *a && bb == *b {
                answer_buffer[*pos] = Some(buf);
                if let Some(t) = targets.next() {
                    target = t;
                } else {
                    break 'outer;
                }
            }
        }
    }

    answer_buffer
        .iter_mut()
        .map(|x| std::mem::take(x).expect("Not all elements were found"))
        .collect()
}

/// Mirror-reflects a value v to fit in a [0; s) range.
pub(super) fn mirror(mut v: isize, s: usize) -> usize {
    // TODO(veluca): consider speeding this up if needed.
    loop {
        if v < 0 {
            v = -v - 1;
        } else if v >= s as isize {
            v = s as isize * 2 - v - 1;
        } else {
            return v as usize;
        }
    }
}
