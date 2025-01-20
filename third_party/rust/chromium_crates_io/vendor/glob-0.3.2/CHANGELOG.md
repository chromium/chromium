# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

## [0.3.2](https://github.com/rust-lang/glob/compare/v0.3.1...v0.3.2) - 2024-12-28

## What's Changed
* Add fs::symlink_metadata to detect broken symlinks by @kyoheiu in https://github.com/rust-lang/glob/pull/105
* Add support for windows verbatim disk paths by @nico-abram in https://github.com/rust-lang/glob/pull/112
* Respect `require_literal_leading_dot` option in `glob_with` method for path components by @JohnTitor in https://github.com/rust-lang/glob/pull/128
* Harden tests for symlink by @JohnTitor in https://github.com/rust-lang/glob/pull/127
* Remove "extern crate" directions from README by @zmitchell in https://github.com/rust-lang/glob/pull/131
* Add FIXME for tempdir by @JohnTitor in https://github.com/rust-lang/glob/pull/126
* Cache information about file type by @Kobzol in https://github.com/rust-lang/glob/pull/135
* Document the behaviour of ** with files by @Wilfred in https://github.com/rust-lang/glob/pull/138
* Add dependabot by @oriontvv in https://github.com/rust-lang/glob/pull/139
* Bump actions/checkout from 3 to 4 by @dependabot in https://github.com/rust-lang/glob/pull/140
* Check only (no longer test) at the MSRV by @tgross35 in https://github.com/rust-lang/glob/pull/151
* Add release-plz for automated releases by @tgross35 in https://github.com/rust-lang/glob/pull/150

## New Contributors
* @kyoheiu made their first contribution in https://github.com/rust-lang/glob/pull/105
* @nico-abram made their first contribution in https://github.com/rust-lang/glob/pull/112
* @zmitchell made their first contribution in https://github.com/rust-lang/glob/pull/131
* @Kobzol made their first contribution in https://github.com/rust-lang/glob/pull/135
* @Wilfred made their first contribution in https://github.com/rust-lang/glob/pull/138
* @oriontvv made their first contribution in https://github.com/rust-lang/glob/pull/139
* @dependabot made their first contribution in https://github.com/rust-lang/glob/pull/140
* @tgross35 made their first contribution in https://github.com/rust-lang/glob/pull/151

**Full Changelog**: https://github.com/rust-lang/glob/compare/0.3.1...0.3.2
