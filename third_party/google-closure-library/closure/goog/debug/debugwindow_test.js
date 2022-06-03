/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.debug.DebugWindowTest');
goog.setTestOnly();

const DebugWindow = goog.require('goog.debug.DebugWindow');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  /** @suppress {visibility} suppression added to enable type checking */
  testGetCookieKey() {
    assertEquals(
        'keyNasty_Debug__Identifier_',
        DebugWindow.getCookieKey_('Nasty_Debug =Identifier;', 'key'));
  },
});
