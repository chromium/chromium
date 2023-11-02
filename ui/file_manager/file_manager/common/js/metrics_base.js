// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

const metricsBase = {};

/**
 * A map from interval name to interval start timestamp.
 */
metricsBase.intervals = {};

/**
 * A mapping of enum names to valid values. This object is consulted
 * any time an enum value is being reported un-accompanied by a list
 * of valid values.
 *
 * <p>Values mut be provided by base classes. Values should correspond exactly
 * with values from histograms.xml.
 *
 * @private {!Object<!Array<*>|number>}
 */
metricsBase.validEnumValues_ = {};


/**
 * Start the named time interval.
 * Should be followed by a call to recordInterval with the same name.
 *
 * @param {string} name Unique interval name.
 */
metricsBase.startInterval = name => {
  metricsBase.intervals[name] = Date.now();
};

/**
 * Convert a short metric name to the full format.
 *
 * @param {string} name Short metric name.
 * @return {string} Full metric name.
 * @private
 */
metricsBase.convertName_ = name => {
  throw new Error('metricsBase.convertName_() must be overrideen by subclass.');
};

/**
 * Wrapper method for calling chrome.fileManagerPrivate safely.
 * @param {string} methodName Method name.
 * @param {Array<Object>} args Arguments.
 * @private
 */
metricsBase.call_ = (methodName, args) => {
  try {
    chrome.metricsPrivate[methodName].apply(chrome.metricsPrivate, args);
  } catch (e) {
    console.error(e.stack);
  }
  // Support writing metrics.log in manual testing to log method calls.
  if (/** @type{{ log: (boolean|undefined) }} */ (metricsBase).log) {
    console.log('chrome.metricsPrivate.' + methodName, args);
  }
};

/**
 * Records a value than can range from 1 to 10,000.
 * @param {string} name Short metric name.
 * @param {number} value Value to be recorded.
 */
metricsBase.recordMediumCount = (name, value) => {
  metricsBase.call_(
      'recordMediumCount', [metricsBase.convertName_(name), value]);
};

/**
 * Records a value than can range from 1 to 100.
 * @param {string} name Short metric name.
 * @param {number} value Value to be recorded.
 */
metricsBase.recordSmallCount = (name, value) => {
  metricsBase.call_(
      'recordSmallCount', [metricsBase.convertName_(name), value]);
};

/**
 * Records an elapsed time of no more than 10 seconds.
 * @param {string} name Short metric name.
 * @param {number} time Time to be recorded in milliseconds.
 */
metricsBase.recordTime = (name, time) => {
  metricsBase.call_('recordTime', [metricsBase.convertName_(name), time]);
};

/**
 * Records a boolean value to the given metric.
 * @param {string} name Short metric name.
 * @param {boolean} value The value to be recorded.
 */
metricsBase.recordBoolean = (name, value) => {
  metricsBase.call_('recordBoolean', [metricsBase.convertName_(name), value]);
};

/**
 * Records an action performed by the user.
 * @param {string} name Short metric name.
 */
metricsBase.recordUserAction = name => {
  metricsBase.call_('recordUserAction', [metricsBase.convertName_(name)]);
};

/**
 * Records an elapsed time of no more than 10 seconds.
 * @param {string} name Short metric name.
 * @param {!chrome.metricsPrivate.MetricTypeType} type
 * @param {number} min
 * @param {number} max
 * @param {number} buckets
 * @param {number} value Numeric value to be recorded in units
 *     that match the histogram definition (in histograms.xml).
 */
metricsBase.recordValue = (name, type, min, max, buckets, value) => {
  metricsBase.call_('recordValue', [
    {
      'metricName': metricsBase.convertName_(name),
      'type': type,
      'min': min,
      'max': max,
      'buckets': buckets,
    },
    value,
  ]);
};

/**
 * Complete the time interval recording.
 *
 * Should be preceded by a call to startInterval with the same name.
 *
 * @param {string} name Unique interval name.
 */
metricsBase.recordInterval = name => {
  if (name in metricsBase.intervals) {
    metricsBase.recordTime(name, Date.now() - metricsBase.intervals[name]);
  } else {
    console.error('Unknown interval: ' + name);
  }
};

/**
 * Complete the time interval recording into appropriate bucket.
 *
 * Should be preceded by a call to startInterval with the same |name|.
 *
 * @param {string} name Unique interval name.
 * @param {number} numFiles The number of files in this current directory.
 * @param {!Array<number>} buckets Array of numbers that correspond to a bucket
 *    value, this will be suffixed to |name| when recorded.
 * @param {number=} tolerance Allowed tolerance for |value| to coalesce into a
 *    bucket.
 */
metricsBase.recordDirectoryListLoadWithTolerance =
    (name, numFiles, buckets, tolerance) => {
      if (name in metricsBase.intervals) {
        for (const bucketValue of buckets) {
          const toleranceMargin = bucketValue * tolerance;
          if (numFiles >= (bucketValue - toleranceMargin) &&
              numFiles <= (bucketValue + toleranceMargin)) {
            metricsBase.recordTime(
                `${name}.${bucketValue}`,
                Date.now() - metricsBase.intervals[name]);
            return;
          }
        }
      } else {
        console.error('Interval not started:', name);
      }
    };

/**
 * Record an enum value.
 *
 * @param {string} name Metric name.
 * @param {*} value Enum value.
 * @param {Array<*>|number=} opt_validValues Array of valid values
 *     or a boundary number (one-past-the-end) value.
 */
metricsBase.recordEnum = (name, value, opt_validValues) => {
  let boundaryValue;
  let index;

  let validValues = opt_validValues;
  if (metricsBase.validEnumValues_ && name in metricsBase.validEnumValues_) {
    console.assert(validValues === undefined);
    validValues = metricsBase.validEnumValues_[name];
  }
  console.assert(validValues !== undefined);

  if (validValues.constructor.name == 'Array') {
    index = validValues.indexOf(value);
    boundaryValue = validValues.length;
  } else {
    index = /** @type {number} */ (value);
    boundaryValue = validValues;
  }
  // Collect invalid values in the overflow bucket at the end.
  if (index < 0 || index >= boundaryValue) {
    index = boundaryValue - 1;
  }

  // Setting min to 1 looks strange but this is exactly the recommended way
  // of using histograms for enum-like types. Bucket #0 works as a regular
  // bucket AND the underflow bucket.
  // (Source: UMA_HISTOGRAM_ENUMERATION definition in base/metrics/histogram.h)
  const metricDescr = {
    'metricName': metricsBase.convertName_(name),
    'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
    'min': 1,
    'max': boundaryValue - 1,
    'buckets': boundaryValue,
  };
  metricsBase.call_('recordValue', [metricDescr, index]);
};

export {metricsBase};
