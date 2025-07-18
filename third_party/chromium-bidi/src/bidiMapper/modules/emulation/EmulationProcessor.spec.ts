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

import {
  isTimeZoneOffsetString,
  isValidLocale,
  isValidTimezone,
} from './EmulationProcessor.js';

describe('EmulationProcessor helper functions', () => {
  describe('isValidLocale', () => {
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

  const VALID_TIMEZONE_OFFSETS = [
    // Hour offsets.
    '-12',
    '+14',
    '+00',
    '-00',
    // Hour and minute offsets.
    '+05:00',
    '-03:30',
    '+00:00',
    '-12:00',
    '+14:00',
    // Redundant zero-padded hour and minute offsets.
    '+00:00',
    '-00:00',
  ];

  const VALID_TIMEZONE_IDENTIFIERS = [
    // Valid IANA timezone.
    'America/New_York',
    // Valid IANA timezone.
    'Europe/London',
    // Valid IANA timezone.
    'Asia/Tokyo',
    // Valid IANA timezone with a slash.
    'America/Los_Angeles',
    // Valid IANA timezone with a digit.
    'Etc/GMT+1',
    // Valid IANA timezone with an underscore.
    'Africa/Johannesburg',
    // Valid IANA timezone with a hyphen.
    'America/Argentina/Buenos_Aires',
    // Valid IANA timezone that is a single word.
    'GMT',
    // Valid IANA timezone that is a single word.
    'UTC',
    // Valid IANA timezone that is a single word.
    'Zulu',
  ];

  const INVALID_TIMEZONE_IDENTIFIERS = [
    // Is an empty string.
    '',
    // Contains an invalid character.
    'America/New_York!',
    // Does not exist.
    'America/NonExistent',
    // Is a number.
    '123',
  ];

  const INVALID_TIMEZONE_OFFSETS = [
    '+1', // Single digit hour offsets
    '-9', // Single digit hour offsets
    '+1:00', // Single digit hour and minute offsets
    '-9:30', // Single digit hour and minute offsets
    '+0:00', // Single digit hour and minute offsets
    '-0:00', // Single digit hour and minute offsets
    '+24:00', // Out of scope
    '-24:00', // Out of scope
    '+05:0', // Single digit minute
    '+05:00:00', // Too many parts
    '12:00', // Missing sign
    '00:00', // Missing sign
    '++05:00', // Double sign
    '--05:00', // Double sign
    'A+05:00', // Invalid character
    '+0A:00', // Invalid character
    '+05:0A', // Invalid character
    '+05:', // Missing minutes
    ':00', // Missing hours and sign
    ' +05:00', // Leading space
    '+05:00 ', // Trailing space
    'PST', // Not an offset string
    'Z', // Not an offset string
    'UTC+5', // Not an offset string
    'GMT-3', // Not an offset string
    '0', // Single digit
    '00', // Two digits, but not an offset
    '+', // Just a sign
    '-', // Just a sign
    '::', // Only colons
    '+24', // Out of scope
    '05:00', // Missing sign
    '00:00', // Missing sign
    '+5:00', // Single digit hour
    '+05:60', // Invalid minute
    '+24:00', // Invalid hour
    'abc', // Not an offset
    '', // Empty string
    '+0500', // Missing colon
    'GMT+5', // Not an offset string
  ];

  describe('isValidTimezone', () => {
    [...VALID_TIMEZONE_IDENTIFIERS, ...VALID_TIMEZONE_OFFSETS].forEach(
      (timezone) => {
        it(`should return true for valid timezone: "${timezone}"`, () => {
          expect(isValidTimezone(timezone), `"${timezone}" should be valid`).to
            .be.true;
        });
      },
    );

    INVALID_TIMEZONE_IDENTIFIERS.forEach((timezone) => {
      it(`should return false for invalid timezone: "${timezone}"`, () => {
        expect(isValidTimezone(timezone), `"${timezone}" should be invalid`).to
          .be.false;
      });
    });
  });

  describe('EmulationProcessor.isTimeZoneOffsetString', () => {
    VALID_TIMEZONE_OFFSETS.forEach((offset) => {
      it(`should return true for valid offset: "${offset}"`, () => {
        expect(isTimeZoneOffsetString(offset), `"${offset}" should be valid`).to
          .be.true;
      });
    });

    [...INVALID_TIMEZONE_OFFSETS, ...VALID_TIMEZONE_IDENTIFIERS].forEach(
      (offset) => {
        it(`should return false for invalid offset: "${offset}"`, () => {
          expect(
            isTimeZoneOffsetString(offset),
            `"${offset}" should be invalid`,
          ).to.be.false;
        });
      },
    );
  });
});
