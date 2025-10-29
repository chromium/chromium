# Adding a third-party Rust library

[TOC]

This document describes how to import a new third-party Rust library from
https://crates.io into Chromium.  Such an import is a prerequisite for
depending on such a library from:

* Chromium's first-party code
  (e.g. `//chrome`, or `//components`)
* Projects that reuse Chromium's `//third_party_rust` such as
  Pdfium, or V8.
* Other third-party dependencies
  (e.g. `//third_party/cloud_authenticator/cbor`)

## Reviews

[`//third_party/rust/OWNERS-review-checklist.md`](OWNERS-review-checklist.md)
requires that appropriate approvals are secured
before landing CLs that import new crates.

All third-party libraries (not just Rust) need to go through third-party review.
See
[`//docs/adding_to_third_party.md`](../../docs/adding_to_third_party.md)
for instructions.

## Importing a crate from crates.io

Third-party crates (from [crates.io](https://crates.io))
that Chromium depends on are described by two files:

* `//third_party/rust/chromium_crates_io/Cargo.toml`.
  This file defines the set of crates
  **directly** depended on from first-party code (from Chromium first-party
  code, but also from Pdfium, V8, etc.).  Their transitive dependencies don't
  need to be listed, because they will be automatically identified and covered
  by tools like `gnrt`.  The file is a [standard `Cargo.toml` file](
  https://doc.rust-lang.org/cargo/reference/manifest.html), even though the crate
  itself is never built - it is only used to enable/disable crate features,
  specify crate versions, etc.
* `//third_party/rust/chromium_crates_io/gnrt_config.toml`.
  This file defines Chromium-specific, `cargo`-agnostic metadata like:
      - Configuring certain aspects of Chromium build (e.g. `allow_unsafe`,
        `allow_unstable_features`, `extra_src_roots`, `group = "test"`, etc.)
      - Specifying licensing information when it can't be automatically inferred
        (e.g. pointing out `license_files` with non-standard filenames).

To import a third-party crate follow the steps below:

1. Change directory to the root `src/` dir of Chromium.
1. Add the crate to `//third_party/rust/chromium_crates_io/Cargo.toml`:
   * `vpython3 ./tools/crates/run_gnrt.py add foo` to add the latest version of `foo`.
   * `vpython3 ./tools/crates/run_gnrt.py add foo@1.2.3` to add a specific version of `foo`.
   * Or, edit `//third_party/rust/chromium_crates_io/Cargo.toml` by hand,
     finding the version you want from [crates.io](https://crates.io).
1. Download the crate's files:
   * `./tools/crates/run_gnrt.py vendor` to download the new crate.
   * This will also apply any patches in `//third_party/rust/chromium_crates_io/patches`.
     See `//third_party/rust/chromium_crates_io/patches/README.md` for more details.
1. Exempt vendored code from inclusive-language checks - e.g.:
   `infra/update_inclusive_language_presubmit_exempt_dirs.sh > infra/inclusive_language_presubmit_exempt_dirs.txt`
1. (optional) If the crate is only to be used by tests and tooling, then
   specify the `"test"` group in `//third_party/rust/chromium_crates_io/gnrt_config.toml`:
   ```
   [crate.foo]
   group = "test"
   ```
1. Generate the `BUILD.gn` file for the new crate:
   * `vpython3 ./tools/crates/run_gnrt.py gen`
1. Add `//third_party/rust/crate_name/OWNERS`
1. Add the new files to git:
   * `git add -f third_party/rust/chromium_crates_io/vendor`.
     (The `-f` is important, as files may be skipped otherwise from a
     `.gitignore` inside the crate.)
   * `git add third_party/rust`
1. Upload the CL and get a review from `//third_party/rust/OWNERS`
   (check
   [`third_party/rust/OWNERS-review-checklist.md`](../third_party/rust/OWNERS-review-checklist.md)
   to see what to expect).

Note that at this point the new crate is still not seen by `gn` nor `ninja`,
and is not covered by CQ.  To make the new crate part of the build,
you need to add a `deps` edge between an existing build target
and the newly added `//third_party/rust/some_crate/v123:lib` target.
This will allow `autoninja -C out/Default third_party/rust/some_crate/v123:lib`
to work.  Additionally, this will help CQ to prevent regressions when updating
`rustc` or enabling new Rust warnings.

## Security

If a shipping library needs security review (has any `unsafe`), and the review
finds it's not satisfying the [rule of 2](../docs/security/rule-of-2.md), then
move it to the `"sandbox"` group in `//third_party/rust/chromium_crates_io/gnrt_config.toml`
to make it clear it can't be used in a privileged process:
```
[crate.foo]
group = "sandbox"
```

If a transitive dependency moves from `"safe"` to `"sandbox"` and causes
a dependency chain across the groups, it will break the `gnrt vendor` step.
You will need to fix the new crate so that it's deemed safe in unsafe review,
or move the other dependent crates out of `"safe"` as well by setting their
group in `gnrt_config.toml`.

## Troubleshooting

### Incomplete sources or inputs listing

`gnrt` enumerates all `.rs` files as crate sources, but may need help
with discovering additional files consumed with something like
[`include_str!`](https://doc.rust-lang.org/std/macro.include_str.html).
So, if you see:

```
ERROR: file not in GN sources:
../../third_party/rust/chromium_crates_io/vendor/some_crate/README.md
```

Then you can:

* Add the missing files to
  `third_party/rust/chromium_crates_io/gnrt_config.toml` - for example:

  ```
  [crate.some_crate]
  extra_input_roots = ['../README.md']
  ```

* Re-generate `BUILD.gn` files by running:
  `tools/crates/run_gnrt.py gen`
