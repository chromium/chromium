# icu_decimal [![crates.io](https://img.shields.io/crates/v/icu_decimal)](https://crates.io/crates/icu_decimal)

<!-- cargo-rdme start -->

Formatting basic decimal numbers.

This module is published as its own crate ([`icu_decimal`](https://docs.rs/icu_decimal/latest/icu_decimal/))
and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.

Support for currencies, measurement units, and compact notation is planned. To track progress,
follow [icu4x#275](https://github.com/unicode-org/icu4x/issues/275).

## Examples

### Format a number with Bangla digits

```rust
use fixed_decimal::FixedDecimal;
use icu::decimal::FixedDecimalFormatter;
use icu::locale::locale;
use writeable::assert_writeable_eq;

let fdf = FixedDecimalFormatter::try_new(
    locale!("bn").into(),
    Default::default(),
)
.expect("locale should be present");

let fixed_decimal = FixedDecimal::from(1000007);

assert_writeable_eq!(fdf.format(&fixed_decimal), "১০,০০,০০৭");
```

### Format a number with digits after the decimal separator

```rust
use fixed_decimal::FixedDecimal;
use icu::decimal::FixedDecimalFormatter;
use icu::locale::Locale;
use writeable::assert_writeable_eq;

let fdf =
    FixedDecimalFormatter::try_new(Default::default(), Default::default())
        .expect("locale should be present");

let fixed_decimal = FixedDecimal::from(200050).multiplied_pow10(-2);

assert_writeable_eq!(fdf.format(&fixed_decimal), "2,000.50");
```

#### Format a number using an alternative numbering system

Numbering systems specified in the `-u-nu` subtag will be followed as long as the locale has
symbols for that numbering system.

```rust
use fixed_decimal::FixedDecimal;
use icu::decimal::FixedDecimalFormatter;
use icu::locale::locale;
use writeable::assert_writeable_eq;

let fdf = FixedDecimalFormatter::try_new(
    locale!("th-u-nu-thai").into(),
    Default::default(),
)
.expect("locale should be present");

let fixed_decimal = FixedDecimal::from(1000007);

assert_writeable_eq!(fdf.format(&fixed_decimal), "๑,๐๐๐,๐๐๗");
```

[`FixedDecimalFormatter`]: FixedDecimalFormatter

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
