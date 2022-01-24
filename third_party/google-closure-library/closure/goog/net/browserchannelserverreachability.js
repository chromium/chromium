/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of events for Server Reachability.
 */

goog.module('goog.net.browserchannelinternal.ServerReachability');
goog.module.declareLegacyNamespace();

/**
 * Types of events which reveal information about the reachability of the
 * server.
 * @enum {number}
 */
const ServerReachability = {
  REQUEST_MADE: 1,
  REQUEST_SUCCEEDED: 2,
  REQUEST_FAILED: 3,
  BACK_CHANNEL_ACTIVITY: 4,
};
exports = ServerReachability;
