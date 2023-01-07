/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Implementation of a WebChannel transport using WebChannelBase.
 *
 * When WebChannelBase is used as the underlying transport, the capabilities
 * of the WebChannel are limited to what's supported by the implementation.
 * Particularly, multiplexing is not possible, and only strings are
 * supported as message types.
 */

goog.provide('goog.labs.net.webChannel.WebChannelBaseTransport');

goog.require('goog.asserts');
goog.require('goog.events.EventTarget');
goog.require('goog.json');
goog.require('goog.labs.net.webChannel.ChannelRequest');
goog.require('goog.labs.net.webChannel.WebChannelBase');
goog.require('goog.labs.net.webChannel.Wire');
goog.require('goog.log');
goog.require('goog.net.WebChannel');
goog.require('goog.net.WebChannelTransport');
goog.require('goog.object');
goog.require('goog.string');



/**
 * Implementation of {@link goog.net.WebChannelTransport} with
 * {@link goog.labs.net.webChannel.WebChannelBase} as the underlying channel
 * implementation.
 *
 * @constructor
 * @struct
 * @implements {goog.net.WebChannelTransport}
 * @final
 */
goog.labs.net.webChannel.WebChannelBaseTransport = function() {
  'use strict';
  if (!goog.labs.net.webChannel.ChannelRequest.supportsXhrStreaming()) {
    throw new Error('Environmental error: no available transport.');
  }
};


goog.scope(function() {
'use strict';
const WebChannelBaseTransport =
    goog.labs.net.webChannel.WebChannelBaseTransport;
const WebChannelBase = goog.labs.net.webChannel.WebChannelBase;
const Wire = goog.labs.net.webChannel.Wire;


/**
 * @override
 */
WebChannelBaseTransport.prototype.createWebChannel = function(
    url, opt_options) {
  'use strict';
  return new WebChannelBaseTransport.Channel(url, opt_options);
};



/**
 * Implementation of the {@link goog.net.WebChannel} interface.
 *
 * @param {string} url The URL path for the new WebChannel instance.
 * @param {!goog.net.WebChannel.Options=} opt_options Configuration for the
 *     new WebChannel instance.
 *
 * @constructor
 * @implements {goog.net.WebChannel}
 * @extends {goog.events.EventTarget}
 * @final
 */
WebChannelBaseTransport.Channel = function(url, opt_options) {
  'use strict';
  WebChannelBaseTransport.Channel.base(this, 'constructor');

  /**
   * @private {!WebChannelBase} The underlying channel object.
   */
  this.channel_ = new WebChannelBase(
      opt_options, goog.net.WebChannelTransport.CLIENT_VERSION);

  /**
   * @private {string} The URL of the target server end-point.
   */
  this.url_ = url;

  /**
   * @private {goog.log.Logger} The logger for this class.
   */
  this.logger_ =
      goog.log.getLogger('goog.labs.net.webChannel.WebChannelBaseTransport');

  /**
   * @private {Object<string, string>} Extra URL parameters
   * to be added to each HTTP request.
   */
  this.messageUrlParams_ =
      (opt_options && opt_options.messageUrlParams) || null;

  let messageHeaders = (opt_options && opt_options.messageHeaders) || null;

  // default is false
  if (opt_options && opt_options.clientProtocolHeaderRequired) {
    if (messageHeaders) {
      goog.object.set(
          messageHeaders, goog.net.WebChannel.X_CLIENT_PROTOCOL,
          goog.net.WebChannel.X_CLIENT_PROTOCOL_WEB_CHANNEL);
    } else {
      messageHeaders = goog.object.create(
          goog.net.WebChannel.X_CLIENT_PROTOCOL,
          goog.net.WebChannel.X_CLIENT_PROTOCOL_WEB_CHANNEL);
    }
  }

  this.channel_.setExtraHeaders(messageHeaders);

  let initHeaders = (opt_options && opt_options.initMessageHeaders) || null;

  if (opt_options && opt_options.messageContentType) {
    if (initHeaders) {
      goog.object.set(
          initHeaders, goog.net.WebChannel.X_WEBCHANNEL_CONTENT_TYPE,
          opt_options.messageContentType);
    } else {
      initHeaders = goog.object.create(
          goog.net.WebChannel.X_WEBCHANNEL_CONTENT_TYPE,
          opt_options.messageContentType);
    }
  }

  if (opt_options && opt_options.clientProfile) {
    if (initHeaders) {
      goog.object.set(
          initHeaders, goog.net.WebChannel.X_WEBCHANNEL_CLIENT_PROFILE,
          opt_options.clientProfile);
    } else {
      initHeaders = goog.object.create(
          goog.net.WebChannel.X_WEBCHANNEL_CLIENT_PROFILE,
          opt_options.clientProfile);
    }
  }

  this.channel_.setInitHeaders(initHeaders);

  const httpHeadersOverwriteParam =
      opt_options && opt_options.httpHeadersOverwriteParam;
  if (httpHeadersOverwriteParam &&
      !goog.string.isEmptyOrWhitespace(httpHeadersOverwriteParam)) {
    this.channel_.setHttpHeadersOverwriteParam(httpHeadersOverwriteParam);
  }

  /**
   * @private {boolean} Whether to enable CORS.
   */
  this.supportsCrossDomainXhr_ =
      (opt_options && opt_options.supportsCrossDomainXhr) || false;

  /**
   * @private {boolean} Whether to send raw Json and bypass v8 wire format.
   */
  this.sendRawJson_ = (opt_options && opt_options.sendRawJson) || false;

  // Note that httpSessionIdParam will be ignored if the same parameter name
  // has already been specified with messageUrlParams
  const httpSessionIdParam = opt_options && opt_options.httpSessionIdParam;
  if (httpSessionIdParam &&
      !goog.string.isEmptyOrWhitespace(httpSessionIdParam)) {
    this.channel_.setHttpSessionIdParam(httpSessionIdParam);
    if (goog.object.containsKey(this.messageUrlParams_, httpSessionIdParam)) {
      goog.object.remove(this.messageUrlParams_, httpSessionIdParam);
      goog.log.warning(this.logger_,
          'Ignore httpSessionIdParam also specified with messageUrlParams: '
          + httpSessionIdParam);
    }
  }

  /**
   * The channel handler.
   *
   * @private {!WebChannelBaseTransport.Channel.Handler_}
   */
  this.channelHandler_ = new WebChannelBaseTransport.Channel.Handler_(this);
};
goog.inherits(WebChannelBaseTransport.Channel, goog.events.EventTarget);


/**
 * @override
 */
WebChannelBaseTransport.Channel.prototype.open = function() {
  'use strict';
  this.channel_.setHandler(this.channelHandler_);
  if (this.supportsCrossDomainXhr_) {
    this.channel_.setSupportsCrossDomainXhrs(true);
  }
  this.channel_.connect(this.url_, (this.messageUrlParams_ || undefined));
};


/**
 * @override
 */
WebChannelBaseTransport.Channel.prototype.close = function() {
  'use strict';
  this.channel_.disconnect();
};


/**
 * @override
 */
WebChannelBaseTransport.Channel.prototype.halfClose = function() {
  'use strict';
  // to be implemented
  throw new Error('Not implemented');
};


/**
 * The WebChannelBase only supports object types.
 *
 * @param {!goog.net.WebChannel.MessageData} message The message to send.
 *
 * @override
 */
WebChannelBaseTransport.Channel.prototype.send = function(message) {
  'use strict';
  goog.asserts.assert(
      goog.isObject(message) || typeof message === 'string',
      'only object type or raw string is supported');

  if (typeof message === 'string') {
    const rawJson = {};
    rawJson[Wire.RAW_DATA_KEY] = message;
    this.channel_.sendMap(rawJson);
  } else if (this.sendRawJson_) {
    const rawJson = {};
    rawJson[Wire.RAW_DATA_KEY] = goog.json.serialize(message);
    this.channel_.sendMap(rawJson);
  } else {
    this.channel_.sendMap(message);
  }
};


/**
 * @override
 */
WebChannelBaseTransport.Channel.prototype.disposeInternal = function() {
  'use strict';
  this.channel_.setHandler(null);
  delete this.channelHandler_;
  this.channel_.disconnect();
  delete this.channel_;

  WebChannelBaseTransport.Channel.base(this, 'disposeInternal');
};



/**
 * The message event.
 *
 * @param {!Array<?>|!Object} array The data array from the underlying channel.
 * @constructor
 * @extends {goog.net.WebChannel.MessageEvent}
 * @final
 */
WebChannelBaseTransport.Channel.MessageEvent = function(array) {
  'use strict';
  WebChannelBaseTransport.Channel.MessageEvent.base(this, 'constructor');

  // single-metadata only
  const metadata = array['__sm__'];
  if (metadata) {
    this.metadataKey = goog.object.getAnyKey(metadata);
    if (this.metadataKey) {
      this.data = goog.object.get(metadata, this.metadataKey);
    } else {
      this.data = metadata;  // empty
    }
  } else {
    this.data = array;
  }
};
goog.inherits(
    WebChannelBaseTransport.Channel.MessageEvent,
    goog.net.WebChannel.MessageEvent);



/**
 * The error event.
 *
 * @param {WebChannelBase.Error} error The error code.
 * @constructor
 * @extends {goog.net.WebChannel.ErrorEvent}
 * @final
 */
WebChannelBaseTransport.Channel.ErrorEvent = function(error) {
  'use strict';
  WebChannelBaseTransport.Channel.ErrorEvent.base(this, 'constructor');

  /**
   * High-level status code.
   */
  this.status = goog.net.WebChannel.ErrorStatus.NETWORK_ERROR;

  /**
   * @const {WebChannelBase.Error} Internal error code, for debugging use only.
   */
  this.errorCode = error;
};
goog.inherits(
    WebChannelBaseTransport.Channel.ErrorEvent, goog.net.WebChannel.ErrorEvent);



/**
 * Implementation of {@link WebChannelBase.Handler} interface.
 *
 * @param {!WebChannelBaseTransport.Channel} channel The enclosing WebChannel.
 *
 * @constructor
 * @extends {WebChannelBase.Handler}
 * @private
 */
WebChannelBaseTransport.Channel.Handler_ = function(channel) {
  'use strict';
  WebChannelBaseTransport.Channel.Handler_.base(this, 'constructor');

  /**
   * @type {!WebChannelBaseTransport.Channel}
   * @private
   */
  this.channel_ = channel;
};
goog.inherits(WebChannelBaseTransport.Channel.Handler_, WebChannelBase.Handler);


/**
 * @override
 */
WebChannelBaseTransport.Channel.Handler_.prototype.channelOpened = function(
    channel) {
  'use strict';
  goog.log.info(
      this.channel_.logger_, 'WebChannel opened on ' + this.channel_.url_);
  this.channel_.dispatchEvent(goog.net.WebChannel.EventType.OPEN);
};


/**
 * @override
 */
WebChannelBaseTransport.Channel.Handler_.prototype.channelHandleArray =
    function(channel, array) {
  'use strict';
  goog.asserts.assert(array, 'array expected to be defined');
  this.channel_.dispatchEvent(
      new WebChannelBaseTransport.Channel.MessageEvent(array));
};


/**
 * @override
 */
WebChannelBaseTransport.Channel.Handler_.prototype.channelError = function(
    channel, error) {
  'use strict';
  goog.log.info(
      this.channel_.logger_,
      'WebChannel aborted on ' + this.channel_.url_ +
          ' due to channel error: ' + error);
  this.channel_.dispatchEvent(
      new WebChannelBaseTransport.Channel.ErrorEvent(error));
};


/**
 * @override
 */
WebChannelBaseTransport.Channel.Handler_.prototype.channelClosed = function(
    channel, opt_pendingMaps, opt_undeliveredMaps) {
  'use strict';
  goog.log.info(
      this.channel_.logger_, 'WebChannel closed on ' + this.channel_.url_);
  this.channel_.dispatchEvent(goog.net.WebChannel.EventType.CLOSE);
};


/**
 * @override
 */
WebChannelBaseTransport.Channel.prototype.getRuntimeProperties = function() {
  'use strict';
  return new WebChannelBaseTransport.ChannelProperties(this.channel_);
};



/**
 * Implementation of the {@link goog.net.WebChannel.RuntimeProperties}.
 *
 * @param {!WebChannelBase} channel The underlying channel object.
 *
 * @constructor
 * @implements {goog.net.WebChannel.RuntimeProperties}
 * @final
 */
WebChannelBaseTransport.ChannelProperties = function(channel) {
  'use strict';
  /**
   * The underlying channel object.
   *
   * @private {!WebChannelBase}
   */
  this.channel_ = channel;
};


/**
 * @override
 */
WebChannelBaseTransport.ChannelProperties.prototype.getConcurrentRequestLimit =
    function() {
  'use strict';
  return this.channel_.getForwardChannelRequestPool().getMaxSize();
};


/**
 * @override
 */
WebChannelBaseTransport.ChannelProperties.prototype.isSpdyEnabled = function() {
  'use strict';
  return this.getConcurrentRequestLimit() > 1;
};


/**
 * @override
 */
WebChannelBaseTransport.ChannelProperties.prototype.getPendingRequestCount =
    function() {
  'use strict';
  return this.channel_.getForwardChannelRequestPool().getRequestCount();
};


/**
 * @override
 */
WebChannelBaseTransport.ChannelProperties.prototype.getHttpSessionId =
    function() {
  'use strict';
  return this.channel_.getHttpSessionId();
};


/**
 * @override
 */
WebChannelBaseTransport.ChannelProperties.prototype.commit = function(
    callback) {
  'use strict';
  this.channel_.setForwardChannelFlushCallback(callback);
};


/**
 * @override
 */
WebChannelBaseTransport.ChannelProperties.prototype.getNonAckedMessageCount =
    goog.abstractMethod;


/**
 * @override
 */
WebChannelBaseTransport.ChannelProperties.prototype.notifyNonAckedMessageCount =
    goog.abstractMethod;


/**
 * @override
 */
WebChannelBaseTransport.ChannelProperties.prototype.onCommit =
    goog.abstractMethod;


/**
 * @override
 */
WebChannelBaseTransport.ChannelProperties.prototype.ackCommit =
    goog.abstractMethod;


/** @override */
WebChannelBaseTransport.ChannelProperties.prototype.getLastStatusCode =
    function() {
  'use strict';
  return this.channel_.getLastStatusCode();
};
});  // goog.scope
