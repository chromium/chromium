# icu_normalizer [![crates.io](https://img.shields.io/crates/v/icu_normalizer)](https://crates.io/crates/icu_normalizer)

<!-- cargo-rdme start -->

Normalizing text into Unicode Normalization Forms.

This module is published as its own crate ([`icu_normalizer`](https://docs.rs/icu_normalizer/latest/icu_normalizer/))
and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.

## Implementation notes

The normalizer operates on a lazy iterator over Unicode scalar values (Rust `char`) internally
and iterating over guaranteed-valid UTF-8, potentially-invalid UTF-8, and potentially-invalid
UTF-16 is a step that doesn’t leak into the normalizer internals. Ill-formed byte sequences are
treated as U+FFFD.

The normalizer data layout is not based on the ICU4C design at all. Instead, the normalization
data layout is a clean-slate design optimized for the concept of fusing the NFD decomposition
into the collator. That is, the decomposing normalizer is a by-product of the collator-motivated
data layout.

Notably, the decomposition data structure is optimized for a starter decomposing to itself,
which is the most common case, and for a starter decomposing to a starter and a non-starter
on the Basic Multilingual Plane. Notably, in this case, the collator makes use of the
knowledge that the second character of such a decomposition is a non-starter. Therefore,
decomposition into two starters is handled by generic fallback path that looks the
decomposition from an array by offset and length instead of baking a BMP starter pair directly
into a trie value.

The decompositions into non-starters are hard-coded. At present in Unicode, these appear
to be special cases falling into three categories:

1. Deprecated combining marks.
2. Particular Tibetan vowel sings.
3. NFKD only: half-width kana voicing marks.

Hopefully Unicode never adds more decompositions into non-starters (other than a character
decomposing to itself), but if it does, a code update is needed instead of a mere data update.

The composing normalizer builds on the decomposing normalizer by performing the canonical
composition post-processing per spec. As an optimization, though, the composing normalizer
attempts to pass through already-normalized text consisting of starters that never combine
backwards and that map to themselves if followed by a character whose decomposition starts
with a starter that never combines backwards.

As a difference with ICU4C, the composing normalizer has only the simplest possible
passthrough (only one inversion list lookup per character in the best case) and the full
decompose-then-canonically-compose behavior, whereas ICU4C has other paths between these
extremes. The ICU4X collator doesn't make use of the FCD concept at all in order to avoid
doing the work of checking whether the FCD condition holds.

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
