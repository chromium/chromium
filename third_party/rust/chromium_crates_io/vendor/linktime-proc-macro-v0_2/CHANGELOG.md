# Changelog

All notable changes to this crate will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.3] - 2026-08-09

### Fixed

- Hashes now always accumulate 64-bit-sized chunks, keeping generated names
  consistent across platforms (https://github.com/mmastrac/linktime/pull/508).
- Span token paths are no longer assumed consistent between the local crate and
  depending crates, fixing a cross-crate section-name error on Rust 1.88-1.94
  (https://github.com/mmastrac/linktime/pull/509).

## [0.2.2] - 2026-07-29

### Changed

- Test-only adjustments to full crate path handling (#506).

## [0.2.1] - 2026-07-29

### Changed

- Make generated hashes more stable with respect to link-section token ids
  (#495).
