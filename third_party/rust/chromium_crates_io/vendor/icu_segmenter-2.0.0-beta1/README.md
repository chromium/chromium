# icu_segmenter [![crates.io](https://img.shields.io/crates/v/icu_segmenter)](https://crates.io/crates/icu_segmenter)

<!-- cargo-rdme start -->

Segment strings by lines, graphemes, words, and sentences.

This module is published as its own crate ([`icu_segmenter`](https://docs.rs/icu_segmenter/latest/icu_segmenter/))
and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.

This module contains segmenter implementation for the following rules.

- Line segmenter that is compatible with [Unicode Standard Annex #14][UAX14], _Unicode Line
  Breaking Algorithm_, with options to tailor line-breaking behavior for CSS [`line-break`] and
  [`word-break`] properties.
- Grapheme cluster segmenter, word segmenter, and sentence segmenter that are compatible with
  [Unicode Standard Annex #29][UAX29], _Unicode Text Segmentation_.

[UAX14]: https://www.unicode.org/reports/tr14/
[UAX29]: https://www.unicode.org/reports/tr29/
[`line-break`]: https://drafts.csswg.org/css-text-3/#line-break-property
[`word-break`]: https://drafts.csswg.org/css-text-3/#word-break-property

## Examples

### Line Break

Find line break opportunities:

```rust
use icu::segmenter::LineSegmenter;

let segmenter = LineSegmenter::new_auto();

let breakpoints: Vec<usize> = segmenter
    .segment_str("Hello World. Xin chào thế giới!")
    .collect();
assert_eq!(&breakpoints, &[0, 6, 13, 17, 23, 29, 36]);
```

See [`LineSegmenter`] for more examples.

### Grapheme Cluster Break

Find all grapheme cluster boundaries:

```rust
use icu::segmenter::GraphemeClusterSegmenter;

let segmenter = GraphemeClusterSegmenter::new();

let breakpoints: Vec<usize> = segmenter
    .segment_str("Hello World. Xin chào thế giới!")
    .collect();
assert_eq!(
    &breakpoints,
    &[
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
        19, 21, 22, 23, 24, 25, 28, 29, 30, 31, 34, 35, 36
    ]
);
```

See [`GraphemeClusterSegmenter`] for more examples.

### Word Break

Find all word boundaries:

```rust
use icu::segmenter::WordSegmenter;

let segmenter = WordSegmenter::new_auto();

let breakpoints: Vec<usize> = segmenter
    .segment_str("Hello World. Xin chào thế giới!")
    .collect();
assert_eq!(
    &breakpoints,
    &[0, 5, 6, 11, 12, 13, 16, 17, 22, 23, 28, 29, 35, 36]
);
```

See [`WordSegmenter`] for more examples.

### Sentence Break

Segment the string into sentences:

```rust
use icu::segmenter::SentenceSegmenter;

let segmenter = SentenceSegmenter::new();

let breakpoints: Vec<usize> = segmenter
    .segment_str("Hello World. Xin chào thế giới!")
    .collect();
assert_eq!(&breakpoints, &[0, 13, 36]);
```

See [`SentenceSegmenter`] for more examples.

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
