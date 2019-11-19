// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @interface
 */
class MetadataParserLogger {
  constructor() {
    /**
     * Verbose logging for the dispatcher.
     * Individual parsers also take this as their default verbosity setting.
     * @public {boolean}
     */
    this.verbose;
  }

  /**
   * Indicate to the caller that an operation has failed.
   *
   * No other messages relating to the failed operation should be sent.
   * @param {...(Object|string)} var_args Arguments.
   */
  error(var_args) {}

  /**
   * Send a log message to the caller.
   *
   * Callers must not parse log messages for control flow.
   * @param {...(Object|string)} var_args Arguments.
   */
  log(var_args) {}

  /**
   * Send a log message to the caller only if this.verbose is true.
   * @param {...(Object|string)} var_args Arguments.
   */
  vlog(var_args) {}
}

/**
 * @param {string} url
 * @param {function(!Entry)} successCallback
 * @param {function(!FileError)=} opt_errorCallback
 */
var webkitResolveLocalFileSystemURL = function(
    url, successCallback, opt_errorCallback) {};
