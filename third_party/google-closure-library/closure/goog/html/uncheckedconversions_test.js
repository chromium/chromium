/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for uncheckedconversions. */

goog.module('goog.html.uncheckedconversionsTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const Dir = goog.require('goog.i18n.bidi.Dir');
const SafeHtml = goog.require('goog.html.SafeHtml');
const SafeScript = goog.require('goog.html.SafeScript');
const SafeStyle = goog.require('goog.html.SafeStyle');
const SafeStyleSheet = goog.require('goog.html.SafeStyleSheet');
const SafeUrl = goog.require('goog.html.SafeUrl');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const testSuite = goog.require('goog.testing.testSuite');
const uncheckedconversions = goog.require('goog.html.uncheckedconversions');

testSuite({
  testSafeHtmlFromStringKnownToSatisfyTypeContract_ok() {
    const html = '<div>irrelevant</div>';
    const safeHtml =
        uncheckedconversions.safeHtmlFromStringKnownToSatisfyTypeContract(
            Const.from('Test'), html, Dir.LTR);
    assertEquals(html, SafeHtml.unwrap(safeHtml));
    assertEquals(Dir.LTR, safeHtml.getDirection());
  },

  testSafeHtmlFromStringKnownToSatisfyTypeContract_error() {
    assertThrows(() => {
      uncheckedconversions.safeHtmlFromStringKnownToSatisfyTypeContract(
          Const.from(''), 'irrelevant');
    });
  },

  testSafeScriptFromStringKnownToSatisfyTypeContract_ok() {
    const script = 'functionCall(\'irrelevant\');';
    const safeScript =
        uncheckedconversions.safeScriptFromStringKnownToSatisfyTypeContract(
            Const.from(
                'Safe because value is constant. Security review: b/7685625.'),
            script);
    assertEquals(script, SafeScript.unwrap(safeScript));
  },

  testSafeScriptFromStringKnownToSatisfyTypeContract_error() {
    assertThrows(() => {
      uncheckedconversions.safeScriptFromStringKnownToSatisfyTypeContract(
          Const.from(''), 'irrelevant');
    });
  },

  testSafeStyleFromStringKnownToSatisfyTypeContract_ok() {
    const style = 'P.special { color:red ; }';
    const safeStyle =
        uncheckedconversions.safeStyleFromStringKnownToSatisfyTypeContract(
            Const.from(
                'Safe because value is constant. Security review: b/7685625.'),
            style);
    assertEquals(style, SafeStyle.unwrap(safeStyle));
  },

  testSafeStyleFromStringKnownToSatisfyTypeContract_error() {
    assertThrows(() => {
      uncheckedconversions.safeStyleFromStringKnownToSatisfyTypeContract(
          Const.from(''), 'irrelevant');
    });
  },

  testSafeStyleSheetFromStringKnownToSatisfyTypeContract_ok() {
    const styleSheet = 'P.special { color:red ; }';
    const safeStyleSheet =
        uncheckedconversions.safeStyleSheetFromStringKnownToSatisfyTypeContract(
            Const.from(
                'Safe because value is constant. Security review: b/7685625.'),
            styleSheet);
    assertEquals(styleSheet, SafeStyleSheet.unwrap(safeStyleSheet));
  },

  testSafeStyleSheetFromStringKnownToSatisfyTypeContract_error() {
    assertThrows(() => {
      uncheckedconversions.safeStyleSheetFromStringKnownToSatisfyTypeContract(
          Const.from(''), 'irrelevant');
    });
  },

  testSafeUrlFromStringKnownToSatisfyTypeContract_ok() {
    const url = 'http://www.irrelevant.com';
    const safeUrl =
        uncheckedconversions.safeUrlFromStringKnownToSatisfyTypeContract(
            Const.from(
                'Safe because value is constant. Security review: b/7685625.'),
            url);
    assertEquals(url, SafeUrl.unwrap(safeUrl));
  },

  testSafeUrlFromStringKnownToSatisfyTypeContract_error() {
    assertThrows(() => {
      uncheckedconversions.safeUrlFromStringKnownToSatisfyTypeContract(
          Const.from(''), 'http://irrelevant.com');
    });
  },

  testTrustedResourceUrlFromStringKnownToSatisfyTypeContract_ok() {
    const url = 'http://www.irrelevant.com';
    const trustedResourceUrl =
        uncheckedconversions
            .trustedResourceUrlFromStringKnownToSatisfyTypeContract(
                Const.from(
                    'Safe because value is constant. Security review: b/7685625.'),
                url);
    assertEquals(url, TrustedResourceUrl.unwrap(trustedResourceUrl));
  },

  testTrustedResourceFromStringKnownToSatisfyTypeContract_error() {
    assertThrows(() => {
      uncheckedconversions
          .trustedResourceUrlFromStringKnownToSatisfyTypeContract(
              Const.from(''), 'http://irrelevant.com');
    });
  },
});
