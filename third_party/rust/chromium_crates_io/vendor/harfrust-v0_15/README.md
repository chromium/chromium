# HarfRust

<div align="center">
<p><img src="HarfRust.png" alt="HarfRust Logo" width="256" align="center"/></p>

[![Build Status](https://github.com/harfbuzz/harfrust/actions/workflows/main.yml/badge.svg)](https://github.com/harfbuzz/harfrust/actions/workflows/main.yml)
[![Crates.io](https://img.shields.io/crates/v/harfrust.svg)](https://crates.io/crates/harfrust)
[![Documentation](https://docs.rs/harfrust/badge.svg)](https://docs.rs/harfrust)

</div>

HarfRust is a Rust port of [HarfBuzz](https://github.com/harfbuzz/harfbuzz) text shaping engine.
See [Major changes](#major-changes) below for major differences between HarfRust and HarfBuzz.

HarfRust started as a fork of [RustyBuzz](https://docs.rs/rustybuzz) to explore porting from `ttf-parser` to
[`read-fonts`](https://docs.rs/read-fonts) to avoid shipping (and maintaining)
multiple implementations of core font parsing for [`skrifa`](https://docs.rs/skrifa) consumers.
Further context in https://github.com/googlefonts/fontations/issues/956.

Matches HarfBuzz [v15.0.0](https://github.com/harfbuzz/harfbuzz/releases/tag/15.0.0).

## Why?

https://github.com/googlefonts/oxidize outlines Google Fonts motivations to try to migrate font
production and consumption to Rust.

## Major changes

- No font size property. Shaping is always using UnitsPerEm. You should scale the result manually.
- Most of the font loading and parsing is done using [`read-fonts`](https://docs.rs/read-fonts).
- HarfRust doesn't provide any integration with external libraries, so no FreeType, CoreText, or Uniscribe/DirectWrite font-loading integration, and no ICU, or GLib Unicode-functions integration, as well as no `graphite2` library support.
- No `graphite` font support.

## Conformance

The following conformance issues need to be fixed:

- HarfRust for the most part passes the HarfBuzz test and fuzzing suites, but there are some known issues. See [HARFBUZZ.md](./HARFBUZZ.md) for details.
- Malformed fonts will cause an error. HarfBuzz uses fallback/dummy shaper in this case.
- Experimental HarfBuzz features like most of the boring-expansion-spec are not supported yet.

## Performance

HarfRust is less than 25% slower than HarfBuzz on most common fonts. For a comparison see this [spreadsheet][3].
You can run `cargo bench` to see the performance of HarfRust on your machine.


## Tracing shaping

Enable the opt-in `tracing` Cargo feature to receive HarfBuzz shaping messages:

```toml
harfrust = { version = "0.15", features = ["tracing"] }
```

Install a callback before shaping with
`buffer.set_message_function(Some(Box::new(callback)))`. It receives the public
`Buffer`, `ShaperFont`, and message text, and can inspect or serialize
intermediate Unicode and glyph contents. Return `true` to continue shaping, or
`false` from a skippable `start` message to skip that operation. Per-glyph and
recursion messages are informational and ignore the return value. Mutable closures
are supported; callbacks own their captures and must be `Send + Sync + 'static`.
Use `buffer.set_message_function(None)` to remove a callback.

Tracing also works with `no_std` and `libm`. Without the `tracing` feature, the
callback storage, message formatting, and intermediate snapshots compile out.
The `hr-shape` tool enables tracing and exposes `--trace` to print messages and
intermediate buffer contents to stderr. Its library rendering API includes traces
in the returned string. Per-glyph messages follow HarfBuzz's detailed trace mode
(`HB_BUFFER_MESSAGE_MORE=1`), which release builds of HarfBuzz normally disable.

## Notes about the port

HarfRust is not a full port of HarfBuzz. HarfBuzz (C++ edition) can roughly be split into 6 parts:

1. shaping, ported to HarfRust
2. Unicode routines, ported to HarfRust
3. font parsing, handled by [`read-fonts`](https://docs.rs/read-fonts)
4. subsetting, handled by [`skera`](https://github.com/googlefonts/fontations/tree/main/skera)
5. custom containers and utilities (HarfBuzz doesn't use C++ standard library), reimplemented in [`fontations`](https://github.com/googlefonts/fontations) where appropriate (e.g. int set)
6. glue for system/3rd party libraries, not ported

## Safety

The library is completely safe.

There are no `unsafe` in this library and in most of its dependencies (excluding `bytemuck`).

## Releasing

* Edit CHANGELOG.md following the conventions in that file, at least
  * Add information about which HarfBuzz release is tracked
  * Update diff links at the bottom
  * Determine diff from previous release and
  * Summarize changes
  * Determine new version number according to SemVer
    Note that increasing a dependency version to a new major release
    means a breaking change for this crate.
* Update the release version following SemVer:
  * Cargo.toml, in TWO places: `version` under `[workspace.package]` and the
    `harfrust` entry under `[workspace.dependencies]`
  * The dependency example in this README
  * The C API's copy of it: `HR_VERSION_*` in `harfrust_capi/src/shape.rs`
    and `HR_VERSION_STRING` in `harfrust_capi/cbindgen.toml`
* If the matched HarfBuzz release changed, update it in:
  * This README ("Matches HarfBuzz ...") and CHANGELOG.md
  * `HR_HARFBUZZ_VERSION_*` and `hr_harfbuzz_version_string` in
    `harfrust_capi/src/shape.rs`
  * `HR_HARFBUZZ_VERSION_STRING` in `harfrust_capi/cbindgen.toml`
  * `harfrust_capi/README.md` and `harfrust_capi/tests/skia_version.rs`
* Regenerate `harfrust_capi/include/hr.h` with cbindgen, as described in
  `harfrust_capi/README.md`, then run `scripts/gen-hb-compat-header.py`.
  `cargo test -p harfrust_capi` checks that the header's versions match.
* Run cargo-semver-checks:
  * `cargo semver-checks check-release -p harfrust`
  * `cargo semver-checks check-release -p hr-shape`
* Commit to a branch and file a PR
* Get PR reviewed and land
* Pull/Rebase local main checkout, then
  * `git tag` release version, e.g. `git tag 0.7.1`
  * `git push origin <tag>`
* Then cargo publish:
  * **IMPORTANT**: Prefer to publish from an OS that supports symlinks.
    When publishing from an OS that does not understand symlinks produced by git,
    version files in crates.io might turn into text files showing the symlink's target.
  * `cargo publish -p harfrust`
  * `cargo publish -p hr-shape`
* Go to GitHub releases page, copy commit message and release tagged version.

## Developer documents

For notes on the backporting process of HarfBuzz code, see [docs/backporting.md](docs/backporting.md).

For notes on generating state machine using `ragel`, see [docs/ragel.md](docs/ragel.md).

The following HarfBuzz _studies_ are relevant to HarfRust development:

- 2025 - [Introducing HarfRust][2]
- 2025 – [Caching][1]

## License

HarfRust is licensed under the **MIT** license.

HarfBuzz is [licensed](https://github.com/harfbuzz/harfbuzz/blob/main/COPYING) under the **Old MIT**

[1]: https://docs.google.com/document/d/1_VgObf6Je0J8byMLsi7HCQHnKo2emGnx_ib_sHo-bt4/preview
[2]: https://docs.google.com/document/d/1aH_waagdEM5UhslQxCeFEb82ECBhPlZjy5_MwLNLBYo/preview
[3]: https://docs.google.com/spreadsheets/d/1lyPPZHXIF8gE0Tpx7_IscwhwaZa4KOpdt7vnV0jQT9o/preview
