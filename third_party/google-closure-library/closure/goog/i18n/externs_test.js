/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Testing inclusion of externs and handling in compiled code.
 * Sets options for ECMASCript Intl object classes, retrieving them
 * as resolved options.
 *
 * This test is useful in two situations:
 *
 * 1. A brower release may include options in the resolvedOptions() result
 *    that are now yet expected by closure/i18n. The test will flag such
 *    options as unexpected. This wil signal that a new ECMAScript
 *    capability is implemented in that browser. In this case, the
 *    option, the browser, and the broswer should be added to
 *    newlySupportedKeys.
 *
 *    Note that when all supported modern browsers support an option, then
 *    the new option key should be added to javascript/externs/intl.js.
 *    This option key should then be removed from the newlySupportedKeys info.
 * 2. TODO(user): This test should be compiled and check at runtime that
 * the keys used in Intl class options bags aren't being unexpectedly renamed
 * and end up dropped by Intl resolvedOptions.
 */

/**
 * Namespaces for Closure classes with Intl implementations.
 */
goog.module('goog.i18n.externsTest');
goog.setTestOnly();

const browser = goog.require('goog.labs.userAgent.browser');

const testSuite = goog.require('goog.testing.testSuite');

/**
 * Check that all the resolved options are legal. If an option is not
 * defined in javascript/externs, compiling will give a garbled, unexpected
 * option in resolved options.
 *
 * Finding these unexpect values indicates that the externs file needs
 * to be updated.
 */

/* Resolved options that are not yet implemented in externs/intl.js
 * but do appear in the resolvedOptions keys of some browsers.
 * When all modern browsers support a given key, it should be added to
 * externs/intl.js for that resolvedOptions call.
 * The following is indexed by the Intl Class, followed by a list of newly
 * added keys with the browser and minimum browser version indicated.
 *
 * When a new key is added to externs/intl.js, the entries here should be
 * removed.
 *
 * Details are in #browser_compatibility in each specific Intl class here:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl
 */
const newlySupportedKeys = new Map();
newlySupportedKeys.set(Intl.NumberFormat, [
  // Safari numberFormat
  {key: 'roundingMode', browser: browser.isSafari(), minVersion: '15.6'},
  {key: 'roundingIncrement', browser: browser.isSafari(), minVersion: '15.6'},
  {key: 'trailingZeroDisplay', browser: browser.isSafari(), minVersion: '15.6'},
  {key: 'roundingPriority', browser: browser.isSafari(), minVersion: '15.6'},
  // Chrome numberFormat
  {key: 'roundingMode', browser: browser.isChrome(), minVersion: '106.0.0.0'},
  {
    key: 'roundingIncrement',
    browser: browser.isChrome(),
    minVersion: '106.0.0.0'
  },
  {
    key: 'trailingZeroDisplay',
    browser: browser.isChrome(),
    minVersion: '106.0.0.0'
  },
  {
    key: 'roundingPriority',
    browser: browser.isChrome(),
    minVersion: '106.0.0.0'
  },
]);

newlySupportedKeys.set(
    Intl.PluralRules,
    [{key: 'roundingMode', browser: browser.isSafari(), minVersion: '15.6'}]);

/* TODO:(b/243945751): Add compiled BUILD test for possible options keymangling.
 */

/**
 * Check if an option is a newly defined key that is not yet
 * in the externs/intl.js file.
 * @param {typeof Intl.NumberFormat|typeof Intl.PluralRules|typeof
 *     Intl.DateTimeFormat|typeof Intl.ListFormat|typeof
 *     Intl.RelativeTimeFormat} intlClass
 * @param {string} checkKey
 * @return {boolean} true if the key is already available.
 */
function checkNewKeys(intlClass, checkKey) {
  const newKeyInfo = newlySupportedKeys.get(intlClass);
  if (newKeyInfo) {
    for (let index in newKeyInfo) {
      const newKey = newKeyInfo[index];
      if (newKey.browser && newKey.key === checkKey &&
          browser.isVersionOrHigher(newKey.minVersion)) {
        return true;
      }
    }
  }
  return false;
}

testSuite({
  setUpPage() {},

  getTestName: function() {
    return 'Intl Extern Options test';
  },

  testDateTimeOptions() {
    /* For each set of options in setup:
       Create an object.
       Get the resolved options.
       Check each to see if it's in the allowed set of output options.
    */
    if (Intl === undefined || Intl.DateTimeFormat === undefined) return;

    const setup = [
      [
        ['dateStyle', 'long'],
        ['timeStyle', 'short'],
      ],
      [
        ['dateStyle', 'short'],
        ['timeStyle', 'full'],
        ['timeZone', 'UTC'],
      ],
      [
        ['calendar', 'islamic'],
        ['numberingSystem', 'arab'],
        ['dateStyle', 'short'],
        ['timeStyle', 'full'],
        ['timeZone', 'UTC'],
      ],
    ];

    const outputOptions = [
      'locale',
      'dateStyle',
      'timeStyle',
      'calendar',
      'dayPeriod',
      'numberingSystem',
      'timeZone',
      'hour12',
      'hourCycle',
      'formatMatcher',
      'weekday',
      'era',
      'year',
      'month',
      'day',
      'hour',
      'minute',
      'second',
      'fractionalSecondDigits',
      'timeZoneName',
    ];
    const locale = 'pl';
    const intlClass = Intl.DateTimeFormat;
    for (let i = 0; i < setup.length; i++) {
      const fmt = new Intl.DateTimeFormat(locale, setup[i]);
      const resolvedOptions = fmt.resolvedOptions();
      let unexpectedOptions = [];
      for (let option in resolvedOptions) {
        // Check that every option is expected unless it's new.
        if (!checkNewKeys(intlClass, option)) {
          if (!outputOptions.includes(option)) {
            unexpectedOptions.push(option);
          }
        }
      }
      /* There should be no values in the unexpectedOptions list. */
      assertArrayEquals(
          'Unexpected options include <' + unexpectedOptions +
              '>, version=' + browser.getVersion(),
          [], unexpectedOptions);
    }
  },

  testNumberFormatOptions() {
    /* For each set of options in setup:
       Create an object.
       Get the resolved options.
       Check each to see if it's in the allowed set of output options.
    */
    if (Intl === undefined || Intl.NumberFormat === undefined) return;

    const setup = [
      [
        ['dateStyle', 'long'],
        ['timeStyle', 'short'],
      ],
      [
        ['dateStyle', 'short'],
        ['timeStyle', 'full'],
        ['timeZone', 'UTC'],
      ],
      [
        ['calendar', 'islamic'],
        ['numberingSystem', 'arab'],
        ['dateStyle', 'short'],
        ['timeStyle', 'full'],
        ['timeZone', 'UTC'],
      ],
    ];
    const outputOptions = [
      'locale', 'numberingSystem', 'notation', 'compactDisplay', 'signDisplay',
      'useGrouping', 'currency', 'currencyDisplay', 'minimumIntegerDigits',
      'minimumFractionDigits', 'maximumFractionDigits',
      'minimumSignificantDigits', 'maximumSignificantDigits', 'style'
    ];
    const locale = 'pl';
    for (let i = 0; i < setup.length; i++) {
      const fmt = new Intl.NumberFormat(locale, setup[i]);
      const resolvedOptions = fmt.resolvedOptions();
      const intlClass = Intl.NumberFormat;

      let unexpectedOptions = [];
      for (let option in resolvedOptions) {
        // Check that every option is expected unless it's new.
        if (!checkNewKeys(intlClass, option)) {
          if (!outputOptions.includes(option)) {
            unexpectedOptions.push(option);
          }
        }
      }
      /* There should be no values in the unexpectedOptions list. */
      assertArrayEquals(
          'Unexpected options include <' + unexpectedOptions +
              '>, version=' + browser.getVersion(),
          [], unexpectedOptions);
    }
  },

  testPluralOptions() {
    /* For each set of options in setup:
       Create an object.
       Get the resolved options.
       Check each to see if it's in the allowed set of output options.
    */
    if (Intl === undefined || Intl.PluralRules === undefined) return;
    const setup = [
      [
        ['type', 'cardinal'], ['minimumIntegerDigits', 1],
        ['minimumFractionDigits', 0], ['maximumFractionDigits', 1]
      ],
      [
        ['type', 'ordinal'], ['minimumIntegerDigits', 1],
        ['minimumFractionDigits', 0], ['maximumFractionDigits', 1]
      ],
      [['minimumSignificantDigits', 1], ['maximumSignificantDigits', 1]],
    ];

    const outputOptions = [
      'locale', 'pluralCategories', 'type', 'minimumIntegerDigits',
      'minimumFractionDigits', 'maximumFractionDigits',
      'minimumSignificantDigits', 'maximumSignificantDigits'
    ];

    const locale = 'fr';
    for (let i = 0; i < setup.length; i++) {
      const fmt = new Intl.PluralRules(locale, setup[i]);
      const resolvedOptions = fmt.resolvedOptions();
      const intlClass = Intl.PluralRules;

      let unexpectedOptions = [];
      for (let option in resolvedOptions) {
        // Check that every option is expected unless it's new.
        if (!checkNewKeys(intlClass, option)) {
          if (!outputOptions.includes(option)) {
            unexpectedOptions.push(option);
          }
        }
      }
      /* There should be no values in the unexpectedOptions list. */
      assertArrayEquals(
          'Unexpected options include <' + unexpectedOptions +
              '>, version=' + browser.getVersion(),
          [], unexpectedOptions);
    }
  },

  testRdtfOptions() {
    /* For each set of options in setup of RelativeTimeFormat
       Create an object.
       Get the resolved options.
       Check each to see if it's in the allowed set of output options.
    */
    if (Intl === undefined || Intl.RelativeTimeFormat === undefined) return;

    const setup = [
      [
        [],
      ],
      [
        ['numeric', 'auto'],
      ],
      [
        ['style', 'long'],
      ],
      [
        ['numeric', 'auto'],
        ['style', 'long'],
      ],
    ];

    const outputOptions = ['locale', 'numberingSystem', 'numeric', 'style'];

    const locale = 'hl';
    for (let i = 0; i < setup.length; i++) {
      const fmt = new Intl.RelativeTimeFormat(locale, setup[i]);
      const resolvedOptions = fmt.resolvedOptions();
      const intlClass = Intl.RelativeTimeFormat;

      let unexpectedOptions = [];
      for (let option in resolvedOptions) {
        // Check that every option is expected unless it's new.
        if (!checkNewKeys(intlClass, option)) {
          if (!outputOptions.includes(option)) {
            unexpectedOptions.push(option);
          }
        }
      }
      /* There should be no values in the unexpectedOptions list. */
      assertArrayEquals(
          'Unexpected options include <' + unexpectedOptions +
              '>, version=' + browser.getVersion(),
          [], unexpectedOptions);
    }
  },

  testListFormat() {
    /* For each set of options in setup of ListFormat
       Create an object.
       Get the resolved options.
       Check each to see if it's in the allowed set of output options.
    */
    if (Intl === undefined || Intl.ListFormat === undefined) return;

    const setup = [
      [
        [],
      ],
      [
        ['type', 'disjunction'],
      ],
      [
        ['style', 'long'],
      ],
      [
        ['type', 'disjunction'],
        ['style', 'long'],
      ],
    ];

    const outputOptions = [
      'locale',
      'style',
      'type',
    ];

    const locale = 'ru';
    for (let i = 0; i < setup.length; i++) {
      const fmt = new Intl.ListFormat(locale, setup[i]);
      const resolvedOptions = fmt.resolvedOptions();
      const intlClass = Intl.ListFormat;

      let unexpectedOptions = [];
      for (let option in resolvedOptions) {
        // Check that every option is expected unless it's new.
        if (!checkNewKeys(intlClass, option)) {
          if (!outputOptions.includes(option)) {
            unexpectedOptions.push(option);
          }
        }
      }
      /* There should be no values in the unexpectedOptions list. */
      assertArrayEquals(
          'Unexpected options include <' + unexpectedOptions +
              '>, version=' + browser.getVersion(),
          [], unexpectedOptions);
    }
  },

  testBogusOption() {
    // Check that nothing is included in resolved options for
    // undefined options.

    if (Intl === undefined || Intl.DateTimeFormat === undefined) return;
    const options = [
      [
        ['unexpectedNumber', 1],
        ['unexpectedString', 'string'],
        ['calendar', 'islamic'],
        ['numberingSystem', 'arab'],
        ['dateStyle', 'short'],
        ['timeStyle', 'full'],
        ['timeZone', 'UTC'],
      ],
    ];

    const expectedOptions = [
      'locale',
      'dateStyle',
      'timeStyle',
      'calendar',
      'dayPeriod',
      'numberingSystem',
      'timeZone',
      'hour12',
      'hourCycle',
      'formatMatcher',
      'weekday',
      'era',
      'year',
      'month',
      'day',
      'hour',
      'minute',
      'second',
      'fractionalSecondDigits',
      'timeZoneName',
    ];
    const locale = 'ar-EG';
    const fmt = new Intl.DateTimeFormat(locale, options);
    const resolvedOptions = fmt.resolvedOptions();

    let resolvedCount = 0;
    for (let prop in resolvedOptions) resolvedCount++;
    let expectedCount = 0;
    const unexpectedResolved = [];
    for (let option in resolvedOptions) {
      // Check that every option is expected.
      if (expectedOptions.includes(option)) {
        expectedCount += 1;
      } else {
        unexpectedResolved.push(option);
      }
    }

    assertEquals(unexpectedResolved.length, 0);
    assertEquals(resolvedCount, expectedCount);
  }
});
