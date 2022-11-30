/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of goog.messaging.RespondingChannel, which wraps a
 * MessageChannel and allows the user to get the response from the services.
 */


goog.provide('goog.messaging.RespondingChannel');

goog.require('goog.Disposable');
goog.require('goog.Promise');
goog.require('goog.dispose');
goog.require('goog.log');
goog.require('goog.messaging.MultiChannel');
goog.requireType('goog.messaging.MessageChannel');



/**
 * Creates a new RespondingChannel wrapping a single MessageChannel.
 * @param {goog.messaging.MessageChannel} messageChannel The messageChannel to
 *     to wrap and allow for responses. This channel must not have any existing
 *     services registered. All service registration must be done through the
 *     {@link RespondingChannel#registerService} api instead. The other end of
 *     channel must also be a RespondingChannel.
 * @constructor
 * @extends {goog.Disposable}
 */
goog.messaging.RespondingChannel = function(messageChannel) {
  'use strict';
  goog.messaging.RespondingChannel.base(this, 'constructor');

  /**
   * The message channel wrapped in a MultiChannel so we can send private and
   * public messages on it.
   * @type {goog.messaging.MultiChannel}
   * @private
   */
  this.messageChannel_ = new goog.messaging.MultiChannel(messageChannel);

  /**
   * Map of invocation signatures to function callbacks. These are used to keep
   * track of the asyncronous service invocations so the result of a service
   * call can be passed back to a callback in the calling frame.
   * @type {Object<number, function(Object)>}
   * @private
   */
  this.sigCallbackMap_ = {};

  /**
   * The virtual channel to send private messages on.
   * @type {goog.messaging.MultiChannel.VirtualChannel}
   * @private
   */
  this.privateChannel_ = this.messageChannel_.createVirtualChannel(
      goog.messaging.RespondingChannel.PRIVATE_CHANNEL_);

  /**
   * The virtual channel to send public messages on.
   * @type {goog.messaging.MultiChannel.VirtualChannel}
   * @private
   */
  this.publicChannel_ = this.messageChannel_.createVirtualChannel(
      goog.messaging.RespondingChannel.PUBLIC_CHANNEL_);

  this.privateChannel_.registerService(
      goog.messaging.RespondingChannel.CALLBACK_SERVICE_,
      goog.bind(this.callbackServiceHandler_, this), true);
};
goog.inherits(goog.messaging.RespondingChannel, goog.Disposable);


/**
 * The name of the method invocation callback service (used internally).
 * @type {string}
 * @const
 * @private
 */
goog.messaging.RespondingChannel.CALLBACK_SERVICE_ = 'mics';


/**
 * The name of the channel to send private control messages on.
 * @type {string}
 * @const
 * @private
 */
goog.messaging.RespondingChannel.PRIVATE_CHANNEL_ = 'private';


/**
 * The name of the channel to send public messages on.
 * @type {string}
 * @const
 * @private
 */
goog.messaging.RespondingChannel.PUBLIC_CHANNEL_ = 'public';


/**
 * The next signature index to save the callback against.
 * @type {number}
 * @private
 */
goog.messaging.RespondingChannel.prototype.nextSignatureIndex_ = 0;


/**
 * Logger object for goog.messaging.RespondingChannel.
 * @type {goog.log.Logger}
 * @private
 */
goog.messaging.RespondingChannel.prototype.logger_ =
    goog.log.getLogger('goog.messaging.RespondingChannel');


/**
 * Gets a random number to use for method invocation results.
 * @return {number} A unique random signature.
 * @private
 */
goog.messaging.RespondingChannel.prototype.getNextSignature_ = function() {
  'use strict';
  return this.nextSignatureIndex_++;
};


/** @override */
goog.messaging.RespondingChannel.prototype.disposeInternal = function() {
  'use strict';
  goog.dispose(this.messageChannel_);
  delete this.messageChannel_;
  // Note: this.publicChannel_ and this.privateChannel_ get disposed by
  //     this.messageChannel_
  delete this.publicChannel_;
  delete this.privateChannel_;
};


/**
 * Sends a message over the channel.
 * @param {string} serviceName The name of the service this message should be
 *     delivered to.
 * @param {string|!Object} payload The value of the message. If this is an
 *     Object, it is serialized to a string before sending if necessary.
 * @param {function(?Object)} callback The callback invoked with
 *     the result of the service call.
 */
goog.messaging.RespondingChannel.prototype.send = function(
    serviceName, payload, callback) {
  'use strict';
  const signature = this.getNextSignature_();
  this.sigCallbackMap_[signature] = callback;

  const message = {};
  message['signature'] = signature;
  message['data'] = payload;

  this.publicChannel_.send(serviceName, message);
};


/**
 * Receives the results of the peer's service results.
 * @param {!Object|string} message The results from the remote service
 *     invocation.
 * @private
 */
goog.messaging.RespondingChannel.prototype.callbackServiceHandler_ = function(
    message) {
  'use strict';
  const signature = message['signature'];
  const result = message['data'];

  if (signature in this.sigCallbackMap_) {
    const callback =
        /** @type {function(Object)} */ (this.sigCallbackMap_[signature]);
    callback(result);
    delete this.sigCallbackMap_[signature];
  } else {
    goog.log.warning(this.logger_, 'Received signature is invalid');
  }
};


/**
 * Registers a service to be called when a message is received.
 * @param {string} serviceName The name of the service.
 * @param {function(!Object)} callback The callback to process the
 *     incoming messages. Passed the payload.
 */
goog.messaging.RespondingChannel.prototype.registerService = function(
    serviceName, callback) {
  'use strict';
  this.publicChannel_.registerService(
      serviceName, goog.bind(this.callbackProxy_, this, callback), true);
};


/**
 * A intermediary proxy for service callbacks to be invoked and return their
 * their results to the remote caller's callback.
 * @param {function((string|!Object))} callback The callback to process the
 *     incoming messages. Passed the payload.
 * @param {!Object|string} message The message containing the signature and
 *     the data to invoke the service callback with.
 * @private
 */
goog.messaging.RespondingChannel.prototype.callbackProxy_ = function(
    callback, message) {
  'use strict';
  const response = callback(message['data']);
  const signature = message['signature'];
  goog.Promise.resolve(response).then(goog.bind(function(result) {
    'use strict';
    this.sendResponse_(result, signature);
  }, this));
};


/**
 * Sends the results of the service callback to the remote caller's callback.
 * @param {(string|!Object)} result The results of the service callback.
 * @param {string} signature The signature of the request to the service
 *     callback.
 * @private
 */
goog.messaging.RespondingChannel.prototype.sendResponse_ = function(
    result, signature) {
  'use strict';
  const resultMessage = {};
  resultMessage['data'] = result;
  resultMessage['signature'] = signature;
  // The callback invoked above may have disposed the channel so check if it
  // exists.
  if (this.privateChannel_) {
    this.privateChannel_.send(
        goog.messaging.RespondingChannel.CALLBACK_SERVICE_, resultMessage);
  }
};
