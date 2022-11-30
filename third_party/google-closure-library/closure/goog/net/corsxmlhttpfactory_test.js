/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.CorsXmlHttpFactoryTest');
goog.setTestOnly();

const CorsXmlHttpFactory = goog.require('goog.net.CorsXmlHttpFactory');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testBrowserSupport() {
    const requestFactory = new CorsXmlHttpFactory();
    assertTrue(requestFactory.createInstance() instanceof XMLHttpRequest);
  },
});
