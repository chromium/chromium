// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `ICU4X-json` filesystem overlay provider (`OverlayDataProvider`) and
//! unified `.patch` application (`prepare_patched_overlay`).

mod patch;
mod provider;

pub(crate) use patch::prepare_patched_overlay;
#[cfg(test)]
pub(crate) use patch::TempDir;
pub(crate) use provider::OverlayDataProvider;
