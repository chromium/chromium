/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Common events for the network classes.
 */


goog.provide('goog.net.EventType');


/**
 * Event names for network events
 * @enum {string}
 */
goog.net.EventType = {
  COMPLETE: 'complete',
  SUCCESS: 'success',
  ERROR: 'error',
  ABORT: 'abort',
  READY: 'ready',
  READY_STATE_CHANGE: 'readystatechange',
  TIMEOUT: 'timeout',
  INCREMENTAL_DATA: 'incrementaldata',
  PROGRESS: 'progress',
  // DOWNLOAD_PROGRESS and UPLOAD_PROGRESS are special events dispatched by
  // goog.net.XhrIo to allow binding listeners specific to each type of
  // progress.
  DOWNLOAD_PROGRESS: 'downloadprogress',
  UPLOAD_PROGRESS: 'uploadprogress',
};
