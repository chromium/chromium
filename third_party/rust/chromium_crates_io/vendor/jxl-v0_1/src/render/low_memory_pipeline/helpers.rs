// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/// Returns a vector of f(&mut vals[idx[i].0][idx[i].1]). Panics if any of the indices are out of
/// bounds or idx[i] == idx[j] for i != j.
pub(super) fn get_distinct_indices<'a, T>(
    vals: &'a mut [impl AsMut<[T]>],
    idx: &[(usize, usize)],
) -> Vec<&'a mut T> {
    let mut sorted_with_pos: Vec<_> = idx.iter().copied().enumerate().collect();
    sorted_with_pos.sort_by_key(|x| x.1);

    let mut answer_buffer = vec![];
    for _ in 0..idx.len() {
        answer_buffer.push(None);
    }

    let mut targets = sorted_with_pos.into_iter();
    let mut target = targets.next().unwrap();
    'outer: for (aa, bufs) in vals.iter_mut().enumerate() {
        for (bb, buf) in bufs.as_mut().iter_mut().enumerate() {
            let (pos, (a, b)) = target;
            if aa == a && bb == b {
                answer_buffer[pos] = Some(buf);
                if let Some(t) = targets.next() {
                    target = t;
                } else {
                    break 'outer;
                }
            }
        }
    }

    answer_buffer
        .into_iter()
        .map(|x| x.expect("Not all elements were found"))
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
