// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `FilteredDataProvider` and `NonClosingExporter` wrappers for applying
//! ICU4C attribute/autonym filters and multi-group locale export.

use crate::marker_path_str;
use icu_provider::export::{DataExporter, ExportMarker, ExporterCloseMetadata, FlushMetadata};
use icu_provider::prelude::*;
use icu_provider_export::baked_exporter::BakedExporter;
use std::collections::{BTreeSet, HashSet};

/// Provider wrapper that applies ICU4C `resourceFilters` attribute and autonym
/// filters to `IterableDataProvider::iter_ids`.
///
/// Note: `FilteredDataProvider` only handles attribute filtering (filtering
/// `DataMarkerAttributes` keys within a locale's data, such as currency codes,
/// target language/variant codes, transliterator IDs, and non-UI autonyms).
/// Whole-locale filtering is handled by `resolve_marker_filter` /
/// `resolve_default_marker_filter` when grouping markers into `ExportDriver`
/// runs.
pub(crate) struct FilteredDataProvider<P> {
    pub(super) inner: P,
    pub(super) allowed_transliterators: Option<HashSet<String>>,
    pub(super) allowed_currencies: Option<HashSet<String>>,
    pub(super) allowed_target_languages: Option<HashSet<String>>,
    pub(super) allowed_target_variants: Option<HashSet<String>>,
    pub(super) abridged_non_ui_locales: HashSet<String>,
}

impl<P> FilteredDataProvider<P> {
    pub(crate) fn unfiltered(inner: P) -> Self {
        Self {
            inner,
            allowed_transliterators: None,
            allowed_currencies: None,
            allowed_target_languages: None,
            allowed_target_variants: None,
            abridged_non_ui_locales: HashSet::new(),
        }
    }
}

impl<P, M: DataMarker> DataProvider<M> for FilteredDataProvider<P>
where
    P: DataProvider<M>,
{
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        self.inner.load(req)
    }
}

impl<P, M: DataMarker> IterableDataProvider<M> for FilteredDataProvider<P>
where
    P: IterableDataProvider<M>,
{
    fn iter_ids(&self) -> Result<BTreeSet<DataIdentifierCow<'_>>, DataError> {
        let mut ids = self.inner.iter_ids()?;
        let m_path = marker_path_str(M::INFO.id);
        if m_path == "transliterator/rules/v1" {
            if let Some(ref allowed) = self.allowed_transliterators {
                ids.retain(|id| allowed.contains(id.marker_attributes.as_str()));
            }
        } else if m_path == "currency/displayname/v1"
            || m_path == "currency/extended/data/v1"
            || m_path == "currency/symbols/v1"
        {
            if let Some(ref allowed) = self.allowed_currencies {
                ids.retain(|id| {
                    id.marker_attributes.is_empty()
                        || allowed.contains(&id.marker_attributes.as_str().to_ascii_uppercase())
                });
            }
        } else if M::INFO.attributes_domain == "locale_names_language" {
            if let Some(ref allowed_langs) = self.allowed_target_languages {
                ids.retain(|id| allowed_langs.contains(id.marker_attributes.as_str()));
            }
            if !self.abridged_non_ui_locales.is_empty() {
                ids.retain(|id| {
                    let loc_str = id.locale.to_string();
                    let lang_str = id.locale.language.as_str();
                    (!self.abridged_non_ui_locales.contains(&loc_str)
                        && !self.abridged_non_ui_locales.contains(lang_str))
                        || id.marker_attributes.as_str() == loc_str
                });
            }
        } else if M::INFO.attributes_domain == "locale_names_variant" {
            if let Some(ref allowed_vars) = self.allowed_target_variants {
                ids.retain(|id| {
                    allowed_vars.contains(&id.marker_attributes.as_str().to_ascii_lowercase())
                });
            }
        }
        Ok(ids)
    }
}

/// Wrapper around `&BakedExporter` that turns `close(&mut self)` into a no-op,
/// allowing multiple `ExportDriver::export()` runs (one per distinct locale
/// filter group) to flush markers into the same `BakedExporter` before calling
/// `BakedExporter::close()` once at the end.
pub(super) struct NonClosingExporter<'a>(pub(super) &'a BakedExporter);

impl DataExporter for NonClosingExporter<'_> {
    fn put_payload(
        &self,
        marker: DataMarkerInfo,
        id: DataIdentifierBorrowed,
        payload: &DataPayload<ExportMarker>,
    ) -> Result<(), DataError> {
        self.0.put_payload(marker, id, payload)
    }

    fn flush_singleton(
        &self,
        marker: DataMarkerInfo,
        payload: &DataPayload<ExportMarker>,
        metadata: FlushMetadata,
    ) -> Result<(), DataError> {
        self.0.flush_singleton(marker, payload, metadata)
    }

    fn flush(&self, marker: DataMarkerInfo, metadata: FlushMetadata) -> Result<(), DataError> {
        self.0.flush(marker, metadata)
    }

    fn close(&mut self) -> Result<ExporterCloseMetadata, DataError> {
        Ok(ExporterCloseMetadata::default())
    }
}
