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

/// Invoke `git apply -p1` in `work_dir`, piping `patch_bytes` over `stdin`.
///
/// Piping via `stdin` with `current_dir(work_dir)` matches
/// `//tools/crates/gnrt/vendor.rs`. Clearing `GIT_*` repo-discovery variables
/// and setting `GIT_CEILING_DIRECTORIES` to `work_dir`'s parent (with
/// `work_dir` already canonicalized) prevents `git apply` from prepending
/// worktree-relative prefixes if `work_dir` lies inside a Git checkout.
fn run_git_apply(
    work_dir: &Path,
    extra_args: &[&str],
    patch_path: &Path,
    patch_bytes: &[u8],
) -> Result<Vec<u8>> {
    let mut cmd = std::process::Command::new("git");
    cmd.current_dir(work_dir)
        .args(["-c", "core.quotePath=false", "-c", "apply.whitespace=nowarn", "apply", "-p1"])
        .args(extra_args)
        .stdin(std::process::Stdio::piped())
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped());
    for var in ["GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_PREFIX", "GIT_COMMON_DIR"] {
        cmd.env_remove(var);
    }
    if let Some(parent) = work_dir.parent() {
        cmd.env("GIT_CEILING_DIRECTORIES", parent);
    }

    let mut child = cmd.spawn().context("Failed to spawn `git apply`")?;
    let mut stdin = child.stdin.take().expect("piped stdin");
    let (write_res, wait_res) = std::thread::scope(|s| {
        let writer = s.spawn(move || {
            use std::io::Write;
            stdin.write_all(patch_bytes)
        });
        let out = child.wait_with_output();
        (writer.join().expect("stdin writer panicked"), out)
    });
    let out = wait_res.context("Failed to wait for `git apply`")?;
    if let Err(err) = write_res {
        if out.status.success() || err.kind() != std::io::ErrorKind::BrokenPipe {
            return Err(err).with_context(|| {
                format!("Failed to write patch {} to `git apply` stdin", patch_path.display())
            });
        }
    }
    anyhow::ensure!(
        out.status.success(),
        "`git apply` failed for patch {}: {}",
        patch_path.display(),
        String::from_utf8_lossy(&out.stderr).trim()
    );
    Ok(out.stdout)
}

/// Inspect unified `.patch` files via `git apply -p1 --summary` and
/// `git apply -p1 --numstat -z`, rejecting unsupported operations (`delete`,
/// `rename`, `copy`, mode changes, symlinks) and classifying each `.json`
/// target as modifying an existing payload or creating a new payload (`@@
/// -0,0`).
fn collect_patched_json_files(
    work_dir: &Path,
    patches: &[(PathBuf, Vec<u8>)],
) -> Result<Vec<PatchedFile>> {
    let mut entries = Vec::new();
    let mut already_in_overlay = HashSet::new();

    for (patch_path, patch_bytes) in patches {
        let summary =
            String::from_utf8(run_git_apply(work_dir, &["--summary"], patch_path, patch_bytes)?)?;
        let mut created_in_patch = HashSet::new();
        for line in summary.lines().filter(|l| !l.trim().is_empty()) {
            let created = if let Some(rest) = line.strip_prefix(" create mode ") {
                rest.strip_prefix("100644 ")
            } else {
                line.strip_prefix(" create ")
            };
            let Some(rel_path) = created else {
                bail!(
                    "Patch {} contains unsupported operation `{}` (only file modifications and \
                     regular-file creations are supported)",
                    patch_path.display(),
                    line.trim()
                );
            };
            created_in_patch.insert(rel_path.to_string());
        }

        let numstat = String::from_utf8(run_git_apply(
            work_dir,
            &["--numstat", "-z"],
            patch_path,
            patch_bytes,
        )?)?;
        let mut record_count = 0usize;
        for record in numstat.split('\0').filter(|r| !r.is_empty()) {
            record_count += 1;
            let rel_path = record.splitn(3, '\t').nth(2).unwrap_or("");
            anyhow::ensure!(
                !rel_path.is_empty() && rel_path.ends_with(".json"),
                "Patch {} targets non-JSON file `{rel_path}`",
                patch_path.display()
            );
            if created_in_patch.remove(rel_path) {
                anyhow::ensure!(
                    already_in_overlay.insert(rel_path.to_string()),
                    "Patch {} creates `{rel_path}` (`@@ -0,0`), which was already created or \
                     modified by an earlier patch",
                    patch_path.display()
                );
                entries.push(PatchedFile {
                    rel_path: rel_path.to_string(),
                    patch_path: patch_path.clone(),
                    is_new_file: true,
                });
            } else if already_in_overlay.insert(rel_path.to_string()) {
                entries.push(PatchedFile {
                    rel_path: rel_path.to_string(),
                    patch_path: patch_path.clone(),
                    is_new_file: false,
                });
            }
        }
        anyhow::ensure!(
            record_count > 0,
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
/// `git apply -p1`.
fn apply_patch_files(work_dir: &Path, patches: &[(PathBuf, Vec<u8>)]) -> Result<()> {
    for (patch_path, patch_bytes) in patches {
        log::info!("Applying patch {}...", patch_path.display());
        run_git_apply(work_dir, &[], patch_path, patch_bytes)?;
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
/// 1. Scans `patches` via `git apply -p1 --summary` and `--numstat -z` to
///    classify modified vs. newly created `.json` files
///    (`collect_patched_json_files`).
/// 2. Exports baseline `.json` files from `provider` into a temporary staging
///    directory, checks for missing baselines or `@@ -0,0` upstream collisions,
///    and copies the exact modified baseline files into `work_dir`
///    (`export_baseline_json_files`).
/// 3. Runs `git apply -p1` in `work_dir` for each file in `patches`
///    (`apply_patch_files`).
pub(crate) fn prepare_patched_overlay(
    provider: &SourceDataProvider,
    fallbacker: LocaleFallbacker,
    patches: &[PathBuf],
) -> Result<TempDir> {
    let work_dir = TempDir::new("icu4x_datagen_patched_overlay")?;
    let canon_work_dir = std::fs::canonicalize(work_dir.path())?;

    let mut loaded_patches = Vec::with_capacity(patches.len());
    for patch_path in patches {
        let text = std::fs::read_to_string(patch_path)
            .with_context(|| format!("Failed to read patch {}", patch_path.display()))?;
        loaded_patches.push((patch_path.clone(), text.replace("\r\n", "\n").into_bytes()));
    }

    let entries = collect_patched_json_files(&canon_work_dir, &loaded_patches)?;
    if !entries.is_empty() {
        export_baseline_json_files(provider, fallbacker, &entries, &canon_work_dir)?;
    }

    apply_patch_files(&canon_work_dir, &loaded_patches)?;

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
        let patches: Vec<PathBuf> =
            ["name_5_langs.patch", "locale1.patch", "google3-overlay.patch"]
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

        let fractions =
            std::fs::read_to_string(overlay_dir.path().join("currency/fractions/v1.json")).unwrap();
        let json: serde_json::Value = serde_json::from_str(&fractions).unwrap();
        assert_eq!(json["fractions"]["RSD"]["digits"], 0);
        assert!(json["fractions"]["RSD"]["cash_digits"].is_null());
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
        //    (e.g. missing attribute `ay/ay.json`) fails before invoking `git
        //    apply`.
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

        // 4. Unprefixed headers, unsupported operations (`delete`, `rename`,
        //    symlink `120000`), non-`.json` targets, empty patches, and
        //    conflicting duplicate patches fail loudly with the patch filename
        //    and diagnostic reason; CRLF-normalized patches succeed.
        let no_prefix_patch = patch_dir.path().join("no_prefix.patch");
        std::fs::write(
            &no_prefix_patch,
            "--- v1.json\n\
             +++ v1.json\n\
             @@ -1 +1 @@\n\
             -old\n\
             +new\n",
        )
        .unwrap();
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            std::slice::from_ref(&no_prefix_patch),
        )
        .err()
        .expect("Expected unprefixed header to fail")
        .to_string();
        assert!(err.contains("no_prefix.patch"), "Error was: {err}");
        assert!(err.contains("unable to find filename in patch"), "Error was: {err}");

        let delete_patch = patch_dir.path().join("delete.patch");
        std::fs::write(
            &delete_patch,
            "--- a/locale/names/language/medium/tiny/v1/en/en.json\n\
             +++ /dev/null\n\
             @@ -1 +0,0 @@\n\
             -\"English\"\n",
        )
        .unwrap();
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            std::slice::from_ref(&delete_patch),
        )
        .err()
        .expect("Expected delete patch to fail")
        .to_string();
        assert!(err.contains("delete.patch"), "Error was: {err}");
        assert!(err.contains("unsupported operation"), "Error was: {err}");

        let rename_patch = patch_dir.path().join("rename.patch");
        std::fs::write(
            &rename_patch,
            "diff --git a/locale/names/language/v0/en.json b/locale/names/language/v0/ay.json\n\
             similarity index 100%\n\
             rename from locale/names/language/v0/en.json\n\
             rename to locale/names/language/v0/ay.json\n",
        )
        .unwrap();
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            std::slice::from_ref(&rename_patch),
        )
        .err()
        .expect("Expected rename patch to fail")
        .to_string();
        assert!(err.contains("rename.patch"), "Error was: {err}");
        assert!(err.contains("unsupported operation"), "Error was: {err}");

        let symlink_patch = patch_dir.path().join("symlink.patch");
        std::fs::write(
            &symlink_patch,
            "diff --git a/locale/names/language/medium/tiny/v1/ay/ay.json b/locale/names/language/medium/tiny/v1/ay/ay.json\n\
             new file mode 120000\n\
             --- /dev/null\n\
             +++ b/locale/names/language/medium/tiny/v1/ay/ay.json\n\
             @@ -0,0 +1 @@\n\
             +../en/en.json\n\
             \\ No newline at end of file\n",
        )
        .unwrap();
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            std::slice::from_ref(&symlink_patch),
        )
        .err()
        .expect("Expected symlink patch to fail")
        .to_string();
        assert!(err.contains("symlink.patch"), "Error was: {err}");
        assert!(err.contains("unsupported operation"), "Error was: {err}");

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
        let err = prepare_patched_overlay(&provider, fallbacker.clone(), &[empty_patch])
            .err()
            .expect("Expected empty patch to fail")
            .to_string();
        assert!(err.contains("empty.patch"), "Error was: {err}");
        assert!(err.contains("No valid patches in input"), "Error was: {err}");

        let root = crate::chromium_src_root();
        let g3_patch = root.join("third_party/rust/icu4x_data/patches/google3-overlay.patch");
        let err = prepare_patched_overlay(
            &provider,
            fallbacker.clone(),
            &[g3_patch.clone(), g3_patch.clone()],
        )
        .err()
        .expect("Expected double-applying google3-overlay.patch to fail")
        .to_string();
        assert!(err.contains("google3-overlay.patch"), "Error was: {err}");
        assert!(err.contains("patch does not apply"), "Error was: {err}");

        let crlf_patch = patch_dir.path().join("crlf.patch");
        let lf_text = std::fs::read_to_string(&g3_patch).unwrap();
        std::fs::write(&crlf_patch, lf_text.replace('\n', "\r\n")).unwrap();
        let crlf_overlay = prepare_patched_overlay(&provider, fallbacker, &[crlf_patch])
            .expect("Expected CRLF-normalized patch to apply cleanly");
        assert!(crlf_overlay.path().join("currency/fractions/v1.json").is_file());
    }

    #[test]
    fn test_git_apply_isolated_from_enclosing_git_repo_and_multi_block_create_modify() {
        let repo_dir = TempDir::new("icu4x_datagen_enclosing_repo").unwrap();
        let status = std::process::Command::new("git")
            .arg("init")
            .current_dir(repo_dir.path())
            .stdout(std::process::Stdio::null())
            .stderr(std::process::Stdio::null())
            .status()
            .expect("Failed to run `git init`");
        assert!(status.success(), "`git init` failed");

        let work_dir = repo_dir.path().join("sub/work_dir");
        std::fs::create_dir_all(&work_dir).unwrap();
        let canon_work_dir = std::fs::canonicalize(&work_dir).unwrap();

        // Multi-block patch that creates a file in the 1st diff block and
        // modifies the same file in the 2nd diff block (e.g. `git format-patch
        // --stdout` of 2 commits), tested inside a subdirectory of a Git repo.
        let patch_path = repo_dir.path().join("create_then_modify.patch");
        let patch_bytes = b"diff --git a/locale/names/language/medium/tiny/v1/ay/ay.json b/locale/names/language/medium/tiny/v1/ay/ay.json\n\
            new file mode 100644\n\
            --- /dev/null\n\
            +++ b/locale/names/language/medium/tiny/v1/ay/ay.json\n\
            @@ -0,0 +1 @@\n\
            +\"Draft\"\n\
            diff --git a/locale/names/language/medium/tiny/v1/ay/ay.json b/locale/names/language/medium/tiny/v1/ay/ay.json\n\
            --- a/locale/names/language/medium/tiny/v1/ay/ay.json\n\
            +++ b/locale/names/language/medium/tiny/v1/ay/ay.json\n\
            @@ -1 +1 @@\n\
            -\"Draft\"\n\
            +\"Aymar\"\n"
            .to_vec();
        let loaded = vec![(patch_path, patch_bytes)];

        let entries = collect_patched_json_files(&canon_work_dir, &loaded).unwrap();
        assert_eq!(entries.len(), 1);
        assert_eq!(entries[0].rel_path, "locale/names/language/medium/tiny/v1/ay/ay.json");
        assert!(entries[0].is_new_file);

        apply_patch_files(&canon_work_dir, &loaded).unwrap();
        let content = std::fs::read_to_string(
            canon_work_dir.join("locale/names/language/medium/tiny/v1/ay/ay.json"),
        )
        .unwrap();
        assert_eq!(content.trim(), "\"Aymar\"");
    }
}
