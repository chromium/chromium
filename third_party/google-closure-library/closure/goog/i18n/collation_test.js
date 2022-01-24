/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.collationTest');
goog.setTestOnly();

const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const collation = goog.require('goog.i18n.collation');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let expectedFailures;
let propertyReplacer;

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
    propertyReplacer = new PropertyReplacer();
  },

  tearDown() {
    expectedFailures.handleTearDown();
  },

  testGetEnComparator() {
    propertyReplacer.replace(goog, 'LOCALE', 'en');
    const compare = collation.createComparator();
    // The côte/coté comparison fails in FF/Linux (v19.0) because
    // calling 'côte'.localeCompare('coté')  gives a negative number (wrong)
    // when the test is run but a positive number (correct) when calling
    // it later in the web console. FF/OSX doesn't have this problem.
    // Mozilla bug: https://bugzilla.mozilla.org/show_bug.cgi?id=856115
    expectedFailures.expectFailureFor(userAgent.GECKO && userAgent.LINUX);
    try {
      assertTrue(compare('côte', 'coté') > 0);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testGetFrComparator() {
    propertyReplacer.replace(goog, 'LOCALE', 'fr-CA');
    const compare = collation.createComparator();
    if (!collation.hasNativeComparator()) return;
    assertTrue(compare('côte', 'coté') < 0);
  },

  testGetNumericComparator() {
    const compare = collation.createComparator('en', {numeric: true});
    if (!collation.hasNativeComparator()) return;
    assertTrue(compare('2', '10') < 0);
  },

  testGetNonNumericComparator() {
    const compare = collation.createComparator('en', {numeric: false});
    if (!collation.hasNativeComparator()) return;
    assertTrue(compare('2', '10') > 0);
  },
});
