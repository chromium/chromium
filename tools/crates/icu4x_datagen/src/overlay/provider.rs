// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `JsonOverlayBufferProvider` and `OverlayDataProvider` for layering an
//! `ICU4X-json` directory tree on top of `SourceDataProvider`.

use crate::marker_path_str;
use icu_locale_core::DataLocale;
use icu_provider::buf::{AsDeserializingBufferProvider, BufferFormat, BufferMarker};
use icu_provider::prelude::*;
use icu_provider_source::SourceDataProvider;
use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

/// Minimal JSON filesystem buffer provider that loads
/// `<marker_path>/<attrs>/<locale>.json` (or `<marker_path>.json` for
/// singletons) directly from a directory, without requiring a `manifest.json`
/// file.
pub(super) struct JsonOverlayBufferProvider {
    root: PathBuf,
}

impl DynamicDataProvider<BufferMarker> for JsonOverlayBufferProvider {
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<BufferMarker>, DataError> {
        let mut path = self.root.join(marker_path_str(marker.id));
        if !marker.is_singleton {
            if !req.id.marker_attributes.is_empty() {
                path.push(req.id.marker_attributes.as_str());
            }
            path.push(req.id.locale.to_string());
        }
        path.set_extension("json");
        if !path.exists() {
            return Err(DataErrorKind::IdentifierNotFound.with_req(marker, req));
        }
        let bytes =
            std::fs::read(&path).map_err(|e| DataError::from(e).with_path_context(&path))?;
        let mut metadata = DataResponseMetadata::default();
        metadata.buffer_format = Some(BufferFormat::Json);
        Ok(DataResponse {
            metadata,
            payload: DataPayload::from_owned_buffer(bytes.into_boxed_slice()),
        })
    }
}

/// Overlay provider combining an optional JSON filesystem overlay
/// (`JsonOverlayBufferProvider`) on top of `SourceDataProvider` and unioning
/// `iter_ids` across both.
pub(crate) struct OverlayDataProvider {
    overlay: Option<JsonOverlayBufferProvider>,
    base: SourceDataProvider,
}

impl OverlayDataProvider {
    pub(crate) fn new(base: SourceDataProvider, overlay_root: Option<PathBuf>) -> Self {
        let overlay = overlay_root.map(|root| JsonOverlayBufferProvider { root });
        Self { overlay, base }
    }
}

fn parse_canonical_locale_json_name(file_name: &str) -> Result<DataLocale, &'static str> {
    let Some(stem) = file_name.strip_suffix(".json") else {
        return Err("Unexpected non-.json file in ICU4X-json overlay");
    };
    let loc = stem
        .parse::<DataLocale>()
        .map_err(|_| "Invalid DataLocale file stem in ICU4X-json overlay")?;
    if loc.to_string() != stem {
        return Err("Non-canonical DataLocale file stem in ICU4X-json overlay");
    }
    Ok(loc)
}

/// Parse an overlay-root-relative `ICU4X-json` path (e.g.
/// `"timezone/names/generic/long/v1/ko.json"`,
/// `"locale/names/language/medium/tiny/v1/dv/dv.json"`, or singleton
/// `"currency/fractions/v1.json"`) against `marker`.
///
/// Returns `None` if `rel_path` does not start with `marker`'s path prefix,
/// `Some(Ok(id))` if `rel_path` is a valid canonical `ICU4X-json` path for
/// `marker`, or `Some(Err(reason))` if `rel_path` matches `marker`'s prefix
/// but violates singleton, attribute, locale, or extension layout rules.
pub(super) fn parse_overlay_rel_path(
    marker: DataMarkerInfo,
    rel_path: &str,
) -> Option<Result<DataIdentifierCow<'static>, &'static str>> {
    let m_path = marker_path_str(marker.id);
    let singleton_rel = format!("{m_path}.json");
    let dir_prefix = format!("{m_path}/");

    if marker.is_singleton {
        if rel_path == singleton_rel {
            return Some(Ok(DataIdentifierCow::from_locale(DataLocale::default())));
        }
        if rel_path.starts_with(&dir_prefix) {
            return Some(Err("Unexpected directory for singleton marker in ICU4X-json overlay"));
        }
        return None;
    }

    if rel_path == singleton_rel {
        return Some(Err(
            "Unexpected singleton .json file for non-singleton marker in ICU4X-json overlay",
        ));
    }
    let tail = rel_path.strip_prefix(&dir_prefix)?;
    if let Some((attr_str, file_name)) = tail.split_once('/') {
        if file_name.contains('/') {
            return Some(Err(
                "Unexpected nested directory inside attribute directory in ICU4X-json overlay",
            ));
        }
        let attrs = match DataMarkerAttributes::try_from_str(attr_str) {
            Ok(a) => a,
            Err(_) => {
                return Some(Err("Invalid DataMarkerAttributes in ICU4X-json overlay"));
            }
        };
        if attrs.is_empty() || attrs.as_str() != attr_str {
            return Some(Err("Non-canonical DataMarkerAttributes in ICU4X-json overlay"));
        }
        Some(
            parse_canonical_locale_json_name(file_name)
                .map(|loc| DataIdentifierCow::from_owned(attrs.to_owned(), loc)),
        )
    } else {
        Some(parse_canonical_locale_json_name(tail).map(DataIdentifierCow::from_locale))
    }
}

fn parse_overlay_entry(
    marker: DataMarkerInfo,
    rel_path: &str,
    fs_path: &Path,
) -> Result<DataIdentifierCow<'static>, DataError> {
    match parse_overlay_rel_path(marker, rel_path) {
        Some(Ok(id)) => Ok(id),
        Some(Err(reason)) => {
            Err(DataError::custom(reason).with_marker(marker).with_path_context(fs_path))
        }
        None => Err(DataError::custom("Mismatched marker path in ICU4X-json overlay")
            .with_marker(marker)
            .with_path_context(fs_path)),
    }
}

fn iter_overlay_ids_for_marker(
    overlay_root: &Path,
    marker: DataMarkerInfo,
) -> Result<BTreeSet<DataIdentifierCow<'static>>, DataError> {
    let mut out = BTreeSet::new();
    let m_path = marker_path_str(marker.id);
    let dir = overlay_root.join(&m_path);
    let singleton_path = dir.with_extension("json");

    if marker.is_singleton {
        if dir.exists() {
            return Err(parse_overlay_entry(marker, &format!("{m_path}/"), &dir).unwrap_err());
        }
        if singleton_path.exists() {
            out.insert(parse_overlay_entry(marker, &format!("{m_path}.json"), &singleton_path)?);
        }
        return Ok(out);
    }

    if singleton_path.exists() {
        return Err(
            parse_overlay_entry(marker, &format!("{m_path}.json"), &singleton_path).unwrap_err()
        );
    }
    if !dir.exists() {
        return Ok(out);
    }

    let entries =
        std::fs::read_dir(&dir).map_err(|e| DataError::from(e).with_path_context(&dir))?;
    for entry in entries {
        let entry = entry.map_err(|e| DataError::from(e).with_path_context(&dir))?;
        let path = entry.path();
        let file_type =
            entry.file_type().map_err(|e| DataError::from(e).with_path_context(&path))?;
        let Some(name) = path.file_name().and_then(|s| s.to_str()) else {
            return Err(DataError::custom("Non-UTF-8 entry name in ICU4X-json overlay")
                .with_marker(marker)
                .with_path_context(&path));
        };
        if file_type.is_dir() {
            let sub_entries = std::fs::read_dir(&path)
                .map_err(|e| DataError::from(e).with_path_context(&path))?;
            let mut saw_file = false;
            for sub in sub_entries {
                let sub = sub.map_err(|e| DataError::from(e).with_path_context(&path))?;
                let sub_path = sub.path();
                let sub_type =
                    sub.file_type().map_err(|e| DataError::from(e).with_path_context(&sub_path))?;
                let Some(sub_name) = sub_path.file_name().and_then(|s| s.to_str()) else {
                    return Err(DataError::custom("Non-UTF-8 entry name in ICU4X-json overlay")
                        .with_marker(marker)
                        .with_path_context(&sub_path));
                };
                if sub_type.is_dir() {
                    return Err(parse_overlay_entry(
                        marker,
                        &format!("{m_path}/{name}/{sub_name}/"),
                        &sub_path,
                    )
                    .unwrap_err());
                }
                saw_file = true;
                out.insert(parse_overlay_entry(
                    marker,
                    &format!("{m_path}/{name}/{sub_name}"),
                    &sub_path,
                )?);
            }
            if !saw_file {
                return Err(DataError::custom(
                    "Unexpected empty attribute directory in ICU4X-json overlay",
                )
                .with_marker(marker)
                .with_path_context(&path));
            }
        } else {
            out.insert(parse_overlay_entry(marker, &format!("{m_path}/{name}"), &path)?);
        }
    }
    Ok(out)
}

impl<M: DataMarker> DataProvider<M> for OverlayDataProvider
where
    for<'de> <M::DataStruct as icu_provider::prelude::yoke::Yokeable<'de>>::Output:
        serde::Deserialize<'de>,
    SourceDataProvider: DataProvider<M>,
{
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        if let Some(ref overlay) = self.overlay {
            let mut silent_req = req;
            silent_req.metadata.silent = true;
            match overlay.as_deserializing().load(silent_req) {
                Ok(resp) => return Ok(resp),
                Err(e)
                    if matches!(
                        e.kind,
                        DataErrorKind::IdentifierNotFound | DataErrorKind::MarkerNotFound
                    ) => {}
                Err(e) => return Err(e),
            }
        }
        self.base.load(req)
    }
}

impl<M: DataMarker> IterableDataProvider<M> for OverlayDataProvider
where
    for<'de> <M::DataStruct as icu_provider::prelude::yoke::Yokeable<'de>>::Output:
        serde::Deserialize<'de>,
    SourceDataProvider: IterableDataProvider<M>,
{
    fn iter_ids(&self) -> Result<BTreeSet<DataIdentifierCow<'_>>, DataError> {
        let mut ids = self.base.iter_ids()?;
        if let Some(ref overlay) = self.overlay {
            ids.extend(iter_overlay_ids_for_marker(&overlay.root, M::INFO)?);
        }
        Ok(ids)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::overlay::TempDir;

    #[test]
    fn test_iter_overlay_ids_for_marker_rejects_invalid_and_non_canonical_names() {
        let marker = icu::plurals::provider::PluralsCardinalV1::INFO;
        let attr_marker = crate::all_registered_markers()
            .into_iter()
            .find(|m| m.name == "LocaleNamesLanguageMediumTinyV1")
            .unwrap()
            .info;

        // 1. Canonical BCP-47 stem (`en-GB.json`) and attribute
        //    (`fr/en-GB.json`) succeed.
        let dir = TempDir::new("icu4x_overlay_valid").unwrap();
        let marker_dir = dir.path().join(marker_path_str(marker.id));
        std::fs::create_dir_all(&marker_dir).unwrap();
        std::fs::write(marker_dir.join("en-GB.json"), "{}").unwrap();
        let ids = iter_overlay_ids_for_marker(dir.path(), marker).unwrap();
        assert_eq!(ids.len(), 1);
        assert_eq!(ids.iter().next().unwrap().locale.to_string(), "en-GB");

        let attr_dir = dir.path().join(marker_path_str(attr_marker.id)).join("fr");
        std::fs::create_dir_all(&attr_dir).unwrap();
        std::fs::write(attr_dir.join("en-GB.json"), "\"French\"").unwrap();
        let attr_ids = iter_overlay_ids_for_marker(dir.path(), attr_marker).unwrap();
        assert_eq!(attr_ids.len(), 1);

        // 2. Non-canonical underscore stem (`en_GB.json`) fails loudly.
        let non_canon_dir = TempDir::new("icu4x_overlay_non_canon").unwrap();
        let m_dir = non_canon_dir.path().join(marker_path_str(marker.id));
        std::fs::create_dir_all(&m_dir).unwrap();
        std::fs::write(m_dir.join("en_GB.json"), "{}").unwrap();
        assert!(iter_overlay_ids_for_marker(non_canon_dir.path(), marker).is_err());

        // 3. Unparseable locale stem (`!invalid!.json`) fails loudly.
        let bad_stem_dir = TempDir::new("icu4x_overlay_bad_stem").unwrap();
        let m_dir = bad_stem_dir.path().join(marker_path_str(marker.id));
        std::fs::create_dir_all(&m_dir).unwrap();
        std::fs::write(m_dir.join("!invalid!.json"), "{}").unwrap();
        assert!(iter_overlay_ids_for_marker(bad_stem_dir.path(), marker).is_err());

        // 4. Unexpected non-`.json` file (e.g. `.orig` backup) fails loudly.
        let orig_dir = TempDir::new("icu4x_overlay_orig").unwrap();
        let m_dir = orig_dir.path().join(marker_path_str(marker.id));
        std::fs::create_dir_all(&m_dir).unwrap();
        std::fs::write(m_dir.join("en-GB.json.orig"), "{}").unwrap();
        assert!(iter_overlay_ids_for_marker(orig_dir.path(), marker).is_err());

        // 5. `parse_overlay_rel_path` handles singletons, attributes, and
        //    mismatches cleanly, and registered marker prefixes are disjoint.
        let singleton = crate::all_registered_markers()
            .into_iter()
            .find(|m| m.name == "CurrencyFractionsV1")
            .unwrap()
            .info;
        assert!(parse_overlay_rel_path(singleton, "currency/fractions/v1.json").unwrap().is_ok());
        assert!(parse_overlay_rel_path(singleton, "currency/fractions/v1/en.json")
            .unwrap()
            .is_err());
        assert!(parse_overlay_rel_path(marker, "plurals/cardinal/v1.json").unwrap().is_err());
        assert!(parse_overlay_rel_path(marker, "currency/fractions/v1.json").is_none());

        let paths: Vec<String> = crate::all_registered_markers()
            .into_iter()
            .map(|m| marker_path_str(m.info.id))
            .collect();
        for (i, a) in paths.iter().enumerate() {
            let prefix = format!("{a}/");
            for (j, b) in paths.iter().enumerate() {
                if i != j {
                    assert!(
                        a != b && !b.starts_with(&prefix),
                        "Overlapping marker paths: {a}, {b}"
                    );
                }
            }
        }
    }
}
