// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 * @suppress {uselessCode} Temporary suppress because of the line exporting.
 */

// #import {metricsBase} from '../../file_manager/common/js/metrics_base.m.js';

/**
 * @extends {metricsBase}
 */
// eslint-disable-next-line no-var
var metrics = metricsBase;

/**
 * Values for "VideoPlayer.PlayType" metrics.
 * @enum {number}
 */
metrics.PLAY_TYPE = {
  LOCAL: 0,
  CAST: 1,
  MAX_VALUE: 2,
};

/**
 * Utility function to check if the given value is in the given values.
 * @param {!Object} values
 * @param {*} value
 * @return {boolean} True if one or more elements of the given values hash have
 *     the given value as value. False otherwise.
 */
metrics.hasValue_ = function(values, value) {
  return Object.keys(values).some(function(key) {
    return values[key] === value;
  });
};

/**
 * Record "VideoPlayer.NumberOfCastDevices" metrics.
 * @param {number} number Value to be recorded.
 */
metrics.recordNumberOfCastDevices = function(number) {
  metrics.recordSmallCount('NumberOfCastDevices', number);
};

/**
 * Record "VideoPlayer.NumberOfOpenedFile" metrics.
 * @param {number} number Value to be recorded.
 */
metrics.recordNumberOfOpenedFiles = function(number) {
  metrics.recordSmallCount('NumberOfOpenedFiles', number);
};

/**
 * Record "VideoPlayer.CastedVideoLength" metrics.
 * @param {number} seconds Value to be recorded.
 */
metrics.recordCastedVideoLength = function(seconds) {
  metrics.recordMediumCount('CastedVideoLength', seconds);
};

/**
 * Record "VideoPlayer.CastVideoError" metrics.
 */
metrics.recordCastVideoErrorAction = function() {
  metrics.recordUserAction('CastVideoError');
};

/**
 * Record "VideoPlayer.OpenVideoPlayer" action.
 */
metrics.recordOpenVideoPlayerAction = function() {
  metrics.recordUserAction('OpenVideoPlayer');
};

/**
 * Record "VideoPlayer.PlayType" metrics.
 * @param {metrics.PLAY_TYPE} type Value to be recorded.
 */
metrics.recordPlayType = function(type) {
  if (!metrics.hasValue_(metrics.PLAY_TYPE, type)) {
    console.error('The given value "' + type + '" is invalid.');
    return;
  }

  metrics.recordEnum('PlayType',
                     type,
                     metrics.PLAY_TYPE.MAX_VALUE);
};

/**
 * Convert a short metric name to the full format.
 *
 * @param {string} name Short metric name.
 * @return {string} Full metric name.
 * @override
 * @private
 */
metrics.convertName_ = function(name) {
  return 'VideoPlayer.' + name;
};

// eslint-disable-next-line semi,no-extra-semi
/* #export */ {metrics};
