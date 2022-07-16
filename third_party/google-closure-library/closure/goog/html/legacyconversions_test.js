/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for legacyconversions. */

goog.module('goog.html.legacyconversionsTest');
goog.setTestOnly();

const SafeHtml = goog.require('goog.html.SafeHtml');
const SafeScript = goog.require('goog.html.SafeScript');
const SafeStyle = goog.require('goog.html.SafeStyle');
const SafeStyleSheet = goog.require('goog.html.SafeStyleSheet');
const SafeUrl = goog.require('goog.html.SafeUrl');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const legacyconversions = goog.require('goog.html.legacyconversions');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Asserts that conversionFunction calls the report callback.
 * @param {function(string) : *} conversionFunction
 */
function assertFunctionReports(conversionFunction) {
  let reported = false;
  try {
    legacyconversions.setReportCallback(() => {
      reported = true;
    });
    conversionFunction('irrelevant');
    assertTrue('Expected legacy conversion to be reported.', reported);
  } finally {
    legacyconversions.setReportCallback(goog.nullFunction);
  }
}
testSuite({
  testSafeHtmlFromString() {
    const html = '<div>irrelevant</div>';
    const safeHtml = legacyconversions.safeHtmlFromString(html);
    assertEquals(html, SafeHtml.unwrap(safeHtml));

    assertFunctionReports(legacyconversions.safeHtmlFromString);
  },

  testSafeScriptFromString() {
    const script = 'alert(1);';
    const safeScript = legacyconversions.safeScriptFromString(script);
    assertEquals(script, SafeScript.unwrap(safeScript));

    assertFunctionReports(legacyconversions.safeScriptFromString);
  },

  testSafeStyleFromString() {
    const style = 'color: red; width: 1em;';
    const safeStyle = legacyconversions.safeStyleFromString(style);
    assertEquals(style, SafeStyle.unwrap(safeStyle));

    assertFunctionReports(legacyconversions.safeStyleFromString);
  },

  testSafeStyleSheetFromString() {
    const styleSheet =
        'P.special { color: red; background: url(http://test); }';
    const safeStyleSheet =
        legacyconversions.safeStyleSheetFromString(styleSheet);
    assertEquals(styleSheet, SafeStyleSheet.unwrap(safeStyleSheet));

    assertFunctionReports(legacyconversions.safeStyleSheetFromString);
  },

  testSafeUrlFromString() {
    const url = 'https://www.google.com';
    const safeUrl = legacyconversions.safeUrlFromString(url);
    assertEquals(url, SafeUrl.unwrap(safeUrl));

    assertFunctionReports(legacyconversions.safeUrlFromString);
  },

  testTrustedResourceUrlFromString() {
    const url = 'https://www.google.com/script.js';
    const trustedResourceUrl =
        legacyconversions.trustedResourceUrlFromString(url);
    assertEquals(url, TrustedResourceUrl.unwrap(trustedResourceUrl));

    assertFunctionReports(legacyconversions.trustedResourceUrlFromString);
  },
});
