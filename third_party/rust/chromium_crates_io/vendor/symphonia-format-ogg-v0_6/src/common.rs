// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::meta::{MetadataRevision, MetadataSideData};

/// Map packet side data variants.
pub enum SideData {
    /// Metadata.
    Metadata { rev: MetadataRevision, side_data: Vec<MetadataSideData> },
}
