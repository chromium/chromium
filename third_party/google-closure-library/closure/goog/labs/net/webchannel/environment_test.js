/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for goog.labs.net.webChannel.environment.
 */

goog.module('goog.labs.net.webChannel.EnvironmentTest');
goog.setTestOnly('goog.labs.net.webChannel.EnvironmentTest');

const environment = goog.require('goog.labs.net.webChannel.environment');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

testSuite({
  testPollingRequiredForEdge: /**
                                 @suppress {strictPrimitiveOperators}
                                 suppression added to enable type checking
                               */
      function() {
        if (!userAgent.EDGE) return;

        assertTrue(environment.isPollingRequired());

        // 100ms as the lower-bound, enforced in
        // tests
        assertTrue(environment.getPollingInterval() > 100);
      },
});
