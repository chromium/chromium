semver
======

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/semver-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/semver)
[<img alt="crates.io" src="https://img.shields.io/crates/v/semver.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/semver)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-semver-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K" height="20">](https://docs.rs/semver/1.0.0)
[<img alt="build status" src="https://img.shields.io/github/workflow/status/dtolnay/semver/CI/master?style=for-the-badge" height="20">](https://github.com/dtolnay/semver/actions?query=branch%3Amaster)

A parser and evaluator for Cargo's flavor of Semantic Versioning.

Semantic Versioning (see <https://semver.org>) is a guideline for how version
numbers are assigned and incremented. It is widely followed within the
Cargo/crates.io ecosystem for Rust.

```toml
[dependencies]
semver = "1.0"
```

*Compiler support: requires rustc 1.31+*

<br>

## Example

```rust
use semver::{BuildMetadata, Prerelease, Version, VersionReq};

fn main() {
    let req = VersionReq::parse(">=1.2.3, <1.8.0").unwrap();

    // Check whether this requirement matches version 1.2.3-alpha.1 (no)
    let version = Version {
        major: 1,
        minor: 2,
        patch: 3,
        pre: Prerelease::new("alpha.1").unwrap(),
        build: BuildMetadata::EMPTY,
    };
    assert!(!req.matches(&version));

    // Check whether it matches 1.3.0 (yes it does)
    let version = Version::parse("1.3.0").unwrap();
    assert!(req.matches(&version));
}
```

<br>

## Scope of this crate

Besides Cargo, several other package ecosystems and package managers for other
languages also use SemVer:&ensp;RubyGems/Bundler for Ruby, npm for JavaScript,
Composer for PHP, CocoaPods for Objective-C...

The `semver` crate is specifically intended to implement Cargo's interpretation
of Semantic Versioning.

Where the various tools differ in their interpretation or implementation of the
spec, this crate follows the implementation choices made by Cargo. If you are
operating on version numbers from some other package ecosystem, you will want to
use a different semver library which is appropriate to that ecosystem.

The extent of Cargo's SemVer support is documented in the *[Specifying
Dependencies]* chapter of the Cargo reference.

[Specifying Dependencies]: https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html

<br>

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>
