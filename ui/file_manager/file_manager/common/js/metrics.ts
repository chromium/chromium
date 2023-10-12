// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

/**
 * A map from interval name to interval start timestamp.
 */
const intervals: Record<string, number> = {};

/**
 * Start the named time interval.
 * Should be followed by a call to recordInterval with the same name.
 *
 * @param name Unique interval name.
 */
export function startInterval(name: string) {
  intervals[name] = Date.now();
}

/** Convert a short metric name to the full format. */
function convertName(name: string) {
  return 'FileBrowser.' + name;
}

/** Wrapper method for calling chrome.fileManagerPrivate safely. */
function callAPI(name: keyof typeof chrome.metricsPrivate, args: any[]) {
  try {
    const method = chrome.metricsPrivate[name] as Function;
    method.apply(chrome.metricsPrivate, args);
  } catch (e) {
    console.error((e as Error).stack);
  }
}

/**
 * Records a value than can range from 1 to 10,000.
 * @param name Short metric name.
 * @param value Value to be recorded.
 */
export function recordMediumCount(name: string, value: number) {
  callAPI('recordMediumCount', [convertName(name), value]);
}

/**
 * Records a value than can range from 1 to 100.
 * @param name Short metric name.
 * @param value Value to be recorded.
 */
export function recordSmallCount(name: string, value: number) {
  callAPI('recordSmallCount', [convertName(name), value]);
}

/**
 * Records an elapsed time of no more than 10 seconds.
 * @param name Short metric name.
 * @param time Time to be recorded in milliseconds.
 */
export function recordTime(name: string, time: number) {
  callAPI('recordTime', [convertName(name), time]);
}

/**
 * Records a boolean value to the given metric.
 * @param name Short metric name.
 * @param value The value to be recorded.
 */
export function recordBoolean(name: string, value: boolean) {
  callAPI('recordBoolean', [convertName(name), value]);
}

/**
 * Records an action performed by the user.
 * @param {string} name Short metric name.
 */
export function recordUserAction(name: string) {
  callAPI('recordUserAction', [convertName(name)]);
}

/**
 * Records an elapsed time of no more than 10 seconds.
 * @param value Numeric value to be recorded in units that match the histogram
 *    definition (in histograms.xml).
 */
export function recordValue(
    name: string, type: chrome.metricsPrivate.MetricTypeType, min: number,
    max: number, buckets: number, value: number) {
  callAPI('recordValue', [
    {
      'metricName': convertName(name),
      'type': type,
      'min': min,
      'max': max,
      'buckets': buckets,
    },
    value,
  ]);
}

/**
 * Complete the time interval recording.
 *
 * Should be preceded by a call to startInterval with the same name.
 *
 * @param {string} name Unique interval name.
 */
export function recordInterval(name: string) {
  const start = intervals[name];
  if (start !== undefined) {
    recordTime(name, Date.now() - start);
  } else {
    console.error('Unknown interval: ' + name);
  }
}

/**
 * Complete the time interval recording into appropriate bucket.
 *
 * Should be preceded by a call to startInterval with the same |name|.
 *
 * @param name Unique interval name.
 * @param numFiles The number of files in this current directory.
 * @param buckets Array of numbers that correspond to a bucket value, this will
 *     be suffixed to |name| when recorded.
 * @param tolerance Allowed tolerance for |value| to coalesce into a
 *    bucket.
 */
export function recordDirectoryListLoadWithTolerance(
    name: string, numFiles: number, buckets: number[], tolerance: number) {
  const start = intervals[name];
  if (start !== undefined) {
    for (const bucketValue of buckets) {
      const toleranceMargin = bucketValue * tolerance;
      if (numFiles >= (bucketValue - toleranceMargin) &&
          numFiles <= (bucketValue + toleranceMargin)) {
        recordTime(`${name}.${bucketValue}`, Date.now() - start);
        return;
      }
    }
  } else {
    console.error('Interval not started:', name);
  }
}

/**
 * Record an enum value.
 *
 * @param name Metric name.
 * @param value Enum value.
 * @param validValues Array of valid values or a boundary number
 *     (one-past-the-end) value.
 */
export function recordEnum(
    name: string, value: any, validValues: readonly any[]) {
  console.assert(validValues !== undefined);

  let index = validValues.indexOf(value);
  const boundaryValue = validValues.length;

  // Collect invalid values in the overflow bucket at the end.
  if (index < 0 || index >= boundaryValue) {
    index = boundaryValue - 1;
  }

  // Setting min to 1 looks strange but this is exactly the recommended way
  // of using histograms for enum-like types. Bucket #0 works as a regular
  // bucket AND the underflow bucket.
  // (Source: UMA_HISTOGRAM_ENUMERATION definition in
  // base/metrics/histogram.h)
  const metricDescr = {
    'metricName': convertName(name),
    'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
    'min': 1,
    'max': boundaryValue - 1,
    'buckets': boundaryValue,
  };
  callAPI('recordValue', [metricDescr, index]);
}
