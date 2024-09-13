<!-- Copyright 2022 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# Internal details

This file documents various internal details of zerocopy and its infrastructure
that consumers don't need to be concerned about. It focuses on details that
affect multiple files, and allows each affected code location to reference this
document rather than requiring us to repeat the same explanation in multiple
locations.

## CI and toolchain versions

In CI (`.github/workflows/ci.yml`), we pin to specific versions or dates of the
stable and nightly toolchains. The reason is twofold: First, our UI tests (see
`tests/trybuild.rs` and `zerocopy-derive/tests/trybuild.rs`) depend on the
format of rustc's error messages, and that format can change between toolchain
versions (we also maintain multiple copies of our UI tests - one for each
toolchain version pinned in CI - for this reason). Second, not all nightlies
have a working Miri, so we need to pin to one that does (see
https://rust-lang.github.io/rustup-components-history/).

Updating the versions pinned in CI may cause the UI tests to break. In order to
fix UI tests after a version update, run:

```
$ TRYBUILD=overwrite ./cargo.sh +all test
```

## Crate versions

We ensure that the crate versions of zerocopy and zerocopy-derive are always the
same in-tree, and that zerocopy depends upon zerocopy-derive using an exact
version match to the current version in-tree. This has the result that, even
when published on crates.io, both crates effectively constitute a single atomic
version. So long as the code in zerocopy is compatible with the code in
zerocopy-derive in the same Git commit, then publishing them both is fine. This
frees us from the normal task of reasoning about compatibility with a range of
semver-compatible versions of different crates.
