/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.locale.timeZoneDetectionTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const timeZoneDetection = goog.require('goog.locale.timeZoneDetection');

/** Mock date class with simplified properties of Date class for testing. */
class MockDate {
  constructor() {
    /**
     * Time zone offset. For time zones with daylight saving, the different
     * offsets are represented as array of offsets.
     * @private {Array<number>}
     */
    this.timezoneOffset_ = [];
    /**
     * Counter storing the index of next offset value to be returned from the
     * array of offset values.
     * @private {number}
     */
    this.offsetArrayCounter_ = 0;
  }

  /**
   * Does nothing because setting the time to calculate offset is not needed
   * in the mock class.
   * @param {number} ms Ignored.
   */
  setTime(ms) {
    // Do nothing.
  }

  /**
   * Sets the time zone offset.
   * @param {Array<number>} offset Time zone offset.
   */
  setTimezoneOffset(offset) {
    this.timezoneOffset_ = offset;
  }

  /**
   * Returns consecutive offsets from array of time zone offsets on each call.
   * @return {number} Time zone offset.
   */
  getTimezoneOffset() {
    return this.timezoneOffset_.length > 1 ?
        this.timezoneOffset_[this.offsetArrayCounter_++] :
        this.timezoneOffset_[0];
  }
}

testSuite({
  testResult() {
    const result = timeZoneDetection.detectTimeZone();
    assertNotEquals('', result);
  },

  testGetFingerprint() {
    timeZoneDetection.useNativeTimezoneDetectionForTesting(false);

    let mockDate = new MockDate();
    mockDate.setTimezoneOffset([-480]);
    /** @suppress {checkTypes} suppression added to enable type checking */
    let fingerprint = timeZoneDetection.getFingerprint(mockDate);
    assertEquals(32, fingerprint);

    mockDate = new MockDate();
    mockDate.setTimezoneOffset(
        [480, 420, 420, 480, 480, 420, 420, 420, 420, 420, 420, 420, 420]);
    /** @suppress {checkTypes} suppression added to enable type checking */
    fingerprint = timeZoneDetection.getFingerprint(mockDate);
    assertEquals(1294772902, fingerprint);
  },

  testDetectTimeZone() {
    timeZoneDetection.useNativeTimezoneDetectionForTesting(false);

    let mockDate = new MockDate();
    mockDate.setTimezoneOffset([-480]);
    /** @suppress {checkTypes} suppression added to enable type checking */
    let timeZoneId = timeZoneDetection.detectTimeZone(undefined, mockDate);
    assertEquals('Asia/Hong_Kong', timeZoneId);

    mockDate = new MockDate();
    mockDate.setTimezoneOffset(
        [480, 420, 420, 480, 480, 420, 420, 420, 420, 420, 420, 420, 420]);
    /** @suppress {checkTypes} suppression added to enable type checking */
    timeZoneId = timeZoneDetection.detectTimeZone('US', mockDate);
    assertEquals('America/Los_Angeles', timeZoneId);

    mockDate = new MockDate();
    mockDate.setTimezoneOffset(
        [480, 420, 420, 480, 480, 420, 420, 420, 420, 420, 420, 420, 420]);
    /** @suppress {checkTypes} suppression added to enable type checking */
    timeZoneId = timeZoneDetection.detectTimeZone('CA', mockDate);
    assertEquals('America/Dawson', timeZoneId);
  },

  testGetTimeZoneList() {
    timeZoneDetection.useNativeTimezoneDetectionForTesting(false);

    let mockDate = new MockDate();
    mockDate.setTimezoneOffset(
        [480, 420, 420, 480, 480, 420, 420, 420, 420, 420, 420, 420, 420]);
    /** @suppress {checkTypes} suppression added to enable type checking */
    let timeZoneList = timeZoneDetection.getTimeZoneList(undefined, mockDate);
    assertEquals('America/Los_Angeles', timeZoneList[0]);
    assertEquals('America/Whitehorse', timeZoneList[4]);
    assertEquals(5, timeZoneList.length);

    mockDate = new MockDate();
    mockDate.setTimezoneOffset([-480]);
    /** @suppress {checkTypes} suppression added to enable type checking */
    timeZoneList = timeZoneDetection.getTimeZoneList(undefined, mockDate);
    assertEquals('Asia/Hong_Kong', timeZoneList[0]);
    assertEquals('Asia/Chongqing', timeZoneList[7]);
    assertEquals(16, timeZoneList.length);

    /** @suppress {checkTypes} suppression added to enable type checking */
    timeZoneList = timeZoneDetection.getTimeZoneList('AU', mockDate);
    assertEquals(1, timeZoneList.length);
    assertEquals('Australia/Perth', timeZoneList[0]);
  },
});
