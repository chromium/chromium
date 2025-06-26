//! A compact, zero copy TZif file.
//!
//! NOTE: This representation does not follow the TZif specification
//! to full detail, but instead attempts to compress TZif data into
//! a functional, data driven equivalent.

#[cfg(feature = "datagen")]
use alloc::vec::Vec;

#[cfg(feature = "datagen")]
use std::{collections::BTreeMap, path::Path};
use zerotrie::ZeroAsciiIgnoreCaseTrie;
#[cfg(feature = "datagen")]
use zerotrie::ZeroTrieBuildError;
use zerovec::{vecs::Index32, VarZeroVec, ZeroVec};
#[cfg(feature = "datagen")]
use zoneinfo_rs::{compiler::CompiledTransitions, ZoneInfoCompiler, ZoneInfoData};

#[cfg(feature = "datagen")]
use crate::tzdb::TzdbDataSource;

#[derive(Debug, Clone)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::tzif))]
pub struct ZoneInfoProvider<'data> {
    // IANA identifier map to TZif index.
    pub ids: ZeroAsciiIgnoreCaseTrie<ZeroVec<'data, u8>>,
    // Vector of TZif data
    pub tzifs: VarZeroVec<'data, ZeroTzifULE, Index32>,
}

#[zerovec::make_varule(ZeroTzifULE)]
#[derive(PartialEq, Debug, Clone)]
#[zerovec::skip_derive(Ord)]
#[zerovec::derive(Debug)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", zerovec::derive(Serialize))]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::tzif))]
pub struct ZeroTzif<'data> {
    pub transitions: ZeroVec<'data, i64>,
    pub transition_types: ZeroVec<'data, u8>,
    // NOTE: zoneinfo64 does a fun little bitmap str
    pub types: ZeroVec<'data, LocalTimeRecord>,
    pub posix: &'data str,
}

#[zerovec::make_ule(LocalTimeRecordULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::tzif))]
pub struct LocalTimeRecord {
    pub offset: i64,
}

#[cfg(feature = "datagen")]
impl From<&zoneinfo_rs::tzif::LocalTimeRecord> for LocalTimeRecord {
    fn from(value: &zoneinfo_rs::tzif::LocalTimeRecord) -> Self {
        Self {
            offset: value.offset,
        }
    }
}

impl ZeroTzif<'_> {
    #[cfg(feature = "datagen")]
    fn from_transition_data(data: &CompiledTransitions) -> Self {
        let tzif = data.to_v2_data_block();
        let transitions = ZeroVec::alloc_from_slice(&tzif.transition_times);
        let transition_types = ZeroVec::alloc_from_slice(&tzif.transition_types);
        let mapped_local_records: Vec<LocalTimeRecord> =
            tzif.local_time_types.iter().map(Into::into).collect();
        let types = ZeroVec::alloc_from_slice(&mapped_local_records);
        let posix = "TODO";

        Self {
            transitions,
            transition_types,
            types,
            posix,
        }
    }
}

#[cfg(feature = "datagen")]
#[derive(Debug)]
pub enum ZoneInfoDataError {
    Build(ZeroTrieBuildError),
}

impl ZoneInfoProvider<'_> {
    #[cfg(feature = "datagen")]
    pub fn build(tzdata: &Path) -> Result<Self, ZoneInfoDataError> {
        let tzdb_source = TzdbDataSource::try_from_zoneinfo_directory(tzdata).unwrap();
        let compiled_transitions = ZoneInfoCompiler::new(tzdb_source.data.clone()).build();

        let mut identifiers = BTreeMap::default();
        let mut primary_zones = Vec::default();

        // Create a Map of <ZoneId | Link, ZoneId>, this is used later to index
        let ZoneInfoData { links, zones, .. } = tzdb_source.data;

        for zone_identifier in zones.into_keys() {
            primary_zones.push(zone_identifier.clone());
            identifiers.insert(zone_identifier.clone(), zone_identifier);
        }
        for (link, zone) in links.into_iter() {
            identifiers.insert(link, zone);
        }

        primary_zones.sort();

        let identifier_map: BTreeMap<Vec<u8>, usize> = identifiers
            .into_iter()
            .map(|(id, zoneid)| {
                (
                    id.to_ascii_lowercase().as_bytes().to_vec(),
                    primary_zones.binary_search(&zoneid).unwrap(),
                )
            })
            .collect();

        let tzifs: Vec<ZeroTzif<'_>> = primary_zones
            .into_iter()
            .map(|id| {
                let data = compiled_transitions
                    .data
                    .get(&id)
                    .expect("all zones should be built");
                ZeroTzif::from_transition_data(data)
            })
            .collect();

        let tzifs_zerovec: VarZeroVec<'static, ZeroTzifULE, Index32> = tzifs.as_slice().into();

        let ids = ZeroAsciiIgnoreCaseTrie::try_from(&identifier_map)
            .map_err(ZoneInfoDataError::Build)?
            .convert_store();

        Ok(ZoneInfoProvider {
            ids,
            tzifs: tzifs_zerovec,
        })
    }
}
