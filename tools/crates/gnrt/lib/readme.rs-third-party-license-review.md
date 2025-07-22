## Quick note how to review license-related changes in `readme.rs`

For changes that add support for a new license kind, please follow the
process described in the "License" section of `//docs/adding_to_third_party.md`.
[An old revision of that doc here](https://source.chromium.org/chromium/chromium/src/+/main:docs/adding_to_third_party.md;l=417-419;drc=c545ffce17a6aba55825a9512db922b064b10dd6)
asks to check if the new license has been
[allowlisted](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:metadata/fields/custom/license_allowlist.py) in a `depot_tools` script.  That old revision
also listed license kinds that should be allowed in Chromium (e.g.
permissive licenses or reciprocal licenses) - whether a license is classified
as permissive or reciprocal can be checked in the internal go/licensetable.

For benign changes (e.g.
[adding](https://chromium-review.googlesource.com/c/chromium/src/+/5773797/2/tools/crates/gnrt/lib/readme.rs)
a new free-form spelling of "MIT or Apache-2")
there is no need for any additional review.
