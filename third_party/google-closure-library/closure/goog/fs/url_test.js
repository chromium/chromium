/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.urlTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const testSuite = goog.require('goog.testing.testSuite');
const url = goog.require('goog.fs.url');

const stubs = new PropertyReplacer();

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  testBrowserSupportsObjectUrls() {
    stubs.remove(globalThis, 'URL');
    stubs.remove(globalThis, 'webkitURL');
    stubs.remove(globalThis, 'createObjectURL');

    assertFalse(url.browserSupportsObjectUrls());
    try {
      url.createObjectUrl();
      fail();
    } catch (e) {
      assertEquals(
          'This browser doesn\'t seem to support blob URLs', e.message);
    }

    const objectUrl = {};
    function createObjectURL() {
      return objectUrl;
    }
    stubs.set(globalThis, 'createObjectURL', createObjectURL);

    assertTrue(url.browserSupportsObjectUrls());
    assertEquals(objectUrl, url.createObjectUrl());

    stubs.reset();
  },
});
