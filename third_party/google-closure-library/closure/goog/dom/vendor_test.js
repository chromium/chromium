/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.vendorTest');
goog.setTestOnly();

const MockUserAgent = goog.require('goog.testing.MockUserAgent');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const dispose = goog.require('goog.dispose');
const googArray = goog.require('goog.array');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');
const userAgentTestUtil = goog.require('goog.userAgentTestUtil');
const vendor = goog.require('goog.dom.vendor');

let documentMode;
let mockUserAgent;
const propertyReplacer = new PropertyReplacer();

let getDocumentMode = () => documentMode;

const UserAgents = {
  GECKO: 'GECKO',
  IE: 'IE',
  WEBKIT: 'WEBKIT'
};

/**
 * Return whether a given user agent has been detected.
 * @param {number} agent Value in UserAgents.
 * @return {boolean} Whether the user agent has been detected.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function getUserAgentDetected(agent) {
  switch (agent) {
    case UserAgents.GECKO:
      return userAgent.GECKO;
    case UserAgents.IE:
      return userAgent.IE;
    case UserAgents.WEBKIT:
      return userAgent.WEBKIT;
  }
  return null;
}

/**
 * Test browser detection for a user agent configuration.
 * @param {Array<number>} expectedAgents Array of expected userAgents.
 * @param {string} uaString User agent string.
 * @param {string=} product Navigator product string.
 * @param {string=} opt_vendor Navigator vendor string.
 */
function assertUserAgent(
    expectedAgents, uaString, product = undefined, opt_vendor) {
  const mockNavigator = {
    'userAgent': uaString,
    'product': product,
    'vendor': opt_vendor
  };

  mockUserAgent.setNavigator(mockNavigator);
  mockUserAgent.setUserAgentString(uaString);

  userAgentTestUtil.reinitializeUserAgent();
  for (let ua in UserAgents) {
    const isExpected = googArray.contains(expectedAgents, UserAgents[ua]);
    assertEquals(isExpected, getUserAgentDetected(UserAgents[ua]));
  }
}

function assertIe(uaString, expectedVersion) {
  assertUserAgent([UserAgents.IE], uaString);
  assertEquals(
      `User agent ${uaString} should have had version ${expectedVersion}` +
          ' but had ' + userAgent.VERSION,
      expectedVersion, userAgent.VERSION);
}

function assertGecko(uaString, expectedVersion) {
  assertUserAgent([UserAgents.GECKO], uaString, 'Gecko');
  assertEquals(
      `User agent ${uaString} should have had version ${expectedVersion}` +
          ' but had ' + userAgent.VERSION,
      expectedVersion, userAgent.VERSION);
}
testSuite({
  setUp() {
    mockUserAgent = new MockUserAgent();
    mockUserAgent.install();
  },

  tearDown() {
    dispose(mockUserAgent);
    documentMode = undefined;
    propertyReplacer.reset();
  },

  /** Tests for the vendor prefix for Webkit. */
  testVendorPrefixWebkit() {
    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    assertEquals('-webkit', vendor.getVendorPrefix());
  },

  /** Tests for the vendor prefix for Mozilla/Gecko. */
  testVendorPrefixGecko() {
    assertUserAgent([UserAgents.GECKO], 'Gecko', 'Gecko');
    assertEquals('-moz', vendor.getVendorPrefix());
  },

  /** Tests for the vendor prefix for IE. */
  testVendorPrefixIE() {
    assertUserAgent([UserAgents.IE], 'MSIE');
    assertEquals('-ms', vendor.getVendorPrefix());
  },

  /** Tests for the vendor Js prefix for Webkit. */
  testVendorJsPrefixWebkit() {
    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    assertEquals('Webkit', vendor.getVendorJsPrefix());
  },

  /** Tests for the vendor Js prefix for Mozilla/Gecko. */
  testVendorJsPrefixGecko() {
    assertUserAgent([UserAgents.GECKO], 'Gecko', 'Gecko');
    assertEquals('Moz', vendor.getVendorJsPrefix());
  },

  /** Tests for the vendor Js prefix for IE. */
  testVendorJsPrefixIE() {
    assertUserAgent([UserAgents.IE], 'MSIE');
    assertEquals('ms', vendor.getVendorJsPrefix());
  },

  /** Tests for the vendor Js prefix if no UA detected. */
  testVendorJsPrefixNone() {
    assertUserAgent([], '');
    assertNull(vendor.getVendorJsPrefix());
  },

  /** Tests for the prefixed property name on Webkit. */
  testPrefixedPropertyNameWebkit() {
    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    assertEquals('webkitFoobar', vendor.getPrefixedPropertyName('foobar'));
  },

  /** Tests for the prefixed property name on Webkit in an object. */
  testPrefixedPropertyNameWebkitAndObject() {
    const mockDocument = {
      // setting a value of 0 on purpose, to ensure we only look for property
      // names, not their values.
      'webkitFoobar': 0,
    };
    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    assertEquals(
        'webkitFoobar', vendor.getPrefixedPropertyName('foobar', mockDocument));
  },

  /** Tests for the prefixed property name. */
  testPrefixedPropertyName() {
    assertUserAgent([], '');
    assertNull(vendor.getPrefixedPropertyName('foobar'));
  },

  /** Tests for the prefixed property name in an object. */
  testPrefixedPropertyNameAndObject() {
    const mockDocument = {'foobar': 0};
    assertUserAgent([], '');
    assertEquals(
        'foobar', vendor.getPrefixedPropertyName('foobar', mockDocument));
  },

  /** Tests for the prefixed property name when it doesn't exist. */
  testPrefixedPropertyNameAndObjectIsEmpty() {
    const mockDocument = {};
    assertUserAgent([], '');
    assertNull(vendor.getPrefixedPropertyName('foobar', mockDocument));
  },

  /** Test for prefixed event type. */
  testPrefixedEventType() {
    assertUserAgent([], '');
    assertEquals('foobar', vendor.getPrefixedEventType('foobar'));
  },

  /** Test for browser-specific prefixed event type. */
  testPrefixedEventTypeForBrowser() {
    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    assertEquals('webkitfoobar', vendor.getPrefixedEventType('foobar'));
  },
});
