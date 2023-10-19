// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @interface
 */
export class MetadataParserLogger {
  constructor() {
    /**
     * Verbose logging for the dispatcher.
     * Individual parsers also take this as their default verbosity setting.
     * @public @type {boolean}
     */
    // @ts-ignore: error TS2339: Property 'verbose' does not exist on type
    // 'MetadataParserLogger'.
    this.verbose;
  }

  /**
   * Indicate to the caller that an operation has failed.
   *
   * No other messages relating to the failed operation should be sent.
   * @param {...(Object|string)} var_args Arguments.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  error(var_args) {}

  /**
   * Send a log message to the caller.
   *
   * Callers must not parse log messages for control flow.
   * @param {...(Object|string)} var_args Arguments.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  log(var_args) {}

  /**
   * Send a log message to the caller only if this.verbose is true.
   * @param {...(Object|string)} var_args Arguments.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  vlog(var_args) {}
}

/**
 * @param {string} url
 * @param {function(!Entry):void} successCallback
 * @param {function(!FileError):void=} opt_errorCallback
 */
export const webkitResolveLocalFileSystemURL = function(
    // @ts-ignore: error TS6133: 'opt_errorCallback' is declared but its value
    // is never read.
    url, successCallback, opt_errorCallback) {};
