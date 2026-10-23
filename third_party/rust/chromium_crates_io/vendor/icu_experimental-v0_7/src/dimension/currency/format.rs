// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// TODO: add more tests for this module to cover more locales & currencies.
#[cfg(test)]
mod tests {
    use icu::locale::locale;
    use icu::locale::preferences::extensions::unicode::keywords::currency;
    use writeable::assert_writeable_eq;

    use crate::dimension::currency::{
        formatter::{CurrencyFormatter, CurrencyFormatterPreferences},
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

        // Short / Symbol
        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs, currency_code, Default::default()).unwrap();
        let fmt_symbol_accounting =
            CurrencyFormatter::try_new_symbol(prefs, currency_code, accounting).unwrap();
        let positive_value = "12345.67".parse().unwrap();
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&positive_value),
            "$12,345.67"
        );
        assert_writeable_eq!(
            fmt_symbol_accounting.format_fixed_decimal(&positive_value),
            "$12,345.67"
        );
        let negative_value = "-12345.67".parse().unwrap();
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&negative_value),
            "-$12,345.67"
        );
        assert_writeable_eq!(
            fmt_symbol_accounting.format_fixed_decimal(&negative_value),
            "($12,345.67)"
        );

        let value_no_decimals = "123".parse().unwrap();
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&value_no_decimals),
            "$123.00"
        );
        let value_4_decimals = "123.4567".parse().unwrap();
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&value_4_decimals),
            "$123.46"
        );

        // Narrow / Symbol Narrow
        let fmt_symbol_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs, currency_code, Default::default())
                .unwrap();
        let fmt_symbol_narrow_accounting =
            CurrencyFormatter::try_new_symbol_narrow(prefs, currency_code, accounting).unwrap();
        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&positive_value),
            "$12,345.67"
        );
        assert_writeable_eq!(
            fmt_symbol_narrow_accounting.format_fixed_decimal(&positive_value),
            "$12,345.67"
        );
        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&negative_value),
            "-$12,345.67"
        );
        assert_writeable_eq!(
            fmt_symbol_narrow_accounting.format_fixed_decimal(&negative_value),
            "($12,345.67)"
        );

        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&value_no_decimals),
            "$123.00"
        );
        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&value_4_decimals),
            "$123.46"
        );

        // Long / Name
        let fmt_name = CurrencyFormatter::try_new_name(prefs, currency_code).unwrap();
        assert_writeable_eq!(
            fmt_name.format_fixed_decimal(&positive_value),
            "12,345.67 US dollars"
        );
        assert_writeable_eq!(
            fmt_name.format_fixed_decimal(&negative_value),
            "-12,345.67 US dollars"
        );

        assert_writeable_eq!(
            fmt_name.format_fixed_decimal(&value_no_decimals),
            "123.00 US dollars"
        );
        assert_writeable_eq!(
            fmt_name.format_fixed_decimal(&value_4_decimals),
            "123.46 US dollars"
        );
    }

    #[test]
    pub fn test_pt_pt_escudo() {
        let prefs = locale!("pt-PT").into();
        let value = "12345.67".parse().unwrap();

        // In CLDR, `pt-PT` sets the decimal separator override for PTE to '$' and the
        // currency symbol to U+200B (ZERO WIDTH SPACE), as the cifrão ('$') serves as
        // the decimal separator (e.g. "12,345$67"). Datagen stores that symbol as the
        // empty string.
        //
        // Because the locale's standard currency pattern ("#,##0.00 ¤") contains a
        // non-breaking space (U+00A0) before the symbol placeholder ('¤'), formatting
        // with the raw symbol would produce a trailing space (tracked in CLDR-19771).
        // To resolve this, when the currency symbol is empty, `CurrencyFormatter` uses
        // `noCurrency` formatting so that the amount formats cleanly without any
        // placeholder or trailing space ("12,345$67").
        let escudo =
            CurrencyFormatter::try_new_symbol(prefs, currency!("PTE"), Default::default()).unwrap();
        assert_writeable_eq!(escudo.format_fixed_decimal(&value), "12,345$67");

        let escudo_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs, currency!("PTE"), Default::default())
                .unwrap();
        // Narrow symbol for PTE is not defined in pt-PT CLDR data, so the narrow data entry
        // carries the standard symbol (#8486), matching `try_new_symbol` above.
        assert_writeable_eq!(escudo_narrow.format_fixed_decimal(&value), "12,345$67");

        let escudo_code =
            CurrencyFormatter::try_new_code(prefs, currency!("PTE"), Default::default()).unwrap();
        assert_writeable_eq!(escudo_code.format_fixed_decimal(&value), "12,345$67 PTE");

        let escudo_name = CurrencyFormatter::try_new_name(prefs, currency!("PTE")).unwrap();
        assert_writeable_eq!(
            escudo_name.format_fixed_decimal(&value),
            "12,345$67 escudos portugueses"
        );

        let escudo_no_currency =
            CurrencyFormatter::try_new_no_currency(prefs, currency!("PTE"), Default::default())
                .unwrap();
        assert_writeable_eq!(escudo_no_currency.format_fixed_decimal(&value), "12,345$67");

        // Currencies the locale does not single out keep its standard separators.
        let euro =
            CurrencyFormatter::try_new_symbol(prefs, currency!("EUR"), Default::default()).unwrap();
        assert_writeable_eq!(euro.format_fixed_decimal(&value), "12 345,67 €");
    }

    #[test]
    pub fn test_fr_fr() {
        let prefs = locale!("fr-FR").into();
        let currency_code = currency!("EUR");

        // Short / Symbol
        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs, currency_code, Default::default()).unwrap();
        let positive_value = "12345.67".parse().unwrap();
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&positive_value),
            "12\u{202f}345,67\u{a0}€"
        );
        let negative_value = "-12345.67".parse().unwrap();
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&negative_value),
            "-12\u{202f}345,67\u{a0}€"
        );

        // Narrow / Symbol Narrow
        let fmt_symbol_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs, currency_code, Default::default())
                .unwrap();
        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&positive_value),
            "12\u{202f}345,67\u{a0}€"
        );
        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&negative_value),
            "-12\u{202f}345,67\u{a0}€"
        );

        // Long / Name
        let fmt_name = CurrencyFormatter::try_new_name(prefs, currency_code).unwrap();
        assert_writeable_eq!(
            fmt_name.format_fixed_decimal(&positive_value),
            "12\u{202f}345,67 euros"
        );
        assert_writeable_eq!(
            fmt_name.format_fixed_decimal(&negative_value),
            "-12\u{202f}345,67 euros"
        );
    }

    #[test]
    pub fn test_ar_eg() {
        let prefs = locale!("ar-EG").into();
        let currency_code = currency!("EGP");

        // Short / Symbol
        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs, currency_code, Default::default()).unwrap();
        let positive_value = "12345.67".parse().unwrap();
        // TODO(#6064)
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&positive_value),
            "\u{200f}١٢٬٣٤٥٫٦٧\u{a0}ج.م.\u{200f}"
        );
        let negative_value = "-12345.67".parse().unwrap();
        // TODO(#6064)
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&negative_value),
            "\u{61c}-\u{200f}١٢٬٣٤٥٫٦٧\u{a0}ج.م.\u{200f}"
        );

        // Narrow / Symbol Narrow
        let fmt_symbol_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs, currency_code, Default::default())
                .unwrap();
        // TODO(#6064)
        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&positive_value),
            "\u{200f}١٢٬٣٤٥٫٦٧\u{a0}E£"
        );
        // TODO(#6064)
        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&negative_value),
            "\u{61c}-\u{200f}١٢٬٣٤٥٫٦٧\u{a0}E£"
        );

        // Long / Name
        let fmt_name = CurrencyFormatter::try_new_name(prefs, currency_code).unwrap();
        assert_writeable_eq!(
            fmt_name.format_fixed_decimal(&positive_value),
            "١٢٬٣٤٥٫٦٧ جنيه مصري"
        );
        assert_writeable_eq!(
            fmt_name.format_fixed_decimal(&negative_value),
            "\u{61c}-١٢٬٣٤٥٫٦٧ جنيه مصري"
        );
    }

    #[test]
    pub fn test_usd_in_fr_fr() {
        let prefs = locale!("fr-FR").into();
        let currency_code = currency!("USD");
        let value = "12345.67".parse().unwrap();

        // Short / Symbol USD in fr-FR should be US$ or $US
        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs, currency_code, Default::default()).unwrap();
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&value),
            "12\u{202f}345,67\u{a0}$US"
        );

        // Narrow / Symbol Narrow USD in fr-FR should be $
        let fmt_symbol_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs, currency_code, Default::default())
                .unwrap();
        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&value),
            "12\u{202f}345,67\u{a0}$"
        );
    }

    #[test]
    pub fn test_explicit_negative_pattern() {
        // `ar` with `latn` numbers has an explicit standard negative subpattern:
        // - Escaped: "\u{200f}#,##0.00\u{a0}\u{a4};\u{200f}-#,##0.00\u{a0}\u{a4}"
        // - Literal: "‏#,##0.00 ¤;‏-#,##0.00 ¤"
        // The explicit negative pattern is used as-is for negative values instead of
        // prepending a minus sign to the positive pattern.
        let prefs = locale!("ar-EG-u-nu-latn").into();
        let currency_code = "EGP".parse().unwrap();
        let negative_value = "-12345.67".parse().unwrap();

        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs, currency_code, Default::default()).unwrap();
        // Expected output:
        // - Escaped: "\u{200f}\u{200e}-12,345.67\u{a0}\u{62c}.\u{645}.\u{200f}"
        // - Literal: "‏\u{200e}-12,345.67 ج.م.‏"
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&negative_value),
            "‏\u{200e}-12,345.67 ج.م.‏"
        );

        // `de-CH` places the minus sign between the currency symbol and the number:
        // - Escaped: "\u{a4}\u{a0}#,##0.00;\u{a4}-#,##0.00"
        // - Literal: "¤ #,##0.00;¤-#,##0.00"
        // This behavior can only be expressed via an explicit negative pattern.
        let prefs = locale!("de-CH").into();
        let currency_code = "CHF".parse().unwrap();
        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs, currency_code, Default::default()).unwrap();
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&negative_value),
            "CHF-12'345.67"
        );
    }

    #[test]
    pub fn test_numbering_system_override() {
        let prefs_arab = locale!("ar-EG").into();
        let prefs_latn = locale!("ar-EG-u-nu-latn").into();
        let currency_code = currency!("EGP");
        let value = "12345.67".parse().unwrap();

        // 1. Default numbering system (arab) - Symbol
        let fmt_arab_symbol =
            CurrencyFormatter::try_new_symbol(prefs_arab, currency_code, Default::default())
                .unwrap();
        assert_writeable_eq!(
            fmt_arab_symbol.format_fixed_decimal(&value),
            "\u{200f}١٢٬٣٤٥٫٦٧\u{a0}ج.م.\u{200f}"
        );

        // 2. Locale extension override (latn) - Symbol
        let fmt_latn_symbol =
            CurrencyFormatter::try_new_symbol(prefs_latn, currency_code, Default::default())
                .unwrap();
        assert_writeable_eq!(
            fmt_latn_symbol.format_fixed_decimal(&value),
            "\u{200f}12,345.67\u{a0}ج.م.\u{200f}"
        );

        // 3. Default numbering system (arab) - Symbol Narrow
        let fmt_arab_symbol_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs_arab, currency_code, Default::default())
                .unwrap();
        assert_writeable_eq!(
            fmt_arab_symbol_narrow.format_fixed_decimal(&value),
            "\u{200f}١٢٬٣٤٥٫٦٧\u{a0}E£"
        );

        // 4. Locale extension override (latn) - Symbol Narrow
        let fmt_latn_symbol_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs_latn, currency_code, Default::default())
                .unwrap();
        assert_writeable_eq!(
            fmt_latn_symbol_narrow.format_fixed_decimal(&value),
            "\u{200f}12,345.67\u{a0}E£"
        );

        // 5. Default numbering system (arab) - Name
        let fmt_arab_name = CurrencyFormatter::try_new_name(prefs_arab, currency_code).unwrap();
        assert_writeable_eq!(
            fmt_arab_name.format_fixed_decimal(&value),
            "١٢٬٣٤٥٫٦٧ جنيه مصري"
        );

        // 6. Locale extension override (latn) - Name
        let fmt_latn_name = CurrencyFormatter::try_new_name(prefs_latn, currency_code).unwrap();
        assert_writeable_eq!(
            fmt_latn_name.format_fixed_decimal(&value),
            "12,345.67 جنيه مصري"
        );
    }

    #[test]
    pub fn test_en_us_cad() {
        let prefs = locale!("en-US").into();
        let currency_code = currency!("CAD");
        let value = "12345.67".parse().unwrap();

        // Short / Symbol
        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs, currency_code, Default::default()).unwrap();
        assert_writeable_eq!(fmt_symbol.format_fixed_decimal(&value), "CA$12,345.67");

        // Narrow / Symbol Narrow
        let fmt_symbol_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs, currency_code, Default::default())
                .unwrap();
        assert_writeable_eq!(fmt_symbol_narrow.format_fixed_decimal(&value), "$12,345.67");
    }

    #[test]
    pub fn test_code() {
        let prefs_en = locale!("en-US").into();
        let currency_usd = currency!("USD");
        let accounting = CurrencyFormatterOptions {
            usage: CurrencyUsage::Accounting,
            ..Default::default()
        };
        let value = "12345.67".parse().unwrap();
        let negative_value = "-12345.67".parse().unwrap();

        let fmt_code_en =
            CurrencyFormatter::try_new_code(prefs_en, currency_usd, Default::default()).unwrap();
        let fmt_code_en_accounting =
            CurrencyFormatter::try_new_code(prefs_en, currency_usd, accounting).unwrap();
        assert_writeable_eq!(
            fmt_code_en.format_fixed_decimal(&value),
            "USD\u{a0}12,345.67"
        );
        assert_writeable_eq!(
            fmt_code_en_accounting.format_fixed_decimal(&value),
            "USD\u{a0}12,345.67"
        );
        assert_writeable_eq!(
            fmt_code_en.format_fixed_decimal(&negative_value),
            "-USD\u{a0}12,345.67"
        );
        assert_writeable_eq!(
            fmt_code_en_accounting.format_fixed_decimal(&negative_value),
            "(USD\u{a0}12,345.67)"
        );

        let prefs_fr = locale!("fr-FR").into();
        let currency_eur = currency!("EUR");
        let fmt_code_fr =
            CurrencyFormatter::try_new_code(prefs_fr, currency_eur, Default::default()).unwrap();
        assert_writeable_eq!(
            fmt_code_fr.format_fixed_decimal(&value),
            "12\u{202f}345,67\u{a0}EUR"
        );
    }

    #[test]
    pub fn test_name_fallback_to_iso_code_name() {
        let prefs_en: CurrencyFormatterPreferences = locale!("en-US").into();
        // Unknown currency code should gracefully fall back to IsoCodeName instead of DataError(IdentifierNotFound)
        let currency_xyz = currency!("XYZ");
        let value = "12345.67".parse().unwrap();

        let fmt_name = CurrencyFormatter::try_new_name(prefs_en, currency_xyz).unwrap();
        assert_writeable_eq!(fmt_name.format_fixed_decimal(&value), "12,345.67 XYZ");
    }

    #[test]
    pub fn test_symbol_fallback_to_iso_code_symbol() {
        let prefs_en: CurrencyFormatterPreferences = locale!("en-US").into();
        // Unknown currency code should gracefully fall back to IsoCodeSymbol instead of DataError(IdentifierNotFound)
        let currency_xyz = currency!("XYZ");
        let value = "12345.67".parse().unwrap();

        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs_en, currency_xyz, Default::default()).unwrap();
        assert_writeable_eq!(
            fmt_symbol.format_fixed_decimal(&value),
            "XYZ\u{a0}12,345.67"
        );

        let fmt_symbol_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs_en, currency_xyz, Default::default())
                .unwrap();
        assert_writeable_eq!(
            fmt_symbol_narrow.format_fixed_decimal(&value),
            "XYZ\u{a0}12,345.67"
        );
    }

    #[test]
    pub fn test_narrow_symbol_fallback_to_standard_symbol() {
        // Test currency where standard symbol is defined, but narrow symbol is not in CLDR.
        // Per UTS #35 lateral inheritance, narrow symbols fall back to standard symbols before
        // the ISO code. The fallback is resolved at datagen time, so the narrow data entry
        // already carries the standard symbol.
        let prefs_fr: CurrencyFormatterPreferences = locale!("fr-FR").into();
        let currency_frf = currency!("FRF");
        let value = "12345.67".parse().unwrap();

        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs_fr, currency_frf, Default::default()).unwrap();
        let fmt_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs_fr, currency_frf, Default::default())
                .unwrap();

        let formatted_symbol = fmt_symbol.format_fixed_decimal(&value);
        let formatted_narrow = fmt_narrow.format_fixed_decimal(&value);

        // Both use "F" (standard symbol) instead of falling back to ISO code "FRF"
        assert_writeable_eq!(formatted_symbol, "12\u{202f}345,67\u{a0}F");
        assert_writeable_eq!(formatted_narrow, "12\u{202f}345,67\u{a0}F");
    }

    #[test]
    pub fn test_jpy() {
        let prefs: CurrencyFormatterPreferences = locale!("en-US").into();
        let currency_code = currency!("JPY");
        let value = "12345.67".parse().unwrap();

        // JPY has 0 decimals (defined in global CurrencyFractionsV1)
        // Short / Symbol
        let fmt_symbol =
            CurrencyFormatter::try_new_symbol(prefs, currency_code, Default::default()).unwrap();
        assert_writeable_eq!(fmt_symbol.format_fixed_decimal(&value), "¥12,346");

        // Narrow / Symbol Narrow
        let fmt_symbol_narrow =
            CurrencyFormatter::try_new_symbol_narrow(prefs, currency_code, Default::default())
                .unwrap();
        assert_writeable_eq!(fmt_symbol_narrow.format_fixed_decimal(&value), "¥12,346");

        // Long / Name
        let fmt_name = CurrencyFormatter::try_new_name(prefs, currency_code).unwrap();
        assert_writeable_eq!(fmt_name.format_fixed_decimal(&value), "12,346 Japanese yen");
    }

    #[test]
    pub fn test_no_currency() {
        let prefs_en = locale!("en-US").into();
        let usd = currency!("USD");
        let jpy = currency!("JPY");
        let bhd = currency!("BHD");

        let positive = "12345.67".parse().unwrap();
        let negative = "-12345.67".parse().unwrap();
        let integer = "123".parse().unwrap();

        let accounting_options = CurrencyFormatterOptions {
            usage: CurrencyUsage::Accounting,
            ..Default::default()
        };

        // USD: 2 decimal places
        let fmt_usd =
            CurrencyFormatter::try_new_no_currency(prefs_en, usd, Default::default()).unwrap();
        let fmt_usd_acc =
            CurrencyFormatter::try_new_no_currency(prefs_en, usd, accounting_options).unwrap();
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&positive), "12,345.67");
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&negative), "-12,345.67");
        assert_writeable_eq!(fmt_usd_acc.format_fixed_decimal(&negative), "(12,345.67)");
        assert_writeable_eq!(fmt_usd.format_fixed_decimal(&integer), "123.00");

        // JPY: 0 decimal places
        let fmt_jpy =
            CurrencyFormatter::try_new_no_currency(prefs_en, jpy, Default::default()).unwrap();
        assert_writeable_eq!(fmt_jpy.format_fixed_decimal(&positive), "12,346");

        // BHD: 3 decimal places
        let fmt_bhd =
            CurrencyFormatter::try_new_no_currency(prefs_en, bhd, Default::default()).unwrap();
        assert_writeable_eq!(fmt_bhd.format_fixed_decimal(&positive), "12,345.670");

        // French locale EUR: 2 decimal places with French grouping
        let prefs_fr = locale!("fr-FR").into();
        let eur = currency!("EUR");
        let fmt_eur_fr =
            CurrencyFormatter::try_new_no_currency(prefs_fr, eur, Default::default()).unwrap();
        assert_writeable_eq!(
            fmt_eur_fr.format_fixed_decimal(&positive),
            "12\u{202f}345,67"
        );

        // Arabic locale ar-EG EGP: 2 decimal places with Arabic-Indic digits, BiDi marks and accounting format
        let prefs_ar = locale!("ar-EG").into();
        let egp = currency!("EGP");
        let fmt_egp_ar =
            CurrencyFormatter::try_new_no_currency(prefs_ar, egp, Default::default()).unwrap();
        let fmt_egp_ar_acc =
            CurrencyFormatter::try_new_no_currency(prefs_ar, egp, accounting_options).unwrap();
        assert_writeable_eq!(
            fmt_egp_ar.format_fixed_decimal(&positive),
            "١٢\u{66c}٣٤٥\u{66b}٦٧"
        );
        assert_writeable_eq!(
            fmt_egp_ar_acc.format_fixed_decimal(&negative),
            "\u{061c}-١٢\u{66c}٣٤٥\u{66b}٦٧"
        );
    }
}
