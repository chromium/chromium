// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![forbid(unsafe_op_in_unsafe_fn)]
#![forbid(unsafe_code)]

pub mod condition;
pub mod config;
pub mod crates;
pub mod deps;
pub mod gn;
pub mod group;
pub mod inherit;
pub mod manifest;
pub mod paths;
pub mod readme;
pub mod target_triple {
    include!(concat!(env!("OUT_DIR"), "/target_triple.rs"));
}
pub mod vet;
