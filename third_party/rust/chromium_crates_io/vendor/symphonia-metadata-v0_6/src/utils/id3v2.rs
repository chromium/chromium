// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Utilities for ID3v2 defacto-standards used by many metadata formats.

use symphonia_core::meta::StandardVisualKey;

/// Try to get a `StandardVisualKey` from the APIC picture type identifier.
pub fn get_visual_key_from_picture_type(apic: u32) -> Option<StandardVisualKey> {
    match apic {
        0x00 => Some(StandardVisualKey::Other),
        0x01 => Some(StandardVisualKey::FileIcon),
        0x02 => Some(StandardVisualKey::OtherIcon),
        0x03 => Some(StandardVisualKey::FrontCover),
        0x04 => Some(StandardVisualKey::BackCover),
        0x05 => Some(StandardVisualKey::Leaflet),
        0x06 => Some(StandardVisualKey::Media),
        0x07 => Some(StandardVisualKey::LeadArtistPerformerSoloist),
        0x08 => Some(StandardVisualKey::ArtistPerformer),
        0x09 => Some(StandardVisualKey::Conductor),
        0x0a => Some(StandardVisualKey::BandOrchestra),
        0x0b => Some(StandardVisualKey::Composer),
        0x0c => Some(StandardVisualKey::Lyricist),
        0x0d => Some(StandardVisualKey::RecordingLocation),
        0x0e => Some(StandardVisualKey::RecordingSession),
        0x0f => Some(StandardVisualKey::Performance),
        0x10 => Some(StandardVisualKey::ScreenCapture),
        0x12 => Some(StandardVisualKey::Illustration),
        0x13 => Some(StandardVisualKey::BandArtistLogo),
        0x14 => Some(StandardVisualKey::PublisherStudioLogo),
        _ => None,
    }
}
