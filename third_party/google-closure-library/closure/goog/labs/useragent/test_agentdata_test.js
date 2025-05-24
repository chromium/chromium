/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for userAgentBrowser. */

goog.module('goog.labs.userAgent.testAgentDataTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const {INCOMPLETE_USERAGENT_DATA, withHighEntropyData} = goog.require('goog.labs.userAgent.testAgentData');

testSuite({
  async testGetHighEntropyValuesRejectsByDefault() {
    await assertRejects(
        INCOMPLETE_USERAGENT_DATA.getHighEntropyValues(['platformVersion']));
  },

  async testGetHighEntropyValuesWithMatchingKey() {
    const hasPlatformVersion = withHighEntropyData(INCOMPLETE_USERAGENT_DATA, {
      platformVersion: '10.0.0',
    });
    assertObjectEquals(
        {platformVersion: '10.0.0'},
        await hasPlatformVersion.getHighEntropyValues(['platformVersion']));
  },

  async testGetHighEntropyValuesWithNonStringValue() {
    const hasPlatformVersion =
        withHighEntropyData(INCOMPLETE_USERAGENT_DATA, {versionList: []});
    assertObjectEquals(
        {versionList: []},
        await hasPlatformVersion.getHighEntropyValues(['versionList']));
  },
});
