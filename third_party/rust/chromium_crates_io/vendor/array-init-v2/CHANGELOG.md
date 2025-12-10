# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## 2.1.0
### Added
- Introduced an MSRV: Rust 1.51
- Added `map_array_init` function ([#38](https://github.com/Manishearth/array-init/pull/38))

## v2.0.1 (2022-06-24)
### Added
- Added `from_iter_reversed` function ([#30](https://github.com/Manishearth/array-init/issues/30))

## v2.0.0 (2021-03-29)
### Breaking
- Removed `IsArray` trait (not necessary anymore with const generics)

## v1.1.0 (yanked)
### Breaking
- Removed `const-generics` feature flag. The MSRV is now rust 1.51

## v1.0.0 (2020-10-14)
### Added
  - Added a `try_array_init` function which initializes an array with a callable that may fail.
  - Added a `const-generics` feature which uses rust (unstable) `const-generics` feature to implement the initializer functions for all array sizes.
  - Added documentation
