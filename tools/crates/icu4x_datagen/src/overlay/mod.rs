// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `ICU4X-json` filesystem overlay provider (`OverlayDataProvider`),
//! unified `.patch` application (`prepare_patched_overlay`), and custom UTS #35
//! transliterator compilation (`validate_and_compile_transliterators`).

mod patch;
mod provider;
mod translit;

pub(crate) use patch::{prepare_patched_overlay, TempDir};
pub(crate) use provider::OverlayDataProvider;
pub(crate) use translit::{
    validate_and_compile_transliterators, BUILTIN_HARDCODED_TRANSLITERATOR_IDS,
    STANDARD_CLDR_TRANSLITERATOR_ALIASES,
};
