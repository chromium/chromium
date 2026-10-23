// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[cfg(test)]
mod tests {
    use icu::locale::locale;
    use icu::locale::preferences::extensions::unicode::keywords::currency;
    use writeable::assert_writeable_eq;

    use crate::dimension::currency::{
        formatter::CurrencyFormatter,
        options::{CurrencyFormatterOptions, CurrencyUsage},
    };

    #[test]
    pub fn test_en_us() {
        let prefs = locale!("en-US").into();
        let currency_code = currency!("USD");
        let accounting = CurrencyFormatterOptions {
            usage: CurrencyUsage::Accounting,
            ..Default::default()
        };
        let fmt =
            CurrencyFormatter::try_new_compact_symbol(prefs, currency_code, Default::default())
                .unwrap();
        let fmt_accounting =
            CurrencyFormatter::try_new_compact_symbol(prefs, currency_code, accounting).unwrap();
        let fmt_narrow = CurrencyFormatter::try_new_compact_symbol_narrow(
            prefs,
            currency_code,
            Default::default(),
        )
        .unwrap();
        let fmt_narrow_accounting =
            CurrencyFormatter::try_new_compact_symbol_narrow(prefs, currency_code, accounting)
                .unwrap();

        // Positive cases
        let positive_value = "12345.67".parse().unwrap();
        assert_writeable_eq!(fmt.format_fixed_decimal(&positive_value), "$12K");
        assert_writeable_eq!(fmt_accounting.format_fixed_decimal(&positive_value), "$12K");
        assert_writeable_eq!(fmt_narrow.format_fixed_decimal(&positive_value), "$12K");
        assert_writeable_eq!(
            fmt_narrow_accounting.format_fixed_decimal(&positive_value),
            "$12K"
        );

        let val_1299 = "1299".parse().unwrap();
        assert_writeable_eq!(fmt.format_fixed_decimal(&val_1299), "$1.3K");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        assert_writeable_eq!(fmt.format_fixed_decimal(&negative_value), "-$12K");
        assert_writeable_eq!(
            fmt_accounting.format_fixed_decimal(&negative_value),
            "($12K)"
        );
        assert_writeable_eq!(fmt_narrow.format_fixed_decimal(&negative_value), "-$12K");
        assert_writeable_eq!(
            fmt_narrow_accounting.format_fixed_decimal(&negative_value),
            "($12K)"
        );
    }

    #[test]
    pub fn test_fr_fr() {
        let prefs = locale!("fr-FR").into();
        let currency_code = currency!("EUR");
        let fmt =
            CurrencyFormatter::try_new_compact_symbol(prefs, currency_code, Default::default())
                .unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12\u{a0}k\u{a0}€");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12\u{a0}k\u{a0}€");
    }

    #[test]
    pub fn test_zh_cn() {
        let prefs = locale!("zh-CN").into();
        let currency_code = currency!("CNY");
        let fmt =
            CurrencyFormatter::try_new_compact_symbol(prefs, currency_code, Default::default())
                .unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "¥1.2万");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-¥1.2万");
    }

    #[test]
    pub fn test_ar_eg() {
        let prefs = locale!("ar-EG").into();
        let currency_code = currency!("EGP");
        let fmt =
            CurrencyFormatter::try_new_compact_symbol(prefs, currency_code, Default::default())
                .unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        // TODO(#6064)
        assert_writeable_eq!(formatted_currency, "\u{200f}١٢\u{a0}ألف\u{a0}ج.م.\u{200f}"); //  "ج.م.١٢ألف"

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        // TODO(#6064)
        assert_writeable_eq!(
            formatted_currency,
            "\u{61c}-\u{200f}١٢\u{a0}ألف\u{a0}ج.م.\u{200f}"
        );
    }
    #[test]
    pub fn test_en_us_long() {
        let prefs = locale!("en-US").into();

        let currency_code = currency!("USD");
        let fmt = CurrencyFormatter::try_new_compact_long_name(prefs, currency_code).unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12 thousand US dollars");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12 thousand US dollars");
    }

    #[test]
    pub fn test_en_us_millions_long() {
        let prefs = locale!("en-US").into();

        let currency_code = currency!("USD");
        let fmt = CurrencyFormatter::try_new_compact_long_name(prefs, currency_code).unwrap();

        // Positive case
        let positive_value = "12345000.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12 million US dollars");

        // Negative case
        let negative_value = "-12345000.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12 million US dollars");
    }

    #[test]
    pub fn test_fr_fr_long() {
        let prefs = locale!("fr-FR").into();

        let currency_code = currency!("USD");
        let fmt = CurrencyFormatter::try_new_compact_long_name(prefs, currency_code).unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12 mille dollars des États-Unis");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12 mille dollars des États-Unis");
    }

    #[test]
    pub fn test_fr_fr_millions_long() {
        let prefs = locale!("fr-FR").into();

        let currency_code = currency!("USD");
        let fmt = CurrencyFormatter::try_new_compact_long_name(prefs, currency_code).unwrap();

        // Positive case
        let positive_value = "12345000.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12 millions dollars des États-Unis");

        // Negative case
        let negative_value = "-12345000.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12 millions dollars des États-Unis");
    }

    #[test]
    pub fn test_alpha_next_to_number_and_small_numbers() {
        let prefs = locale!("en-US").into();
        let usd = currency!("USD");
        let sek = currency!("SEK");

        let fmt_usd =
            CurrencyFormatter::try_new_compact_symbol(prefs, usd, Default::default()).unwrap();
        let fmt_sek =
            CurrencyFormatter::try_new_compact_symbol(prefs, sek, Default::default()).unwrap();

        // Small number (magnitude < 3, no compact suffix): should fall back cleanly
        let small_value = "123".parse().unwrap();
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&small_value), "$123");
        assert_writeable_eq!(fmt_sek.format_fixed_decimal(&small_value), "SEK\u{a0}123");

        // Compact number with alphabetical currency code: should use alpha_next_to_number pattern with non-breaking space
        let compact_value = "12345.67".parse().unwrap();
        assert_writeable_eq!(fmt_sek.format_fixed_decimal(&compact_value), "SEK\u{a0}12K");
    }

    #[test]
    pub fn test_tr35_rule_3_below_1000() {
        // Per UTS #35 (LDML Part 3: Numbers, Section 3.8, Rule 3 in Compact Number Formatting):
        // "If the element value of P is '0', then use the corresponding non-compact number formatting instead,"
        //
        // Specifically, for numbers below 1,000 (which map to the uncompacted pattern "0"):
        // * Per UTS #35: Suffix division and compact abbreviation steps (like "K" or "M") are skipped in favor
        //   of standard non-compact pattern structures.
        // * Per current ICU4X `CompactDecimalFormatter` implementation: Maximum fractional digits are trimmed to 0
        //   by default, stripping trailing zeros (so 12 USD formats as "$12" instead of "$12.00", and 990 USD as "$990").
        let prefs_en = locale!("en-US").into();
        let usd = currency!("USD");
        let fmt_usd =
            CurrencyFormatter::try_new_compact_symbol(prefs_en, usd, Default::default()).unwrap();

        // Positive cases
        let val_12 = "12".parse().unwrap();
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&val_12), "$12");

        let val_990 = "990".parse().unwrap();
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&val_990), "$990");

        let val_1200 = "1200".parse().unwrap();
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&val_1200), "$1.2K");

        // Negative cases (pattern selection and Rule 3 fallback operate on absolute magnitude)
        let val_neg_12 = "-12".parse().unwrap();
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&val_neg_12), "-$12");

        let val_neg_990 = "-990".parse().unwrap();
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&val_neg_990), "-$990");

        let val_neg_1200 = "-1200".parse().unwrap();
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&val_neg_1200), "-$1.2K");
    }

    #[test]
    pub fn test_compact_name_and_compact_long_symbol() {
        let prefs = locale!("en-US").into();
        let usd = currency!("USD");
        let accounting = CurrencyFormatterOptions {
            usage: CurrencyUsage::Accounting,
            ..Default::default()
        };

        let fmt_compact_name = CurrencyFormatter::try_new_compact_name(prefs, usd).unwrap();
        let fmt_compact_long_symbol =
            CurrencyFormatter::try_new_compact_long_symbol(prefs, usd, Default::default()).unwrap();
        let fmt_compact_long_symbol_accounting =
            CurrencyFormatter::try_new_compact_long_symbol(prefs, usd, accounting).unwrap();

        let val = "12345.67".parse().unwrap();
        let negative_val = "-12345.67".parse().unwrap();
        assert_writeable_eq!(
            fmt_compact_name.format_fixed_decimal(&val),
            "12K US dollars"
        );
        assert_writeable_eq!(
            fmt_compact_long_symbol.format_fixed_decimal(&val),
            "$12 thousand"
        );
        assert_writeable_eq!(
            fmt_compact_long_symbol_accounting.format_fixed_decimal(&val),
            "$12 thousand"
        );
        assert_writeable_eq!(
            fmt_compact_long_symbol.format_fixed_decimal(&negative_val),
            "-$12 thousand"
        );
        assert_writeable_eq!(
            fmt_compact_long_symbol_accounting.format_fixed_decimal(&negative_val),
            "($12 thousand)"
        );
    }

    #[test]
    pub fn test_compact_code() {
        let prefs_en = locale!("en-US").into();
        let currency_usd = currency!("USD");
        let accounting = CurrencyFormatterOptions {
            usage: CurrencyUsage::Accounting,
            ..Default::default()
        };
        let value = "12345.67".parse().unwrap();
        let negative_value = "-12345.67".parse().unwrap();

        let fmt_compact_code =
            CurrencyFormatter::try_new_compact_code(prefs_en, currency_usd, Default::default())
                .unwrap();
        let fmt_compact_code_accounting =
            CurrencyFormatter::try_new_compact_code(prefs_en, currency_usd, accounting).unwrap();
        let fmt_compact_long_code = CurrencyFormatter::try_new_compact_long_code(
            prefs_en,
            currency_usd,
            Default::default(),
        )
        .unwrap();
        let fmt_compact_long_code_accounting =
            CurrencyFormatter::try_new_compact_long_code(prefs_en, currency_usd, accounting)
                .unwrap();

        assert_writeable_eq!(
            fmt_compact_code.format_fixed_decimal(&value),
            "USD\u{a0}12K"
        );
        assert_writeable_eq!(
            fmt_compact_code_accounting.format_fixed_decimal(&value),
            "USD\u{a0}12K"
        );
        assert_writeable_eq!(
            fmt_compact_code.format_fixed_decimal(&negative_value),
            "-USD\u{a0}12K"
        );
        assert_writeable_eq!(
            fmt_compact_code_accounting.format_fixed_decimal(&negative_value),
            "(USD\u{a0}12K)"
        );
        assert_writeable_eq!(
            fmt_compact_long_code.format_fixed_decimal(&value),
            "USD\u{a0}12 thousand"
        );
        assert_writeable_eq!(
            fmt_compact_long_code_accounting.format_fixed_decimal(&value),
            "USD\u{a0}12 thousand"
        );
        assert_writeable_eq!(
            fmt_compact_long_code.format_fixed_decimal(&negative_value),
            "-USD\u{a0}12 thousand"
        );
        assert_writeable_eq!(
            fmt_compact_long_code_accounting.format_fixed_decimal(&negative_value),
            "(USD\u{a0}12 thousand)"
        );
    }

    #[test]
    pub fn test_pt_pt_escudo_compact() {
        let prefs = locale!("pt-PT").into();
        let value = "1299".parse().unwrap();

        // In CLDR, `pt-PT` has no symbol for PTE: it marks this with U+200B
        // (ZERO WIDTH SPACE), which datagen stores as the empty string. An empty symbol
        // means the compact currency formatters use the no-currency pattern, so no
        // placeholder or trailing space is left behind (CLDR-19771).
        let fmt =
            CurrencyFormatter::try_new_compact_symbol(prefs, currency!("PTE"), Default::default())
                .unwrap();
        assert_writeable_eq!(fmt.format_fixed_decimal(&value), "1,3 mil");

        let fmt_long = CurrencyFormatter::try_new_compact_long_symbol(
            prefs,
            currency!("PTE"),
            Default::default(),
        )
        .unwrap();
        assert_writeable_eq!(fmt_long.format_fixed_decimal(&value), "1,3 mil");

        // The ISO code and the currency name are not affected.
        let fmt_code =
            CurrencyFormatter::try_new_compact_code(prefs, currency!("PTE"), Default::default())
                .unwrap();
        assert_writeable_eq!(fmt_code.format_fixed_decimal(&value), "1,3 mil PTE");
    }
}
