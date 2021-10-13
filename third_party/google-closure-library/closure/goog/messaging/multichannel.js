/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of goog.messaging.MultiChannel, which uses a
 * single underlying MessageChannel to carry several independent virtual message
 * channels.
 */


goog.provide('goog.messaging.MultiChannel');
goog.provide('goog.messaging.MultiChannel.VirtualChannel');

goog.require('goog.Disposable');
goog.require('goog.dispose');
goog.require('goog.log');
goog.require('goog.messaging.MessageChannel');  // interface
goog.require('goog.object');



/**
 * Creates a new MultiChannel wrapping a single MessageChannel. The
 * underlying channel shouldn't have any other listeners registered, but it
 * should be connected.
 *
 * Note that the other side of the channel should also be connected to a
 * MultiChannel with the same number of virtual channels.
 *
 * @param {goog.messaging.MessageChannel} underlyingChannel The underlying
 *     channel to use as transport for the virtual channels.
 * @constructor
 * @extends {goog.Disposable}
 * @final
 */
goog.messaging.MultiChannel = function(underlyingChannel) {
  'use strict';
  goog.messaging.MultiChannel.base(this, 'constructor');

  /**
   * The underlying channel across which all requests are sent.
   * @type {goog.messaging.MessageChannel}
   * @private
   */
  this.underlyingChannel_ = underlyingChannel;

  /**
   * All the virtual channels that are registered for this MultiChannel.
   * These are null if they've been disposed.
   * @type {Object<?goog.messaging.MultiChannel.VirtualChannel>}
   * @private
   */
  this.virtualChannels_ = {};

  this.underlyingChannel_.registerDefaultService(
      goog.bind(this.handleDefault_, this));
};
goog.inherits(goog.messaging.MultiChannel, goog.Disposable);


/**
 * Logger object for goog.messaging.MultiChannel.
 * @type {goog.log.Logger}
 * @private
 */
goog.messaging.MultiChannel.prototype.logger_ =
    goog.log.getLogger('goog.messaging.MultiChannel');


/**
 * Creates a new virtual channel that will communicate across the underlying
 * channel.
 * @param {string} name The name of the virtual channel. Must be unique for this
 *     MultiChannel. Cannot contain colons.
 * @return {!goog.messaging.MultiChannel.VirtualChannel} The new virtual
 *     channel.
 */
goog.messaging.MultiChannel.prototype.createVirtualChannel = function(name) {
  'use strict';
  if (name.indexOf(':') != -1) {
    throw new Error(
        'Virtual channel name "' + name + '" should not contain colons');
  }

  if (name in this.virtualChannels_) {
    throw new Error(
        'Virtual channel "' + name + '" was already created for ' +
        'this multichannel.');
  }

  const channel = new goog.messaging.MultiChannel.VirtualChannel(this, name);
  this.virtualChannels_[name] = channel;
  return channel;
};


/**
 * Handles the default service for the underlying channel. This dispatches any
 * unrecognized services to the appropriate virtual channel.
 *
 * @param {string} serviceName The name of the service being called.
 * @param {string|!Object} payload The message payload.
 * @private
 */
goog.messaging.MultiChannel.prototype.handleDefault_ = function(
    serviceName, payload) {
  'use strict';
  const match = serviceName.match(/^([^:]*):(.*)/);
  if (!match) {
    goog.log.warning(
        this.logger_, 'Invalid service name "' + serviceName + '": no ' +
            'virtual channel specified');
    return;
  }

  const channelName = match[1];
  serviceName = match[2];
  if (!(channelName in this.virtualChannels_)) {
    goog.log.warning(
        this.logger_, 'Virtual channel "' + channelName + ' does not ' +
            'exist, but a message was received for it: "' + serviceName + '"');
    return;
  }

  const virtualChannel = this.virtualChannels_[channelName];
  if (!virtualChannel) {
    goog.log.warning(
        this.logger_, 'Virtual channel "' + channelName + ' has been ' +
            'disposed, but a message was received for it: "' + serviceName +
            '"');
    return;
  }

  if (!virtualChannel.defaultService_) {
    goog.log.warning(
        this.logger_, 'Service "' + serviceName + '" is not registered ' +
            'on virtual channel "' + channelName + '"');
    return;
  }

  virtualChannel.defaultService_(serviceName, payload);
};


/** @override */
goog.messaging.MultiChannel.prototype.disposeInternal = function() {
  'use strict';
  goog.object.forEach(this.virtualChannels_, function(channel) {
    'use strict';
    goog.dispose(channel);
  });
  goog.dispose(this.underlyingChannel_);
  delete this.virtualChannels_;
  delete this.underlyingChannel_;
};



/**
 * A message channel that proxies its messages over another underlying channel.
 *
 * @param {goog.messaging.MultiChannel} parent The MultiChannel
 *     which created this channel, and which contains the underlying
 *     MessageChannel that's used as the transport.
 * @param {string} name The name of this virtual channel. Unique among the
 *     virtual channels in parent.
 * @constructor
 * @implements {goog.messaging.MessageChannel}
 * @extends {goog.Disposable}
 * @final
 */
goog.messaging.MultiChannel.VirtualChannel = function(parent, name) {
  'use strict';
  goog.messaging.MultiChannel.VirtualChannel.base(this, 'constructor');

  /**
   * The MultiChannel containing the underlying transport channel.
   * @type {goog.messaging.MultiChannel}
   * @private
   */
  this.parent_ = parent;

  /**
   * The name of this virtual channel.
   * @type {string}
   * @private
   */
  this.name_ = name;
};
goog.inherits(goog.messaging.MultiChannel.VirtualChannel, goog.Disposable);


/**
 * The default service to run if no other services match.
 * @type {?function(string, (string|!Object))}
 * @private
 */
goog.messaging.MultiChannel.VirtualChannel.prototype.defaultService_;


/**
 * Logger object for goog.messaging.MultiChannel.VirtualChannel.
 * @type {goog.log.Logger}
 * @private
 */
goog.messaging.MultiChannel.VirtualChannel.prototype.logger_ =
    goog.log.getLogger('goog.messaging.MultiChannel.VirtualChannel');


/**
 * This is a no-op, since the underlying channel is expected to already be
 * initialized when it's passed in.
 *
 * @override
 */
goog.messaging.MultiChannel.VirtualChannel.prototype.connect = function(
    opt_connectCb) {
  'use strict';
  if (opt_connectCb) {
    opt_connectCb();
  }
};


/**
 * This always returns true, since the underlying channel is expected to already
 * be initialized when it's passed in.
 *
 * @override
 */
goog.messaging.MultiChannel.VirtualChannel.prototype.isConnected = function() {
  'use strict';
  return true;
};


/**
 * @override
 */
goog.messaging.MultiChannel.VirtualChannel.prototype.registerService = function(
    serviceName, callback, opt_objectPayload) {
  'use strict';
  this.parent_.underlyingChannel_.registerService(
      this.name_ + ':' + serviceName,
      goog.bind(this.doCallback_, this, callback), opt_objectPayload);
};


/**
 * @override
 */
goog.messaging.MultiChannel.VirtualChannel.prototype.registerDefaultService =
    function(callback) {
  'use strict';
  this.defaultService_ = goog.bind(this.doCallback_, this, callback);
};


/**
 * @override
 */
goog.messaging.MultiChannel.VirtualChannel.prototype.send = function(
    serviceName, payload) {
  'use strict';
  if (this.isDisposed()) {
    throw new Error('#send called for disposed VirtualChannel.');
  }

  this.parent_.underlyingChannel_.send(this.name_ + ':' + serviceName, payload);
};


/**
 * Wraps a callback with a function that will log a warning and abort if it's
 * called when this channel is disposed.
 *
 * @param {!Function} callback The callback to wrap.
 * @param {...*} var_args Other arguments, passed to the callback.
 * @private
 */
goog.messaging.MultiChannel.VirtualChannel.prototype.doCallback_ = function(
    callback, var_args) {
  'use strict';
  if (this.isDisposed()) {
    goog.log.warning(
        this.logger_, 'Virtual channel "' + this.name_ + '" received ' +
            ' a message after being disposed.');
    return;
  }

  callback.apply({}, Array.prototype.slice.call(arguments, 1));
};


/** @override */
goog.messaging.MultiChannel.VirtualChannel.prototype.disposeInternal =
    function() {
  'use strict';
  this.parent_.virtualChannels_[this.name_] = null;
  this.parent_ = null;
};
