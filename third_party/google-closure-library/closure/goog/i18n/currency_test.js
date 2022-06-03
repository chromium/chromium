/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.currencyTest');
goog.setTestOnly();

const NumberFormat = goog.require('goog.i18n.NumberFormat');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const RawCurrencyInfo = goog.require('goog.i18n.currency.CurrencyInfo');
const currency = goog.require('goog.i18n.currency');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

let CurrencyInfo = RawCurrencyInfo;

const stubs = new PropertyReplacer();

testSuite({
  setUp() {
    stubs.replace(
        currency, 'CurrencyInfo', googObject.clone(currency.CurrencyInfo));
    CurrencyInfo = currency.CurrencyInfo;
  },

  tearDown() {
    stubs.reset();
  },

  testIsAvailable() {
    assertTrue(currency.isAvailable('USD'));
    assertTrue(currency.isAvailable('CLP'));
    assertFalse(currency.isAvailable('LRD'));
    assertFalse(currency.isAvailable('XYZ'));
  },

  testAddTier2Support() {
    assertFalse('LRD' in CurrencyInfo);
    assertThrows(() => {
      currency.getLocalCurrencyPattern('LRD');
    });

    currency.addTier2Support();
    assertTrue('LRD' in CurrencyInfo);
    assertEquals('\'$\'#,##0.00', currency.getLocalCurrencyPattern('LRD'));
  },

  testCurrencyPattern() {
    assertEquals('\'$\'#,##0.00', currency.getLocalCurrencyPattern('USD'));
    assertEquals('\'US$\'#,##0.00', currency.getPortableCurrencyPattern('USD'));
    assertEquals('USD \'$\'#,##0.00', currency.getGlobalCurrencyPattern('USD'));

    assertEquals('\'¥\'#,##0', currency.getLocalCurrencyPattern('JPY'));
    assertEquals('\'JP¥\'#,##0', currency.getPortableCurrencyPattern('JPY'));
    assertEquals('JPY \'¥\'#,##0', currency.getGlobalCurrencyPattern('JPY'));

    assertEquals('\'€\'#,##0.00', currency.getLocalCurrencyPattern('EUR'));
    assertEquals('\'€\'#,##0.00', currency.getPortableCurrencyPattern('EUR'));
    assertEquals('EUR \'€\'#,##0.00', currency.getGlobalCurrencyPattern('EUR'));

    assertEquals('\'¥\'#,##0.00', currency.getLocalCurrencyPattern('CNY'));
    assertEquals(
        '\'RMB¥\'#,##0.00', currency.getPortableCurrencyPattern('CNY'));
    assertEquals('CNY \'¥\'#,##0.00', currency.getGlobalCurrencyPattern('CNY'));

    assertEquals('\'Rial\'#,##0', currency.getLocalCurrencyPattern('YER'));
    assertEquals('\'Rial\'#,##0', currency.getPortableCurrencyPattern('YER'));
    assertEquals('YER \'Rial\'#,##0', currency.getGlobalCurrencyPattern('YER'));

    assertEquals('\'CHF\'#,##0.00', currency.getLocalCurrencyPattern('CHF'));
    assertEquals('\'CHF\'#,##0.00', currency.getPortableCurrencyPattern('CHF'));
    assertEquals('\'CHF\'#,##0.00', currency.getGlobalCurrencyPattern('CHF'));

    assertEquals('\'$\'#,##0.00', currency.getLocalCurrencyPattern('TWD'));
    assertEquals('\'NT$\'#,##0.00', currency.getPortableCurrencyPattern('TWD'));
    assertEquals('TWD \'$\'#,##0.00', currency.getGlobalCurrencyPattern('TWD'));
  },

  testCurrencyFormatTWD() {
    let formatter;
    let str;

    formatter = new NumberFormat(currency.getLocalCurrencyPattern('TWD'));
    str = formatter.format(123456.7899);
    assertEquals('$123,456.79', str);

    formatter = new NumberFormat(currency.getPortableCurrencyPattern('TWD'));
    str = formatter.format(123456.7899);
    assertEquals('NT$123,456.79', str);

    formatter = new NumberFormat(currency.getGlobalCurrencyPattern('TWD'));
    str = formatter.format(123456.7899);
    assertEquals('TWD $123,456.79', str);
  },

  testCurrencyFormatCHF() {
    let formatter;
    let str;

    formatter = new NumberFormat(currency.getLocalCurrencyPattern('CHF'));
    str = formatter.format(123456.7899);
    assertEquals('CHF123,456.79', str);

    formatter = new NumberFormat(currency.getPortableCurrencyPattern('CHF'));
    str = formatter.format(123456.7899);
    assertEquals('CHF123,456.79', str);

    formatter = new NumberFormat(currency.getGlobalCurrencyPattern('CHF'));
    str = formatter.format(123456.7899);
    assertEquals('CHF123,456.79', str);
  },

  testCurrencyFormatYER() {
    let formatter;
    let str;

    formatter = new NumberFormat(currency.getLocalCurrencyPattern('YER'));
    str = formatter.format(123456.7899);
    assertEquals('Rial123,457', str);

    formatter = new NumberFormat(currency.getPortableCurrencyPattern('YER'));
    str = formatter.format(123456.7899);
    assertEquals('Rial123,457', str);

    formatter = new NumberFormat(currency.getGlobalCurrencyPattern('YER'));
    str = formatter.format(123456.7899);
    assertEquals('YER Rial123,457', str);
  },

  testCurrencyFormatCNY() {
    let formatter;
    let str;

    formatter = new NumberFormat(currency.getLocalCurrencyPattern('CNY'));
    str = formatter.format(123456.7899);
    assertEquals('¥123,456.79', str);

    formatter = new NumberFormat(currency.getPortableCurrencyPattern('CNY'));
    str = formatter.format(123456.7899);
    assertEquals('RMB¥123,456.79', str);

    formatter = new NumberFormat(currency.getGlobalCurrencyPattern('CNY'));
    str = formatter.format(123456.7899);
    assertEquals('CNY ¥123,456.79', str);
  },

  testCurrencyFormatCZK() {
    let formatter;
    let str;

    formatter = new NumberFormat(currency.getLocalCurrencyPattern('CZK'));
    str = formatter.format(123456.7899);
    assertEquals('123,456.79 Kč', str);

    formatter = new NumberFormat(currency.getPortableCurrencyPattern('CZK'));
    str = formatter.format(123456.7899);
    assertEquals('123,456.79 Kč', str);

    formatter = new NumberFormat(currency.getGlobalCurrencyPattern('CZK'));
    str = formatter.format(123456.7899);
    assertEquals('CZK 123,456.79 Kč', str);
  },

  testCurrencyFormatEUR() {
    let formatter;
    let str;

    formatter = new NumberFormat(currency.getLocalCurrencyPattern('EUR'));
    str = formatter.format(123456.7899);
    assertEquals('€123,456.79', str);

    formatter = new NumberFormat(currency.getPortableCurrencyPattern('EUR'));
    str = formatter.format(123456.7899);
    assertEquals('€123,456.79', str);

    formatter = new NumberFormat(currency.getGlobalCurrencyPattern('EUR'));
    str = formatter.format(123456.7899);
    assertEquals('EUR €123,456.79', str);
  },

  testCurrencyFormatJPY() {
    let formatter;
    let str;

    formatter = new NumberFormat(currency.getLocalCurrencyPattern('JPY'));
    str = formatter.format(123456.7899);
    assertEquals('¥123,457', str);

    formatter = new NumberFormat(currency.getPortableCurrencyPattern('JPY'));
    str = formatter.format(123456.7899);
    assertEquals('JP¥123,457', str);

    formatter = new NumberFormat(currency.getGlobalCurrencyPattern('JPY'));
    str = formatter.format(123456.7899);
    assertEquals('JPY ¥123,457', str);
  },

  testCurrencyFormatPLN() {
    let formatter;
    let str;

    formatter = new NumberFormat(currency.getLocalCurrencyPattern('PLN'));
    str = formatter.format(123456.7899);
    assertEquals('123,456.79 zł', str);

    formatter = new NumberFormat(currency.getPortableCurrencyPattern('PLN'));
    str = formatter.format(123456.7899);
    assertEquals('123,456.79 zł', str);

    formatter = new NumberFormat(currency.getGlobalCurrencyPattern('PLN'));
    str = formatter.format(123456.7899);
    assertEquals('PLN 123,456.79 zł', str);
  },

  testCurrencyFormatUSD() {
    let formatter;
    let str;

    formatter = new NumberFormat(currency.getLocalCurrencyPattern('USD'));
    str = formatter.format(123456.7899);
    assertEquals('$123,456.79', str);

    formatter = new NumberFormat(currency.getPortableCurrencyPattern('USD'));
    str = formatter.format(123456.7899);
    assertEquals('US$123,456.79', str);

    formatter = new NumberFormat(currency.getGlobalCurrencyPattern('USD'));
    str = formatter.format(123456.7899);
    assertEquals('USD $123,456.79', str);
  },

  testIsPrefixSignPosition() {
    assertTrue(currency.isPrefixSignPosition('USD'));
    assertTrue(currency.isPrefixSignPosition('EUR'));
  },

  testGetCurrencySign() {
    assertEquals('USD $', currency.getGlobalCurrencySign('USD'));
    assertEquals('$', currency.getLocalCurrencySign('USD'));
    assertEquals('US$', currency.getPortableCurrencySign('USD'));

    assertEquals('YER Rial', currency.getGlobalCurrencySign('YER'));
    assertEquals('Rial', currency.getLocalCurrencySign('YER'));
    assertEquals('Rial', currency.getPortableCurrencySign('YER'));

    assertThrows(() => {
      currency.getGlobalCurrencySign('XXY');
    });

    assertThrows(() => {
      currency.getLocalCurrencySign('XXY');
    });

    assertThrows(() => {
      currency.getPortableCurrencySign('XXY');
    });
  },

  testGetCurrencySignWithFallback() {
    assertEquals('USD $', currency.getGlobalCurrencySignWithFallback('USD'));
    assertEquals('$', currency.getLocalCurrencySignWithFallback('USD'));
    assertEquals('US$', currency.getPortableCurrencySignWithFallback('USD'));

    assertEquals('XXY', currency.getGlobalCurrencySignWithFallback('XXY'));
    assertEquals('XXY', currency.getLocalCurrencySignWithFallback('XXY'));
    assertEquals('XXY', currency.getPortableCurrencySignWithFallback('XXY'));
  },

  testAdjustPrecision() {
    // Known currency code, change to pattern
    assertEquals('0', currency.adjustPrecision('0.00', 'JPY'));

    // Known currency code, no change to pattern
    assertEquals('0.00', currency.adjustPrecision('0.00', 'USD'));

    // Unknown currency code
    assertEquals('0.00', currency.adjustPrecision('0.00', 'XXY'));
  },

  testIsValidCurrencyCode() {
    assertTrue(currency.isValid('USD'));
    assertTrue(currency.isValid('RUR'));
    assertTrue(currency.isValid('XXY'));
    assertTrue(currency.isValid('usd'));
    assertFalse(currency.isValid('invalid!'));
  },
});
