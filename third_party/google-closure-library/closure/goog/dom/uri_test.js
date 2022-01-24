/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.uriTest');
goog.setTestOnly();

const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');
const uri = goog.require('goog.dom.uri');

testSuite({
  testNormalizeUri() {
    const baseUri = uri.normalizeUri('/');
    assertEquals(baseUri + 'foo', uri.normalizeUri('/foo'));
    assertEquals(baseUri + 'foo', uri.normalizeUri('/bar/../foo'));
    assertEquals('javascript:test', uri.normalizeUri('javascript:test'));
    assertEquals(
        'https://google.com/test', uri.normalizeUri('https://google.com/test'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetHref_withoutCredentials() {
    const div = document.createElement('div');
    div.innerHTML = '<a href="http://domain.com/">foo</a>';
    assertEquals(uri.getHref(div.children[0]), 'http://domain.com/');
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetHref_withCredentials() {
    const div = document.createElement('div');
    div.innerHTML = '<a href="http://user:pass@domain.com/">foo</a>';
    assertEquals(
        uri.getHref(div.children[0]),
        (product.EDGE || product.IE) ? null : 'http://user:pass@domain.com/');
  }
});
