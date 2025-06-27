/*
 *  Copyright 2025 Google LLC.
 *  Copyright (c) Microsoft Corporation.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

import {expect} from 'chai';

import {isValidLocale} from './EmulationProcessor.js';

describe('EmulationProcessor.isValidLocale', () => {
  const invalidLocales = [
    // Is an empty string.
    '',
    // Language subtag is too short.
    'a',
    // Language subtag is too long.
    'abcd',
    // Language subtag contains invalid characters (numbers).
    '12',
    // Language subtag contains invalid characters (symbols).
    'en$',
    // Uses underscore instead of hyphen as a separator.
    'en_US',
    // Region subtag is too short.
    'en-U',
    // Region subtag is too long.
    'en-USAXASDASD',
    // Region subtag contains invalid characters (numbers not part of a valid UN M49 code).
    'en-U1',
    // Region subtag contains invalid characters (symbols).
    'en-US$',
    // Script subtag is too short.
    'en-Lat',
    // Script subtag is too long.
    'en-Somelongsubtag',
    // Script subtag contains invalid characters (numbers).
    'en-La1n',
    // Script subtag contains invalid characters (symbols).
    'en-Lat$',
    // Variant subtag is too short (must be 5-8 alphanumeric chars, or 4 if starting with a digit).
    'en-US-var',
    // Variant subtag contains invalid characters (symbols).
    'en-US-variant$',
    // Extension subtag is malformed (singleton 'u' not followed by anything).
    'en-u-',
    // Extension subtag is malformed (singleton 't' not followed by anything).
    'de-t-',
    // Private use subtag 'x-' is not followed by anything.
    'x-',
    // Locale consisting only of a private use subtag.
    'x-another-private-tag',
    // Private use subtag contains invalid characters (underscore).
    'en-x-private_use',
    // Contains an empty subtag (double hyphen).
    'en--US',
    // Starts with a hyphen.
    '-en-US',
    // Ends with a hyphen.
    'en-US-',
    // Contains only a hyphen.
    '-',
    // Contains non-ASCII characters.
    'en-US-ñ',
    // Grandfathered tag with invalid structure.
    'i-notarealtag',
    // Invalid UN M49 region code (not 3 digits).
    'en-01',
    // Invalid UN M49 region code (contains letters).
    'en-0A1',
    // Malformed language tag with numbers.
    '123',
    // Locale with only script.
    'Latn',
    // Locale with script before language.
    'Latn-en',
    // Repeated separator.
    'en--US',
    // Invalid character in an otherwise valid structure.
    'en-US-!',
    // Too many subtags of a specific type (e.g., multiple script tags).
    'en-Latn-Cyrl-US',
  ];

  invalidLocales.forEach((locale) => {
    it(`should return false for invalid locale: "${locale}"`, () => {
      expect(isValidLocale(locale), `"${locale}" should be invalid`).to.be
        .false;
    });
  });

  const validLocales = [
    // Simple language code (2-letter).
    'en',
    // Simple language code (3-letter ISO 639-2/3).
    'ast',
    // Language and region (both 2-letter).
    'en-US',
    // Language and script (4-letter).
    'sr-Latn',
    // Language, script, and region.
    'zh-Hans-CN',
    // Language and variant (longer variant).
    'de-DE-1996',
    // Language and multiple variants.
    'sl-Roza-biske',
    // Language, region, and variant.
    'ca-ES-valencia',
    // Language and variant (4-char variant starting with digit).
    'sl-1994',
    // Locale with Unicode extension keyword for numbering system.
    'th-TH-u-nu-thai',
    // Locale with Unicode extension for calendar.
    'en-US-u-ca-gregory',
    // Canonicalized extended language subtag (Yue Chinese).
    'yue',
    // Canonicalized extended language subtag (North Levantine Arabic).
    'apc',
    // Language with a less common but valid 3-letter code.
    'gsw',
    // A complex but valid tag with multiple subtags including extension and private use.
    'zh-Latn-CN-variant1-a-extend1-u-co-pinyin-x-private',
    // Locale with Unicode extension keyword for collation.
    'de-DE-u-co-phonebk',
    // Lowercase language and region.
    'fr-ca',
    // Uppercase language and region (should be normalized by Intl.Locale).
    'FR-CA',
    // Mixed case language and region (should be normalized by Intl.Locale).
    'fR-cA',
    // Locale with transform extension (simple case).
    'en-t-zh',
    // Language (2-letter) and region (3-digit UN M49).
    'es-419',
  ];

  validLocales.forEach((locale) => {
    it(`should return true for valid locale: "${locale}"`, () => {
      expect(isValidLocale(locale), `"${locale}" should be valid`).to.be.true;
    });
  });
});
