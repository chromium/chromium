# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.11](https://github.com/rust-lang/flate2-rs/compare/v1.1.10...v1.1.11) - 2026-11-06

### Fixed

- fixup! Complete sync flushes across output buffer boundaries ([#581](https://github.com/rust-lang/flate2-rs/pull/581))
- Separate gzip header reads from compressed data
- Defer gzip input errors without losing read progress
- Preserve gzip header progress on input read errors ([#571](https://github.com/rust-lang/flate2-rs/pull/571))

### Other

- Merge pull request #582 from rust-lang/fix-lacking-buffer-drain
- Merge pull request #574 from Sharadhi-v98/decompress-reset-window-bits
- Merge pull request #577 from rust-lang/fix-loop
- Setup trusted publishing for the flate2 crate
- Complete sync flushes across output buffer boundaries ([#581](https://github.com/rust-lang/flate2-rs/pull/581))
- Fix "can by extended" typos in deflate and zlib write docs
- Merge pull request #579 from NUAAwqy/docs/fix-double-space
- Fix double space in `read` module doc comment
- Add missing preposition in README description
- Cover decoder retries across the output buffer boundary
- Drain output before refilling the buffer
- Clarify partial-write buffering behavior
- Avoid shifting buffered output on partial writes
- Merge pull request #567 from ndossche/more-asserts
- Merge pull request #566 from HDauven/reject-incomplete-write-decoder-streams
- Fix encoder writes hanging after try_finish ([#576](https://github.com/rust-lang/flate2-rs/pull/576))
- Avoid panicking when dropping unfinished compressors
- Merge pull request #565 from HDauven/zlib-rs-uninit-output
- still allow resetting incomplete write decoders
- Assert return result of reset calls
- Reject incomplete streams in write decoders
- Merge pull request #573 from NUAAwqy/taiyi/fix-gzip-read-error-header
- review
- Reject incomplete streams when finishing write-side decoders
- Preserve window size and gzip mode across Decompress::reset
- preserve runtime detection in backend examples ([#570](https://github.com/rust-lang/flate2-rs/pull/570))
- Make miniz_oxide dictionary clearing explicit on reset
- Harden miniz_oxide decompressor reset
- Deduplicate zlib-rs flush conversions
- Use native zlib-rs uninitialized output
