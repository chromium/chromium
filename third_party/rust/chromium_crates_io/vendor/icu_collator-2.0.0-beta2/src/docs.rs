// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module exists to contain implementation docs and notes for people who want to contribute.
//!
//! # Contributor Notes
//!
//! ## Development environment (on Linux) for fuzzing and generating data
//!
//! These notes assume that ICU4X itself has been cloned to `$PROJECTS/icu4x`.
//!
//! Clone ICU4C from <https://github.com/hsivonen/icu> to `$PROJECTS/icu` and switch
//! to the branch `icu4x-collator`.
//!
//! Create a directory `$PROJECTS/localicu`
//!
//! Create a directory `$PROJECTS/icu-build` and `cd` into it.
//!
//! Run `../icu/icu4c/source/runConfigureICU --enable-debug Linux --prefix $PROJECTS/localicu --enable-static`
//!
//! Run `make`
//!
//! ### Generating data
//!
//!
//!
//! ### Testing
//!
//! `cargo test --features serde`
//!
//! Note: some tests depend on collation test data files.
//! These files are copied from the ICU and CLDR codebases,
//! and they are stored in `tests/data/`.
//! New versions of collation data from CLDR/ICU are kept in sync with these collation test data files.
//! When updating ICU4X to pick up new Unicode data, including collation data, from ICU,
//! the copies of collation test data files in maintained in ICU4X's icu::collator will need to be overridden with their newer corresponding versions.
//! See the Readme in `/tests/data/README.md` for details.
//!
//! ### Fuzzing
//!
//! `cargo install cargo-fuzz`
//!
//! Clone `rust_icu` from <https://github.com/google/rust_icu> to `$PROJECTS/rust_icu`.
//!
//! In `$PROJECTS/icu-build` run `make install`.
//!
//! `cd $PROJECTS/icu4x/components/collator`
//!
//! Run the fuzzer until a panic:
//!
//! `PKG_CONFIG_PATH="$PROJECTS/localicu/lib/pkgconfig" PATH="$PROJECTS/localicu/bin:$PATH" LD_LIBRARY_PATH="/$PROJECTS/localicu/lib" RUSTC_BOOTSTRAP=1 cargo +stable fuzz run compare_utf16`
//!
//! Once there is a panic, recompile with debug symbols by adding `--dev`:
//!
//! `PKG_CONFIG_PATH="$PROJECTS/localicu/lib/pkgconfig" PATH="$PROJECTS/localicu/bin:$PATH" LD_LIBRARY_PATH="$PROJECTS/localicu/lib" RUSTC_BOOTSTRAP=1 cargo +stable fuzz run --dev compare_utf16 fuzz/artifacts/compare_utf16/crash-$ARTIFACTHASH`
//!
//! Record with
//!
//! `LD_LIBRARY_PATH="$PROJECTS/localicu/lib" rr fuzz/target/x86_64-unknown-linux-gnu/debug/compare_utf16 -artifact_prefix=$PROJECTS/icu4x/components/collator/fuzz/artifacts/compare_utf16/ fuzz/artifacts/compare_utf16/crash-$ARTIFACTHASH`
//!
//! # Design notes
//!
//! * The collation element design comes from ICU4C. Some parts of the ICU4C design, notably,
//!   `Tag::BuilderDataTag`, `Tag::LeadSurrogateTag`, `Tag::LatinExpansionTag`, `Tag::U0000Tag`,
//!   and `Tag::HangulTag` are unused.
//!   - `Tag::LatinExpansionTag` might be reallocated to search expansions for archaic jamo
//!     in the future.
//!   - `Tag::HangulTag` might be reallocated to compressed hanja expansions in the future.
//!     See [issue 1315](https://github.com/unicode-org/icu4x/issues/1315).
//! * The key design difference between ICU4C and ICU4X is that ICU4C puts the canonical
//!   closure in the data (larger data) to enable lookup directly by precomposed characters
//!   while ICU4X always omits the canonical closure and always normalizes to NFD on the fly.
//! * Compared to ICU4C, normalization cannot be turned off. There also isn't a separate
//!   "Fast Latin" mode.
//! * The normalization is fused into the collation element lookup algorithm to optimize the
//!   case where an input character decomposes into two BMP characters: a base letter and a
//!   diacritic.
//!   - To optimize away a trie lookup when the combining diacritic doesn't contract,
//!     there is a linear lookup table for the combining diacritics block. Three languages
//!     tailor diacritics: Ewe, Lithuanian, and Vietnamese. Vietnamese and Ewe load an
//!     alternative table. The Lithuanian special cases are hard-coded and activatable by
//!     a metadata bit.
//! * Unfortunately, contractions that contract starters don't fit this model nicely. Therefore,
//!   there's duplicated normalization code for normalizing the lookahead for contractions.
//!   This code can, in principle, do duplicative work, but it shouldn't be excessive with
//!   real-world inputs.
//! * As a result, in terms of code provenance, the algorithms come from ICU4C, except the
//!   normalization part of the code is novel to ICU4X, and the contraction code is custom
//!   to ICU4X despite being informed by ICU4C.
//! * The way input characters are iterated over and resulting collation elements are
//!   buffered is novel to ICU4X.
//! * ICU4C can iterate backwards but ICU4X cannot. ICU4X keeps a buffer of the two most
//!   recent characters for handling prefixes. As of CLDR 40, there were only two kinds
//!   of prefixes: a single starter and a starter followed by a kana voicing mark.
//! * ICU4C sorts unpaired surrogates in their lexical order. ICU4X operates on Unicode
//!   [scalar values](https://unicode.org/glossary/#unicode_scalar_value) (any Unicode
//!   code point except high-surrogate and low-surrogate code points), so unpaired
//!   surrogates sort as REPLACEMENT CHARACTERs. Therefore, all unpaired
//!   surrogates are equal with each other.
//! * Skipping over a bit-identical prefix and then going back over "backward-unsafe"
//!   characters is currently unimplemented but isn't architecturally precluded.
//! * Hangul is handled specially:
//!   - Precomposed syllables are checked for as the first step of processing an
//!     incoming character.
//!   - Individual jamo are lookup up from a linear table instead of a trie. Unlike
//!     in ICU4C, this table covers the whole Unicode block whereas in ICU4C it covers
//!     only modern jamo for use in decomposing the precomposed syllables. The point
//!     is that search collations have a lot of duplicative (across multiple search)
//!     collations data for making archaic jamo searchable by modern jamo.
//!     Unfortunately, the shareable part isn't currently actually shareable, because
//!     the tailored CE32s refer to the expansions table in each collation. To make
//!     them truly shareable, the archaic jamo expansions need to become self-contained
//!     the way Latin mini expansions in ICU4C are self-contained.
//!
//!     One possible alternative to loading a different table for "search" would be
//!     performing the mapping of archaic jamo to the modern approximations as a
//!     special preprocessing step for the incoming characters, which would allow
//!     the lookup of the resulting modern jamo from the normal root jamo table.
//!
//!     "searchjl" is even more problematic than "search", since "searchjl" uses
//!     prefixes matches with jamo, and currently Hangul is assumed not to participate
//!     in prefix or contraction matching.
//!
//! # Notes about index generation
//!
//! ICU4X currently does not have code or data for generating [collation
//! indexes](https://www.unicode.org/reports/tr35/tr35-collation.html#Collation_Indexes).
//!
//! On the data side, ICU4X doesn't have data for `<exemplarCharacters type="index">`
//! (or when that's missing, plain `<exemplarCharacters>`).
//!
//! Of the collations, `zh-u-co-pinyin`, `zh-u-co-stroke`, `zh-u-co-zhuyin`, and
//! `*-u-co-unihan` are special: They bake a contraction of U+FDD0 and an index
//! character in the collation order. ICU4X collation data already includes this.
//! For `*-u-co-unihan` this index character data is repeated in all three tailorings
//! instead of being in the root. If it was in the root, code for extracting the
//! index characters from the collation data would need to avoid confusing the
//! `unihan` index contractions (if they were in the root) and the `zh-u-co-pinyin`,
//! `zh-u-co-stroke`, and `zh-u-co-zhuyin` in the tailoring. This seems feasible,
//! but isn't how CLDR and ICU4C do it. (If the index characters for
//! `*-u-co-unihan` were in the root, `ko-u-co-unihan` would become a mere
//! script reordering.)
//!
//! It's unclear how useful it would be size-wise to have code to extract the
//! index characters from the collations: For `zh-u-co-pinyin`, `zh-u-co-stroke`,
//! `zh-u-co-zhuyin`, the index characters are contiguous ranges that could be
//! efficiently stored as start and end. Moreover, the in-data index character
//! for `stroke` isn't the label to be rendered to the user, so special-casing
//! is needed anyway.
//!
//! This means that there's a tradeoff between having duplicate data (relative to
//! the collation tailorings) for the `unihan` index character list vs. having
//! code for extracting the list from the tailorings. It's not at all clear that
//! having the code is better for size than having the list of 238 ideographs
//! itself as data (476 bytes as UTF-16).
//!
//! Note: Investigate [#2723](https://github.com/unicode-org/icu4x/issues/2723)
