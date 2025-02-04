// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::skeleton::{reference::Skeleton, PatternPlurals, SkeletonError};
use core::convert::TryFrom;
use litemap::LiteMap;

/// Skeleton data for dates and times, along with the corresponding plural pattern
/// information.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, Default)]
pub struct DateSkeletonPatternsV1<'data>(pub LiteMap<SkeletonV1, PatternPlurals<'data>>);

/// This struct is a public wrapper around the internal `Skeleton` struct.
///
/// This allows
/// access to the serialization and deserialization capabilities, without exposing the
/// internals of the skeleton machinery.
///
/// The `Skeleton` is an "exotic type" in the serialization process, and handles its own
/// custom serialization practices.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub struct SkeletonV1(pub Skeleton);

impl TryFrom<&str> for SkeletonV1 {
    type Error = SkeletonError;

    fn try_from(skeleton_string: &str) -> Result<Self, Self::Error> {
        match Skeleton::try_from(skeleton_string) {
            Ok(skeleton) => Ok(Self(skeleton)),
            Err(err) => Err(err),
        }
    }
}
