// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Extraction of the optional `jhgm` HDR gain map box of a JPEG XL container.

use jxl::api::{Event, JxlAuxBoxType, JxlDecoder};
use jxl_gainmap::JxlGainMap;

use crate::ffi::JxlRsGainMap;

/// Returns the gain map of the container `decoder` has been parsing, or `None`
/// if the container holds no gain map, or holds one this parser cannot use.
/// `remaining_input` is the rest of the container, which must end the input.
pub(crate) fn collect(decoder: &mut JxlDecoder, remaining_input: &[u8]) -> Option<JxlRsGainMap> {
    // Requested boxes keep the decoder running past the last frame until the
    // input ends.
    let mut input = remaining_input;
    decoder.close_input();
    loop {
        match decoder.process(&mut input, None, None) {
            Ok(Event::Complete) | Ok(Event::NeedMoreInput { .. }) => break,
            Ok(_) => {}
            Err(_) => return None,
        }
    }

    // The standard disallows a compressed gain map box, and several gain maps
    // are ambiguous, so only a single uncompressed box is usable.
    let boxes = decoder.aux_boxes(JxlAuxBoxType::HDR_GAIN_MAP);
    let payload = match (boxes, decoder.trailing_box()) {
        ([single], None) if !single.is_compressed() => single.raw_data().to_vec(),
        ([], Some(trailing)) if !trailing.is_compressed() => {
            // A box whose size extends to the end of the file is still being
            // parsed, so its tail is the unconsumed input.
            [trailing.raw_data(), input].concat()
        }
        _ => return None,
    };

    let gain_map = JxlGainMap::parse(&payload).ok()?;
    if gain_map.gain_map.is_empty() {
        return None;
    }
    // No color profile means the gain map math uses the base image's profile.
    let alternate_icc = match gain_map.color_profile {
        Some(profile) => profile.try_as_icc()?.into_owned(),
        None => Vec::new(),
    };
    Some(JxlRsGainMap {
        metadata: gain_map.metadata.to_vec(),
        gain_map: gain_map.gain_map.to_vec(),
        alternate_icc,
    })
}
