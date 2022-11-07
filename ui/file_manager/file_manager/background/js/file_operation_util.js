// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../common/js/util.js';

/**
 * Utilities for file operations.
 */
const fileOperationUtil = {};

/**
 * Error class used to report problems with a copy operation.
 * If the code is UNEXPECTED_SOURCE_FILE, data should be a path of the file.
 * If the code is TARGET_EXISTS, data should be the existing Entry.
 * If the code is FILESYSTEM_ERROR, data should be the FileError.
 */
export class FileOperationError {
  /**
   * @param {util.FileOperationErrorType} code Error type.
   * @param {string|Entry|DOMError} data Additional data.
   */
  constructor(code, data) {
    this.code = code;
    this.data = data;
  }
}

/**
 * Resolves a path to either a DirectoryEntry or a FileEntry, regardless of
 * whether the path is a directory or file.
 *
 * @param {DirectoryEntry} root The root of the filesystem to search.
 * @param {string} path The path to be resolved.
 * @return {!Promise<DirectoryEntry|FileEntry>} Promise fulfilled with the
 *     resolved entry, or rejected with FileError.
 */
fileOperationUtil.resolvePath = (root, path) => {
  if (path === '' || path === '/') {
    return Promise.resolve(root);
  }
  return new Promise(root.getFile.bind(root, path, {create: false}))
      .catch(error => {
        if (error.name === util.FileError.TYPE_MISMATCH_ERR) {
          // Bah.  It's a directory, ask again.
          return new Promise(
              root.getDirectory.bind(root, path, {create: false}));
        } else {
          return Promise.reject(error);
        }
      });
};

/**
 * Checks if an entry exists at |relativePath| in |dirEntry|.
 * If exists, tries to deduplicate the path by inserting parenthesized number,
 * such as " (1)", before the extension. If it still exists, tries the
 * deduplication again by increasing the number.
 * For example, suppose "file.txt" is given, "file.txt", "file (1).txt",
 * "file (2).txt", ... will be tried.
 *
 * @param {DirectoryEntry} dirEntry The target directory entry.
 * @param {string} relativePath The path to be deduplicated.
 * @param {function(string)=} opt_successCallback Callback run with the
 *     deduplicated path on success.
 * @param {function(FileOperationError)=} opt_errorCallback
 *     Callback run on error.
 * @return {!Promise<string>} Promise fulfilled with available path.
 */
fileOperationUtil.deduplicatePath =
    (dirEntry, relativePath, opt_successCallback, opt_errorCallback) => {
      // Crack the path into three part. The parenthesized number (if exists)
      // will be replaced by incremented number for retry. For example, suppose
      // |relativePath| is "file (10).txt", the second check path will be
      // "file (11).txt".
      const match = /^(.*?)(?: \((\d+)\))?(\.[^.]*?)?$/.exec(relativePath);
      const prefix = match[1];
      const ext = match[3] || '';

      // Check to see if the target exists.
      const resolvePath = (trialPath, copyNumber) => {
        return fileOperationUtil.resolvePath(dirEntry, trialPath)
            .then(
                () => {
                  const newTrialPath = prefix + ' (' + copyNumber + ')' + ext;
                  return resolvePath(newTrialPath, copyNumber + 1);
                },
                error => {
                  // We expect to be unable to resolve the target file, since
                  // we're going to create it during the copy.  However, if the
                  // resolve fails with anything other than NOT_FOUND, that's
                  // trouble.
                  if (error.name === util.FileError.NOT_FOUND_ERR) {
                    return trialPath;
                  } else {
                    return Promise.reject(error);
                  }
                });
      };

      const promise = resolvePath(relativePath, 1).catch(error => {
        if (error instanceof Error) {
          return Promise.reject(error);
        }
        return Promise.reject(new FileOperationError(
            util.FileOperationErrorType.FILESYSTEM_ERROR, error));
      });
      if (opt_successCallback) {
        promise.then(opt_successCallback, opt_errorCallback);
      }
      return promise;
    };

/**
 * Class to calculate transfer speed and remaining time.
 *
 * Each update from the transfer task stores a sample in a queue.
 *
 * Current speed and remaining time are calculated using a linear interpolation
 * of the kept samples.
 */
fileOperationUtil.Speedometer = class {
  /**
   * @param {number} maxSamples Max number of samples to keep.
   */
  constructor(maxSamples = 20) {
    /**
     * @private @const {number} Max number of samples to keep.
     */
    this.maxSamples_ = maxSamples;

    /**
     * @private {!Array<!{time: number, bytes: number}>} Recent samples.
     *     |time| is in milliseconds.
     */
    this.samples_ = [];

    /**
     * @private {?{time: number, bytes: number}} First sample.
     *     |time| is in milliseconds.
     */
    this.first_ = null;

    /**
     * @private {number} Total number of bytes to be processed by the task.
     */
    this.totalBytes_ = 0;
  }

  /**
   * @returns {number} Number of kept samples.
   */
  getSampleCount() {
    return this.samples_.length;
  }

  /**
   * @returns {number} Remaining time in seconds, or NaN if there aren't enough
   *     samples.
   */
  getRemainingTime() {
    const a = this.interpolate_();
    if (!a) {
      return NaN;
    }

    // Compute remaining time in milliseconds.
    const targetTime =
        (this.totalBytes_ - a.averageBytes) / a.speed + a.averageTime;
    const remainingTime = targetTime - Date.now();

    // Convert remaining time from milliseconds to seconds.
    return remainingTime / 1000;
  }

  /**
   * @param {number} totalBytes Number of total bytes task handles.
   */
  setTotalBytes(totalBytes) {
    this.totalBytes_ = totalBytes;
  }

  /**
   * The current speedometer implementation is not designed to be automatically
   * reused by different "in progress" operations. Call this method after
   * finishing a given operation (or batch of simultaneous "in progress"
   * operations) to make sure the speedometer reports proper remaining time
   * estimates when it's reused.
   */
  reset() {
    this.samples_ = [];
    this.first_ = null;
    this.totalBytes_ = 0;
  }

  /**
   * Adds a sample with the current timestamp and the given number of |bytes|
   * if the previous sample was received more than a second ago.
   * Does nothing if the previous sample was received less than a second ago.
   * @param {number} bytes Total number of bytes processed by the task so far.
   */
  update(bytes) {
    const time = Date.now();
    const sample = {time, bytes};

    // Is this the first sample?
    if (this.first_ == null) {
      // Remember this sample as the first one.
      this.first_ = sample;
    } else {
      // Drop this sample if we already received one less than a second ago.
      const last = this.samples_[this.samples_.length - 1];
      if (sample.time - last.time < 1000) {
        return;
      }
    }

    // Queue this sample.
    if (this.samples_.push(sample) > this.maxSamples_) {
      // Remove old sample.
      this.samples_.shift();
    }
  }

  /**
   * Computes a linear interpolation of the samples stored in |this.samples_|.
   * @private
   * @returns {?{speed: number, averageTime: number, averageBytes: number}} null
   *     if there aren't enough samples, or the result of the linear
   *     interpolation. |speed| is the slope of the linear interpolation in
   *     bytes per millisecond. The linear interpolation goes through the point
   *     |(averageTime, averageBytes)|.
   */
  interpolate_() {
    // Don't even try to compute the linear interpolation unless we have enough
    // samples.
    const n = this.samples_.length;
    if (n < 2) {
      return null;
    }

    // First pass to compute averages.
    let averageTime = 0;
    let averageBytes = 0;

    for (const {time, bytes} of this.samples_) {
      averageTime += time;
      averageBytes += bytes;
    }

    averageTime /= n;
    averageBytes /= n;

    // Second pass to compute variances.
    let varianceTime = 0;
    let covarianceTimeBytes = 0;

    for (const {time, bytes} of this.samples_) {
      const timeDiff = time - averageTime;
      varianceTime += timeDiff * timeDiff;
      covarianceTimeBytes += timeDiff * (bytes - averageBytes);
    }

    // Strictly speaking, both varianceTime and covarianceTimeBytes should be
    // scaled down by the number of samples n. But since we're only interested
    // in the ratio of these two quantities, we can avoid these two divisions.
    //
    // varianceTime /= n;
    // covarianceTimeBytes /= n;

    // Compute speed.
    const speed = covarianceTimeBytes / varianceTime;
    return {speed, averageTime, averageBytes};
  }
};

export {fileOperationUtil};
