/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.textassert_test');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const textAssert = goog.require('goog.dom.textAssert');
const userAgent = goog.require('goog.userAgent');

testSuite({
  shouldRunTests() {
    return !userAgent.IE || userAgent.isVersionOrHigher(9);
  },

  testAssertIsTextThrowsWithHtmlTags: function() {
    const e = assertThrows(() => textAssert.assertHtmlFree('<b>a<\\b>'));
    assertEquals(
        'Assertion failed: String has HTML original: ' +
            '<b>a<\\b>, escaped: &lt;b&gt;a&lt;\\b&gt;',
        e.message);
  },

  testAssertIsTextThrowsWithHtmlEntities: function() {
    const e = assertThrows(() => {
      textAssert.assertHtmlFree('a&amp;b');
    });
    assertEquals(
        'Assertion failed: String has HTML original: ' +
            'a&amp;b, escaped: a&amp;amp;b',
        e.message);
  },

  testAssertIsTextDoesNotChangeText: function() {
    const plain = 'text';
    assertEquals(plain, textAssert.assertHtmlFree(plain));
  },
});
