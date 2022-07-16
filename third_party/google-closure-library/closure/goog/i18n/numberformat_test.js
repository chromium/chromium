/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} Swapping using fully qualified name
 */
goog.module('goog.i18n.NumberFormatTest');
goog.setTestOnly();

// This tests in both polyfill and native ECMAScript mode for
// browsers that support native Intl NumberFormat.

// Note that tests with string patterns fall back to the polyfill version.

// In some cases, exact formatted output equivalence is not required
// between Closure and ECMAScript implementations.

// Note also that all parsing functions are performed by polyfill code.

// Sets up goog.USE_ECMASCRIPT_I18N_NUMF flag in each function.
let testECMAScriptOptions = [false];
if (Intl.NumberFormat) {
  // Add test if the browser environment supports ECMAScript implementation.

  // Check if compact and formatToParts are implemented.
  const probeOptions = {notation: 'compact', compactDisplay: 'short'};
  try {
    const fmt = new Intl.NumberFormat('en', probeOptions);
    let result = fmt.formatToParts(999999);
    if (result && result[0].value == '1' && result[1].value == 'M') {
      testECMAScriptOptions.push(true);
    }
  } catch (error) {
    // How to indicate failure?
  }
}

const CompactNumberFormatSymbols_de = goog.require('goog.i18n.CompactNumberFormatSymbols_de');
const CompactNumberFormatSymbols_en = goog.require('goog.i18n.CompactNumberFormatSymbols_en');
const CompactNumberFormatSymbols_fr = goog.require('goog.i18n.CompactNumberFormatSymbols_fr');
const CompactNumberFormatSymbols_sw = goog.require('goog.i18n.CompactNumberFormatSymbols_sw');
const CompactNumberFormatSymbols_sw_KE = goog.require('goog.i18n.CompactNumberFormatSymbols_sw_KE');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const NumberFormat = goog.require('goog.i18n.NumberFormat');
/** @suppress {extraRequire} */
const NumberFormatSymbols = goog.require('goog.i18n.NumberFormatSymbols');
const NumberFormatSymbols_ar = goog.require('goog.i18n.NumberFormatSymbols_ar');
const NumberFormatSymbols_ar_EG = goog.require('goog.i18n.NumberFormatSymbols_ar_EG');
const NumberFormatSymbols_ar_EG_u_nu_latn = goog.require('goog.i18n.NumberFormatSymbols_ar_EG_u_nu_latn');
const NumberFormatSymbols_bn = goog.require('goog.i18n.NumberFormatSymbols_bn');
const NumberFormatSymbols_de = goog.require('goog.i18n.NumberFormatSymbols_de');
const NumberFormatSymbols_en = goog.require('goog.i18n.NumberFormatSymbols_en');
const NumberFormatSymbols_en_AU = goog.require('goog.i18n.NumberFormatSymbols_en_AU');
const NumberFormatSymbols_en_US = goog.require('goog.i18n.NumberFormatSymbols_en_US');
const NumberFormatSymbols_fa = goog.require('goog.i18n.NumberFormatSymbols_fa');
const NumberFormatSymbols_ff_Adlm = goog.require('goog.i18n.NumberFormatSymbols_ff_Adlm');
const NumberFormatSymbols_fi = goog.require('goog.i18n.NumberFormatSymbols_fi');
const NumberFormatSymbols_fr = goog.require('goog.i18n.NumberFormatSymbols_fr');
const NumberFormatSymbols_ml = goog.require('goog.i18n.NumberFormatSymbols_ml');
const NumberFormatSymbols_mr = goog.require('goog.i18n.NumberFormatSymbols_mr');
const NumberFormatSymbols_my = goog.require('goog.i18n.NumberFormatSymbols_my');
const NumberFormatSymbols_ne = goog.require('goog.i18n.NumberFormatSymbols_ne');
const NumberFormatSymbols_pl = goog.require('goog.i18n.NumberFormatSymbols_pl');
const NumberFormatSymbols_ro = goog.require('goog.i18n.NumberFormatSymbols_ro');
const NumberFormatSymbols_sw = goog.require('goog.i18n.NumberFormatSymbols_sw');
const NumberFormatSymbols_sw_KE = goog.require('goog.i18n.NumberFormatSymbols_sw_KE');

/** @suppress {extraRequire} */
const NumberFormatSymbols_u_nu_latn = goog.require('goog.i18n.NumberFormatSymbols_u_nu_latn');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const googString = goog.require('goog.string');
const isVersion = goog.require('goog.userAgent.product.isVersion');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let expectedFailures;

const stubs = new PropertyReplacer();

// Helpers to set/get NativeMode

/**
 * Changes setting for Native mode
 * @param {boolean} new_setting
 */
function setNativeMode(new_setting) {
  stubs.set(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', new_setting);
}

/**
 * Assert that a pair of very large numbers represented as formatted strings are
 * approximately equal.
 * @param {string} str1
 * @param {string} str2
 */
function assertVeryBigNumberEquals(str1, str2) {
  const digitsAnonymized = (s) => s.replace(/[0-9]/g, '#');
  const valueOf = (s) => parseFloat(s.replace(/[^0-9.e+-]/g, ''));

  assertEquals(digitsAnonymized(str1), digitsAnonymized(str2));

  const val1 = valueOf(str1);
  const val2 = valueOf(str2);
  assertTrue(val1 > 1);
  assertTrue(val2 > 1);

  // Equal within the limits of JavaScript number precision.
  assertRoughlyEquals(val1, val2, 1);
}

/** @return {boolean} Whether we're on Linux Firefox 3.6.3. */
function isFirefox363Linux() {
  return product.FIREFOX && userAgent.LINUX && isVersion('3.6.3') &&
      !isVersion('3.6.4');
}

testSuite({
  getTestName: function() {
    return 'NumberFormat Tests';
  },

  setUpPage() {
    expectedFailures = new ExpectedFailures();
    stubs.set(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', false);
    setNativeMode(false);
  },

  /** @suppress {const} See go/const-js-library-faq */
  setUp() {
    // Always switch back to English on startup.
    stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_en);
    stubs.set(
        goog.i18n, 'NumberFormatSymbols_u_nu_latn', NumberFormatSymbols_en);
    stubs.set(
        goog.i18n, 'CompactNumberFormatSymbols', CompactNumberFormatSymbols_en);

    NumberFormat.setEnforceAsciiDigits(false);
  },

  tearDown() {
    expectedFailures.handleTearDown();
    stubs.reset();
    setNativeMode(false);
  },

  testVeryBigNumber() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;
      let fmt = new NumberFormat(NumberFormat.Format.CURRENCY);
      str = fmt.format(1785599999999999888888888888888);
      // when comparing big number, various platform have small different in
      // precision. We have to tolerate that using assertVeryBigNumberEquals.
      assertVeryBigNumberEquals(
          '$1,785,599,999,999,999,888,888,888,888,888.00', str);
      str = fmt.format(1.7856E30);
      assertVeryBigNumberEquals(
          '$1,785,600,000,000,000,000,000,000,000,000.00', str);
      str = fmt.format(1.3456E20);
      assertVeryBigNumberEquals('$134,560,000,000,000,000,000.00', str);

      fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      str = fmt.format(1.3456E20);
      assertVeryBigNumberEquals('134,560,000,000,000,000,000', str);

      fmt = new NumberFormat(NumberFormat.Format.PERCENT);
      str = fmt.format(1.3456E20);
      assertVeryBigNumberEquals('13,456,000,000,000,000,000,000%', str);

      fmt = new NumberFormat(NumberFormat.Format.SCIENTIFIC);
      str = fmt.format(1.3456E20);
      assertEquals('1E20', str);

      fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      str = fmt.format(-1.234567890123456e306);
      assertEquals(1 + 1 + 306 + 306 / 3, str.length);
      assertEquals('-1,234,567,890,123,45', str.substr(0, 21));

      str = fmt.format(Infinity);
      assertEquals('\u221e', str);
      str = fmt.format(-Infinity);
      assertEquals('-\u221e', str);

      fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);
      str = fmt.format(Infinity);
      assertEquals('\u221e', str);
      str = fmt.format(-Infinity);
      assertEquals('-\u221e', str);
    }
  },

  testStandardFormat() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;
      let fmt;
      try {
        let fmt = new NumberFormat(NumberFormat.Format.CURRENCY);
        str = fmt.format(1234.579);
        assertEquals('$1,234.58', str);
      } catch (err) {
        assert(err !== null);
      }
      fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      str = fmt.format(1234.579);
      assertEquals('1,234.579', str);
      fmt = new NumberFormat(NumberFormat.Format.PERCENT);
      str = fmt.format(1234.579);
      assertEquals('123,458%', str);
      fmt = new NumberFormat(NumberFormat.Format.SCIENTIFIC);
      str = fmt.format(1234.579);
      assertEquals('1E3', str);
      // Math.log(1000000)/Math.LN10 is strictly less than 6. Make sure it gets
      // formatted correctly.
      str = fmt.format(1000000);
      assertEquals('1E6', str);
    }
  },

  testNegativePercentage() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;
      let fmt = new NumberFormat('#,##0.00%');
      str = fmt.format(-1234.56);
      assertEquals('-123,456.00%', str);

      fmt = new NumberFormat(NumberFormat.Format.PERCENT);
      str = fmt.format(-1234.579);
      assertEquals('-123,458%', str);
    }
  },

  testNegativePercentagePattern() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;
      try {
        const fmt = new NumberFormat('#,##0.00%;(#,##0.00%)');
        str = fmt.format(1234.56);
        assertEquals('123,456.00%', str);
        str = fmt.format(-1234.56);
        assertEquals('(123,456.00%)', str);
      } catch (error) {
        // Custom patterns not yet supported in native mode.
        expectedFailures.handleException(error);
      }
    }
  },

  testCustomPercentage() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;
      try {
        const fmt = new NumberFormat(NumberFormat.Format.PERCENT);
        fmt.setMaximumFractionDigits(1);
        fmt.setMinimumFractionDigits(1);
        str = fmt.format(0.1291);
        assertEquals('12.9%', str);
        fmt.setMaximumFractionDigits(2);
        fmt.setMinimumFractionDigits(1);
        str = fmt.format(0.129);
        assertEquals('12.9%', str);
        fmt.setMaximumFractionDigits(2);
        fmt.setMinimumFractionDigits(1);
        str = fmt.format(0.12);
        assertEquals('12.0%', str);
        fmt.setMaximumFractionDigits(2);
        fmt.setMinimumFractionDigits(1);
        str = fmt.format(0.12911);
        assertEquals('12.91%', str);
      } catch (err) {
        assert(err !== null);
      }
    }
  },

  testBasicParse() {
    let value;
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const fmt = new NumberFormat('0.0000');
      value = fmt.parse('123.4579');
      assertEquals(123.4579, value);

      value = fmt.parse('+123.4579');
      assertEquals(123.4579, value);

      value = fmt.parse('-123.4579');
      assertEquals(-123.4579, value);
    }
  },

  testPrefixParse() {
    let value;
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const fmt = new NumberFormat('0.0;(0.0)');
      value = fmt.parse('123.4579');
      assertEquals(123.4579, value);

      value = fmt.parse('(123.4579)');
      assertEquals(-123.4579, value);
    }
  },

  testPercentParse() {
    let value;
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      let fmt = new NumberFormat('0.0;(0.0)');
      value = fmt.parse('123.4579%');
      assertEquals((123.4579 / 100), value);

      value = fmt.parse('(%123.4579)');
      assertEquals((-123.4579 / 100), value);

      value = fmt.parse('123.4579\u2030');
      assertEquals((123.4579 / 1000), value);

      value = fmt.parse('(\u2030123.4579)');
      assertEquals((-123.4579 / 1000), value);
    }
  },

  testPercentAndPerMillAdvance() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let value;
      let pos = [0];
      let fmt = new NumberFormat('0');
      value = fmt.parse('120%', pos);
      assertEquals(1.2, value);
      assertEquals(4, pos[0]);
      pos[0] = 0;
      value = fmt.parse('120\u2030', pos);
      assertEquals(0.12, value);
      assertEquals(4, pos[0]);
    }
  },

  testPercentAndPerMillParsing() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const implicitFmt = new NumberFormat('0;(0)');
      assertEquals(123 / 100, implicitFmt.parse('123%'));
      assertEquals(-123 / 100, implicitFmt.parse('(123%)'));
      assertEquals(123 / 1000, implicitFmt.parse('123‰'));
      assertEquals(-123 / 1000, implicitFmt.parse('(123‰)'));

      const explicitFmtPercent = new NumberFormat('0%;(0%)');
      assertEquals(123 / 100, explicitFmtPercent.parse('123%'));
      assertEquals(-123 / 100, explicitFmtPercent.parse('(123%)'));

      const explicitFmtPermill = new NumberFormat('0‰;(0‰)');
      assertEquals(123 / 1000, explicitFmtPermill.parse('123‰'));
      assertEquals(-123 / 1000, explicitFmtPermill.parse('(123‰)'));
    }
  },

  testInfinityParse() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      let value;
      const fmt = new NumberFormat('0.0;(0.0)');

      // gwt need to add those symbols first
      value = fmt.parse('\u221e');
      assertEquals(Number.POSITIVE_INFINITY, value);

      value = fmt.parse('(\u221e)');
      assertEquals(Number.NEGATIVE_INFINITY, value);
    }
  },

  testExponentParse() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let value;
      let fmt;

      fmt = new NumberFormat('#E0');
      value = fmt.parse('1.234E3');
      assertEquals(1.234E+3, value);

      fmt = new NumberFormat('0.###E0');
      value = fmt.parse('1.234E3');
      assertEquals(1.234E+3, value);

      fmt = new NumberFormat('#E0');
      value = fmt.parse('1.2345E4');
      assertEquals(12345.0, value);

      value = fmt.parse('1.2345E4');
      assertEquals(12345.0, value);

      value = fmt.parse('1.2345E+4');
      assertEquals(12345.0, value);
    }
  },

  testGroupingParse() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let value;

      let fmt = new NumberFormat('#,###');
      value = fmt.parse('1,234,567,890');
      assertEquals(1234567890, value);
      value = fmt.parse('12,3456,7890');
      assertEquals(1234567890, value);

      fmt = new NumberFormat('#');
      value = fmt.parse('1234567890');
      assertEquals(1234567890, value);
    }
  },

  testParsingStop() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const pos = [0];
      const fmt = new NumberFormat('###0.###E0');

      assertEquals(123.457, fmt.parse('123.457', pos));
      assertEquals(7, pos[0]);

      pos[0] = 0;
      assertEquals(123.457, fmt.parse('+123.457', pos));
      assertEquals(8, pos[0]);

      pos[0] = 0;
      assertEquals(123, fmt.parse('123 cars in the parking lot.', pos));
      assertEquals(3, pos[0]);

      pos[0] = 0;
      assertEquals(12, fmt.parse('12 + 12', pos));
      assertEquals(2, pos[0]);

      pos[0] = 0;
      assertEquals(12, fmt.parse('12+12', pos));
      assertEquals(2, pos[0]);

      pos[0] = 0;
      assertEquals(120, fmt.parse('1.2E+2', pos));
      assertEquals(6, pos[0]);

      pos[0] = 0;
      assertEquals(120, fmt.parse('1.2E+2-12', pos));
      assertEquals(6, pos[0]);
    }
  },

  testBasicFormat() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const fmt = new NumberFormat('0.0000');
      const str = fmt.format(123.45789179565757);
      assertEquals('123.4579', str);
    }
  },

  testGrouping() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;

      let fmt = new NumberFormat('#,###');
      str = fmt.format(1234567890);
      assertEquals('1,234,567,890', str);

      fmt = new NumberFormat('#,####');
      str = fmt.format(1234567890);
      assertEquals('12,3456,7890', str);

      fmt = new NumberFormat('#');
      str = fmt.format(1234567890);
      assertEquals('1234567890', str);
    }
  },

  testIndiaNumberGrouping() {
    stubs.replace(goog, 'LOCALE', 'hi-IN');
    let fmt;
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      // Test for a known grouping used and recognized in India
      if (!nativeMode) {
        fmt = new NumberFormat('#,##,###');
      } else {
        // Native mode should handle this via the locale.
        fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      }

      // Replace this string pattern

      let str = fmt.format(1);
      assertEquals('1', str);
      str = fmt.format(12);
      assertEquals('12', str);
      str = fmt.format(123);
      assertEquals('123', str);
      str = fmt.format(1234);
      assertEquals('1,234', str);
      str = fmt.format(12345);
      assertEquals('12,345', str);
      str = fmt.format(123456);
      assertEquals('1,23,456', str);
      str = fmt.format(1234567);
      assertEquals('12,34,567', str);
      str = fmt.format(12345678);
      assertEquals('1,23,45,678', str);
      str = fmt.format(123456789);
      assertEquals('12,34,56,789', str);
      str = fmt.format(1234567890);
      assertEquals('1,23,45,67,890', str);
      str = fmt.format(0);
      assertEquals('0', str);
      str = fmt.format(-1);
      assertEquals('-1', str);
      str = fmt.format(-12);
      assertEquals('-12', str);
      str = fmt.format(-123);
      assertEquals('-123', str);
      str = fmt.format(-1234);
      assertEquals('-1,234', str);
      str = fmt.format(-12345);
      assertEquals('-12,345', str);
      str = fmt.format(-123456);
      assertEquals('-1,23,456', str);
      str = fmt.format(-1234567);
      assertEquals('-12,34,567', str);
      str = fmt.format(-12345678);
      assertEquals('-1,23,45,678', str);
      str = fmt.format(-123456789);
      assertEquals('-12,34,56,789', str);
      str = fmt.format(-1234567890);
      assertEquals('-1,23,45,67,890', str);
    }
  },

  testUnknownNumberGroupings() {
    // Test for any future unknown grouping format in addition to India
    // Note: This test fails in native mode without a locale.
    let fmt = new NumberFormat('#,####,##,###');
    let str = fmt.format(1);
    assertEquals('1', str);
    str = fmt.format(12);
    assertEquals('12', str);
    str = fmt.format(123);
    assertEquals('123', str);
    str = fmt.format(1234);
    assertEquals('1,234', str);
    str = fmt.format(12345);
    assertEquals('12,345', str);
    str = fmt.format(123456);
    assertEquals('1,23,456', str);
    str = fmt.format(1234567);
    assertEquals('12,34,567', str);
    str = fmt.format(12345678);
    assertEquals('123,45,678', str);
    str = fmt.format(123456789);
    assertEquals('1234,56,789', str);
    str = fmt.format(1234567890);
    assertEquals('1,2345,67,890', str);
    str = fmt.format(11234567890);
    assertEquals('11,2345,67,890', str);
    str = fmt.format(111234567890);
    assertEquals('111,2345,67,890', str);
    str = fmt.format(1111234567890);
    assertEquals('1111,2345,67,890', str);
    str = fmt.format(11111234567890);
    assertEquals('1,1111,2345,67,890', str);
    str = fmt.format(0);
    assertEquals('0', str);
    str = fmt.format(-1);
    assertEquals('-1', str);
    str = fmt.format(-12);
    assertEquals('-12', str);
    str = fmt.format(-123);
    assertEquals('-123', str);
    str = fmt.format(-1234);
    assertEquals('-1,234', str);
    str = fmt.format(-12345);
    assertEquals('-12,345', str);
    str = fmt.format(-123456);
    assertEquals('-1,23,456', str);
    str = fmt.format(-1234567);
    assertEquals('-12,34,567', str);
    str = fmt.format(-12345678);
    assertEquals('-123,45,678', str);
    str = fmt.format(-123456789);
    assertEquals('-1234,56,789', str);
    str = fmt.format(-1234567890);
    assertEquals('-1,2345,67,890', str);
    str = fmt.format(-11234567890);
    assertEquals('-11,2345,67,890', str);
    str = fmt.format(-111234567890);
    assertEquals('-111,2345,67,890', str);
    str = fmt.format(-1111234567890);
    assertEquals('-1111,2345,67,890', str);
    str = fmt.format(-11111234567890);
    assertEquals('-1,1111,2345,67,890', str);

    fmt = new NumberFormat('#,#,##,###,#');
    str = fmt.format(1);
    assertEquals('1', str);
    str = fmt.format(12);
    assertEquals('1,2', str);
    str = fmt.format(123);
    assertEquals('12,3', str);
    str = fmt.format(1234);
    assertEquals('123,4', str);
    str = fmt.format(12345);
    assertEquals('1,234,5', str);
    str = fmt.format(123456);
    assertEquals('12,345,6', str);
    str = fmt.format(1234567);
    assertEquals('1,23,456,7', str);
    str = fmt.format(12345678);
    assertEquals('1,2,34,567,8', str);
    str = fmt.format(123456789);
    assertEquals('1,2,3,45,678,9', str);
    str = fmt.format(1234567890);
    assertEquals('1,2,3,4,56,789,0', str);
    str = fmt.format(0);
    assertEquals('0', str);
    str = fmt.format(-1);
    assertEquals('-1', str);
    str = fmt.format(-12);
    assertEquals('-1,2', str);
    str = fmt.format(-123);
    assertEquals('-12,3', str);
    str = fmt.format(-1234);
    assertEquals('-123,4', str);
    str = fmt.format(-12345);
    assertEquals('-1,234,5', str);
    str = fmt.format(-123456);
    assertEquals('-12,345,6', str);
    str = fmt.format(-1234567);
    assertEquals('-1,23,456,7', str);
    str = fmt.format(-12345678);
    assertEquals('-1,2,34,567,8', str);
    str = fmt.format(-123456789);
    assertEquals('-1,2,3,45,678,9', str);
    str = fmt.format(-1234567890);
    assertEquals('-1,2,3,4,56,789,0', str);
  },

  testPerMill() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;

      const fmt = new NumberFormat('###.###\u2030');
      str = fmt.format(0.4857);
      assertEquals('485.7\u2030', str);
    }
  },

  testCurrency() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(goog, 'LOCALE', 'en-CA');
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;
      let matched;

      let fmt = new NumberFormat('\u00a4#,##0.00;-\u00a4#,##0.00');
      str = fmt.format(1234.56);
      assertEquals('$1,234.56', str);
      str = fmt.format(-1234.56);
      assertEquals('-$1,234.56', str);

      // These with string patterns do not use native mode.
      fmt = new NumberFormat(
          '\u00a4#,##0.00;-\u00a4#,##0.00', 'USD',
          NumberFormat.CurrencyStyle.LOCAL);
      str = fmt.format(1234.56);
      assertEquals('$1,234.56', str);
      str = fmt.format(-1234.56);
      assertEquals('-$1,234.56', str);
      fmt = new NumberFormat(
          '\u00a4#,##0.00;-\u00a4#,##0.00', 'USD',
          NumberFormat.CurrencyStyle.PORTABLE);
      str = fmt.format(1234.56);
      assertEquals('US$1,234.56', str);
      str = fmt.format(-1234.56);
      assertEquals('-US$1,234.56', str);
      fmt = new NumberFormat(
          '\u00a4#,##0.00;-\u00a4#,##0.00', 'USD',
          NumberFormat.CurrencyStyle.GLOBAL);
      str = fmt.format(1234.56);
      assertEquals('USD $1,234.56', str);
      str = fmt.format(-1234.56);
      assertEquals('-USD $1,234.56', str);

      // Try LOCAL, PORTABLE, GLOBAL CurrencyStyle options with standard
      // patterns that excercise native mode.
      fmt = new NumberFormat(
          NumberFormat.Format.CURRENCY, 'USD',
          NumberFormat.CurrencyStyle.LOCAL);
      str = fmt.format(1234.56);
      assertEquals('$1,234.56', str);

      str = fmt.format(-1234.56);
      assertEquals('-$1,234.56', str);

      fmt = new NumberFormat(
          NumberFormat.Format.CURRENCY, 'CAD',
          NumberFormat.CurrencyStyle.LOCAL);
      str = fmt.format(1234.56);
      assertEquals('$1,234.56', str);

      // Check for US English locale.
      stubs.replace(goog, 'LOCALE', 'en-US');
      // PORTABLE behaves differently between JavaScript and native
      fmt = new NumberFormat(
          NumberFormat.Format.CURRENCY, 'USD',
          NumberFormat.CurrencyStyle.PORTABLE);
      str = fmt.format(1234.56);
      // Matches '(US)?$1,234.56' in locale en-US.
      matched = /^(US)?\$1,234\.56$/.test(str);
      assertTrue(matched);

      fmt = new NumberFormat(
          NumberFormat.Format.CURRENCY, 'CAD',
          NumberFormat.CurrencyStyle.PORTABLE);
      str = fmt.format(1234.56);
      // Matches 'C(A)?$1,234.56' in locale en-US.
      matched = /^C(A)?\$1,234\.56$/.test(str);
      assertTrue(matched);

      // Back to formatting for Canada.
      stubs.replace(goog, 'LOCALE', 'en-CA');
      fmt = new NumberFormat(
          NumberFormat.Format.CURRENCY, 'USD',
          NumberFormat.CurrencyStyle.PORTABLE);
      str = fmt.format(1234.56);
      // Matches 'US$1,234.56' as long as not in locale en-US.
      matched = /^US\$1,234\.56$/.test(str);
      assertTrue(matched);

      str = fmt.format(-1234.56);
      assertEquals('-US$1,234.56', str);

      fmt = new NumberFormat(
          NumberFormat.Format.CURRENCY, 'USD',
          NumberFormat.CurrencyStyle.GLOBAL);
      str = fmt.format(1234.56);

      // ECMAScript result not same as Closure
      // Expect 'USD 1,234.56' or 'USD $1,234.56'.
      matched = /^USD\s\$?1,234\.56$/.test(str);
      assertTrue(matched);

      str = fmt.format(-1234.56);
      // Expect '-USD 1,234.56' or '-USD $1,234.56'.
      matched = /^-USD\s(\$)?1,234\.56$/.test(str);
      assertTrue(matched);

      // Note that custom patterns use JavaScript, not native ECMAScript
      fmt = new NumberFormat('\u00a4\u00a4 #,##0.00;-\u00a4\u00a4 #,##0.00');
      str = fmt.format(1234.56);
      assertEquals('USD 1,234.56', str);
      fmt = new NumberFormat('\u00a4\u00a4 #,##0.00;\u00a4\u00a4 -#,##0.00');
      str = fmt.format(-1234.56);
      assertEquals('USD -1,234.56', str);

      fmt = new NumberFormat('\u00a4#,##0.00;-\u00a4#,##0.00', 'BRL');
      str = fmt.format(1234.56);
      assertEquals('R$1,234.56', str);
      str = fmt.format(-1234.56);
      assertEquals('-R$1,234.56', str);

      fmt = new NumberFormat(
          '\u00a4\u00a4 #,##0.00;(\u00a4\u00a4 #,##0.00)', 'BRL');
      str = fmt.format(1234.56);
      assertEquals('BRL 1,234.56', str);
      str = fmt.format(-1234.56);
      assertEquals('(BRL 1,234.56)', str);

      // Test implicit negative pattern.
      fmt = new NumberFormat('\u00a4#,##0.00');
      str = fmt.format(1234.56);
      assertEquals('$1,234.56', str);
      str = fmt.format(-1234.56);
      assertEquals('-$1,234.56', str);

      // Test lowercase currency code
      fmt = new NumberFormat('\u00a4#,##0.00', 'eur');
      str = fmt.format(1234.56);
      assertEquals('€1,234.56', str);
      str = fmt.format(-1234.56);
      assertEquals('-€1,234.56', str);
    }
  },

  testQuotes() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;

      let fmt = new NumberFormat('a\'fo\'\'o\'b#');
      str = fmt.format(123);
      assertEquals('afo\'ob123', str);

      fmt = new NumberFormat('a\'\'b#');
      str = fmt.format(123);
      assertEquals('a\'b123', str);

      fmt = new NumberFormat('a\'fo\'\'o\'b#');
      str = fmt.format(-123);
      assertEquals('-afo\'ob123', str);

      fmt = new NumberFormat('a\'\'b#');
      str = fmt.format(-123);
      assertEquals('-a\'b123', str);
    }
  },

  testZeros() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;
      let fmt;

      fmt = new NumberFormat('#.#');
      str = fmt.format(0);
      assertEquals('0', str);
      fmt = new NumberFormat('#.');
      str = fmt.format(0);
      assertEquals('0.', str);
      fmt = new NumberFormat('.#');
      str = fmt.format(0);
      assertEquals('.0', str);
      fmt = new NumberFormat('#');
      str = fmt.format(0);
      assertEquals('0', str);

      fmt = new NumberFormat('#0.#');
      str = fmt.format(0);
      assertEquals('0', str);
      fmt = new NumberFormat('#0.');
      str = fmt.format(0);
      assertEquals('0.', str);
      fmt = new NumberFormat('#.0');
      str = fmt.format(0);
      assertEquals('.0', str);
      fmt = new NumberFormat('#');
      str = fmt.format(0);
      assertEquals('0', str);
      fmt = new NumberFormat('000');
      str = fmt.format(0);
      assertEquals('000', str);
    }
  },

  testExponential() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let str;
      let fmt;

      fmt = new NumberFormat('0.####E0');
      str = fmt.format(0.01234);
      assertEquals('1.234E-2', str);
      fmt = new NumberFormat('00.000E00');
      str = fmt.format(0.01234);
      assertEquals('12.340E-03', str);
      fmt = new NumberFormat('##0.######E000');
      str = fmt.format(0.01234);
      assertEquals('12.34E-003', str);
      fmt = new NumberFormat('0.###E0;[0.###E0]');
      str = fmt.format(0.01234);
      assertEquals('1.234E-2', str);

      fmt = new NumberFormat('0.####E0');
      str = fmt.format(123456789);
      assertEquals('1.2346E8', str);
      fmt = new NumberFormat('00.000E00');
      str = fmt.format(123456789);
      assertEquals('12.346E07', str);
      fmt = new NumberFormat('##0.######E000');
      str = fmt.format(123456789);
      assertEquals('123.456789E006', str);
      fmt = new NumberFormat('0.###E0;[0.###E0]');
      str = fmt.format(123456789);
      assertEquals('1.235E8', str);

      fmt = new NumberFormat('0.####E0');
      str = fmt.format(1.23e300);
      assertEquals('1.23E300', str);
      fmt = new NumberFormat('00.000E00');
      str = fmt.format(1.23e300);
      assertEquals('12.300E299', str);
      fmt = new NumberFormat('##0.######E000');
      str = fmt.format(1.23e300);
      assertEquals('1.23E300', str);
      fmt = new NumberFormat('0.###E0;[0.###E0]');
      str = fmt.format(1.23e300);
      assertEquals('1.23E300', str);

      fmt = new NumberFormat('0.####E0');
      str = fmt.format(-3.141592653e-271);
      assertEquals('-3.1416E-271', str);
      fmt = new NumberFormat('00.000E00');
      str = fmt.format(-3.141592653e-271);
      assertEquals('-31.416E-272', str);
      fmt = new NumberFormat('##0.######E000');
      str = fmt.format(-3.141592653e-271);
      assertEquals('-314.159265E-273', str);
      fmt = new NumberFormat('0.###E0;[0.###E0]');
      str = fmt.format(-3.141592653e-271);
      assertEquals('[3.142E-271]', str);

      fmt = new NumberFormat('0.####E0');
      str = fmt.format(0);
      assertEquals('0E0', str);
      fmt = new NumberFormat('00.000E00');
      str = fmt.format(0);
      assertEquals('00.000E00', str);
      fmt = new NumberFormat('##0.######E000');
      str = fmt.format(0);
      assertEquals('0E000', str);
      fmt = new NumberFormat('0.###E0;[0.###E0]');
      str = fmt.format(0);
      assertEquals('0E0', str);

      fmt = new NumberFormat('0.####E0');
      str = fmt.format(-1);
      assertEquals('-1E0', str);
      fmt = new NumberFormat('00.000E00');
      str = fmt.format(-1);
      assertEquals('-10.000E-01', str);
      fmt = new NumberFormat('##0.######E000');
      str = fmt.format(-1);
      assertEquals('-1E000', str);
      fmt = new NumberFormat('0.###E0;[0.###E0]');
      str = fmt.format(-1);
      assertEquals('[1E0]', str);

      fmt = new NumberFormat('0.####E0');
      str = fmt.format(1);
      assertEquals('1E0', str);
      fmt = new NumberFormat('00.000E00');
      str = fmt.format(1);
      assertEquals('10.000E-01', str);
      fmt = new NumberFormat('##0.######E000');
      str = fmt.format(1);
      assertEquals('1E000', str);
      fmt = new NumberFormat('0.###E0;[0.###E0]');
      str = fmt.format(1);
      assertEquals('1E0', str);

      fmt = new NumberFormat('#E0');
      str = fmt.format(12345.0);
      assertEquals('1E4', str);
      fmt = new NumberFormat('0E0');
      str = fmt.format(12345.0);
      assertEquals('1E4', str);
      fmt = new NumberFormat('##0.###E0');
      str = fmt.format(12345.0);
      assertEquals('12.345E3', str);
      fmt = new NumberFormat('##0.###E0');
      str = fmt.format(12345.00001);
      assertEquals('12.345E3', str);
      fmt = new NumberFormat('##0.###E0');
      str = fmt.format(12345);
      assertEquals('12.345E3', str);

      fmt = new NumberFormat('##0.####E0');
      str = fmt.format(789.12345e-9);
      // Firefox 3.6.3 Linux is known to fail here with a rounding error.
      // fmt.format will return '789.1234E-9'.
      expectedFailures.expectFailureFor(isFirefox363Linux());
      try {
        assertEquals('789.1235E-9', str);
      } catch (e) {
        expectedFailures.handleException(e);
      }
      fmt = new NumberFormat('##0.####E0');
      str = fmt.format(780.e-9);
      assertEquals('780E-9', str);
      fmt = new NumberFormat('.###E0');
      str = fmt.format(45678.0);
      assertEquals('.457E5', str);
      fmt = new NumberFormat('.###E0');
      str = fmt.format(0);
      assertEquals('.0E0', str);

      fmt = new NumberFormat('#E0');
      str = fmt.format(45678000);
      assertEquals('5E7', str);
      fmt = new NumberFormat('##E0');
      str = fmt.format(45678000);
      assertEquals('46E6', str);
      fmt = new NumberFormat('####E0');
      str = fmt.format(45678000);
      assertEquals('4568E4', str);
      fmt = new NumberFormat('0E0');
      str = fmt.format(45678000);
      assertEquals('5E7', str);
      fmt = new NumberFormat('00E0');
      str = fmt.format(45678000);
      assertEquals('46E6', str);
      fmt = new NumberFormat('000E0');
      str = fmt.format(45678000);
      assertEquals('457E5', str);
      fmt = new NumberFormat('###E0');
      str = fmt.format(0.0000123);
      assertEquals('12E-6', str);
      fmt = new NumberFormat('###E0');
      str = fmt.format(0.000123);
      assertEquals('123E-6', str);
      fmt = new NumberFormat('###E0');
      str = fmt.format(0.00123);
      assertEquals('1E-3', str);
      fmt = new NumberFormat('###E0');
      str = fmt.format(0.0123);
      assertEquals('12E-3', str);
      fmt = new NumberFormat('###E0');
      str = fmt.format(0.123);
      assertEquals('123E-3', str);
      fmt = new NumberFormat('###E0');
      str = fmt.format(1.23);
      assertEquals('1E0', str);
      fmt = new NumberFormat('###E0');
      str = fmt.format(12.3);
      assertEquals('12E0', str);
      fmt = new NumberFormat('###E0');
      str = fmt.format(123.0);
      assertEquals('123E0', str);
      fmt = new NumberFormat('###E0');
      str = fmt.format(1230.0);
      assertEquals('1E3', str);
    }
  },

  testPlusSignInExponentPart() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let fmt = new NumberFormat('0E+0');
      let str = fmt.format(45678000);
      assertEquals('5E+7', str);
    }
  },

  testGroupingParse2() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let value;
      let fmt;

      fmt = new NumberFormat('#,###');
      value = fmt.parse('1,234,567,890');
      assertEquals(1234567890, value);
      fmt = new NumberFormat('#,###');
      value = fmt.parse('12,3456,7890');
      assertEquals(1234567890, value);

      fmt = new NumberFormat('#');
      value = fmt.parse('1234567890');
      assertEquals(1234567890, value);
    }
  },

  testApis() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      let fmt;
      let str;

      fmt = new NumberFormat('#,###');
      str = fmt.format(1234567890);
      assertEquals('1,234,567,890', str);

      fmt = new NumberFormat('\u00a4#,##0.00;-\u00a4#,##0.00');
      str = fmt.format(1234.56);
      assertEquals('$1,234.56', str);
      fmt = new NumberFormat('\u00a4#,##0.00;(\u00a4#,##0.00)');
      str = fmt.format(-1234.56);
      assertEquals('($1,234.56)', str);

      fmt = new NumberFormat('\u00a4#,##0.00;-\u00a4#,##0.00', 'SEK');
      str = fmt.format(1234.56);
      assertEquals('kr1,234.56', str);
      fmt = new NumberFormat('\u00a4#,##0.00;(\u00a4#,##0.00)', 'SEK');
      str = fmt.format(-1234.56);
      assertEquals('(kr1,234.56)', str);
    }
  },

  testLocaleSwitch() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      stubs.replace(goog, 'LOCALE', 'fr');

      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_fr);
      stubs.set(
          goog.i18n, 'NumberFormatSymbols_u_nu_latn', NumberFormatSymbols_fr);
      stubs.set(
          goog.i18n, 'CompactNumberFormatSymbols',
          CompactNumberFormatSymbols_fr);

      // When this test is performed in test cluster, 2 out of 60 machines have
      // problem getting the symbol. It is likely to be caused by size of
      // uncompiled symbol file. There will not be an issue after it is
      // compiled.
      if (NumberFormatSymbols_fr.DECIMAL_SEP ==
          NumberFormatSymbols_en.DECIMAL_SEP) {
        // fails to load French symbols, skip the test.
        return;
      }

      let fmt = new NumberFormat('#,###');
      let str = fmt.format(1234567890);
      assertEquals('1\u202F234\u202F567\u202F890', str);

      fmt = new NumberFormat('\u00a4#,##0.00;-\u00a4#,##0.00');
      str = fmt.format(1234.56);
      assertEquals('\u20AC1\u202F234,56', str);
      fmt = new NumberFormat('\u00a4#,##0.00;(\u00a4#,##0.00)');
      str = fmt.format(-1234.56);
      assertEquals('(\u20AC1\u202F234,56)', str);

      fmt = new NumberFormat('\u00a4#,##0.00;-\u00a4#,##0.00', 'SEK');
      str = fmt.format(1234.56);
      assertEquals('kr1\u202F234,56', str);
      fmt = new NumberFormat('\u00a4#,##0.00;(\u00a4#,##0.00)', 'SEK');
      str = fmt.format(-1234.56);
      assertEquals('(kr1\u202F234,56)', str);
    }
  },

  testFrenchParse() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      stubs.replace(goog, 'LOCALE', 'fr');

      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_fr);
      stubs.set(
          goog.i18n, 'NumberFormatSymbols_u_nu_latn', NumberFormatSymbols_fr);
      stubs.set(
          goog.i18n, 'CompactNumberFormatSymbols',
          CompactNumberFormatSymbols_fr);

      // When this test is performed in test cluster, 2 out of 60 machines have
      // problem getting the symbol. It is likely to be caused by size of
      // uncompiled symbol file. There will not be an issue after it is
      // compiled.
      if (NumberFormatSymbols_fr.DECIMAL_SEP ==
          NumberFormatSymbols_en.DECIMAL_SEP) {
        // fails to load French symbols, skip the test.
        return;
      }

      let fmt = new NumberFormat('0.0000');
      let value = fmt.parse('0,30');
      assertEquals(0.30, value);

      fmt = new NumberFormat(NumberFormat.Format.CURRENCY);
      value = fmt.parse('0,30\u00A0\u20AC');
      assertEquals(0.30, value);
      fmt = new NumberFormat('#,##0.00');
      value = fmt.parse('123 456,99');
      assertEquals(123456.99, value);

      fmt = new NumberFormat('#,##0.00');
      value = fmt.parse('123\u00a0456,99');
      assertEquals(123456.99, value);

      fmt = new NumberFormat('#,##0.00');
      value = fmt.parse('8 123\u00a0456,99');
      assertEquals(8123456.99, value);
    }
  },

  testFailParseShouldThrow() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      let fmt = new NumberFormat('0.0000');
      let value = fmt.parse('x');
      assertNaN(value);

      fmt = new NumberFormat('0.000x');
      value = fmt.parse('3y');
      assertNaN(value);

      fmt = new NumberFormat('x0.000');
      value = fmt.parse('y3');
      assertNaN(value);
    }
  },

  testEnforceAscii() {
    stubs.replace(goog, 'LOCALE', 'ar-EG');
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_ar_EG);

      stubs.set(
          goog.i18n, 'NumberFormatSymbols_u_nu_latn',
          NumberFormatSymbols_ar_EG_u_nu_latn);

      NumberFormat.setEnforceAsciiDigits(false);
      let fmt = new NumberFormat(NumberFormat.Format.PERCENT);
      fmt.setMinimumFractionDigits(4);
      fmt.setMaximumFractionDigits(4);

      // let fmt = new NumberFormat('0.0000%');
      let str = fmt.format(123.45789179565757);
      assertEquals('١٢٬٣٤٥٫٧٨٩٢٪؜', str);
      // Formatted with pattern
      // assertEquals('١٢٣٤٥٫٧٨٩٢٪؜', str);

      NumberFormat.setEnforceAsciiDigits(true);
      fmt = new NumberFormat(NumberFormat.Format.PERCENT);
      fmt.setMinimumFractionDigits(4);
      fmt.setMaximumFractionDigits(4);
      // fmt = new NumberFormat('0.0000%');
      str = fmt.format(123.45789179565757);
      assertEquals('12,345.7892‎%‎', str);
    }
  },

  testFractionDigits() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      fmt.setMinimumFractionDigits(4);
      fmt.setMaximumFractionDigits(6);
      assertEquals('0.1230', fmt.format(0.123));
      assertEquals('0.123456', fmt.format(0.123456));
      assertEquals('0.123457', fmt.format(0.12345678));
    }
  },

  testFractionDigits_possibleLossOfPrecision() {
    // See: https://github.com/google/closure-library/issues/916

    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      // Given
      const fracDigits = 12;
      const mantissa = 1.1;
      const magnitude = 15;
      const value = parseFloat(`${mantissa}e${magnitude}`);
      const shiftedValue =
          parseFloat(`${mantissa}e` + (fracDigits + magnitude));

      // Confirm that this case risks loss of precision.
      assertNotEquals(shiftedValue / Math.pow(10, fracDigits), value);

      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      fmt.setMaximumFractionDigits(fracDigits);

      // When & Then
      assertEquals('1,100,000,000,000,000', fmt.format(value));
    }
  },

  testFractionDigitsSetOutOfOrder() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      // First, setup basic min/max
      fmt.setMinimumFractionDigits(2);
      fmt.setMaximumFractionDigits(2);
      // Now change to a lower min & max, but change the max value first so that
      // it is temporarily less than the current "min" value.  This makes sure
      // that we don't throw an error.
      fmt.setMaximumFractionDigits(1);
      fmt.setMinimumFractionDigits(1);
      assertEquals('2.3', fmt.format(2.34));
    }
  },

  testFractionDigitsInvalid() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      fmt.setMinimumFractionDigits(2);
      fmt.setMaximumFractionDigits(1);
      try {
        fmt.format(0.123);
        fail('Should have thrown exception.');
      } catch (e) {
        assert(e !== null);
      }
    }
  },

  testFractionDigitsTooHigh() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      fmt.setMaximumFractionDigits(308);
      const err = assertThrows(() => {
        fmt.setMaximumFractionDigits(309);
      });
      assertEquals('Unsupported maximum fraction digits: 309', err.message);
    }
  },

  testSignificantDigitsEqualToMax() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      fmt.setMinimumFractionDigits(0);
      fmt.setMaximumFractionDigits(2);

      // NOTE: Significant digits are interpreted differently (incorrectly)
      // in Closure polyfill, applied only to the fraction part.
      // See b/191144056
      fmt.setSignificantDigits(2);

      let str = fmt.format(123.4);
      if (!nativeMode) {
        assertEquals('123', str);
      } else {
        assertEquals('120', str);
      }

      str = fmt.format(12.34);
      assertEquals('12', str);
      assertEquals('1.2', fmt.format(1.234));
      assertEquals('0.12', fmt.format(0.1234));
      assertEquals('0.13', fmt.format(0.1284));

      // When number of significant digits plus max fraction digits is greater
      // than the precision of numbers, rounding errors can occur.
      fmt.setSignificantDigits(12);
      fmt.setMaximumFractionDigits(12);
      assertEquals('60,000', fmt.format(60000));
    }
  },

  testSignificantDigitsLessThanMax() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      fmt.setMinimumFractionDigits(0);
      fmt.setMaximumFractionDigits(4);
      fmt.setSignificantDigits(1);

      // Differences in handling significant digits. See b/191144056
      let str = fmt.format(123.4);
      if (!nativeMode) {
        assertEquals('123', str);
      } else {
        assertEquals('100', str);
      }
      str = fmt.format(12.34);
      if (!nativeMode) {
        assertEquals('12', str);
      } else {
        assertEquals('10', str);
      }

      assertEquals('1', fmt.format(1.234));
      assertEquals('0.1', fmt.format(0.1234));
      assertEquals('0.2', fmt.format(0.1834));
    }
  },

  testSignificantDigitsMoreThanMax() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      // Max fractional digits should be absolute
      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      fmt.setMinimumFractionDigits(0);
      fmt.setMaximumFractionDigits(2);
      fmt.setSignificantDigits(3);

      assertEquals('123', fmt.format(123.4));
      assertEquals('12.3', fmt.format(12.34));
      assertEquals('1.23', fmt.format(1.234));
      assertEquals('0.12', fmt.format(0.1234));
      assertEquals('0.13', fmt.format(0.1284));
      assertEquals('-0.12', fmt.format(-0.1234));
      assertEquals('-0.13', fmt.format(-0.1284));
    }
  },

  testNegativeDecimalFinnish() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      stubs.replace(goog, 'LOCALE', 'fi');

      // Finnish uses a full-width dash for negative.
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_fi);

      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);

      const str = fmt.format(-123);
      assertEquals('−123', str);
    }
  },

  testSimpleCompactFrench() {
    // Switch to French.
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      stubs.replace(goog, 'LOCALE', 'fr');

      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_fr);
      stubs.set(
          goog.i18n, 'NumberFormatSymbols_u_nu_latn', NumberFormatSymbols_fr);
      stubs.set(
          goog.i18n, 'CompactNumberFormatSymbols',
          CompactNumberFormatSymbols_fr);

      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);

      const str = fmt.format(123400000);
      assertEquals('123\u00A0M', str);
    }
  },

  testSimpleCompactGerman() {
    stubs.replace(goog, 'LOCALE', 'de');
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      // Switch to German.
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_de);
      stubs.set(
          goog.i18n, 'NumberFormatSymbols_u_nu_latn', NumberFormatSymbols_de);
      stubs.set(
          goog.i18n, 'CompactNumberFormatSymbols',
          CompactNumberFormatSymbols_de);

      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);

      // The german short compact decimal has a simple '0' for 1000's, which is
      // supposed to be interpreted as 'leave the number as-is'.
      // (The number itself will still be formatted with the '.', but no
      // rounding)
      const str = fmt.format(1234);
      if (!nativeMode) {
        assertEquals('1.234', str);
      } else {
        assertEquals('1234', str);
      }
      const str2 = fmt.format(12345);
      assertEquals('12.345', str2);
    }
  },

  testSimpleCompact1() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);

      const str = fmt.format(1234);
      assertEquals('1.2K', str);
    }
  },

  testSimpleCompact2() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);

      const str = fmt.format(12345);
      assertEquals('12K', str);
    }
  },

  testRoundingCompact() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);

      const str = fmt.format(999999);
      assertEquals('1M', str);  // as opposed to 1000k
    }
  },

  testRoundingCompactNegative() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);

      const str = fmt.format(-999999);
      assertEquals('-1M', str);
    }
  },

  testCompactSmall() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);

      const str = fmt.format(0.1234);
      assertEquals('0.12', str);
    }
  },

  testCompactLong() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_LONG);

      const str = fmt.format(12345);
      assertEquals('12 thousand', str);
    }
  },

  testCompactWithoutSignificant() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);
      fmt.setSignificantDigits(0);
      fmt.setMinimumFractionDigits(2);
      fmt.setMaximumFractionDigits(2);

      assertEquals('1.23K', fmt.format(1234));
      assertEquals('1.00K', fmt.format(1000));
      assertEquals('123.46K', fmt.format(123456.7));
      assertEquals('999.99K', fmt.format(999994));
      assertEquals('1.00M', fmt.format(999995));
    }
  },

  testCompactWithoutSignificant2() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);
      fmt.setSignificantDigits(0);
      fmt.setMinimumFractionDigits(0);
      fmt.setMaximumFractionDigits(2);

      assertEquals('1.23K', fmt.format(1234));
      assertEquals('1K', fmt.format(1000));
      assertEquals('123.46K', fmt.format(123456.7));
      assertEquals('999.99K', fmt.format(999994));
      assertEquals('1M', fmt.format(999995));
    }
  },

  testCompactFallbacks() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const cdfSymbols = {
        COMPACT_DECIMAL_SHORT_PATTERN: {'1000': {'other': '0K'}}
      };

      stubs.set(goog.i18n, 'CompactNumberFormatSymbols', cdfSymbols);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_LONG);
      const str = fmt.format(220000000000000);
      if (!nativeMode) {
        assertEquals('220,000,000,000K', str);
      } else {
        // Resetting cdfSymbols will not change native results
        assertEquals('220 trillion', str);
      }
    }
  },

  testShowTrailingZerosWithSignificantDigits() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      fmt.setSignificantDigits(2);
      fmt.setShowTrailingZeros(true);

      let result = fmt.format(2);
      assertEquals('2.0', result);
      result = fmt.format(2000);
      assertEquals('2,000', result);
      result = fmt.format(0.2);
      assertEquals('0.20', result);
      result = fmt.format(0.02);
      assertEquals('0.02', result);
      result = fmt.format(0.002);
      assertEquals('0.002', result);
      result = fmt.format(0);
      assertEquals('0.00', result);

      fmt.setShowTrailingZeros(false);
      assertEquals('2', fmt.format(2));
      assertEquals('0.2', fmt.format(0.2));
    }
  },

  testShowTrailingZerosWithSignificantDigitsCompactShort() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);
      fmt.setSignificantDigits(2);
      fmt.setShowTrailingZeros(true);

      let result = fmt.format(2);
      assertEquals('2.0', result);
      result = fmt.format(2000);
      assertEquals('2.0K', result);
      result = fmt.format(20);
      assertEquals('20', result);
    }
  },

  testCurrencyCodeOrder() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_fr);
      stubs.set(
          goog.i18n, 'CompactNumberFormatSymbols',
          CompactNumberFormatSymbols_fr);
      stubs.replace(goog, 'LOCALE', 'fr');

      let fmt = new NumberFormat(NumberFormat.Format.CURRENCY);
      assertFalse(fmt.isCurrencyCodeBeforeValue());

      stubs.replace(goog, 'LOCALE', 'en');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_en);
      stubs.set(
          goog.i18n, 'CompactNumberFormatSymbols',
          CompactNumberFormatSymbols_en);
      const fmt1 = new NumberFormat(NumberFormat.Format.CURRENCY);
      assertTrue(fmt1.isCurrencyCodeBeforeValue());

      // Check that we really have different formatters with different patterns
      assertFalse(fmt.isCurrencyCodeBeforeValue());

      // Using custom patterns instead of standard locale ones

      fmt = new NumberFormat('\u00A4 #0');
      assertTrue(fmt.isCurrencyCodeBeforeValue());

      fmt = new NumberFormat('\u00A4 0 and #');
      assertTrue(fmt.isCurrencyCodeBeforeValue());

      fmt = new NumberFormat('#0 \u00A4');
      assertFalse(fmt.isCurrencyCodeBeforeValue());

      fmt = new NumberFormat('0 and # \u00A4');
      assertFalse(fmt.isCurrencyCodeBeforeValue());

      fmt = new NumberFormat('\u00A4 0');
      assertTrue(fmt.isCurrencyCodeBeforeValue());

      fmt = new NumberFormat('0 \u00A4');
      assertFalse(fmt.isCurrencyCodeBeforeValue());

      fmt = new NumberFormat('\u00A4 #');
      assertTrue(fmt.isCurrencyCodeBeforeValue());

      fmt = new NumberFormat('# \u00A4');
      assertFalse(fmt.isCurrencyCodeBeforeValue());

      // Edge cases, should never happen (like #0 separated by currency symbol,
      // or missing currency symbol, or missing both # and 0, or missing all)
      // We still make sure we get reasonable results (as much as possible)

      fmt = new NumberFormat('0 \u00A4 #');
      assertFalse(fmt.isCurrencyCodeBeforeValue());

      fmt = new NumberFormat('# \u00A4 0');
      assertFalse(fmt.isCurrencyCodeBeforeValue());

      fmt = new NumberFormat('\u00A4');
      assertTrue(
          fmt.isCurrencyCodeBeforeValue());  // currency first, en_US style

      fmt = new NumberFormat('0');
      assertTrue(
          fmt.isCurrencyCodeBeforeValue());  // currency first, en_US style

      fmt = new NumberFormat('#');
      assertTrue(
          fmt.isCurrencyCodeBeforeValue());  // currency first, en_US style

      fmt = new NumberFormat('#0');
      assertTrue(
          fmt.isCurrencyCodeBeforeValue());  // currency first, en_US style

      fmt = new NumberFormat('0 and #');
      assertTrue(
          fmt.isCurrencyCodeBeforeValue());  // currency first, en_US style

      fmt = new NumberFormat('nothing');
      assertTrue(
          fmt.isCurrencyCodeBeforeValue());  // currency first, en_US style
    }
  },

  testCompactWithBaseFormattingNumber() {
    stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_en);
    stubs.set(
        goog.i18n, 'CompactNumberFormatSymbols', CompactNumberFormatSymbols_en);
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);

      // Expect that results will be computed with Javascript, not ECMAScript
      // because base formatting is not implementive natively.
      fmt.setBaseFormatting(1000);
      let result = fmt.format(800);
      assertEquals('0.8K', result);

      fmt.setBaseFormatting(null);
      result = fmt.format(800);
      assertEquals('800', result);

      fmt.setBaseFormatting(1000);
      result = fmt.format(1200000);
      assertEquals('1,200K', result);

      result = fmt.format(10);
      assertEquals('0.01K', result);

      fmt.setSignificantDigits(0);
      fmt.setMinimumFractionDigits(2);
      result = fmt.format(1);
      assertEquals('0.00K', result);
    }
  },

  testCompactWithBaseFormattingFrench() {
    // Switch to French.
    stubs.replace(goog, 'LOCALE', 'fr');
    stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_fr);
    stubs.set(
        goog.i18n, 'CompactNumberFormatSymbols', CompactNumberFormatSymbols_fr);
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);
      let str = fmt.format(123400000);
      let matched =
          /^123(\u00A0)?M$/.test(str);  // Optional non-breaking space.
      assertTrue(matched);
      fmt.setBaseFormatting(1000);
      str = fmt.format(123400000);
      // let matched = /^123\202F400(\u00A0)?k$/.test(str);  // Optional
      // non-breaking space.
      matched = /^123\u202F400\s?k$/.test(str);
      assertTrue(matched);
    }
  },

  testGetBaseFormattingNumber() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);
      assertEquals(null, fmt.getBaseFormatting());
      fmt.setBaseFormatting(10000);
      assertEquals(10000, fmt.getBaseFormatting());
    }
  },

  // Moved Polish, Romanian, other currencies to tier 2, check that it works now
  testPolish() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_pl);

      // Native mode formats LOCAL currencies differently from JavaScript.
      const fmPl = new NumberFormat(NumberFormat.Format.CURRENCY);
      let str = fmPl.format(100);
      if (nativeMode) {
        assertEquals('zł 100.00', str);  // local symbol before number
      } else {
        assertEquals('100,00\u00A0z\u0142', str);  // 100.00 zł
      }

      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_ro);
      const fmRo = new NumberFormat(NumberFormat.Format.CURRENCY);
      str = fmRo.format(100);
      if (nativeMode) {
        assertEquals('lei 100.00', str);  // local symbol before number
      } else {
        assertEquals('100,00\u00A0RON', str);
      }
    }
  },

  testCurrencyWithReducedFractionSize() {
    for (let nativeMode of testECMAScriptOptions) {
      // Check overriding the number of fractional digits for currency
      stubs.replace(goog, 'LOCALE', 'en-US');
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      let fmt = new NumberFormat(NumberFormat.Format.CURRENCY, 'USD');

      let result = fmt.format(1234.567);
      assertEquals('$1,234.57', result);

      fmt.setMinimumFractionDigits(0);
      fmt.setMaximumFractionDigits(0);
      result = fmt.format(1234.567);
      assertEquals('$1,235', result);

      fmt.setMinimumFractionDigits(4);
      fmt.setMaximumFractionDigits(4);
      result = fmt.format(1234.567);
      assertEquals('$1,234.5670', result);
    }
  },

  testVerySmallNumberScientific() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const f = new NumberFormat(NumberFormat.Format.SCIENTIFIC);
      const result = f.format(5e-324);
      // Be flexible in test results. Safari browser gives zero.
      assertTrue(result == '5E-324' || result == '0E0');
    }
  },

  testVerySmallNumberDecimal() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const f = new NumberFormat(NumberFormat.Format.DECIMAL);
      f.setSignificantDigits(3);
      f.setMaximumFractionDigits(100);

      let expected = '0.' + googString.repeat('0', 89) + '387';
      assertEquals(expected, f.format(3.87e-90));
      expected = '0.' + googString.repeat('0', 8) + '387';
      assertEquals(expected, f.format(3.87e-9));
      expected = '0.' + googString.repeat('0', 89) + '342';
      assertEquals(expected, f.format(3.42e-90));
      expected = '0.' + googString.repeat('0', 8) + '342';
      assertEquals(expected, f.format(3.42e-9));

      f.setSignificantDigits(2);
      expected = '0.' + googString.repeat('0', 89) + '39';
      assertEquals(expected, f.format(3.87e-90));
      expected = '0.' + googString.repeat('0', 8) + '39';
      assertEquals(expected, f.format(3.87e-9));
      expected = '0.' + googString.repeat('0', 89) + '34';
      assertEquals(expected, f.format(3.42e-90));
      expected = '0.' + googString.repeat('0', 8) + '34';
      assertEquals(expected, f.format(3.42e-9));

      f.setSignificantDigits(1);
      expected = '0.' + googString.repeat('0', 89) + '4';
      assertEquals(expected, f.format(3.87e-90));
      expected = '0.' + googString.repeat('0', 8) + '4';
      assertEquals(expected, f.format(3.87e-9));
      expected = '0.' + googString.repeat('0', 89) + '3';
      assertEquals(expected, f.format(3.42e-90));
      expected = '0.' + googString.repeat('0', 8) + '3';
      assertEquals(expected, f.format(3.42e-9));
    }
  },

  testSigDigitVsMaxFractionDigits() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const f = new NumberFormat(NumberFormat.Format.DECIMAL);
      f.setSignificantDigits(4);
      f.setMaximumFractionDigits(2);

      const result = f.format(0.12345);
      assertEquals('0.12', result);
    }
  },

  testSymbols_percent() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      stubs.replace(goog, 'LOCALE', 'en');
      /** @suppress {checkTypes} suppression added to enable type checking */
      const f = new NumberFormat(
          NumberFormat.Format.PERCENT, undefined, undefined,
          // Alternate percent symbol.
          Object.create(NumberFormatSymbols, {PERCENT: {'value': 'Percent'}}));
      let str = f.format(-0.25);
      assertEquals('-25Percent', str);
      str = f.format(0.25);
      assertEquals('25Percent', str);

      const f2 = new NumberFormat(
          NumberFormat.Format.PERCENT, undefined, undefined,
          NumberFormatSymbols_en);
      str = f2.format(-0.25);
      assertEquals('-25%', str);
      str = f2.format(0.25);
      assertEquals('25%', str);

      stubs.replace(goog, 'LOCALE', 'ar_EG');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_ar_EG);
      str = f.format(-0.25);
      assertEquals('-25Percent', str);
      str = f.format(0.25);
      assertEquals('25Percent', str);
      str = f2.format(-0.25);
      assertEquals('-25%', str);
      str = f2.format(0.25);
      assertEquals('25%', str);
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSymbols_permill() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const f = new NumberFormat(
          '#,##0\u2030', undefined, undefined,
          Object.create(NumberFormatSymbols, {PERMILL: {'value': 'Permill'}}));
      assertEquals('0Permill', f.format(0));

      assertEquals('0\u2030', new NumberFormat('#,##0\u2030').format(0));
    }
  },

  testSymbols_expSymbol() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      stubs.replace(goog, 'LOCALE', 'en_AU');
      const f = new NumberFormat(
          NumberFormat.Format.SCIENTIFIC, undefined, undefined,
          NumberFormatSymbols_en_AU);
      let str = f.format(1000);
      assertEquals('1e3', str);

      stubs.replace(goog, 'LOCALE', 'en');
      let defaultLocale = new NumberFormat(NumberFormat.Format.SCIENTIFIC);

      str = f.format(1000);
      assertEquals('1e3', str);
      str = defaultLocale.format(1000);
      assertEquals('1E3', str);

      stubs.replace(goog, 'LOCALE', 'en_AU');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_en_AU);
      defaultLocale = new NumberFormat(NumberFormat.Format.SCIENTIFIC);
      str = f.format(1000);
      assertEquals('1e3', str);
      str = defaultLocale.format(1000);
      assertEquals('1e3', str);

      stubs.replace(goog, 'LOCALE', 'en_US');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_en_US);
      defaultLocale = new NumberFormat(NumberFormat.Format.SCIENTIFIC);
      str = f.format(1000);
      assertEquals('1e3', str);
      str = defaultLocale.format(1000);
      assertEquals('1E3', str);
    }
  },

  testScientific_ar_rtl() {
    stubs.replace(goog, 'LOCALE', 'ar-EG');
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      const scientific = new NumberFormat(
          NumberFormat.Format.SCIENTIFIC, undefined, undefined,
          NumberFormatSymbols_ar_EG);
      // TODO(user) Fix polyfill mode to output exponent in preferred
      // digits
      if (!nativeMode) {
        assertEquals('١اس3', scientific.format(1000));
        assertEquals('١اس5', scientific.format(123456));
      } else {
        // Expect results in all native digits
        assertEquals('١اس٣', scientific.format(1000));
        assertEquals('١اس٥', scientific.format(123456));
      }
    }
  },

  testUnknownCurrency() {
    // Tests with custom pattern - no native mode.
    const nativeMode = true;
    stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
    const cases = [
      // GMD is a known currency where the symbol is itself the ISO Code
      ['GMD', NumberFormat.CurrencyStyle.LOCAL, 'GMD100.00'],
      ['GMD', NumberFormat.CurrencyStyle.PORTABLE, 'GMD100.00'],
      ['GMD', NumberFormat.CurrencyStyle.GLOBAL, 'GMD100.00'],
      // XXY is an unknown currency
      ['XXY', NumberFormat.CurrencyStyle.LOCAL, 'XXY100.00'],
      ['XXY', NumberFormat.CurrencyStyle.PORTABLE, 'XXY100.00'],
      ['XXY', NumberFormat.CurrencyStyle.GLOBAL, 'XXY100.00'],
      // Test lowercase currency code
      ['xxy', NumberFormat.CurrencyStyle.GLOBAL, 'XXY100.00'],
    ];
    for (let [isoCode, style, expected] of cases) {
      const fmt = new NumberFormat('¤#,##0.00', isoCode, style);
      assertEquals(expected, fmt.format(100));
    }
  },

  testUnknownCurrency2Native() {
    // Tests native mode with LOCAL, PORTABLE, and GLOBAL options.
    const nativeMode = true;
    stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
    // Output from native mode must be flexible due to slightly different
    // implementations.
    const cases = [
      // GMD is a known currency where the symbol is itself the ISO Code
      ['GMD', NumberFormat.CurrencyStyle.LOCAL, 'GMD 100.00', 'D100.00', ''],
      [
        'GMD', NumberFormat.CurrencyStyle.PORTABLE, 'GMD 100.00', 'GMD 100.00',
        'D100.00'
      ],
      [
        'GMD', NumberFormat.CurrencyStyle.GLOBAL, 'GMD 100.00', 'GMD 100.00', ''
      ],
      // XXY is an unknown currency
      ['XXY', NumberFormat.CurrencyStyle.LOCAL, 'XXY 100.00', 'XXY100.00', ''],
      [
        'XXY', NumberFormat.CurrencyStyle.PORTABLE, 'XXY 100.00', 'XXY 100.00',
        'XXY100.00'
      ],
      [
        'XXY', NumberFormat.CurrencyStyle.GLOBAL, 'XXY 100.00', 'XXY 100.00', ''
      ],
      // Test lowercase currency code
      [
        'xxy', NumberFormat.CurrencyStyle.GLOBAL, 'XXY 100.00', 'XXY 100.00', ''
      ],
    ];
    for (let [isoCode, style, expected, alternate, alternative2] of cases) {
      let fmt = null;
      try {
        fmt = new NumberFormat(NumberFormat.Format.CURRENCY, isoCode, style);
        const result = fmt.format(100);
        assertTrue(
            result === expected || result === alternate ||
            result == alternative2);
      } catch (err) {
        // This will fail with Internet Explorer for some cases.
        // Since native mode will not be used with IE, this is OK
      }
    }
  },

  testThrowsOnInvalidCurrency() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      assertThrows(() => {
        new google.i18n.NumberFormat('¤#,##0.00', 'invalid!');
      });
    }
  },

  testCheckSwKeThousands() {
    stubs.replace(goog, 'LOCALE', 'sw');
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_sw_KE);
      stubs.set(
          goog.i18n, 'CompactNumberFormatSymbols',
          CompactNumberFormatSymbols_sw_KE);

      let fmt = new NumberFormat(NumberFormat.Format.COMPACT_LONG);

      // The Kenyan Swahili short compact decimal has two forms.
      // Check if it works.
      // (The number itself will still be formatted with the '.', but no
      // rounding)
      let str = fmt.format(1234);
      assertEquals('elfu 1.2', str);
      let negstr = fmt.format(-1234);
      assertEquals('elfu -1.2', negstr);
    }
  },

  testCheckSwCompactDecimal() {
    stubs.replace(goog, 'LOCALE', 'sw');
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_sw);
      stubs.set(
          goog.i18n, 'CompactNumberFormatSymbols',
          CompactNumberFormatSymbols_sw);

      const fmt = new NumberFormat(NumberFormat.Format.COMPACT_SHORT);
      const fmt_long = new NumberFormat(NumberFormat.Format.COMPACT_LONG);

      // The Swahili long compact decimal has two forms.
      // Check if it works. A: no, it doesn't
      let str = fmt.format(1234);
      assertEquals('elfu 1.2', str);
      let negstr = fmt.format(-1234);
      assertEquals('elfu -1.2', negstr);

      str = fmt.format(123400);
      assertEquals('elfu 123', str);
      negstr = fmt.format(-123400);
      assertEquals('elfu -123', negstr);
      negstr = fmt.format(-1234000);
      assertEquals('-1.2M', negstr);

      str = fmt_long.format(12340000);
      assertEquals('milioni 12', str);
      negstr = fmt_long.format(-123400000);
      assertEquals('milioni -123', negstr);

      negstr = fmt_long.format(-123400000000000);
      assertEquals('trilioni -123', negstr);
    }
  },

  testUnsetClosureLocale() {
    stubs.replace(goog, 'LOCALE', '');
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_sw);
      const fmt = new NumberFormat(NumberFormat.Format.DECIMAL);
      assertTrue(fmt != null);
    }
  },

  testAdlamDigits() {
    stubs.replace(goog, 'LOCALE', 'ff-Adlm');
    stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_ff_Adlm);
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);
      const fmt = new NumberFormat(
          NumberFormat.Format.DECIMAL, undefined, undefined,
          NumberFormatSymbols_ff_Adlm);

      // TODO: b/197251343
      let str = fmt.format(123.45);

      // Expect Adlam digits when ff_Adlm data is set for Adlam digits.
      // Some browsers don't support all locales.
      assertTrue(
          str == '123.45' || str === '𞥑𞥒𞥓.𞥔𞥕' ||
          str === '123,45');
    }
  },

  testNonAsciiDigitsNative() {
    for (let nativeMode of testECMAScriptOptions) {
      stubs.replace(NumberFormat, 'USE_ECMASCRIPT_I18N_NUMFORMAT', nativeMode);

      // Arabic with ASCII
      stubs.replace(goog, 'LOCALE', 'ar');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_ar);
      let ar = new NumberFormat(NumberFormat.Format.DECIMAL);
      let expected = '123';
      let result = ar.format(123);
      assertEquals(expected, result);

      // Egyptian Arabic
      stubs.replace(goog, 'LOCALE', 'ar-EG');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_ar_EG);
      let ar_EG = new NumberFormat(NumberFormat.Format.DECIMAL);
      expected = '١٢٣';
      result = ar_EG.format(123);
      assertEquals(expected, result);

      // Bengali
      stubs.replace(goog, 'LOCALE', 'bn');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_bn);
      let bn = new NumberFormat(NumberFormat.Format.DECIMAL);
      expected = '১২৩';
      result = bn.format(123);
      assertEquals(expected, result);

      // Persian / Farsi
      stubs.replace(goog, 'LOCALE', 'fa');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_fa);
      let fa = new NumberFormat(NumberFormat.Format.DECIMAL);
      expected = '۱۲۳';  // Different from Arabic digits
      result = fa.format(123);
      assertEquals(expected, result);

      // Malayalam
      stubs.replace(goog, 'LOCALE', 'ml');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_ml);
      let ml = new NumberFormat(NumberFormat.Format.DECIMAL);
      expected = '123';
      result = ml.format(123);
      assertEquals(expected, result);

      // Marathi
      stubs.replace(goog, 'LOCALE', 'mr');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_mr);
      let mr = new NumberFormat(NumberFormat.Format.DECIMAL);
      expected = '१२३';
      result = mr.format(123);
      assertEquals(expected, result);

      // Myanmar
      stubs.replace(goog, 'LOCALE', 'my');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_my);
      let my = new NumberFormat(NumberFormat.Format.DECIMAL);
      result = my.format(123);
      expected = '၁၂၃';
      assertEquals(expected, result);

      // Nepali
      stubs.replace(goog, 'LOCALE', 'ne');
      stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_ne);
      let ne = new NumberFormat(NumberFormat.Format.DECIMAL);
      result = ne.format(123);
      expected = '१२३';
      assertEquals(expected, result);
    }
  }
});
