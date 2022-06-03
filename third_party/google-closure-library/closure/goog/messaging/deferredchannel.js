/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A MessageChannel decorator that wraps a deferred MessageChannel
 * and enqueues messages and service registrations until that channel exists.
 */

goog.provide('goog.messaging.DeferredChannel');

goog.require('goog.Disposable');
goog.require('goog.messaging.MessageChannel');
goog.requireType('goog.async.Deferred');


/**
 * Creates a new DeferredChannel, which wraps a deferred MessageChannel and
 * enqueues messages to be sent once the wrapped channel is resolved.
 *
 * @param {!goog.async.Deferred<!goog.messaging.MessageChannel>} deferredChannel
 *     The underlying deferred MessageChannel.
 * @constructor
 * @extends {goog.Disposable}
 * @implements {goog.messaging.MessageChannel}
 * @final
 */
goog.messaging.DeferredChannel = function(deferredChannel) {
  'use strict';
  goog.messaging.DeferredChannel.base(this, 'constructor');

  /** @private {!goog.async.Deferred<!goog.messaging.MessageChannel>} */
  this.deferred_ = deferredChannel;
};
goog.inherits(goog.messaging.DeferredChannel, goog.Disposable);


/**
 * Cancels the wrapped Deferred.
 */
goog.messaging.DeferredChannel.prototype.cancel = function() {
  'use strict';
  this.deferred_.cancel();
};


/** @override */
goog.messaging.DeferredChannel.prototype.connect = function(opt_connectCb) {
  'use strict';
  if (opt_connectCb) {
    opt_connectCb();
  }
};


/** @override */
goog.messaging.DeferredChannel.prototype.isConnected = function() {
  'use strict';
  return true;
};


/** @override */
goog.messaging.DeferredChannel.prototype.registerService = function(
    serviceName, callback, opt_objectPayload) {
  'use strict';
  this.deferred_.addCallback(function(resolved) {
    'use strict';
    resolved.registerService(serviceName, callback, opt_objectPayload);
  });
};


/** @override */
goog.messaging.DeferredChannel.prototype.registerDefaultService = function(
    callback) {
  'use strict';
  this.deferred_.addCallback(function(resolved) {
    'use strict';
    resolved.registerDefaultService(callback);
  });
};


/** @override */
goog.messaging.DeferredChannel.prototype.send = function(serviceName, payload) {
  'use strict';
  this.deferred_.addCallback(function(resolved) {
    'use strict';
    resolved.send(serviceName, payload);
  });
};


/** @override */
goog.messaging.DeferredChannel.prototype.disposeInternal = function() {
  'use strict';
  this.cancel();
  goog.messaging.DeferredChannel.base(this, 'disposeInternal');
};
