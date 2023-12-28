// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for the holding space feature.
 */

import {recordValue} from '../../common/js/metrics.js';
import {storage} from '../../common/js/storage.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';

/**
 * Key in localStorage to store the time (in milliseconds) of the first pin to
 * holding space.
 */
const TIME_OF_FIRST_PIN_KEY = 'holdingSpaceTimeOfFirstPin';

/**
 * Key in localStorage to store the time (in milliseconds) of the first showing
 * of the holding space welcome banner.
 */
const TIME_OF_FIRST_WELCOME_BANNER_SHOW_KEY =
    'holdingSpaceTimeOfFirstWelcomeBannerShow';


/** Gets the volume types for which the holding space feature is allowed. */
export function getAllowedVolumeTypes(): VolumeType[] {
  return [
    VolumeType.ANDROID_FILES,
    VolumeType.CROSTINI,
    VolumeType.GUEST_OS,
    VolumeType.DRIVE,
    VolumeType.DOWNLOADS,
  ];
}

/**
 * Returns a promise which resolves to the time (in milliseconds) of the first
 * pin to holding space. If no pin has occurred, resolves to `undefined`.
 */
function getTimeOfFirstPin(): Promise<number|undefined> {
  const key = TIME_OF_FIRST_PIN_KEY;
  return new Promise(
      resolve => storage.local.get(
          key, (values: Record<string, any>) => resolve(values[key])));
}

/**
 * Returns a promise which resolves to the time (in milliseconds) of the first
 * showing of the holding space welcome banner. If no showing has occurred,
 * resolves to `undefined`.
 */
function getTimeOfFirstWelcomeBannerShow(): Promise<number|undefined> {
  const key = TIME_OF_FIRST_WELCOME_BANNER_SHOW_KEY;
  return new Promise(
      resolve => storage.local.get(
          key, (values: Record<string, any>) => resolve(values[key])));
}

/**
 * If not previously stored, stores now (in milliseconds) as the time of the
 * first pin to holding space.
 */
export async function maybeStoreTimeOfFirstPin() {
  const now = Date.now();

  // Time of first pin should only be stored once.
  if (await getTimeOfFirstPin()) {
    return;
  }

  // Store time of first pin.
  storage.local.set({[TIME_OF_FIRST_PIN_KEY]: now});

  // Record a metric of the interval from the first time the holding space
  // welcome banner was shown to the time of the first pin to holding space.
  // If the welcome banner was not shown prior to the first pin, record zero.
  const timeOfFirstWelcomeBannerShow =
      await getTimeOfFirstWelcomeBannerShow() || now;
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
export async function maybeStoreTimeOfFirstWelcomeBannerShow() {
  const now = Date.now();

  // Time of first show should only be stored once.
  if (await getTimeOfFirstWelcomeBannerShow()) {
    return;
  }

  // Store time of first show.
  storage.local.set({[TIME_OF_FIRST_WELCOME_BANNER_SHOW_KEY]: now});
}
