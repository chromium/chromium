// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use std::simd::prelude::*;

chromium::import! {
    "//ui/android:texture_compressor";
}

use texture_compressor::selectors::{
    flip_pixels, flip_selectors, search_table_and_selectors_subblock, TABLES,
};

#[gtest(TextureCompressorTest, FlipPixels)]
fn test() {
    #[rustfmt::skip]
    let input = [
        [ 0,  1,  2,  3],
        [ 4,  5,  6,  7],
        [ 8,  9, 10, 11],
        [12, 13, 14, 15],
    ];
    let input = input.map(|row| row.map(|x| [Simd::splat(x); 3]));
    #[rustfmt::skip]
    let expected = [
        [ 0,  1,  8,  9],
        [ 4,  5, 12, 13],
        [ 2,  3, 10, 11],
        [ 6,  7, 14, 15],
    ];
    let expected = expected.map(|row| row.map(|x| [Simd::splat(x); 3]));
    expect_eq!(flip_pixels(&input, Mask::splat(true)), expected);
}

#[gtest(TextureCompressorTest, FlipSelectors)]
fn test() {
    #[rustfmt::skip]
    let input = [
         0,  1,  2,  3,
         4,  5,  6,  7,
         8,  9, 10, 11,
        12, 13, 14, 15,
    ];
    let input = input.map(|x| Simd::splat(1u16 << x));
    #[rustfmt::skip]
    let expected = [
         0,  1,  8,  9,
         4,  5, 12, 13,
         2,  3, 10, 11,
         6,  7, 14, 15,
    ];
    let expected = expected.map(|x| Simd::splat(1u16 << x));
    for (input, expected) in input.into_iter().zip(expected) {
        expect_eq!(flip_selectors(input, Mask::splat(true)), expected);
    }
}

#[gtest(TextureCompressorTest, SearchTableAndSelectors)]
fn test() {
    for (table_idx, table) in TABLES.iter().enumerate() {
        let (sm, lg) = (table[0], table[1]);
        let input = [[sm, lg, -sm, -lg], [-lg, -sm, lg, sm]]
            .map(|row| row.map(|x| [Simd::splat(128 + x); 3]));
        let base_color = [Simd::splat(128); 3];
        let result = search_table_and_selectors_subblock(&input, base_color);
        // The bits are arranged as col3_col2_col1_col0, with top 2 bits of each column
        // being zeroed
        expect_eq!(result.selector_hi, Simd::splat(0b0001_0001_0010_0010));
        expect_eq!(result.selector_lo, Simd::splat(0b0001_0010_0001_0010));
        expect_eq!(result.table_idx, Simd::splat(table_idx as u16));
    }
}

#[gtest(TextureCompressorTest, SearchTableAndSelectorsMax)]
fn test() {
    // Test for overflow handling of the error function.
    // The base color is set to an arbitrarily far value rather than being the
    // average. In this case, table 7 has the smallest but non-zero error value.
    // This test can catch overflows if it causes smaller tables to achieve a
    // smaller error value than table 7.
    let input = [[[Simd::splat(255); 3]; 4]; 2];
    let base_color = [Simd::splat(0); 3];
    let result = search_table_and_selectors_subblock(&input, base_color);
    // See test_search_table_and_selectors for notes on bit arrangement.
    expect_eq!(result.selector_hi, Simd::splat(0)); // All positive
    expect_eq!(result.selector_lo, Simd::splat(0b0011_0011_0011_0011)); // All large
    expect_eq!(result.table_idx, Simd::splat(7)); // Largest variance table
}
