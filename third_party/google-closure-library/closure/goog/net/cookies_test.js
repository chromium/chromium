/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.cookiesTest');
goog.setTestOnly();

const Cookies = goog.require('goog.net.Cookies');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const cookies = goog.require('goog.net.cookies');
const googArray = goog.require('goog.array');
const testSuite = goog.require('goog.testing.testSuite');

let baseCount = 0;
const stubs = new PropertyReplacer();

function checkForCookies() {
  if (!cookies.isEnabled()) {
    let message = 'Cookies must be enabled to run this test.';
    if (location.protocol == 'file:') {
      message += '\nNote that cookies for local files are disabled in some ' +
          'browsers.\nThey can be enabled in Chrome with the ' +
          '--enable-file-cookies flag.';
    }

    fail(message);
  }
}

// TODO(chrisn): Testing max age > 0 requires a mock clock.

function mockSetCookie(var_args) {
  /** @suppress {visibility} suppression added to enable type checking */
  const setCookie = cookies.setCookie_;
  try {
    let result;
    /** @suppress {visibility} suppression added to enable type checking */
    cookies.setCookie_ = function(arg) {
      result = arg;
    };
    cookies.set.apply(cookies, arguments);
    return result;
  } finally {
    /** @suppress {visibility} suppression added to enable type checking */
    cookies.setCookie_ = setCookie;
  }
}

function assertValidName(name) {
  assertTrue(`${name} should be valid`, cookies.isValidName(name));
}

function assertInvalidName(name) {
  assertFalse(`${name} should be invalid`, cookies.isValidName(name));
  assertThrows(() => {
    cookies.set(name, 'value');
  });
}

function assertValidValue(val) {
  assertTrue(`${val} should be valid`, cookies.isValidValue(val));
}

function assertInvalidValue(val) {
  assertFalse(`${val} should be invalid`, cookies.isValidValue(val));
  assertThrows(() => {
    cookies.set('name', val);
  });
}

testSuite({
  setUp() {
    checkForCookies();

    // Make sure there are no cookies set by previous, bad tests.
    cookies.clear();
    baseCount = cookies.getCount();
  },

  tearDown() {
    // Clear up after ourselves.
    cookies.clear();
    stubs.reset();
  },

  testIsEnabledWithNavigatorCookieEnabledFalse() {
    // PropertyReplacer does not support overwriting read-only properties, which
    // includes all of window.navigator:
    // https://developer.mozilla.org/en-US/docs/Web/API/Navigator

    // Save off original value and set navigator.cookieEnabled to false
    const originalValue = goog.global.navigator;
    Object.defineProperty(
        goog.global, 'navigator', {value: {cookieEnabled: false}});

    assertFalse(cookies.isEnabled());

    // Restore window.navigator
    Object.defineProperty(goog.global, 'navigator', {value: originalValue});
  },

  testIsEnabledWithExistingCookies() {
    stubs.set(cookies, 'isEmpty', () => false);

    assertTrue(cookies.isEnabled());
  },

  testIsEnabledWhenUnableToSetCookie() {
    stubs.set(cookies, 'isEmpty', () => true);
    stubs.set(cookies, 'set', () => {});
    stubs.set(cookies, 'get', () => '');

    assertFalse(cookies.isEnabled());
  },

  testIsEnabledWhenAbleToSetCookie() {
    const initialCookieCount = cookies.getCount();
    stubs.set(cookies, 'isEmpty', () => true);

    assertTrue(cookies.isEnabled());
    // Ensure the test cookie is cleaned up.
    assertEquals(initialCookieCount, cookies.getCount());
  },

  testCount() {
    // setUp empties the cookies

    cookies.set('testa', 'A');
    assertEquals(baseCount + 1, cookies.getCount());
    cookies.set('testb', 'B');
    cookies.set('testc', 'C');
    assertEquals(baseCount + 3, cookies.getCount());
    cookies.remove('testa');
    cookies.remove('testb');
    assertEquals(baseCount + 1, cookies.getCount());
    cookies.remove('testc');
    assertEquals(baseCount + 0, cookies.getCount());
  },

  testSet() {
    cookies.set('testa', 'testb');
    assertEquals('testb', cookies.get('testa'));
    cookies.remove('testa');
    assertEquals(undefined, cookies.get('testa'));
    // check for invalid characters in name and value
  },

  testGetKeys() {
    cookies.set('testa', 'A');
    cookies.set('testb', 'B');
    cookies.set('testc', 'C');
    const keys = cookies.getKeys();
    assertTrue(googArray.contains(keys, 'testa'));
    assertTrue(googArray.contains(keys, 'testb'));
    assertTrue(googArray.contains(keys, 'testc'));
  },

  testGetValues() {
    cookies.set('testa', 'A');
    cookies.set('testb', 'B');
    cookies.set('testc', 'C');
    const values = cookies.getValues();
    assertTrue(googArray.contains(values, 'A'));
    assertTrue(googArray.contains(values, 'B'));
    assertTrue(googArray.contains(values, 'C'));
  },

  testContainsKey() {
    assertFalse(cookies.containsKey('testa'));
    cookies.set('testa', 'A');
    assertTrue(cookies.containsKey('testa'));
    cookies.set('testb', 'B');
    assertTrue(cookies.containsKey('testb'));
    cookies.remove('testb');
    assertFalse(cookies.containsKey('testb'));
    cookies.remove('testa');
    assertFalse(cookies.containsKey('testa'));
  },

  testContainsValue() {
    assertFalse(cookies.containsValue('A'));
    cookies.set('testa', 'A');
    assertTrue(cookies.containsValue('A'));
    cookies.set('testb', 'B');
    assertTrue(cookies.containsValue('B'));
    cookies.remove('testb');
    assertFalse(cookies.containsValue('B'));
    cookies.remove('testa');
    assertFalse(cookies.containsValue('A'));
  },

  testIsEmpty() {
    // we cannot guarantee that we have no cookies so testing for the true
    // case cannot be done without a mock document.cookie
    cookies.set('testa', 'A');
    assertFalse(cookies.isEmpty());
    cookies.set('testb', 'B');
    assertFalse(cookies.isEmpty());
    cookies.remove('testb');
    assertFalse(cookies.isEmpty());
    cookies.remove('testa');
  },

  testRemove() {
    assertFalse(
        '1. Cookie should not contain "testa"', cookies.containsKey('testa'));
    cookies.set('testa', 'A', {path: '/'});
    assertTrue(
        '2. Cookie should contain "testa"', cookies.containsKey('testa'));
    cookies.remove('testa', '/');
    assertFalse(
        '3. Cookie should not contain "testa"', cookies.containsKey('testa'));

    cookies.set('testa', 'A');
    assertTrue(
        '4. Cookie should contain "testa"', cookies.containsKey('testa'));
    cookies.remove('testa');
    assertFalse(
        '5. Cookie should not contain "testa"', cookies.containsKey('testa'));
  },

  testStrangeValue() {
    // This ensures that the pattern key2=value in the value does not match
    // the key2 cookie.
    const value = 'testb=bbb';
    const value2 = 'ccc';

    cookies.set('testa', value);
    cookies.set('testb', value2);

    assertEquals(value, cookies.get('testa'));
    assertEquals(value2, cookies.get('testb'));
  },

  testSetCookiePath() {
    assertEquals(
        'foo=bar;path=/xyz', mockSetCookie('foo', 'bar', {path: '/xyz'}));
  },

  testSetCookieDomain() {
    assertEquals(
        'foo=bar;domain=google.com',
        mockSetCookie('foo', 'bar', {domain: 'google.com'}));
  },

  testSetCookieSecure() {
    assertEquals('foo=bar;secure', mockSetCookie('foo', 'bar', {secure: true}));
  },

  testSetCookieMaxAgeZero() {
    const result = mockSetCookie('foo', 'bar', {maxAge: 0});
    const pattern =
        new RegExp('foo=bar;expires=' + new Date(1970, 1, 1).toUTCString());
    if (!result.match(pattern)) {
      fail(`expected match against ${pattern} got ${result}`);
    }
  },

  testSetCookieSameSite() {
    assertEquals(
        'foo=bar;samesite=lax',
        mockSetCookie('foo', 'bar', {sameSite: Cookies.SameSite.LAX}));
    assertEquals(
        'foo=bar;samesite=strict',
        mockSetCookie('foo', 'bar', {sameSite: Cookies.SameSite.STRICT}));
  },

  testGetEmptyCookie() {
    const value = '';

    cookies.set('test', value);

    assertEquals(value, cookies.get('test'));
  },

  testGetEmptyCookieIE() {
    stubs.set(cookies, 'getCookie_', () => 'test1; test2; test3');

    assertEquals('', cookies.get('test1'));
    assertEquals('', cookies.get('test2'));
    assertEquals('', cookies.get('test3'));
  },

  testGetReallyEmptyCookieIE() {
    stubs.set(cookies, 'getCookie_', () => 'test1; ; test3');

    assertEquals('', cookies.get('test1'));
    assertEquals('', cookies.get(''));
    assertEquals('', cookies.get('test3'));
    assertEquals(3, cookies.getCount());
  },

  testValidName() {
    assertValidName('foo');
    assertInvalidName('foo bar');
    assertInvalidName('foo=bar');
    assertInvalidName('foo;bar');
    assertInvalidName('foo\nbar');
  },

  testValidValue() {
    assertValidValue('foo');
    assertValidValue('foo bar');
    assertValidValue('foo=bar');
    assertInvalidValue('foo;bar');
    assertInvalidValue('foo\nbar');
  },
});
