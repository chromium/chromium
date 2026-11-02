// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl::api::{JxlColorEncoding, JxlColorProfile};
use jxl_gainmap::JxlGainMap;

macro_rules! parse {
    ($name:literal) => {
        JxlGainMap::parse(include_bytes!(concat!("testdata/", $name, ".jhgm")))
    };
}

#[test]
fn color_encoding() {
    let expected_structured = Some(JxlColorProfile::Simple(JxlColorEncoding::linear_srgb(
        false,
    )));

    let structured = parse!("structured").unwrap();
    assert_eq!(structured.color_profile, expected_structured);

    let icc = parse!("icc").unwrap();
    assert!(matches!(icc.color_profile, Some(JxlColorProfile::Icc(_))));

    let want_icc = parse!("want_icc").unwrap();
    assert!(matches!(
        want_icc.color_profile,
        Some(JxlColorProfile::Icc(_))
    ));
}

#[test]
fn invalid_color_encoding() {
    assert!(parse!("malformed_color").is_err());
    assert!(parse!("missing_icc").is_err());
}
