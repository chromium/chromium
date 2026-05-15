#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates the list of canonical language codes for base::LanguageCode.

This script generates a C++ include file (`.inc`) containing
`IMPL_LANGUAGECODE_TAG_NAME(tag, name)` macros for all supported language codes
in Chromium. The tag is the BCP47 language code and `name` is the all caps
name of the language. For example:

IMPL_LANGUAGECODE_TAG_NAME("en", ENGLISH)
IMPL_LANGUAGECODE_TAG_NAME("pt-BR", PORTUGUESE_BRAZIL)
IMPL_LANGUAGECODE_TAG_NAME("zh-Hans", CHINESE_SIMPLIFIED)

The script uses a hardcoded list of locales defined within it, and can
optionally include pseudolocales if requested by the build configuration.

Usage:
  generate_canonical_locales_list.py <output_file> <enable_pseudolocales>
    <output_file>: The path to the file to generate (usually in $root_gen_dir).
    <enable_pseudolocales>: "true" or "false" whether to include pseudolocales.

This script is run by the `canonical_language_codes_gen` action in
`base/i18n/internal/BUILD.gn`. The generated file is included in
`base/i18n/internal/language_code_builder.h` and `base/i18n/language_codes.h`.
"""

from __future__ import print_function

import sys


def gen_locale(locale_tuple):  # type: (tuple) -> str
  """Returns the generated code for a given locale in the list."""
  code, name = locale_tuple
  # We assume that all locale codes have only letters, numbers and hyphens.
  assert code.replace('-', '').isalnum(), code
  # clang-format enforces a four-space indent for initializer lists.
  return '    IMPL_LANGUAGECODE_TAG_NAME("{code}", {name})'.format(code=code,
                                                                   name=name)


def gen_locales(locales):  # type: (list) -> str
  """Returns the generated code for the locale list.

    The list is guaranteed to be in sorted order without duplicates.
    """
  return '\n'.join(gen_locale(locale) for locale in sorted(set(locales)))


_ALL_LOCALES = [
    ("af", "AFRIKAANS"),
    ("ak", "AKAN"),
    ("am", "AMHARIC"),
    ("an", "ARAGONESE"),
    ("ar", "ARABIC"),
    ("as", "ASSAMESE"),
    ("ast", "ASTURIAN"),
    ("ay", "AYMARA"),
    ("az", "AZERBAIJANI"),
    ("be", "BELARUSIAN"),
    ("bg", "BULGARIAN"),
    ("bho", "BHOJPURI"),
    ("bm", "BAMBARA"),
    ("bn", "BENGALI"),
    ("br", "BRETON"),
    ("bs", "BOSNIAN"),
    ("ca", "CATALAN"),
    ("ceb", "CEBUANO"),
    ("chr", "CHEROKEE"),
    ("ckb", "KURDISH_ARABIC_SORANI"),
    ("co", "CORSICAN"),
    ("cs", "CZECH"),
    ("cy", "WELSH"),
    ("da", "DANISH"),
    ("de", "GERMAN"),
    ("de-AT", "GERMAN_AUSTRIA"),
    ("de-CH", "GERMAN_SWITZERLAND"),
    ("de-DE", "GERMAN_GERMANY"),
    ("de-LI", "GERMAN_LIECHTENSTEIN"),
    ("doi", "DOGRI"),
    ("dv", "DHIVEHI"),
    ("ee", "EWE"),
    ("el", "GREEK"),
    ("en", "ENGLISH"),
    ("en-AU", "ENGLISH_AUSTRALIA"),
    ("en-CA", "ENGLISH_CANADA"),
    ("en-GB", "ENGLISH_UK"),
    ("en-IE", "ENGLISH_IRELAND"),
    ("en-IN", "ENGLISH_INDIA"),
    ("en-NZ", "ENGLISH_NEW_ZEALAND"),
    ("en-US", "ENGLISH_US"),
    ("en-ZA", "ENGLISH_SOUTH_AFRICA"),
    ("eo", "ESPERANTO"),
    ("es", "SPANISH"),
    ("es-419", "SPANISH_LATIN_AMERICAN"),
    ("es-AR", "SPANISH_ARGENTINA"),
    ("es-CL", "SPANISH_CHILE"),
    ("es-CO", "SPANISH_COLOMBIA"),
    ("es-CR", "SPANISH_COSTA_RICA"),
    ("es-ES", "SPANISH_SPAIN"),
    ("es-HN", "SPANISH_HONDURAS"),
    ("es-MX", "SPANISH_MEXICO"),
    ("es-PE", "SPANISH_PERU"),
    ("es-US", "SPANISH_US"),
    ("es-UY", "SPANISH_URUGUAY"),
    ("es-VE", "SPANISH_VENEZUELA"),
    ("et", "ESTONIAN"),
    ("eu", "BASQUE"),
    ("fa", "PERSIAN"),
    ("fi", "FINNISH"),
    ("fil", "FILIPINO"),
    ("fo", "FAROESE"),
    ("fr", "FRENCH"),
    ("fr-CA", "FRENCH_CANADA"),
    ("fr-CH", "FRENCH_SWITZERLAND"),
    ("fr-FR", "FRENCH_FRANCE"),
    ("fy", "FRISIAN"),
    ("ga", "IRISH"),
    ("gd", "SCOTS_GAELIC"),
    ("gl", "GALICIAN"),
    ("gn", "GUARANI"),
    ("gu", "GUJARATI"),
    ("ha", "HAUSA"),
    ("haw", "HAWAIIAN"),
    ("he", "HEBREW"),
    ("hi", "HINDI"),
    ("hmn", "HMONG"),
    ("hr", "CROATIAN"),
    ("ht", "HAITIAN_CREOLE"),
    ("hu", "HUNGARIAN"),
    ("hy", "ARMENIAN"),
    ("ia", "INTERLINGUA"),
    ("id", "INDONESIAN"),
    ("ig", "IGBO"),
    ("ilo", "ILOCANO"),
    ("is", "ICELANDIC"),
    ("it", "ITALIAN"),
    ("it-CH", "ITALIAN_SWITZERLAND"),
    ("it-IT", "ITALIAN_ITALY"),
    ("ja", "JAPANESE"),
    ("jv", "JAVANESE"),
    ("ka", "GEORGIAN"),
    ("kk", "KAZAKH"),
    ("km", "KHMER"),
    ("kn", "KANNADA"),
    ("ko", "KOREAN"),
    ("kok", "KONKANI"),
    ("kri", "KRIO"),
    ("ku", "KURDISH"),
    ("ky", "KYRGYZ"),
    ("la", "LATIN"),
    ("lb", "LUXEMBOURGISH"),
    ("lg", "LUGANDA"),
    ("ln", "LINGALA"),
    ("lo", "LAO"),
    ("lt", "LITHUANIAN"),
    ("lus", "MIZO"),
    ("lv", "LATVIAN"),
    ("mai", "MAITHILI"),
    ("mg", "MALAGASY"),
    ("mi", "MAORI"),
    ("mk", "MACEDONIAN"),
    ("ml", "MALAYALAM"),
    ("mn", "MONGOLIAN"),
    ("mni-Mtei", "MANIPURI_MEITEI_MAYEK"),
    ("ro-MD", "MOLDAVIAN"),
    ("mr", "MARATHI"),
    ("ms", "MALAY"),
    ("mt", "MALTESE"),
    ("my", "BURMESE"),
    ("nb", "NORWEGIAN_BOKMAL"),
    ("ne", "NEPALI"),
    ("nl", "DUTCH"),
    ("nn", "NORWEGIAN_NYNORSK"),
    ("no", "NORWEGIAN"),
    ("nso", "SEPEDI"),
    ("ny", "NYANJA"),
    ("oc", "OCCITAN"),
    ("om", "OROMO"),
    ("or", "ORIYA"),
    ("pa", "PUNJABI"),
    ("pl", "POLISH"),
    ("ps", "PASHTO"),
    ("pt", "PORTUGUESE"),
    ("pt-BR", "BRAZILIAN_PORTUGUESE"),
    ("pt-PT", "PORTUGAL_PORTUGUESE"),
    ("qu", "QUECHUA"),
    ("rm", "ROMANSH"),
    ("ro", "ROMANIAN"),
    ("ru", "RUSSIAN"),
    ("rw", "KINYARWANDA"),
    ("sa", "SANSKRIT"),
    ("sd", "SINDHI"),
    ("sr-Latn", "SERBO_CROATIAN"),
    ("sr-Latn-RS", "SERBIAN_LATIN_SERBIA"),
    ("si", "SINHALESE"),
    ("sk", "SLOVAK"),
    ("sl", "SLOVENIAN"),
    ("sm", "SAMOAN"),
    ("sn", "SHONA"),
    ("so", "SOMALI"),
    ("sq", "ALBANIAN"),
    ("sr", "SERBIAN"),
    ("sr-Cyrl", "SERBIAN_CYRILLIC"),
    ("sr-Cyrl-RS", "SERBIAN_CYRILLIC_SERBIA"),
    ("st", "SESOTHO"),
    ("su", "SUNDANESE"),
    ("sv", "SWEDISH"),
    ("sw", "SWAHILI"),
    ("ta", "TAMIL"),
    ("te", "TELUGU"),
    ("tg", "TAJIK"),
    ("th", "THAI"),
    ("ti", "TIGRINYA"),
    ("tk", "TURKMEN"),
    ("tn", "TSWANA"),
    ("to", "TONGA"),
    ("tr", "TURKISH"),
    ("ts", "TSONGA"),
    ("tt", "TATAR"),
    ("ak", "TWI"),  # The 'tw' code is deprecated.
    ("ug", "UYGHUR"),
    ("uk", "UKRAINIAN"),
    ("ur", "URDU"),
    ("uz", "UZBEK"),
    ("vi", "VIETNAMESE"),
    ("wa", "WALLOON"),
    ("wo", "WOLOF"),
    ("xh", "XHOSA"),
    ("yi", "YIDDISH"),
    ("yo", "YORUBA"),
    ("yue", "CANTONESE"),
    ("zh", "CHINESE"),
    ("zh-CN", "CHINA_CHINESE"),
    ("zh-Hans", "CHINESE_SIMPLIFIED"),
    ("zh-Hans-CN", "CHINA_CHINESE_SIMPLIFIED"),
    ("zh-Hant", "CHINESE_TRADITIONAL"),
    ("zh-Hant-TW", "TAIWAN_CHINESE_TRADITIONAL"),
    ("zh-HK", "HONGKONG_CHINESE"),
    ("zh-TW", "TAIWAN_CHINESE"),
    ("zu", "ZULU"),
]

_PSEUDO_LOCALES = [
    ("ar-XB", "RTL_PSEUDOLOCALE"),
    ("en-XA", "LONG_STRINGS_PSEUDOLOCALE"),
]


def main():  # type: () -> None
  import doctest
  doctest.testmod()

  if len(sys.argv) < 2:
    print('{}: only ran tests'.format(sys.argv[0]))
    return

  is_pseudo_locales_enabled = sys.argv[2] == "true"
  all_locales = sorted(
      list(
          set(_ALL_LOCALES +
              (_PSEUDO_LOCALES if is_pseudo_locales_enabled else []))))
  output = gen_locales(all_locales)
  with open(sys.argv[1], 'w') as f:
    f.write(output)


if __name__ == '__main__':
  main()
