/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.textassert_test');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const textAssert = goog.require('goog.dom.textAssert');

testSuite({
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
