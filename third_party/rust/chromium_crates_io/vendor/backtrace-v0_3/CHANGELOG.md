# Changelog

Notable changes to this project should be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
The backtrace crate attempts to adhere to the modified [Cargo interpretation of SemVer](https://doc.rust-lang.org/cargo/reference/resolver.html#semver-compatibility).
As a unique component of `std` it may make exceptional changes in order to support `std`.

## [Unreleased]

## [0.3.77](https://github.com/rust-lang/backtrace-rs/compare/backtrace-v0.3.76...backtrace-v0.3.77) - 2026-10-27

### Platform Support
- [AIX] Update backtrace to account for libc changes and fix XCOFF parsing
- Swap `WaitForSingleObjectEx` with `WaitForSingleObject` and check result
- Always use `extern "system"` for Windows imports
- Micro-optimize code size of /proc/maps parsing code
- simplify scan for xcoff .text section
- deny(unsafe_op_in_unsafe_fn) in backtrace/win32.rs
- GetCurrentProcess only once

### Dependencies and versioning
- Update ruzstd to 0.9.1
- Disable fuchsia dependencies on `backtrace` when `backtrace` is disabled (rust-lang/backtrace-rs#776)
- Update `object` to 0.40.0
- Upgrade addr2line, libloading, miniz_oxide
- Bump rustc-demangle
- update to edition 2024 and MSRV 1.88

### CI and publishing
- A bunch of CI work motivated by `zizmor`
- Remove redundant `homepage` from Cargo.toml
- Switch to trusted publishing
- Test i686-msvc as a target only
- Fix apt-get install failures in CI
- Fix accuracy test with new cargo build layout
- don't test -no_uuid in CI
- Fix line-tables-only on macOS 15
- Fix `function_casts_as_integer` failures

## [0.3.76](https://github.com/rust-lang/backtrace-rs/compare/backtrace-v0.3.75...backtrace-v0.3.76) - 2025-09-26

### Behavior
- Fix inverted polarity of "full printing" logic in rust-lang/backtrace-rs#726:
  Previously we used to do the opposite of what you would expect.

### Platform Support

- Windows: Removed hypothetical soundness risk from padding bytes in rust-lang/backtrace-rs#737
- Fuchsia: Added appropriate alignment checks during `Elf_Nhdr` parsing in rust-lang/backtrace-rs#725
- Cygwin: Added support in rust-lang/backtrace-rs#704
- Windows (32-bit Arm): Restore support in rust-lang/backtrace-rs#685
- NuttX (32-bit Arm): Use builtin `_Unwind_GetIP` in rust-lang/backtrace-rs#692
- RTEMS: Enable libunwind in rust-lang/backtrace-rs#682

### Dependencies

- Update cpp_demangle to 0.5 in rust-lang/backtrace-rs#732
- Update memchr to 2.7.6 in rust-lang/backtrace-rs#734
- Switch from windows-targets to windows-link in rust-lang/backtrace-rs#727
- Update ruzstd to 0.8.1 in rust-lang/backtrace-rs#718
- Update object to 0.37 in rust-lang/backtrace-rs#718
- Update addr2line to 0.25 in rust-lang/backtrace-rs#718
