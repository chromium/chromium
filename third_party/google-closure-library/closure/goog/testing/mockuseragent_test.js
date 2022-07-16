/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.MockUserAgentTest');
goog.setTestOnly();

const MockUserAgent = goog.require('goog.testing.MockUserAgent');
const dispose = goog.require('goog.dispose');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let mockUserAgent;

testSuite({
  setUp() {
    mockUserAgent = new MockUserAgent();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  tearDown() {
    dispose(mockUserAgent);
    assertFalse(mockUserAgent.installed_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMockUserAgentInstall() {
    const originalUserAgentFunction = userAgent.getUserAgentString;

    assertFalse(!!mockUserAgent.installed_);

    mockUserAgent.install();
    assertTrue(mockUserAgent.installed_);
    assertNotEquals(userAgent.getUserAgentString, originalUserAgentFunction);

    mockUserAgent.uninstall();
    assertFalse(mockUserAgent.installed_);
    assertEquals(originalUserAgentFunction, userAgent.getUserAgentString);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMockUserAgentGetAgent() {
    const uaString = 'Mozilla/5.0 (Windows; U; Windows NT 6.0; en-US) ' +
        'AppleWebKit/525.13 (KHTML, like Gecko) ' +
        'Chrome/0.2.149.27 Safari/525.13';

    mockUserAgent = new MockUserAgent();
    mockUserAgent.setUserAgentString(uaString);
    mockUserAgent.install();

    assertTrue(mockUserAgent.installed_);
    assertEquals(uaString, userAgent.getUserAgentString());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMockUserAgentNavigator() {
    const fakeNavigator = {};

    mockUserAgent = new MockUserAgent();
    mockUserAgent.setNavigator(fakeNavigator);
    mockUserAgent.install();

    assertTrue(mockUserAgent.installed_);
    assertEquals(fakeNavigator, userAgent.getNavigator());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMockUserAgentDocumentMode() {
    const fakeDocumentMode = -1;

    mockUserAgent = new MockUserAgent();
    mockUserAgent.setDocumentMode(fakeDocumentMode);
    mockUserAgent.install();

    assertTrue(mockUserAgent.installed_);
    assertEquals(fakeDocumentMode, userAgent.getDocumentMode_());
    assertEquals(fakeDocumentMode, userAgent.DOCUMENT_MODE);
  },
});
