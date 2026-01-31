// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::sync::Arc;

use rand::SeedableRng;
use test_log::test;

use super::*;
use crate::{error::Result, features::epf::SigmaSource, image::Image};

#[test]
fn epf0_consistency() -> Result<()> {
    let mut rng = rand_xorshift::XorShiftRng::seed_from_u64(0);
    let sigma = SigmaSource::Variable(Arc::new(Image::new_random((128, 128), &mut rng).unwrap()));
    crate::render::test::test_stage_consistency(
        || Epf0Stage::new(0.9, 2.3 / 3.0, [40.0, 5.0, 3.5], sigma.clone()),
        (512, 512),
        4,
    )
}

#[test]
fn epf1_consistency() -> Result<()> {
    let mut rng = rand_xorshift::XorShiftRng::seed_from_u64(0);
    let sigma = SigmaSource::Variable(Arc::new(Image::new_random((128, 128), &mut rng).unwrap()));
    crate::render::test::test_stage_consistency(
        || Epf1Stage::new(1.0, 2.3 / 3.0, [40.0, 5.0, 3.5], sigma.clone()),
        (512, 512),
        4,
    )
}

#[test]
fn epf2_consistency() -> Result<()> {
    let mut rng = rand_xorshift::XorShiftRng::seed_from_u64(0);
    let sigma = SigmaSource::Variable(Arc::new(Image::new_random((128, 128), &mut rng).unwrap()));
    crate::render::test::test_stage_consistency(
        || Epf2Stage::new(6.5, 2.3 / 3.0, [40.0, 5.0, 3.5], sigma.clone()),
        (512, 512),
        4,
    )
}
