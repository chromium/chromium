/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for goog.html.trustedtypes package. */

goog.module('goog.html.trustedtypesTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const trustedtypes = goog.require('goog.html.trustedtypes');

const stubs = new PropertyReplacer();

testSuite({

  setUp() {
    /** @suppress {visibility} suppression added to enable type checking */
    trustedtypes.cachedPolicy_ = undefined;  // reset the cache.
  },

  tearDown() {
    stubs.reset();
  },

  testGetPolicyPrivateDoNotAccessOrElse_noPolicyName() {
    stubs.set(goog, 'TRUSTED_TYPES_POLICY_NAME', '');
    const recorder = recordFunction(goog.createTrustedTypesPolicy);
    stubs.set(goog, 'createTrustedTypesPolicy', recorder);
    const policy = trustedtypes.getPolicyPrivateDoNotAccessOrElse();
    recorder.assertCallCount(0);
    assertNull(policy);
  },

  testGetPolicyPrivateDoNotAccessOrElse_withPolicyName() {
    stubs.set(goog, 'TRUSTED_TYPES_POLICY_NAME', 'foo');
    const recorder = recordFunction(goog.createTrustedTypesPolicy);
    stubs.set(goog, 'createTrustedTypesPolicy', recorder);
    const policy = trustedtypes.getPolicyPrivateDoNotAccessOrElse();
    recorder.assertCallCount(1);
    assertEquals('foo#html', recorder.getLastCall().getArguments()[0]);
    assertEquals(recorder.getLastCall().getReturnValue(), policy);
  },

  testGetPolicyPrivateDoNotAccessOrElse_caching() {
    stubs.set(goog, 'TRUSTED_TYPES_POLICY_NAME', 'foo');
    const recorder = recordFunction(goog.createTrustedTypesPolicy);
    stubs.set(goog, 'createTrustedTypesPolicy', recorder);
    trustedtypes.getPolicyPrivateDoNotAccessOrElse();
    trustedtypes.getPolicyPrivateDoNotAccessOrElse();
    recorder.assertCallCount(1);
  },

});
