// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for the holding space feature.
 */

import {recordValue} from '../../common/js/metrics.js';
import {storage} from '../../common/js/storage.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

export class HoldingSpaceUtil {
  /**
   * Returns the key in localStorage to store the time (in milliseconds) of the
   * first pin to holding space.
   * @return {string}
   * @private
   */
  static get TIME_OF_FIRST_PIN_KEY_() {
    return 'holdingSpaceTimeOfFirstPin';
  }

  /**
   * Returns the key in localStorage to store the time (in milliseconds) of the
   * first showing of the holding space welcome banner.
   * @constructor
   * @private
   */
  static get TIME_OF_FIRST_WELCOME_BANNER_SHOW_KEY_() {
    return 'holdingSpaceTimeOfFirstWelcomeBannerShow';
  }

  /**
   * Returns the volume types for which the holding space feature is allowed.
   * @return {!Array<?VolumeManagerCommon.VolumeType>}
   */
  static getAllowedVolumeTypes() {
    return [
      VolumeManagerCommon.VolumeType.ANDROID_FILES,
      VolumeManagerCommon.VolumeType.CROSTINI,
      VolumeManagerCommon.VolumeType.GUEST_OS,
      VolumeManagerCommon.VolumeType.DRIVE,
      VolumeManagerCommon.VolumeType.DOWNLOADS,
    ];
  }

  /**
   * Returns a promise which resolves to the time (in milliseconds) of the first
   * pin to holding space. If no pin has occurred, resolves to `undefined`.
   * @return {Promise<?number>}
   * @private
   */
  static getTimeOfFirstPin_() {
    return new Promise(resolve => {
      const key = HoldingSpaceUtil.TIME_OF_FIRST_PIN_KEY_;
      storage.local.get(key, values => {
        resolve(values[key]);
      });
    });
  }

  /**
   * Returns a promise which resolves to the time (in milliseconds) of the first
   * showing of the holding space welcome banner. If no showing has occurred,
   * resolves to `undefined`.
   * @return {Promise<?number>}
   * @private
   */
  static getTimeOfFirstWelcomeBannerShow_() {
    return new Promise(resolve => {
      const key = HoldingSpaceUtil.TIME_OF_FIRST_WELCOME_BANNER_SHOW_KEY_;
      storage.local.get(key, values => {
        resolve(values[key]);
      });
    });
  }

  /**
   * If not previously stored, stores now (in milliseconds) as the time of the
   * first pin to holding space.
   */
  static async maybeStoreTimeOfFirstPin() {
    const now = Date.now();

    // Time of first pin should only be stored once.
    if (await HoldingSpaceUtil.getTimeOfFirstPin_()) {
      return;
    }

    // Store time of first pin.
    const values = {};
    values[HoldingSpaceUtil.TIME_OF_FIRST_PIN_KEY_] = now;
    storage.local.set(values);

    // Record a metric of the interval from the first time the holding space
    // welcome banner was shown to the time of the first pin to holding space.
    // If the welcome banner was not shown prior to the first pin, record zero.
    const timeOfFirstWelcomeBannerShow =
        await HoldingSpaceUtil.getTimeOfFirstWelcomeBannerShow_() || now;
    // We trim the max value to be 2^31 - 1, which is the maximum integer value
    // that histograms can record.
    const timeFromFirstWelcomeBannerShowToFirstPin =
        Math.min(2 ** 31 - 1, now - timeOfFirstWelcomeBannerShow);

    // The histogram will use min values of 1 second and max of 1 day. Note
    // that it's permissible to record values smaller/larger than the min/max
    // and they will fall into the histogram's underflow/overflow bucket
    // respectively.
    const oneSecondInMillis = 1000;
    const oneDayInMillis = 24 * 60 * 60 * 1000;
    recordValue(
        /*name=*/ 'HoldingSpace.TimeFromFirstWelcomeBannerShowToFirstPin',
        chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
        /*min=*/ oneSecondInMillis,
        /*max=*/ oneDayInMillis,
        /*buckets=*/ 50,
        /*value=*/ timeFromFirstWelcomeBannerShowToFirstPin);
  }

  /**
   * If not previously stored, stores now (in milliseconds) as the time of the
   * first showing of the holding space welcome banner.
   */
  static async maybeStoreTimeOfFirstWelcomeBannerShow() {
    const now = Date.now();

    // Time of first show should only be stored once.
    if (await HoldingSpaceUtil.getTimeOfFirstWelcomeBannerShow_()) {
      return;
    }

    // Store time of first show.
    const values = {};
    values[HoldingSpaceUtil.TIME_OF_FIRST_WELCOME_BANNER_SHOW_KEY_] = now;
    storage.local.set(values);
  }
}
