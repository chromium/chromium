# Chromium Custom UTS #35 Transliterators

This directory contains multi-rule UTS #35 transform files referenced via
`rules_file` under `[[transliterators.custom]]` in
`//tools/crates/icu4x_datagen/config.toml` (single-line custom transliterators
are configured inline via `rules = "..."` in `config.toml`).

`//tools/crates/run_icu4x_datagen.py` compiles these rule sets at datagen time
into `TransliteratorRulesV1` payloads under private-use `und-t-und-x0-*`
BCP-47-T IDs in `//third_party/rust/icu4x_data/src/data/`, avoiding shipping
`icu_experimental::transliterate::RuleCollectionProvider` or Unicode
property-name parsing tables in Chromium binaries.

While the legacy `!BUILDFLAG(BUILD_RUST_TRANSLIT)` ICU4C fallback remains in
`//base/i18n/transliterator.cc`, keep these rules synchronized with
`CustomRulesForBcp47Id`:

- `de_normalize_for_address.txt` (`und-t-und-x0-afillde`):
  `DE_NormalizForAddress` in
  `//components/autofill/core/browser/data_model/transliterator.cc`.
- `extra_conf.txt` (`und-t-und-x0-xconf`): `ExtraConf` in
  `//components/url_formatter/spoof_checks/skeleton_generator.cc`.
- `diacritic_stripper.txt` (`und-t-und-x0-diacstr`): `DiacriticStripper` in
  `//chrome/browser/ash/input_method/diacritics_insensitive_string_comparator.cc`.
