#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates the list of known BCP47 subtags.

This script uses separated lists for each type of subtag (language, script,
region, and variant) to generate C++ macro files.
"""

import argparse
import sys

_LANGUAGES = [
    "af", "ak", "am", "an", "ar", "as", "ast", "ay", "az", "be", "bg", "bho",
    "bm", "bn", "br", "bs", "ca", "ceb", "chr", "ckb", "co", "cs", "cy", "da",
    "de", "doi", "dv", "ee", "el", "en", "eo", "es", "et", "eu", "fa", "fi",
    "fil", "fo", "fr", "fy", "ga", "gd", "gl", "gn", "gu", "ha", "haw", "he",
    "hi", "hmn", "hr", "ht", "hu", "hy", "ia", "id", "ig", "ilo", "is", "it",
    "ja", "jv", "ka", "kk", "km", "kn", "ko", "kok", "kri", "ku", "ky", "la",
    "lb", "lg", "ln", "lo", "lt", "lus", "lv", "mai", "mg", "mi", "mk", "ml",
    "mn", "mni", "mr", "ms", "mt", "my", "nb", "ne", "nl", "nn", "no", "nso",
    "ny", "oc", "om", "or", "pa", "pl", "ps", "pt", "qu", "rm", "ro", "ru",
    "rw", "sa", "sd", "sh", "si", "sk", "sl", "sm", "sn", "so", "sq", "sr",
    "st", "su", "sv", "sw", "ta", "te", "tg", "th", "ti", "tk", "tn", "to",
    "tr", "ts", "tt", "ug", "uk", "und", "ur", "uz", "vi", "wa", "wo", "xh",
    "yi", "yo", "yue", "zh", "zu"
]

_SCRIPTS = ["Cyrl", "Hans", "Hant", "Latn", "Mtei"]

_REGIONS = [
    "001", "419", "AC", "AD", "AE", "AF", "AG", "AI", "AL", "AM", "AO", "AQ",
    "AR", "AS", "AT", "AU", "AW", "AX", "AZ", "BA", "BB", "BD", "BE", "BF",
    "BG", "BH", "BI", "BJ", "BL", "BM", "BN", "BO", "BQ", "BR", "BS", "BT",
    "BV", "BW", "BY", "BZ", "CA", "CC", "CD", "CF", "CG", "CH", "CI", "CK",
    "CL", "CM", "CN", "CO", "CR", "CU", "CV", "CW", "CX", "CY", "CZ", "DE",
    "DJ", "DK", "DM", "DO", "DZ", "EC", "EE", "EG", "EH", "ER", "ES", "ET",
    "FI", "FJ", "FK", "FM", "FO", "FR", "GA", "GB", "GD", "GE", "GF", "GG",
    "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GS", "GT", "GU", "GW",
    "GY", "HK", "HM", "HN", "HR", "HT", "HU", "ID", "IE", "IL", "IM", "IN",
    "IO", "IQ", "IR", "IS", "IT", "JE", "JM", "JO", "JP", "KE", "KG", "KH",
    "KI", "KM", "KN", "KP", "KR", "KW", "KY", "KZ", "LA", "LB", "LC", "LI",
    "LK", "LR", "LS", "LT", "LU", "LV", "LY", "MA", "MC", "MD", "ME", "MF",
    "MG", "MH", "MK", "ML", "MM", "MN", "MO", "MP", "MQ", "MR", "MS", "MT",
    "MU", "MV", "MW", "MX", "MY", "MZ", "NA", "NC", "NE", "NF", "NG", "NI",
    "NL", "NO", "NP", "NR", "NU", "NZ", "OM", "PA", "PE", "PF", "PG", "PH",
    "PK", "PL", "PM", "PN", "PR", "PS", "PT", "PW", "PY", "QA", "RE", "RO",
    "RS", "RU", "RW", "SA", "SB", "SC", "SD", "SE", "SG", "SH", "SI", "SJ",
    "SK", "SL", "SM", "SN", "SO", "SR", "SS", "ST", "SV", "SX", "SY", "SZ",
    "TA", "TC", "TD", "TF", "TG", "TH", "TJ", "TK", "TL", "TM", "TN", "TO",
    "TR", "TT", "TV", "TW", "TZ", "UA", "UG", "UM", "US", "UY", "UZ", "VA",
    "VC", "VE", "VG", "VI", "VN", "VU", "WF", "WS", "XA", "XB", "XC", "XK",
    "YE", "YT", "ZA", "ZM", "ZW"
]

_VARIANTS = ["oxendict"]


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('output_file',
                        help='The path to the file to generate.')
    parser.add_argument('type',
                        choices=['language', 'script', 'region', 'variant'],
                        help='The type of subtag to extract.')
    args = parser.parse_args()

    subtag_type = args.type

    subtags = []
    if subtag_type == 'language':
        subtags = _LANGUAGES
    elif subtag_type == 'script':
        subtags = _SCRIPTS
    elif subtag_type == 'region':
        subtags = _REGIONS
    elif subtag_type == 'variant':
        subtags = _VARIANTS

    subtags = sorted(list(set(subtags)))

    with open(args.output_file, 'w') as f:
        macro_name = f"IMPL_BCP47_{subtag_type.upper()}"
        for subtag in subtags:
            f.write(f'    {macro_name}("{subtag}")\n')

    return 0


if __name__ == '__main__':
    sys.exit(main())
