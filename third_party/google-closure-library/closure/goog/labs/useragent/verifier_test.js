/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Description of this file.
 */
goog.module('goog.labs.useragent.verifierTest');
goog.setTestOnly();

const browser = goog.require('goog.labs.userAgent.browser');
const testSuite = goog.require('goog.testing.testSuite');
const verifier = goog.require('goog.labs.useragent.verifier');


testSuite({
  testIEVersion: function() {
    const isUserAgentIE = browser.isIE();
    const versionByBehavior = verifier.detectIeVersionByBehavior();
    const versionByNavigator = verifier.detectIeVersionByNavigator();
    const correctedVersion = verifier.getCorrectedIEVersionByNavigator();

    if (isUserAgentIE) {
      const version = Number(browser.getVersion());
      assertEquals('behavior detection incorrect', version, versionByBehavior);
      if (version != 11) {
        assertEquals(
            'navigator version incorrect', version, versionByNavigator);
      } else {
        // IE 11 doesn't want to be detected as IE
        assertEquals('navigator version incorrect', 0, versionByNavigator);
      }
      assertEquals('corrected version incorrect', version, correctedVersion);
    } else {
      assertEquals(0, versionByBehavior);
    }
  },
});
