## Unstable Rust usage in Chromium

This document maintains a list of exceptions from the unstable Rust usage
policy, which is documented in [`docs/rust.md`](../../docs/rust.md).

Note: Before the policy was introduced, several targets were already utilizing
unstable features. Such pre-existing instances (that are not in this list)
should not be considered precedents that justify approval for future use of such
features.

-   `#![feature(portable_simd)]` in the ETC1 encoder:
    https://docs.google.com/document/d/1lh9x43gtqXFh5bP1LeYevWj0EcIRgIu0XGahHY08aeY/edit?tab=t.0
