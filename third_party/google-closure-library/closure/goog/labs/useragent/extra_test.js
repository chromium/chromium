/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Description of this file.
 */
goog.module('goog.labs.userAgent.extraTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const browser = goog.require('goog.labs.userAgent.browser');
const extra = goog.require('goog.labs.userAgent.extra');
const testAgents = goog.require('goog.labs.userAgent.testAgents');
const testSuite = goog.require('goog.testing.testSuite');
const util = goog.require('goog.labs.userAgent.util');

const stubs = new PropertyReplacer();

/**
 * Replaces the navigator object on globalThis.
 * @param {?Object|undefined} navigatorObj The navigator object to set
 */
function setGlobalNavigator(navigatorObj) {
  const mockGlobal = {
    'navigator': navigatorObj,
  };
  stubs.set(goog, 'global', mockGlobal);
}

testSuite({
  tearDown: function() {
    stubs.reset();
  },
  testSafariDesktopOnMobile: function() {
    util.setUserAgent(testAgents.SAFARI_13);
    setGlobalNavigator({'maxTouchPoints': 5});
    assertTrue(browser.isSafari());
    assertFalse(browser.isChrome());
    assertTrue(extra.isSafariDesktopOnMobile());

    util.setUserAgent(testAgents.CHROME_IPAD_DESKTOP);
    setGlobalNavigator({'maxTouchPoints': 5});
    assertTrue(browser.isChrome());
    assertFalse(browser.isSafari());
    assertTrue(extra.isSafariDesktopOnMobile());

    setGlobalNavigator({'maxTouchPoints': 0});
    assertFalse(extra.isSafariDesktopOnMobile());

    setGlobalNavigator({});
    assertFalse(extra.isSafariDesktopOnMobile());

    util.setUserAgent(testAgents.IPAD_6);
    assertFalse(extra.isSafariDesktopOnMobile());
  },
});
