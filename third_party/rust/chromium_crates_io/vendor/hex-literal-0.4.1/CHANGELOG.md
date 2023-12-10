# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## 0.4.1 (2023-04-05)
### Changed
- Enforce const evaluation ([#889])

[#889]: https://github.com/RustCrypto/utils/pull/889

## 0.4.0 (2023-04-02)
### Changed
- Disallow comments inside hex strings ([#816])
- Migrate to 2021 edition and bump MSRV to 1.57 ([#816])
- Use CTFE instead of proc macro ([#816])

[#816]: https://github.com/RustCrypto/utils/pull/816

## 0.3.4 (2021-11-11)
### Changed
- Provide more info in `panic!` messages ([#664])
- Minor changes in the comments filtration code ([#666])

### Added
- New tests for the `hex!()` macro and internal documentation ([#664])

### Fixed
- Make `hex!()` error when forward slash encountered as last byte ([#665])

[#664]: https://github.com/RustCrypto/utils/pull/664
[#665]: https://github.com/RustCrypto/utils/pull/665
[#666]: https://github.com/RustCrypto/utils/pull/666

## 0.3.3 (2021-07-17)
### Added
- Accept sequence of string literals ([#519])

[#519]: https://github.com/RustCrypto/utils/pull/519

## 0.3.2 (2021-07-02)
### Added
- Allow line (`//`) and block (`/* */`) comments ([#512])

[#512]: https://github.com/RustCrypto/utils/pull/512

## 0.3.1 (2020-08-01)
### Added
- Documentation for the `hex!` macro ([#73])

[#73]: https://github.com/RustCrypto/utils/pull/73

## 0.3.0 (2020-07-16)
### Changed
- MSRV bump to 1.45 ([#53])

[#53]: https://github.com/RustCrypto/utils/pull/53
