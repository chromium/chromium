// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::bit_reader::BitReader;
use crate::error::Error;
use std::collections::HashSet;

use crate::entropy_coding::decode::*;

fn move_to_front(v: &mut [u8], index: u8) {
    let value = v[index as usize];
    for i in (1..=index as usize).rev() {
        v[i] = v[i - 1];
    }
    v[0] = value;
}

fn inverse_move_to_front(v: &mut [u8]) {
    let mut mtf: [u8; 256] = std::array::from_fn(|x| x as u8);
    for val in v.iter_mut() {
        let index = *val;
        *val = mtf[index as usize];
        if index != 0 {
            move_to_front(&mut mtf, index);
        }
    }
}

fn verify_context_map(ctx_map: &[u8]) -> Result<(), Error> {
    let num_histograms = *ctx_map.iter().max().unwrap() as u32 + 1;
    let distinct_histograms = ctx_map.iter().collect::<HashSet<_>>().len() as u32;
    if distinct_histograms != num_histograms {
        return Err(Error::InvalidContextMapHole(
            num_histograms,
            distinct_histograms,
        ));
    }
    Ok(())
}

pub fn decode_context_map(num_contexts: usize, br: &mut BitReader) -> Result<Vec<u8>, Error> {
    let is_simple = br.read(1)? != 0;
    if is_simple {
        let bits_per_entry = br.read(2)? as usize;
        if bits_per_entry != 0 {
            (0..num_contexts)
                .map(|_| Ok(br.read(bits_per_entry)? as u8))
                .collect()
        } else {
            Ok(vec![0u8; num_contexts])
        }
    } else {
        let use_mtf = br.read(1)? != 0;
        let histograms = Histograms::decode(1, br, /*allow_lz77=*/ num_contexts > 2)?;
        let mut reader = SymbolReader::new(&histograms, br, None)?;

        let mut ctx_map: Vec<u8> = (0..num_contexts)
            .map(|_| {
                let mv = reader.read_unsigned(&histograms, br, 0usize);
                if mv > u8::MAX as u32 {
                    Err(Error::InvalidContextMap(mv))
                } else {
                    Ok(mv as u8)
                }
            })
            .collect::<Result<_, _>>()?;
        reader.check_final_state(&histograms, br)?;
        if use_mtf {
            inverse_move_to_front(&mut ctx_map[..]);
        }
        verify_context_map(&ctx_map[..])?;
        Ok(ctx_map)
    }
}
