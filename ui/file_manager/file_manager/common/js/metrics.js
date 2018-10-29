// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

var metrics = metrics || metricsBase;

/**
 * Convert a short metric name to the full format.
 *
 * @param {string} name Short metric name.
 * @return {string} Full metric name.
 * @override
 * @private
 */
metrics.convertName_ = function(name) {
  return 'FileBrowser.' + name;
};

/** @private {analytics.GoogleAnalytics} */
metrics.analytics_ = null;

/** @private {analytics.Tracker} */
metrics.tracker_ = null;

/** @private {boolean} */
metrics.enabled_ = false;

/** @return {!analytics.Tracker} */
metrics.getTracker = function() {
  if (!metrics.tracker_) {
    metrics.createTracker_();
  }
  return /** @type {!analytics.Tracker} */ (metrics.tracker_);
};

/**
 * Creates a new analytics tracker.
 * @private
 */
metrics.createTracker_ = function() {
  var chromeVersion = /Chrome\/([0-9]*)\.[0-9.]*/.exec(navigator.userAgent);
  if (chromeVersion && chromeVersion[1]) {
    metrics.analytics_ = analytics.getService('Files app', chromeVersion[1]);
  } else {
    metrics.analytics_ = analytics.getService('Files app', '0.0');
  }

  // Create a tracker, add a filter that only enables analytics when UMA is
  // enabled.
  const kFilesAppTrackingId = 'UA-38248358-9';
  metrics.tracker_ = metrics.analytics_.getTracker(kFilesAppTrackingId);
  metrics.tracker_.addFilter(metrics.umaEnabledFilter_);
};

/**
 * Queries the chrome UMA enabled setting, and filters hits based on that.
 * @param {!analytics.Tracker.Hit} hit
 * @return {!goog.async.Deferred} A deferred indicating when the filter has
 *     completed running.
 * @private
 */
metrics.umaEnabledFilter_ = function(hit) {
  // TODO(kenobi): Change this to use Promises when analytics supports it.
  var deferred = new goog.async.Deferred();

  chrome.fileManagerPrivate.isUMAEnabled(
      function(enabled) {
        if (chrome.runtime.lastError) {
          console.error(chrome.runtime.lastError.message);
          return;
        }
        assert(enabled !== undefined);
        if (!enabled) {
          // If UMA was just toggled, reset the analytics ID.
          if (metrics.enabled_) {
            metrics.clearUserId_();
          }
          hit.cancel();
        }
        metrics.enabled_ = enabled;
        // TODO(sashab): We should call deferred.callback(enabled) here, but
        // this can cause strange issues when behind certain VPNs. In the
        // meantime, don't call anything, so Analytics is never contacted, which
        // prevents this issue. See https://crbug.com/842880 for details.
      });

  return deferred;
};

/**
 * Clears the previously set analytics user id.
 * @return {!Promise} Resolves when the analytics ID has been reset.
 */
metrics.clearUserId_ = function() {
  return metrics.analytics_.getConfig().then(
      /** @param {!analytics.Config} config */
      function(config) {
        config.resetUserId();
      });
};
