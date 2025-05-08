/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.assertsTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const StrictMock = goog.require('goog.testing.StrictMock');
const asserts = goog.require('goog.dom.asserts');
const testSuite = goog.require('goog.testing.testSuite');

let stubs;

testSuite({
  setUpPage() {
    stubs = new PropertyReplacer();
  },

  tearDown() {
    stubs.reset();
  },

  testAssertIsLocation() {
    assertNotThrows(() => {
      asserts.assertIsLocation(window.location);
    });

    // Ad-hoc mock objects are allowed.
    const o = {foo: 'bar'};
    assertNotThrows(() => {
      asserts.assertIsLocation(o);
    });

    // So are fancy mocks.
    const mock = new StrictMock(window.location);
    assertNotThrows(() => {
      asserts.assertIsLocation(mock);
    });

    const linkElement = document.createElement('LINK');
    const ex = assertThrows(() => {
      asserts.assertIsLocation(linkElement);
    });
    assertContains('Argument is not a Location', ex.message);
  },
});
