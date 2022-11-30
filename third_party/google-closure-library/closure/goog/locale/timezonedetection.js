/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Functions for detecting user's time zone.
 * This work is based on Charlie Luo and Hong Yan's time zone detection work
 * for CBG.
 */
goog.provide('goog.locale.timeZoneDetection');

goog.require('goog.asserts');
goog.require('goog.locale.TimeZoneFingerprint');


/**
 * Whether to use the native API for time zone detection (if the runtime
 * supports it). You might turn this off if a downstream system can't handle a
 * user's timezone as reported by the browser.
 * @define {boolean}
 */
goog.locale.timeZoneDetection.USE_NATIVE_TIMEZONE_DETECTION = goog.define(
    'goog.locale.timeZoneDetection.USE_NATIVE_TIMEZONE_DETECTION',
    goog.FEATURESET_YEAR >= 2021);


/**
 * Whether to include the fingerprint algorithm so it can be used as a fallback.
 * Without this, the code may be stripped for modern browsers that can be
 * assumed to support the native API.
 * @define {boolean}
 */
goog.locale.timeZoneDetection.INCLUDE_FINGERPRINT_DETECTION = goog.define(
    'goog.locale.timeZoneDetection.INCLUDE_FINGERPRINT_DETECTION',
    !goog.locale.timeZoneDetection.USE_NATIVE_TIMEZONE_DETECTION);


/** @private {boolean} */
goog.locale.timeZoneDetection.useNativeTimezoneDetection_ =
    goog.locale.timeZoneDetection.USE_NATIVE_TIMEZONE_DETECTION;


/**
 * Allows disabling the use of native APIs so that the fingerprinting algorithm
 * can be tested.
 * @param {boolean} useNative
 */
goog.locale.timeZoneDetection.useNativeTimezoneDetectionForTesting = function(
    useNative) {
  goog.locale.timeZoneDetection.useNativeTimezoneDetection_ = useNative;
};


/**
 * Array of time instances for checking the time zone offset.
 * @type {Array<number>}
 * @private
 */
goog.locale.timeZoneDetection.TZ_POKE_POINTS_ = [
  1109635200, 1128902400, 1130657000, 1143333000, 1143806400, 1145000000,
  1146380000, 1152489600, 1159800000, 1159500000, 1162095000, 1162075000,
  1162105500
];


/**
 * Calculates time zone fingerprint by poking time zone offsets for 13
 * preselected time points.
 * See {@link goog.locale.timeZoneDetection.TZ_POKE_POINTS_}
 * @param {Date} date Date for calculating the fingerprint.
 * @return {number} Fingerprint of user's time zone setting.
 */
goog.locale.timeZoneDetection.getFingerprint = function(date) {
  'use strict';
  var hash = 0;
  var stdOffset;
  var isComplex = false;
  for (var i = 0; i < goog.locale.timeZoneDetection.TZ_POKE_POINTS_.length;
       i++) {
    date.setTime(goog.locale.timeZoneDetection.TZ_POKE_POINTS_[i] * 1000);
    var offset = date.getTimezoneOffset() / 30 + 48;
    if (i == 0) {
      stdOffset = offset;
    } else if (stdOffset != offset) {
      isComplex = true;
    }
    hash = (hash << 2) ^ offset;
  }
  return isComplex ? hash : /** @type {number} */ (stdOffset);
};


/**
 * @return {string?} The local timezone, if the browser supports it and the
 * functionality is enabled.
 * @private
 */
goog.locale.timeZoneDetection.getNatively_ = function() {
  if (!goog.locale.timeZoneDetection.useNativeTimezoneDetection_) {
    return null;
  }
  if (typeof Intl == 'undefined' || typeof Intl.DateTimeFormat == 'undefined') {
    return null;
  }
  const dateTimeFormat = new Intl.DateTimeFormat();
  if (typeof dateTimeFormat.resolvedOptions == 'undefined') {
    return null;
  }
  return dateTimeFormat.resolvedOptions().timeZone || null;
};


/**
 * Detects browser's time zone setting. If user's country is known, a better
 * time zone choice could be guessed. Note that in many browsers this is
 * available natively as `new Intl.DateTimeFormat().resolvedOptions().timeZone`.
 * @param {string=} opt_country Two-letter ISO 3166 country code.
 * @param {Date=} opt_date Date for calculating the fingerprint. Defaults to the
 *     current date.
 * @return {string} Time zone ID of best guess.
 */
goog.locale.timeZoneDetection.detectTimeZone = function(opt_country, opt_date) {
  'use strict';
  goog.asserts.assert(
      goog.locale.timeZoneDetection.USE_NATIVE_TIMEZONE_DETECTION ||
          goog.locale.timeZoneDetection.INCLUDE_FINGERPRINT_DETECTION,
      'At least one of USE_NATIVE_TIMEZONE_DETECTION or ' +
          'INCLUDE_FINGERPRINT_DETECTION must be true');
  const nativeResult = goog.locale.timeZoneDetection.getNatively_();
  if (nativeResult != null) {
    return nativeResult;
  }
  if (!goog.locale.timeZoneDetection.useNativeTimezoneDetection_ ||
      goog.locale.timeZoneDetection.INCLUDE_FINGERPRINT_DETECTION) {
    var date = opt_date || new Date();
    var fingerprint = goog.locale.timeZoneDetection.getFingerprint(date);
    var timeZoneList = goog.locale.TimeZoneFingerprint[fingerprint];
    // Timezones in goog.locale.TimeZoneDetection.TimeZoneMap are in the format
    // US-America/Los_Angeles. Country code needs to be stripped before a
    // timezone is returned.
    if (timeZoneList) {
      if (opt_country) {
        for (var i = 0; i < timeZoneList.length; ++i) {
          if (timeZoneList[i].indexOf(opt_country) == 0) {
            return timeZoneList[i].substring(3);
          }
        }
      }
      return timeZoneList[0].substring(3);
    }
  }
  return '';
};


/**
 * Returns an array of time zones that are consistent with user's platform
 * setting. If user's country is given, only the time zone for that country is
 * returned.
 * @param {string=} opt_country 2 letter ISO 3166 country code. Helps in making
 *     a better guess for user's time zone.
 * @param {Date=} opt_date Date for retrieving timezone list. Defaults to the
 *     current date.
 * @return {!Array<string>} Array of time zone IDs.
 */
goog.locale.timeZoneDetection.getTimeZoneList = function(
    opt_country, opt_date) {
  'use strict';
  var date = opt_date || new Date();
  var fingerprint = goog.locale.timeZoneDetection.getFingerprint(date);
  var timeZoneList = goog.locale.TimeZoneFingerprint[fingerprint];
  if (!timeZoneList) {
    return [];
  }
  var chosenList = [];
  for (var i = 0; i < timeZoneList.length; i++) {
    if (!opt_country || timeZoneList[i].indexOf(opt_country) == 0) {
      chosenList.push(timeZoneList[i].substring(3));
    }
  }
  return chosenList;
};
