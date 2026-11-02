// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Unified `.patch` application (`prepare_patched_overlay`) against upstream
//! `ICU4X-json` payloads.

use super::provider::parse_overlay_rel_path;
use crate::dump_json::export_icu4x_json;
use crate::{all_registered_markers, MarkerEntry};
use anyhow::{bail, Context, Result};
use icu_locale_fallback::LocaleFallbacker;
use icu_provider::prelude::*;
use icu_provider_export::prelude::*;
use icu_provider_source::SourceDataProvider;
use std::collections::{BTreeSet, HashMap, HashSet};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{SystemTime, UNIX_EPOCH};

/// RAII temporary directory under `std::env::temp_dir()` that is uniquely
/// created via atomic `std::fs::create_dir` and deleted on `Drop`.
pub(crate) struct TempDir {
    path: PathBuf,
}

impl TempDir {
    pub(crate) fn new(prefix: &str) -> Result<Self> {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        let base = std::env::temp_dir();
        let pid = std::process::id();
        for _ in 0..1024 {
            let seq = COUNTER.fetch_add(1, Ordering::Relaxed);
            let nanos =
                SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_nanos()).unwrap_or(0);
            let candidate = base.join(format!("{prefix}_{pid}_{nanos}_{seq}"));
            match std::fs::create_dir(&candidate) {
                Ok(()) => return Ok(Self { path: candidate }),
                Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => continue,
                Err(e) => {
                    return Err(e).with_context(|| {
                        format!("Failed to create temporary directory {}", candidate.display())
                    });
                }
            }
        }
        bail!("Failed to allocate unique temporary directory for prefix `{prefix}`");
    }

    pub(crate) fn path(&self) -> &Path {
        &self.path
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.path);
    }
}

#[derive(Debug, Clone)]
struct PatchedFile {
    rel_path: String,
    patch_path: PathBuf,
    is_new_file: bool,
}

struct ResolvedPatchedFile {
    file: PatchedFile,
    marker: DataMarkerInfo,
    id: DataIdentifierCow<'static>,
}

fn parse_hunk_counts(line: &str) -> Option<(usize, usize)> {
    let rest = line.strip_prefix("@@ -")?;
    let (header, _) = rest.split_once(" @@")?;
    let (old_spec, new_spec) = header.split_once(" +")?;
    let parse_count = |spec: &str| -> Option<usize> {
        match spec.split_once(',') {
            Some((start, cnt)) => {
                let _: usize = start.parse().ok()?;
                cnt.parse().ok()
            }
            None => {
                let _: usize = spec.parse().ok()?;
                Some(1)
            }
        }
    };
    Some((parse_count(old_spec)?, parse_count(new_spec)?))
}

/// Scan unified `.patch` files for `.json` file headers and classify each
/// target file as either modifying an existing upstream payload or creating a
/// new payload from scratch (`--- /dev/null` or `@@ -0,0 +...`).
fn collect_patched_json_files(patches: &[PathBuf]) -> Result<Vec<PatchedFile>> {
    let mut entries = Vec::new();
    let mut already_in_overlay = HashSet::new();

    for patch_path in patches {
        let patch_text = std::fs::read_to_string(patch_path)
            .with_context(|| format!("Failed to read patch {}", patch_path.display()))?;
        // `Some(None)` for `--- /dev/null`; `Some(Some(rel))` for `---
        // a/<rel>`.
        let mut old_header: Option<Option<String>> = None;
        // `(rel_path, from_dev_null, hunk_count)` for the active file pair.
        let mut active_file: Option<(String, bool, usize)> = None;
        let mut rem_old: usize = 0;
        let mut rem_new: usize = 0;
        let mut patch_hunk_total: usize = 0;

        for line in patch_text.lines() {
            if rem_old > 0 || rem_new > 0 {
                if line.starts_with(' ') || (line.is_empty() && rem_old > 0 && rem_new > 0) {
                    anyhow::ensure!(
                        rem_old > 0 && rem_new > 0,
                        "Unexpected context line in hunk in patch {}: `{line}`",
                        patch_path.display()
                    );
                    rem_old -= 1;
                    rem_new -= 1;
                } else if line.starts_with('-') {
                    anyhow::ensure!(
                        rem_old > 0,
                        "Unexpected `-` line in hunk in patch {}: `{line}`",
                        patch_path.display()
                    );
                    rem_old -= 1;
                } else if line.starts_with('+') {
                    anyhow::ensure!(
                        rem_new > 0,
                        "Unexpected `+` line in hunk in patch {}: `{line}`",
                        patch_path.display()
                    );
                    rem_new -= 1;
                } else if line.starts_with('\\') {
                    // `\ No newline at end of file`
                } else {
                    bail!("Malformed hunk body line in patch {}: `{line}`", patch_path.display());
                }
                continue;
            }

            if line.starts_with("--- ") || line == "---" {
                anyhow::ensure!(
                    old_header.is_none(),
                    "Dangling `---` header before `{line}` in patch {}",
                    patch_path.display()
                );
                if let Some((rel, _, 0)) = &active_file {
                    bail!(
                        "Diff header for `{rel}` in patch {} has no `@@` hunks",
                        patch_path.display()
                    );
                }
                active_file = None;

                if line == "--- /dev/null" || line.starts_with("--- /dev/null\t") {
                    old_header = Some(None);
                } else if let Some(rest) = line.strip_prefix("--- a/") {
                    let rel = rest.split('\t').next().unwrap_or(rest).trim();
                    anyhow::ensure!(
                        rel.ends_with(".json"),
                        "Patch {} targets non-JSON file `{rel}` in `{line}`",
                        patch_path.display()
                    );
                    old_header = Some(Some(rel.to_string()));
                } else {
                    bail!(
                        "Unsupported `---` header in patch {}: `{line}` \
                         (expected `--- a/<path>.json` or `--- /dev/null`)",
                        patch_path.display()
                    );
                }
            } else if line.starts_with("+++ ") || line == "+++" {
                let Some(old) = old_header.take() else {
                    bail!(
                        "Unexpected `+++` header without preceding `---` header in patch {}: \
                         `{line}`",
                        patch_path.display()
                    );
                };
                if line == "+++ /dev/null" || line.starts_with("+++ /dev/null\t") {
                    let old_rel = old.unwrap_or_else(|| "/dev/null".to_string());
                    bail!(
                        "Patch {} deletes `{old_rel}`, which is unsupported in ICU4X-json \
                         overlays (missing overlay files fall back to upstream data)",
                        patch_path.display()
                    );
                } else if let Some(rest) = line.strip_prefix("+++ b/") {
                    let new_rel = rest.split('\t').next().unwrap_or(rest).trim();
                    anyhow::ensure!(
                        new_rel.ends_with(".json"),
                        "Patch {} targets non-JSON file `{new_rel}` in `{line}`",
                        patch_path.display()
                    );
                    match old {
                        None => {
                            active_file = Some((new_rel.to_string(), true, 0));
                        }
                        Some(old_rel) => {
                            anyhow::ensure!(
                                old_rel == new_rel,
                                "Patch {} renames `{old_rel}` to `{new_rel}`, which is unsupported",
                                patch_path.display()
                            );
                            active_file = Some((new_rel.to_string(), false, 0));
                        }
                    }
                } else {
                    bail!(
                        "Unsupported `+++` header in patch {}: `{line}` \
                         (expected `+++ b/<path>.json`)",
                        patch_path.display()
                    );
                }
            } else if line.starts_with("@@") {
                anyhow::ensure!(
                    old_header.is_none(),
                    "`---` header without `+++` header before hunk `{line}` in patch {}",
                    patch_path.display()
                );
                let Some((rel, from_dev_null, hunk_count)) = active_file.as_mut() else {
                    bail!(
                        "Unexpected hunk header `{line}` without preceding `--- a/...` and \
                         `+++ b/...` in patch {}",
                        patch_path.display()
                    );
                };
                let (o_cnt, n_cnt) = parse_hunk_counts(line).with_context(|| {
                    format!("Invalid hunk header `{line}` in patch {}", patch_path.display())
                })?;
                let is_new_file = *from_dev_null || o_cnt == 0;
                if *hunk_count == 0 {
                    if is_new_file {
                        anyhow::ensure!(
                            already_in_overlay.insert(rel.clone()),
                            "Patch {} creates `{rel}` (`@@ -0,0`), which was already created or \
                             modified by an earlier patch",
                            patch_path.display()
                        );
                        entries.push(PatchedFile {
                            rel_path: rel.clone(),
                            patch_path: patch_path.clone(),
                            is_new_file: true,
                        });
                    } else if already_in_overlay.insert(rel.clone()) {
                        entries.push(PatchedFile {
                            rel_path: rel.clone(),
                            patch_path: patch_path.clone(),
                            is_new_file: false,
                        });
                    }
                } else {
                    anyhow::ensure!(
                        !is_new_file,
                        "New-file diff for `{rel}` in patch {} cannot have multiple `@@` hunks",
                        patch_path.display()
                    );
                }
                *hunk_count += 1;
                patch_hunk_total += 1;
                rem_old = o_cnt;
                rem_new = n_cnt;
            }
        }

        anyhow::ensure!(
            rem_old == 0 && rem_new == 0,
            "Truncated hunk at end of patch {}",
            patch_path.display()
        );
        anyhow::ensure!(
            old_header.is_none(),
            "Dangling `---` header at end of patch {}",
            patch_path.display()
        );
        if let Some((rel, _, 0)) = &active_file {
            bail!("Diff header for `{rel}` in patch {} has no `@@` hunks", patch_path.display());
        }
        anyhow::ensure!(
            patch_hunk_total > 0,
            "Patch {} contains no unified diff hunks",
            patch_path.display()
        );
    }

    Ok(entries)
}

/// Resolve a relative `ICU4X-json` path (e.g.
/// `"timezone/names/generic/long/v1/ko.json"`,
/// `"locale/names/language/medium/tiny/v1/dv/dv.json"`, or singleton
/// `"currency/fractions/v1.json"`) to its registered `DataMarkerInfo` and
/// canonical `DataIdentifierCow`.
fn resolve_patched_file(
    file: PatchedFile,
    all_markers: &[MarkerEntry],
) -> Result<ResolvedPatchedFile> {
    for m in all_markers {
        if let Some(res) = parse_overlay_rel_path(m.info, &file.rel_path) {
            let id = res.map_err(|reason| {
                anyhow::anyhow!(
                    "Patch {} targets `{}`: {reason}",
                    file.patch_path.display(),
                    file.rel_path,
                )
            })?;
            return Ok(ResolvedPatchedFile { file, marker: m.info, id });
        }
    }

    bail!(
        "Patch {} targets `{}`, which does not match any registered ICU4X marker",
        file.patch_path.display(),
        file.rel_path,
    );
}

/// Export baseline `ICU4X-json` files from `provider` for the markers and
/// locales referenced by `entries`, verify that every modified file exists and
/// every newly created (`@@ -0,0`) file does *not* already exist upstream, and
/// copy only the modified baseline `.json` files into `work_dir`.
///
/// Exporting into a staging directory (`base_dump_dir`) first and copying only
/// modified baseline files ensures that `work_dir` does not accumulate
/// unmodified sibling attribute files (for example, exporting
/// `locale/names/language/medium/tiny/v1` for locale `dv` exports all
/// `<attr>/dv.json` files, whereas we only want `dv/dv.json` in the overlay).
fn export_baseline_json_files(
    provider: &SourceDataProvider,
    fallbacker: LocaleFallbacker,
    entries: &[PatchedFile],
    work_dir: &Path,
) -> Result<()> {
    let all_markers = all_registered_markers();
    let mut resolved = Vec::with_capacity(entries.len());
    let mut seen_markers = HashSet::new();
    let mut markers_to_dump = Vec::new();
    let mut target_locales = HashSet::new();

    for entry in entries {
        let r = resolve_patched_file(entry.clone(), &all_markers)?;
        if seen_markers.insert(r.marker) {
            markers_to_dump.push(r.marker);
        }
        target_locales.insert(r.id.locale);
        resolved.push(r);
    }

    if markers_to_dump.is_empty() {
        return Ok(());
    }

    let families = target_locales.into_iter().map(DataLocaleFamily::single).collect::<Vec<_>>();

    log::info!(
        "Exporting baseline JSON for {} patched files across {} markers...",
        resolved.len(),
        markers_to_dump.len(),
    );
    let base_dump_dir = TempDir::new("icu4x_datagen_base_dump")?;
    let dump_root = base_dump_dir.path().join("export");
    export_icu4x_json(provider, fallbacker, &dump_root, markers_to_dump, families)?;

    let mut supported_ids_cache: HashMap<DataMarkerInfo, BTreeSet<DataIdentifierCow<'_>>> =
        HashMap::new();

    for r in &resolved {
        let src_file = dump_root.join(&r.file.rel_path);
        if r.file.is_new_file {
            // For non-attributed markers, `ExportDriver` with
            // `DeduplicationStrategy::None` synthesizes missing locales by
            // falling back to `und`, so check `iter_ids_for_marker` to verify
            // whether upstream actually provides `r.id`. For singleton and
            // attributed markers (`und` attributes are not pulled up to child
            // locales), also check `src_file.exists()`.
            let supported_ids = match supported_ids_cache.entry(r.marker) {
                std::collections::hash_map::Entry::Occupied(e) => e.into_mut(),
                std::collections::hash_map::Entry::Vacant(e) => {
                    let ids = icu_provider::IterableDynamicDataProvider::<
                        icu_provider::export::ExportMarker,
                    >::iter_ids_for_marker(provider, r.marker)
                    .with_context(|| format!("Failed to list upstream IDs for {:?}", r.marker))?;
                    e.insert(ids)
                }
            };
            let exists_upstream = if r.marker.is_singleton || !r.id.marker_attributes.is_empty() {
                src_file.exists() || supported_ids.contains(&r.id)
            } else {
                supported_ids.contains(&r.id)
            };
            if exists_upstream {
                bail!(
                    "Patch {} creates `{}` from scratch (`@@ -0,0`), but upstream data already \
                     provides `{}`; please update or remove the patch",
                    r.file.patch_path.display(),
                    r.file.rel_path,
                    r.file.rel_path,
                );
            }
        } else {
            if !src_file.exists() {
                bail!(
                    "Patch {} modifies `{}`, which was not produced by upstream baseline export",
                    r.file.patch_path.display(),
                    r.file.rel_path,
                );
            }
            let dst_file = work_dir.join(&r.file.rel_path);
            if let Some(parent) = dst_file.parent() {
                std::fs::create_dir_all(parent)?;
            }
            std::fs::copy(&src_file, &dst_file)?;
        }
    }
    Ok(())
}

/// Apply each unified `.patch` file in `patches` to `work_dir` using
/// `patch --batch --no-backup-if-mismatch -p1`.
fn apply_patch_files(work_dir: &Path, patches: &[PathBuf]) -> Result<()> {
    for patch_path in patches {
        log::info!("Applying patch {}...", patch_path.display());
        let status = std::process::Command::new("patch")
            .arg("--batch")
            .arg("--no-backup-if-mismatch")
            .arg("-p1")
            .arg("-d")
            .arg(work_dir)
            .arg("-i")
            .arg(std::fs::canonicalize(patch_path)?)
            .status()
            .context("Failed to execute `patch` command")?;
        if !status.success() {
            bail!("Failed to apply patch {}", patch_path.display());
        }
    }
    Ok(())
}

/// Prepare a unique temporary `ICU4X-json` overlay directory containing the
/// results of applying `patches` against pristine upstream payloads from
/// `provider`.
///
/// Because `SourceDataProvider` reads upstream CLDR/ICU ZIP archives on the fly
/// rather than storing an uncompressed tree of `ICU4X-json` files on disk,
/// `prepare_patched_overlay`:
/// 1. Scans `patches` to classify modified vs. newly created `.json` files
///    (`collect_patched_json_files`).
/// 2. Exports baseline `.json` files from `provider` into a temporary staging
///    directory, checks for missing baselines or `@@ -0,0` upstream collisions,
///    and copies the exact modified baseline files into `work_dir`
///    (`export_baseline_json_files`).
/// 3. Runs `patch -p1` in `work_dir` for each file in `patches`
///    (`apply_patch_files`).
pub(crate) fn prepare_patched_overlay(
    provider: &SourceDataProvider,
    fallbacker: LocaleFallbacker,
    patches: &[PathBuf],
) -> Result<TempDir> {
    let work_dir = TempDir::new("icu4x_datagen_patched_overlay")?;

    let entries = collect_patched_json_files(patches)?;
    if !entries.is_empty() {
        export_baseline_json_files(provider, fallbacker, &entries, work_dir.path())?;
    }

    apply_patch_files(work_dir.path(), patches)?;

    Ok(work_dir)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::SourceTags;

    #[test]
    fn test_temp_dir_creates_unique_dirs_and_cleans_up_on_drop() {
        let path_a;
        let path_b;
        {
            let dir_a = TempDir::new("icu4x_datagen_test").unwrap();
            let dir_b = TempDir::new("icu4x_datagen_test").unwrap();
            path_a = dir_a.path().to_path_buf();
            path_b = dir_b.path().to_path_buf();
            assert_ne!(path_a, path_b);
            assert!(path_a.is_dir());
            assert!(path_b.is_dir());
            std::fs::write(path_a.join("marker.json"), "{}").unwrap();
        }
        assert!(!path_a.exists());
        assert!(!path_b.exists());
    }

    #[test]
    fn test_prepare_patched_overlay_applies_all_patches() {
        let root = crate::chromium_src_root();
        let patches_dir = root.join("third_party/rust/icu4x_data/patches");
        let patches: Vec<PathBuf> = ["name_5_langs.patch", "locale1.patch"]
            .into_iter()
            .map(|name| {
                let p = patches_dir.join(name);
                assert!(p.is_file(), "Expected patch file {}", p.display());
                p
            })
            .collect();
        let provider = SourceTags::default().source_data_provider();
        let fallbacker = LocaleFallbacker::try_new_unstable(&provider).unwrap();
        let overlay_dir = prepare_patched_overlay(&provider, fallbacker, &patches).unwrap();

        let ay_tiny = std::fs::read_to_string(
            overlay_dir.path().join("locale/names/language/medium/tiny/v1/ay/ay.json"),
        )
        .unwrap();
        assert_eq!(ay_tiny.trim(), "\"Aymar\"");
        let an_tiny = std::fs::read_to_string(
            overlay_dir.path().join("locale/names/language/medium/tiny/v1/an/an.json"),
        )
        .unwrap();
        assert_eq!(an_tiny.trim(), "\"aragonés\"");
        let wa_tiny = std::fs::read_to_string(
            overlay_dir.path().join("locale/names/language/medium/tiny/v1/wa/wa.json"),
        )
        .unwrap();
        assert_eq!(wa_tiny.trim(), "\"Walon\"");
        let ay_v0: serde_json::Value = serde_json::from_str(
            &std::fs::read_to_string(overlay_dir.path().join("locale/names/language/v0/ay.json"))
                .unwrap(),
        )
        .unwrap();
        assert_eq!(ay_v0["names"]["ay"], "Aymar");

        let ko_range = std::fs::read_to_string(
            overlay_dir.path().join("datetime/patterns/range/date/gregorian/v1/ym0d/ko.json"),
        )
        .unwrap();
        assert!(ko_range.contains("y년 M월 d일 ~ d일"));
        let ko_tz = std::fs::read_to_string(
            overlay_dir.path().join("timezone/names/generic/long/v1/ko.json"),
        )
        .unwrap();
        assert!(ko_tz.contains("북미 중부 시간"));
        assert!(ko_tz.contains("북미 로키 산맥 시간"));
    }

    #[test]
    fn test_prepare_patched_overlay_rejects_missing_baseline_and_new_file_collision() {
        let provider = SourceTags::default().source_data_provider();
        let fallbacker = LocaleFallbacker::try_new_unstable(&provider).unwrap();
        let patch_dir = TempDir::new("icu4x_datagen_bad_patches").unwrap();

        // 1. Unknown marker path in patch fails with patch path and target
        //    file.
        let unknown_marker_patch = patch_dir.path().join("unknown_marker.patch");
        std::fs::write(
            &unknown_marker_patch,
            "--- a/nonexistent/marker/v1/en.json\n\
             +++ b/nonexistent/marker/v1/en.json\n\
             @@ -1 +1 @@\n\
             -\"old\"\n\
             +\"new\"\n",
        )
        .unwrap();
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            std::slice::from_ref(&unknown_marker_patch),
        )
        .err()
        .expect("Expected unknown marker patch to fail")
        .to_string();
        assert!(err.contains("unknown_marker.patch"), "Error was: {err}");
        assert!(err.contains("nonexistent/marker/v1/en.json"), "Error was: {err}");

        // 2. Modifying an upstream file that is not produced by baseline export
        //    (e.g. missing attribute `ay/ay.json`) fails before invoking
        //    `patch`.
        let missing_baseline_patch = patch_dir.path().join("missing_baseline.patch");
        std::fs::write(
            &missing_baseline_patch,
            "--- a/locale/names/language/medium/tiny/v1/ay/ay.json\n\
             +++ b/locale/names/language/medium/tiny/v1/ay/ay.json\n\
             @@ -1 +1 @@\n\
             -\"Old\"\n\
             +\"Aymar\"\n",
        )
        .unwrap();
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            std::slice::from_ref(&missing_baseline_patch),
        )
        .err()
        .expect("Expected missing baseline file patch to fail")
        .to_string();
        assert!(err.contains("missing_baseline.patch"), "Error was: {err}");
        assert!(
            err.contains("locale/names/language/medium/tiny/v1/ay/ay.json"),
            "Error was: {err}"
        );

        // 3. `@@ -0,0` (new file) hunk targeting a file that ALREADY exists
        //    upstream (`en/en.json`) fails instead of silently overwriting it.
        let collision_patch = patch_dir.path().join("collision.patch");
        std::fs::write(
            &collision_patch,
            "--- a/locale/names/language/medium/tiny/v1/en/en.json\n\
             +++ b/locale/names/language/medium/tiny/v1/en/en.json\n\
             @@ -0,0 +1 @@\n\
             +\"Custom English\"\n",
        )
        .unwrap();
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            std::slice::from_ref(&collision_patch),
        )
        .err()
        .expect("Expected upstream collision on new-file hunk to fail")
        .to_string();
        assert!(err.contains("collision.patch"), "Error was: {err}");
        assert!(
            err.contains("locale/names/language/medium/tiny/v1/en/en.json"),
            "Error was: {err}"
        );

        // 4. Non-`a/`/`b/` headers (`--- old/...` / `+++ new/...`), non-`.json`
        //    targets, and empty patch files fail loudly in
        //    `collect_patched_json_files` before reaching `patch -p1`.
        let non_ab_patch = patch_dir.path().join("non_ab.patch");
        std::fs::write(
            &non_ab_patch,
            "--- /dev/null\n\
             +++ new/locale/names/language/medium/tiny/v1/ay/ay.json\n\
             @@ -0,0 +1 @@\n\
             +\"Aymar\"\n",
        )
        .unwrap();
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            std::slice::from_ref(&non_ab_patch),
        )
        .err()
        .expect("Expected non-b/ header to fail")
        .to_string();
        assert!(err.contains("non_ab.patch"), "Error was: {err}");
        assert!(err.contains("Unsupported `+++` header"), "Error was: {err}");

        let non_json_patch = patch_dir.path().join("non_json.patch");
        std::fs::write(
            &non_json_patch,
            "--- a/README.txt\n\
             +++ b/README.txt\n\
             @@ -1 +1 @@\n\
             -old\n\
             +new\n",
        )
        .unwrap();
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            std::slice::from_ref(&non_json_patch),
        )
        .err()
        .expect("Expected non-JSON target to fail")
        .to_string();
        assert!(err.contains("non_json.patch"), "Error was: {err}");
        assert!(err.contains("non-JSON file"), "Error was: {err}");

        let empty_patch = patch_dir.path().join("empty.patch");
        std::fs::write(&empty_patch, "# comment only\n").unwrap();
        let err = prepare_patched_overlay(&provider, fallbacker, &[empty_patch])
            .err()
            .expect("Expected empty patch to fail")
            .to_string();
        assert!(err.contains("empty.patch"), "Error was: {err}");
        assert!(err.contains("no unified diff hunks"), "Error was: {err}");
    }
}
