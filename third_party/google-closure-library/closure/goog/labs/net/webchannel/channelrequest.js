/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the ChannelRequest class. The request
 * object encapsulates the logic for making a single request, either for the
 * forward channel, back channel, or test channel, to the server. It contains
 * the logic for the two types of transports we use:
 * XMLHTTP and Image request. It provides timeout detection. More transports
 * to be added in future, such as Fetch, WebSocket.
 *
 */


goog.provide('goog.labs.net.webChannel.ChannelRequest');

goog.require('goog.Timer');
goog.require('goog.asserts');
goog.require('goog.async.Throttle');
goog.require('goog.dispose');
goog.require('goog.events.EventHandler');
goog.require('goog.labs.net.webChannel.Channel');
goog.require('goog.labs.net.webChannel.WebChannelDebug');
goog.require('goog.labs.net.webChannel.environment');
goog.require('goog.labs.net.webChannel.requestStats');
goog.require('goog.net.ErrorCode');
goog.require('goog.net.EventType');
goog.require('goog.net.WebChannel');
goog.require('goog.net.XmlHttp');
goog.require('goog.object');
goog.require('goog.string');
goog.require('goog.userAgent');
goog.requireType('goog.Uri');
goog.requireType('goog.events.Event');
goog.requireType('goog.labs.net.webChannel.Wire.QueuedMap');
goog.requireType('goog.net.XhrIo');



/**
 * A new ChannelRequest is created for each request to the server.
 *
 * @param {goog.labs.net.webChannel.Channel} channel
 *     The channel that owns this request.
 * @param {goog.labs.net.webChannel.WebChannelDebug} channelDebug A
 *     WebChannelDebug to use for logging.
 * @param {string=} opt_sessionId The session id for the channel.
 * @param {string|number=} opt_requestId The request id for this request.
 * @param {number=} opt_retryId The retry id for this request.
 * @constructor
 * @struct
 * @final
 */
goog.labs.net.webChannel.ChannelRequest = function(
    channel, channelDebug, opt_sessionId, opt_requestId, opt_retryId) {
  'use strict';
  /**
   * The channel object that owns the request.
   * @private {goog.labs.net.webChannel.Channel}
   */
  this.channel_ = channel;

  /**
   * The channel debug to use for logging
   * @private {goog.labs.net.webChannel.WebChannelDebug}
   */
  this.channelDebug_ = channelDebug;

  /**
   * The Session ID for the channel.
   * @private {string|undefined}
   */
  this.sid_ = opt_sessionId;

  /**
   * The RID (request ID) for the request.
   * @private {string|number|undefined}
   */
  this.rid_ = opt_requestId;

  /**
   * The attempt number of the current request.
   * @private {number}
   */
  this.retryId_ = opt_retryId || 1;

  /**
   * An object to keep track of the channel request event listeners.
   * @private {!goog.events.EventHandler<
   *     !goog.labs.net.webChannel.ChannelRequest>}
   */
  this.eventHandler_ = new goog.events.EventHandler(this);

  /**
   * The timeout in ms before failing the request.
   * @private {number}
   */
  this.timeout_ = goog.labs.net.webChannel.ChannelRequest.TIMEOUT_MS_;

  /**
   * A timer for polling responseText in browsers that don't fire
   * onreadystatechange during incremental loading of responseText.
   * @private {goog.Timer}
   */
  this.pollingTimer_ =
      new goog.Timer(goog.labs.net.webChannel.environment.getPollingInterval());

  /**
   * Extra HTTP headers to add to all the requests sent to the server.
   * @private {?Object}
   */
  this.extraHeaders_ = null;


  /**
   * Whether the request was successful. This is only set to true after the
   * request successfully completes.
   * @private {boolean}
   */
  this.successful_ = false;


  /**
   * The TimerID of the timer used to detect if the request has timed-out.
   * @type {?number}
   * @private
   */
  this.watchDogTimerId_ = null;

  /**
   * The time in the future when the request will timeout.
   * @private {?number}
   */
  this.watchDogTimeoutTime_ = null;

  /**
   * The time the request started.
   * @private {?number}
   */
  this.requestStartTime_ = null;

  /**
   * The type of request (XMLHTTP, IMG)
   * @private {?number}
   */
  this.type_ = null;

  /**
   * The base Uri for the request. The includes all the parameters except the
   * one that indicates the retry number.
   * @private {?goog.Uri}
   */
  this.baseUri_ = null;

  /**
   * The request Uri that was actually used for the most recent request attempt.
   * @private {?goog.Uri}
   */
  this.requestUri_ = null;

  /**
   * The post data, if the request is a post.
   * @private {?string}
   */
  this.postData_ = null;

  /**
   * An array of pending messages that we have either received a non-successful
   * response for, or no response at all, and which therefore may or may not
   * have been received by the server.
   * @private {!Array<goog.labs.net.webChannel.Wire.QueuedMap>}
   */
  this.pendingMessages_ = [];

  /**
   * The XhrLte request if the request is using XMLHTTP
   * @private {?goog.net.XhrIo}
   */
  this.xmlHttp_ = null;

  /**
   * The position of where the next unprocessed chunk starts in the response
   * text.
   * @private {number}
   */
  this.xmlHttpChunkStart_ = 0;

  /**
   * The verb (Get or Post) for the request.
   * @private {?string}
   */
  this.verb_ = null;

  /**
   * The last error if the request failed.
   * @private {?goog.labs.net.webChannel.ChannelRequest.Error}
   */
  this.lastError_ = null;

  /**
   * The response headers received along with the non-200 status.
   *
   * @private {!Object<string, string>|undefined}
   */
  this.errorResponseHeaders_ = undefined;

  /**
   * The last status code received.
   * @private {number}
   */
  this.lastStatusCode_ = -1;

  /**
   * Whether the request has been cancelled due to a call to cancel.
   * @private {boolean}
   */
  this.cancelled_ = false;

  /**
   * A throttle time in ms for readystatechange events for the backchannel.
   * Useful for throttling when ready state is INTERACTIVE (partial data).
   * If set to zero no throttle is used.
   *
   * See WebChannelBase.prototype.readyStateChangeThrottleMs_
   *
   * @private {number}
   */
  this.readyStateChangeThrottleMs_ = 0;

  /**
   * The throttle for readystatechange events for the current request, or null
   * if there is none.
   * @private {?goog.async.Throttle}
   */
  this.readyStateChangeThrottle_ = null;

  /**
   * Whether to the result is expected to be encoded for chunking and thus
   * requires decoding.
   * @private {boolean}
   */
  this.decodeChunks_ = false;

  /**
   * Whether to decode x-http-initial-response.
   * @private {boolean}
   */
  this.decodeInitialResponse_ = false;

  /**
   * Whether x-http-initial-response has been decoded (dispatched).
   * @private {boolean}
   */
  this.initialResponseDecoded_ = false;

  /**
   * Whether the first byte of response body has arrived, for a successful
   * response.
   * @private {boolean}
   */
  this.firstByteReceived_ = false;

  /**
   * The current state of fetch responses if webchannel is using WHATWG
   * fetch/streams.
   * @private {!goog.labs.net.webChannel.FetchResponseState}
   */
  this.fetchResponseState_ = new goog.labs.net.webChannel.FetchResponseState();
};

/**
 * A collection of fetch/stream properties.
 * @struct
 * @constructor
 */
goog.labs.net.webChannel.FetchResponseState = function() {
  'use strict';
  /**
   * The TextDecoder for decoding Uint8Array responses from fetch request.
   * @type {?goog.global.TextDecoder}
   */
  this.textDecoder = null;

  /**
   * The unconsumed response text from the fetch requests.
   * @type {string}
   */
  this.responseBuffer = '';

  /**
   * Whether or not the response body has arrived.
   * @type {boolean}
   */
  this.responseArrivedForFetch = false;
};


goog.scope(function() {
'use strict';
const WebChannel = goog.net.WebChannel;
const Channel = goog.labs.net.webChannel.Channel;
const ChannelRequest = goog.labs.net.webChannel.ChannelRequest;
const FetchResponseState = goog.labs.net.webChannel.FetchResponseState;
const requestStats = goog.labs.net.webChannel.requestStats;
const WebChannelDebug = goog.labs.net.webChannel.WebChannelDebug;
const environment = goog.labs.net.webChannel.environment;

/**
 * Default timeout in MS for a request. The server must return data within this
 * time limit for the request to not timeout.
 * @private {number}
 */
ChannelRequest.TIMEOUT_MS_ = 45 * 1000;


/**
 * Enum for channel requests type
 * @enum {number}
 * @private
 */
ChannelRequest.Type_ = {
  /**
   * XMLHTTP requests.
   */
  XML_HTTP: 1,

  /**
   * IMG requests.
   */
  CLOSE_REQUEST: 2
};


/**
 * Enum type for identifying an error.
 * @enum {number}
 */
ChannelRequest.Error = {
  /**
   * Errors due to a non-200 status code.
   */
  STATUS: 0,

  /**
   * Errors due to no data being returned.
   */
  NO_DATA: 1,

  /**
   * Errors due to a timeout.
   */
  TIMEOUT: 2,

  /**
   * Errors due to the server returning an unknown.
   */
  UNKNOWN_SESSION_ID: 3,

  /**
   * Errors due to bad data being received.
   */
  BAD_DATA: 4,

  /**
   * Errors due to the handler throwing an exception.
   */
  HANDLER_EXCEPTION: 5,

  /**
   * The browser declared itself offline during the request.
   */
  BROWSER_OFFLINE: 6
};


/**
 * Returns a useful error string for debugging based on the specified error
 * code.
 * @param {?ChannelRequest.Error} errorCode The error code.
 * @param {number} statusCode The HTTP status code.
 * @return {string} The error string for the given code combination.
 */
ChannelRequest.errorStringFromCode = function(errorCode, statusCode) {
  'use strict';
  switch (errorCode) {
    case ChannelRequest.Error.STATUS:
      return 'Non-200 return code (' + statusCode + ')';
    case ChannelRequest.Error.NO_DATA:
      return 'XMLHTTP failure (no data)';
    case ChannelRequest.Error.TIMEOUT:
      return 'HttpConnection timeout';
    default:
      return 'Unknown error';
  }
};


/**
 * Sentinel value used to indicate an invalid chunk in a multi-chunk response.
 * @private {!Object}
 */
ChannelRequest.INVALID_CHUNK_ = {};


/**
 * Sentinel value used to indicate an incomplete chunk in a multi-chunk
 * response.
 * @private {!Object}
 */
ChannelRequest.INCOMPLETE_CHUNK_ = {};


/**
 * Returns whether XHR streaming is supported on this browser.
 *
 * @return {boolean} Whether XHR streaming is supported.
 * @see http://code.google.com/p/closure-library/issues/detail?id=346
 */
ChannelRequest.supportsXhrStreaming = function() {
  'use strict';
  return !goog.userAgent.IE || goog.userAgent.isDocumentModeOrHigher(10);
};


/**
 * Sets extra HTTP headers to add to all the requests sent to the server.
 *
 * @param {Object} extraHeaders The HTTP headers.
 */
ChannelRequest.prototype.setExtraHeaders = function(extraHeaders) {
  'use strict';
  this.extraHeaders_ = extraHeaders;
};


/**
 * Overrides the default HTTP method.
 *
 * @param {string} verb The HTTP method
 */
ChannelRequest.prototype.setVerb = function(verb) {
  'use strict';
  this.verb_ = verb;
};


/**
 * Sets the timeout for a request
 *
 * @param {number} timeout   The timeout in MS for when we fail the request.
 */
ChannelRequest.prototype.setTimeout = function(timeout) {
  'use strict';
  this.timeout_ = timeout;
};


/**
 * Sets the throttle for handling onreadystatechange events for the request.
 *
 * @param {number} throttle The throttle in ms.  A value of zero indicates
 *     no throttle.
 */
ChannelRequest.prototype.setReadyStateChangeThrottle = function(throttle) {
  'use strict';
  this.readyStateChangeThrottleMs_ = throttle;
};


/**
 * Sets the pending messages that this request is handling.
 *
 * @param {!Array<goog.labs.net.webChannel.Wire.QueuedMap>} pendingMessages
 *     The pending messages for this request.
 */
ChannelRequest.prototype.setPendingMessages = function(pendingMessages) {
  'use strict';
  this.pendingMessages_ = pendingMessages;
};


/**
 * Gets the pending messages that this request is handling, in case of a retry.
 *
 * @return {!Array<goog.labs.net.webChannel.Wire.QueuedMap>} The pending
 *     messages for this request.
 */
ChannelRequest.prototype.getPendingMessages = function() {
  'use strict';
  return this.pendingMessages_;
};


/**
 * Uses XMLHTTP to send an HTTP POST to the server.
 *
 * @param {goog.Uri} uri  The uri of the request.
 * @param {?string} postData  The data for the post body.
 * @param {boolean} decodeChunks  Whether to the result is expected to be
 *     encoded for chunking and thus requires decoding.
 */
ChannelRequest.prototype.xmlHttpPost = function(uri, postData, decodeChunks) {
  'use strict';
  this.type_ = ChannelRequest.Type_.XML_HTTP;
  this.baseUri_ = uri.clone().makeUnique();
  this.postData_ = postData;
  this.decodeChunks_ = decodeChunks;
  this.sendXmlHttp_(null /* hostPrefix */);
};


/**
 * Uses XMLHTTP to send an HTTP GET to the server.
 *
 * @param {goog.Uri} uri  The uri of the request.
 * @param {boolean} decodeChunks  Whether to the result is expected to be
 *     encoded for chunking and thus requires decoding.
 * @param {?string} hostPrefix  The host prefix, if we might be using a
 *     secondary domain.  Note that it should also be in the URL, adding this
 *     won't cause it to be added to the URL.
 */
ChannelRequest.prototype.xmlHttpGet = function(uri, decodeChunks, hostPrefix) {
  'use strict';
  this.type_ = ChannelRequest.Type_.XML_HTTP;
  this.baseUri_ = uri.clone().makeUnique();
  this.postData_ = null;
  this.decodeChunks_ = decodeChunks;

  this.sendXmlHttp_(hostPrefix);
};


/**
 * Sends a request via XMLHTTP according to the current state of the request
 * object.
 *
 * @param {?string} hostPrefix The host prefix, if we might be using a secondary
 *     domain.
 * @private
 */
ChannelRequest.prototype.sendXmlHttp_ = function(hostPrefix) {
  'use strict';
  this.requestStartTime_ = Date.now();
  this.ensureWatchDogTimer_();

  // clone the base URI to create the request URI. The request uri has the
  // attempt number as a parameter which helps in debugging.
  this.requestUri_ = this.baseUri_.clone();
  this.requestUri_.setParameterValues('t', this.retryId_);

  // send the request either as a POST or GET
  this.xmlHttpChunkStart_ = 0;
  const useSecondaryDomains = this.channel_.shouldUseSecondaryDomains();
  this.fetchResponseState_ = new FetchResponseState();
  // If the request is a GET request, start a backchannel to transfer streaming
  // data. Note that WebChannel GET request can also be used for closing the
  // channel as in method ChannelRequest#sendCloseRequest.
  // The second parameter of Channel#createXhrIo is JS only.
  this.xmlHttp_ = this.channel_.createXhrIo(
      useSecondaryDomains ? hostPrefix : null, !this.postData_);

  if (this.readyStateChangeThrottleMs_ > 0) {
    this.readyStateChangeThrottle_ = new goog.async.Throttle(
        goog.bind(this.xmlHttpHandler_, this, this.xmlHttp_),
        this.readyStateChangeThrottleMs_);
  }

  this.eventHandler_.listen(
      this.xmlHttp_, goog.net.EventType.READY_STATE_CHANGE,
      this.readyStateChangeHandler_);

  const headers =
      this.extraHeaders_ ? goog.object.clone(this.extraHeaders_) : {};
  if (this.postData_) {
    if (!this.verb_) {
      this.verb_ = 'POST';
    }
    headers['Content-Type'] = 'application/x-www-form-urlencoded';
    this.xmlHttp_.send(this.requestUri_, this.verb_, this.postData_, headers);
  } else {
    this.verb_ = 'GET';
    this.xmlHttp_.send(this.requestUri_, this.verb_, null, headers);
  }
  requestStats.notifyServerReachabilityEvent(
      requestStats.ServerReachability.REQUEST_MADE);
  this.channelDebug_.xmlHttpChannelRequest(
      this.verb_, this.requestUri_, this.rid_, this.retryId_, this.postData_);
};


/**
 * Handles a readystatechange event.
 * @param {goog.events.Event} evt The event.
 * @private
 */
ChannelRequest.prototype.readyStateChangeHandler_ = function(evt) {
  'use strict';
  const xhr = /** @type {goog.net.XhrIo} */ (evt.target);
  const throttle = this.readyStateChangeThrottle_;
  if (throttle &&
      xhr.getReadyState() == goog.net.XmlHttp.ReadyState.INTERACTIVE) {
    // Only throttle in the partial data case.
    this.channelDebug_.debug('Throttling readystatechange.');
    throttle.fire();
  } else {
    // If we haven't throttled, just handle response directly.
    this.xmlHttpHandler_(xhr);
  }
};


/**
 * XmlHttp handler
 * @param {goog.net.XhrIo} xmlhttp The XhrIo object for the current request.
 * @private
 */
ChannelRequest.prototype.xmlHttpHandler_ = function(xmlhttp) {
  'use strict';
  requestStats.onStartExecution();

  try {
    if (xmlhttp == this.xmlHttp_) {
      this.onXmlHttpReadyStateChanged_();
    } else {
      this.channelDebug_.warning(
          'Called back with an ' +
          'unexpected xmlhttp');
    }
  } catch (ex) {
    this.channelDebug_.debug('Failed call to OnXmlHttpReadyStateChanged_');
    if (this.hasResponseBody_()) {
      const channelRequest = this;
      this.channelDebug_.dumpException(ex, function() {
        'use strict';
        return 'ResponseText: ' + channelRequest.xmlHttp_.getResponseText();
      });
    } else {
      this.channelDebug_.dumpException(ex, 'No response text');
    }
  } finally {
    requestStats.onEndExecution();
  }
};


/**
 * Called by the readystate handler for XMLHTTP requests.
 *
 * @private
 */
ChannelRequest.prototype.onXmlHttpReadyStateChanged_ = function() {
  'use strict';
  const readyState = this.xmlHttp_.getReadyState();
  const errorCode = this.xmlHttp_.getLastErrorCode();
  const statusCode = this.xmlHttp_.getStatus();

  // we get partial results in browsers that support ready state interactive.
  // We also make sure that getResponseText is not null in interactive mode
  // before we continue.
  if (readyState < goog.net.XmlHttp.ReadyState.INTERACTIVE ||
      (readyState == goog.net.XmlHttp.ReadyState.INTERACTIVE &&
       !environment.isPollingRequired() &&  // otherwise, go on to startPolling
       !this.hasResponseBody_())) {
    return;  // not yet ready
  }

  // Dispatch any appropriate network events.
  if (!this.cancelled_ && readyState == goog.net.XmlHttp.ReadyState.COMPLETE &&
      errorCode != goog.net.ErrorCode.ABORT) {
    // Pretty conservative, these are the only known scenarios which we'd
    // consider indicative of a truly non-functional network connection.
    if (errorCode == goog.net.ErrorCode.TIMEOUT || statusCode <= 0) {
      requestStats.notifyServerReachabilityEvent(
          requestStats.ServerReachability.REQUEST_FAILED);
    } else {
      requestStats.notifyServerReachabilityEvent(
          requestStats.ServerReachability.REQUEST_SUCCEEDED);
    }
  }

  // got some data so cancel the watchdog timer
  this.cancelWatchDogTimer_();

  const status = this.xmlHttp_.getStatus();
  this.lastStatusCode_ = status;
  const responseText = this.decodeXmlHttpResponse_();

  if (!this.hasResponseBody_()) {
    const channelRequest = this;
    this.channelDebug_.debug(function() {
      'use strict';
      return 'No response text for uri ' + channelRequest.requestUri_ +
          ' status ' + status;
    });
  }
  this.successful_ = (status == 200);

  this.channelDebug_.xmlHttpChannelResponseMetaData(
      /** @type {string} */ (this.verb_), this.requestUri_, this.rid_,
      this.retryId_, readyState, status);

  if (!this.successful_) {
    this.errorResponseHeaders_ = this.xmlHttp_.getResponseHeaders();
    if (status == 400 && responseText.indexOf('Unknown SID') > 0) {
      // the server error string will include 'Unknown SID' which indicates the
      // server doesn't know about the session (maybe it got restarted, maybe
      // the user got moved to another server, etc.,). Handlers can special
      // case this error
      this.lastError_ = ChannelRequest.Error.UNKNOWN_SESSION_ID;
      requestStats.notifyStatEvent(
          requestStats.Stat.REQUEST_UNKNOWN_SESSION_ID);
      this.channelDebug_.warning('XMLHTTP Unknown SID (' + this.rid_ + ')');
    } else {
      this.lastError_ = ChannelRequest.Error.STATUS;
      requestStats.notifyStatEvent(requestStats.Stat.REQUEST_BAD_STATUS);
      this.channelDebug_.warning(
          'XMLHTTP Bad status ' + status + ' (' + this.rid_ + ')');
    }
    this.cleanup_();
    this.dispatchFailure_();
    return;
  }

  if (this.shouldCheckInitialResponse_()) {
    const initialResponse = this.getInitialResponse_();
    if (initialResponse) {
      this.channelDebug_.xmlHttpChannelResponseText(
          this.rid_, initialResponse,
          'Initial handshake response via ' +
              WebChannel.X_HTTP_INITIAL_RESPONSE);
      this.initialResponseDecoded_ = true;
      this.safeOnRequestData_(initialResponse);
    } else {
      this.successful_ = false;
      this.lastError_ = ChannelRequest.Error.UNKNOWN_SESSION_ID;  // fail-fast
      requestStats.notifyStatEvent(
          requestStats.Stat.REQUEST_UNKNOWN_SESSION_ID);
      this.channelDebug_.warning(
          'XMLHTTP Missing X_HTTP_INITIAL_RESPONSE' +
          ' (' + this.rid_ + ')');
      this.cleanup_();
      this.dispatchFailure_();
      return;
    }
  }

  if (this.decodeChunks_) {
    this.decodeNextChunks_(readyState, responseText);
    if (environment.isPollingRequired() && this.successful_ &&
        readyState == goog.net.XmlHttp.ReadyState.INTERACTIVE) {
      this.startPolling_();
    }
  } else {
    this.channelDebug_.xmlHttpChannelResponseText(
        this.rid_, responseText, null);
    this.safeOnRequestData_(responseText);
  }

  if (readyState == goog.net.XmlHttp.ReadyState.COMPLETE) {
    this.cleanup_();
  }

  if (!this.successful_) {
    return;
  }

  if (!this.cancelled_) {
    if (readyState == goog.net.XmlHttp.ReadyState.COMPLETE) {
      this.channel_.onRequestComplete(this);
    } else {
      // The default is false, the result from this callback shouldn't carry
      // over to the next callback, otherwise the request looks successful if
      // the watchdog timer gets called
      this.successful_ = false;
      this.ensureWatchDogTimer_();
    }
  }
};


/**
 * Whether we need check the initial-response header that is sent during the
 * fast handshake.
 *
 * @return {boolean} true if the initial-response header is yet to be processed.
 * @private
 */
ChannelRequest.prototype.shouldCheckInitialResponse_ = function() {
  'use strict';
  return this.decodeInitialResponse_ && !this.initialResponseDecoded_;
};


/**
 * Queries the initial response header that is sent during the handshake.
 *
 * @return {?string} The non-empty header value or null.
 * @private
 */
ChannelRequest.prototype.getInitialResponse_ = function() {
  'use strict';
  if (this.xmlHttp_) {
    const value = this.xmlHttp_.getStreamingResponseHeader(
        WebChannel.X_HTTP_INITIAL_RESPONSE);
    if (value && !goog.string.isEmptyOrWhitespace(value)) {
      return value;
    }
  }

  return null;
};


/**
 * Check if the initial response header has been handled.
 *
 * @return {boolean} true if X_HTTP_INITIAL_RESPONSE has been handled.
 */
ChannelRequest.prototype.isInitialResponseDecoded = function() {
  'use strict';
  return this.initialResponseDecoded_;
};


/**
 * Decodes X_HTTP_INITIAL_RESPONSE if present.
 */
ChannelRequest.prototype.setDecodeInitialResponse = function() {
  'use strict';
  this.decodeInitialResponse_ = true;
};



/**
 * Decodes the responses from XhrIo object.
 * @returns {string} responseText
 * @private
 */
ChannelRequest.prototype.decodeXmlHttpResponse_ = function() {
  'use strict';
  if (!this.useFetchStreamsForResponse_()) {
    return this.xmlHttp_.getResponseText();
  }
  const responseChunks =
      /** @type {!Array<!Uint8Array>} */ (this.xmlHttp_.getResponse());
  let responseText = '';
  const responseLength = responseChunks.length;
  const requestCompleted =
      this.xmlHttp_.getReadyState() == goog.net.XmlHttp.ReadyState.COMPLETE;
  if (!this.fetchResponseState_.textDecoder) {
    if (typeof TextDecoder === 'undefined') {
      this.channelDebug_.severe(
          'TextDecoder is not supported by this browser.');
      this.cleanup_();
      this.dispatchFailure_();
      return '';
    }
    this.fetchResponseState_.textDecoder = new goog.global.TextDecoder();
  }
  for (let i = 0; i < responseLength; i++) {
    this.fetchResponseState_.responseArrivedForFetch = true;
    const isLastChunk = requestCompleted && i == responseLength - 1;
    responseText += this.fetchResponseState_.textDecoder.decode(
        responseChunks[i], {stream: isLastChunk});
  }
  responseChunks.splice(0, responseLength);
  this.fetchResponseState_.responseBuffer += responseText;
  this.xmlHttpChunkStart_ = 0;
  return this.fetchResponseState_.responseBuffer;
};


/**
 * Whether or not the response has response body.
 * @private
 * @returns {boolean}
 */
ChannelRequest.prototype.hasResponseBody_ = function() {
  'use strict';
  if (!this.xmlHttp_) {
    return false;
  }
  if (this.fetchResponseState_.responseArrivedForFetch) {
    return true;
  }
  return !(!this.xmlHttp_.getResponseText() && !this.xmlHttp_.getResponse());
};

/**
 * Whether or not the response body is streamed.
 * @private
 * @returns {boolean}
 */
ChannelRequest.prototype.useFetchStreamsForResponse_ = function() {
  'use strict';
  if (!this.xmlHttp_) {
    return false;
  }
  return (
      this.verb_ == 'GET' && this.type_ != ChannelRequest.Type_.CLOSE_REQUEST &&
      this.channel_.usesFetchStreams());
};


/**
 * Resets the response buffer if the saved chunk has been processed.
 * @private
 * @param {string|!Object|undefined} chunkText
 */
ChannelRequest.prototype.maybeResetBuffer_ = function(chunkText) {
  'use strict';
  if (this.useFetchStreamsForResponse_() &&
      chunkText != ChannelRequest.INCOMPLETE_CHUNK_ &&
      chunkText != ChannelRequest.INVALID_CHUNK_) {
    this.fetchResponseState_.responseBuffer = '';
    this.xmlHttpChunkStart_ = 0;
  }
};


/**
 * Decodes the next set of available chunks in the response.
 * @param {number} readyState The value of readyState.
 * @param {string} responseText The value of responseText.
 * @private
 */
ChannelRequest.prototype.decodeNextChunks_ = function(
    readyState, responseText) {
  'use strict';
  let decodeNextChunksSuccessful = true;

  let chunkText;
  while (!this.cancelled_ && this.xmlHttpChunkStart_ < responseText.length) {
    chunkText = this.getNextChunk_(responseText);
    if (chunkText == ChannelRequest.INCOMPLETE_CHUNK_) {
      if (readyState == goog.net.XmlHttp.ReadyState.COMPLETE) {
        // should have consumed entire response when the request is done
        this.lastError_ = ChannelRequest.Error.BAD_DATA;
        requestStats.notifyStatEvent(requestStats.Stat.REQUEST_INCOMPLETE_DATA);
        decodeNextChunksSuccessful = false;
      }
      this.channelDebug_.xmlHttpChannelResponseText(
          this.rid_, null, '[Incomplete Response]');
      break;
    } else if (chunkText == ChannelRequest.INVALID_CHUNK_) {
      this.lastError_ = ChannelRequest.Error.BAD_DATA;
      requestStats.notifyStatEvent(requestStats.Stat.REQUEST_BAD_DATA);
      this.channelDebug_.xmlHttpChannelResponseText(
          this.rid_, responseText, '[Invalid Chunk]');
      decodeNextChunksSuccessful = false;
      break;
    } else {
      this.channelDebug_.xmlHttpChannelResponseText(
          this.rid_, /** @type {string} */ (chunkText), null);
      this.safeOnRequestData_(/** @type {string} */ (chunkText));
    }
  }

  this.maybeResetBuffer_(chunkText);

  if (readyState == goog.net.XmlHttp.ReadyState.COMPLETE &&
      responseText.length == 0 &&
      !this.fetchResponseState_.responseArrivedForFetch) {
    // also an error if we didn't get any response
    this.lastError_ = ChannelRequest.Error.NO_DATA;
    requestStats.notifyStatEvent(requestStats.Stat.REQUEST_NO_DATA);
    decodeNextChunksSuccessful = false;
  }

  this.successful_ = this.successful_ && decodeNextChunksSuccessful;

  if (!decodeNextChunksSuccessful) {
    // malformed response - we make this trigger retry logic
    this.channelDebug_.xmlHttpChannelResponseText(
        this.rid_, responseText, '[Invalid Chunked Response]');
    this.cleanup_();
    this.dispatchFailure_();
  } else {
    if (responseText.length > 0 && !this.firstByteReceived_) {
      this.firstByteReceived_ = true;
      this.channel_.onFirstByteReceived(this, responseText);
    }
  }
};


/**
 * Polls the response for new data.
 * @private
 */
ChannelRequest.prototype.pollResponse_ = function() {
  'use strict';
  if (!this.xmlHttp_) {
    return;  // already closed
  }
  const readyState = this.xmlHttp_.getReadyState();
  const responseText = this.xmlHttp_.getResponseText();
  if (this.xmlHttpChunkStart_ < responseText.length) {
    this.cancelWatchDogTimer_();
    this.decodeNextChunks_(readyState, responseText);
    if (this.successful_ &&
        readyState != goog.net.XmlHttp.ReadyState.COMPLETE) {
      this.ensureWatchDogTimer_();
    }
  }
};


/**
 * Starts a polling interval for changes to responseText of the
 * XMLHttpRequest, for browsers that don't fire onreadystatechange
 * as data comes in incrementally.  This timer is disabled in
 * cleanup_().
 * @private
 */
ChannelRequest.prototype.startPolling_ = function() {
  'use strict';
  this.eventHandler_.listen(
      this.pollingTimer_, goog.Timer.TICK, this.pollResponse_);
  this.pollingTimer_.start();
};


/**
 * Returns the next chunk of a chunk-encoded response. This is not standard
 * HTTP chunked encoding because browsers don't expose the chunk boundaries to
 * the application through XMLHTTP. So we have an additional chunk encoding at
 * the application level that lets us tell where the beginning and end of
 * individual responses are so that we can only try to eval a complete JS array.
 *
 * The encoding is the size of the chunk encoded as a decimal string followed
 * by a newline followed by the data.
 *
 * @param {string} responseText The response text from the XMLHTTP response.
 * @return {string|!Object} The next chunk string or a sentinel object
 *                         indicating a special condition.
 * @private
 */
ChannelRequest.prototype.getNextChunk_ = function(responseText) {
  'use strict';
  const sizeStartIndex = this.xmlHttpChunkStart_;
  const sizeEndIndex = responseText.indexOf('\n', sizeStartIndex);
  if (sizeEndIndex == -1) {
    return ChannelRequest.INCOMPLETE_CHUNK_;
  }

  const sizeAsString = responseText.substring(sizeStartIndex, sizeEndIndex);
  const size = Number(sizeAsString);
  if (isNaN(size)) {
    return ChannelRequest.INVALID_CHUNK_;
  }

  const chunkStartIndex = sizeEndIndex + 1;
  if (chunkStartIndex + size > responseText.length) {
    return ChannelRequest.INCOMPLETE_CHUNK_;
  }

  const chunkText = responseText.slice(chunkStartIndex, chunkStartIndex + size);
  this.xmlHttpChunkStart_ = chunkStartIndex + size;
  return chunkText;
};


/**
 * Uses an IMG tag or navigator.sendBeacon to send an HTTP get to the server.
 *
 * This is only currently used to terminate the connection, as an IMG tag is
 * the most reliable way to send something to the server while the page
 * is getting torn down.
 *
 * Navigator.sendBeacon is available on Chrome and Firefox as a formal
 * solution to ensure delivery without blocking window close. See
 * https://developer.mozilla.org/en-US/docs/Web/API/Navigator/sendBeacon
 *
 * For Chrome Apps, sendBeacon is always necessary due to Content Security
 * Policy (CSP) violation of using an IMG tag.
 *
 * For react-native, we use xhr to send the actual close request, and assume
 * there is no page-close issue with react-native.
 *
 * @param {goog.Uri} uri The uri to send a request to.
 */
ChannelRequest.prototype.sendCloseRequest = function(uri) {
  'use strict';
  this.type_ = ChannelRequest.Type_.CLOSE_REQUEST;
  this.baseUri_ = uri.clone().makeUnique();

  let requestSent = false;

  if (goog.global.navigator && goog.global.navigator.sendBeacon) {
    try {
      // empty string body to avoid 413 error on chrome < 41
      requestSent =
          goog.global.navigator.sendBeacon(this.baseUri_.toString(), '');
    } catch {
      // Intentionally left empty; sendBeacon might throw TypeError in certain
      // unexpected cases.
    }
  }

  if (!requestSent && goog.global.Image) {
    const eltImg = new Image();
    eltImg.src = this.baseUri_;
    requestSent = true;
  }

  if (!requestSent) {
    // no handler is set to match the sendBeacon/Image behavior
    this.xmlHttp_ = this.channel_.createXhrIo(null);
    this.xmlHttp_.send(this.baseUri_);
  }

  this.requestStartTime_ = Date.now();
  this.ensureWatchDogTimer_();
};


/**
 * Cancels the request no matter what the underlying transport is.
 */
ChannelRequest.prototype.cancel = function() {
  'use strict';
  this.cancelled_ = true;
  this.cleanup_();
};


/**
 * Resets the timeout.
 *
 * @param {number=} opt_timeout The new timeout
 */
ChannelRequest.prototype.resetTimeout = function(opt_timeout) {
  'use strict';
  if (opt_timeout) {
    this.setTimeout(opt_timeout);
  }
  // restart only if a timer is currently set
  if (this.watchDogTimerId_) {
    this.cancelWatchDogTimer_();
    this.ensureWatchDogTimer_();
  }
};


/**
 * Ensures that there is watchdog timeout which is used to ensure that
 * the connection completes in time.
 *
 * @private
 */
ChannelRequest.prototype.ensureWatchDogTimer_ = function() {
  'use strict';
  this.watchDogTimeoutTime_ = Date.now() + this.timeout_;
  this.startWatchDogTimer_(this.timeout_);
};


/**
 * Starts the watchdog timer which is used to ensure that the connection
 * completes in time.
 * @param {number} time The number of milliseconds to wait.
 * @private
 */
ChannelRequest.prototype.startWatchDogTimer_ = function(time) {
  'use strict';
  if (this.watchDogTimerId_ != null) {
    // assertion
    throw new Error('WatchDog timer not null');
  }
  this.watchDogTimerId_ =
      requestStats.setTimeout(goog.bind(this.onWatchDogTimeout_, this), time);
};


/**
 * Cancels the watchdog timer if it has been started.
 *
 * @private
 */
ChannelRequest.prototype.cancelWatchDogTimer_ = function() {
  'use strict';
  if (this.watchDogTimerId_) {
    goog.global.clearTimeout(this.watchDogTimerId_);
    this.watchDogTimerId_ = null;
  }
};


/**
 * Called when the watchdog timer is triggered. It also handles a case where it
 * is called too early which we suspect may be happening sometimes
 * (not sure why)
 *
 * @private
 */
ChannelRequest.prototype.onWatchDogTimeout_ = function() {
  'use strict';
  this.watchDogTimerId_ = null;
  const now = Date.now();
  goog.asserts.assert(
      this.watchDogTimeoutTime_, 'WatchDog timeout time missing?');
  if (now - this.watchDogTimeoutTime_ >= 0) {
    this.handleTimeout_();
  } else {
    // got called too early for some reason
    this.channelDebug_.warning('WatchDog timer called too early');
    this.startWatchDogTimer_(this.watchDogTimeoutTime_ - now);
  }
};


/**
 * Called when the request has actually timed out. Will cleanup and notify the
 * channel of the failure.
 *
 * @private
 */
ChannelRequest.prototype.handleTimeout_ = function() {
  'use strict';
  if (this.successful_) {
    // Should never happen.
    this.channelDebug_.severe(
        'Received watchdog timeout even though request loaded successfully');
  }

  this.channelDebug_.timeoutResponse(this.requestUri_);

  // IMG or SendBeacon requests never notice if they were successful,
  // and always 'time out'. This fact says nothing about reachability.
  if (this.type_ != ChannelRequest.Type_.CLOSE_REQUEST) {
    requestStats.notifyServerReachabilityEvent(
        requestStats.ServerReachability.REQUEST_FAILED);
    requestStats.notifyStatEvent(requestStats.Stat.REQUEST_TIMEOUT);
  }

  this.cleanup_();

  // Set error and dispatch failure.
  // This is called for CLOSE_REQUEST too to ensure channel_.onRequestComplete.
  this.lastError_ = ChannelRequest.Error.TIMEOUT;
  this.dispatchFailure_();
};


/**
 * Notifies the channel that this request failed.
 * @private
 */
ChannelRequest.prototype.dispatchFailure_ = function() {
  'use strict';
  if (this.channel_.isClosed() || this.cancelled_) {
    return;
  }

  this.channel_.onRequestComplete(this);
};


/**
 * Cleans up the objects used to make the request. This function is
 * idempotent.
 *
 * @private
 */
ChannelRequest.prototype.cleanup_ = function() {
  'use strict';
  this.cancelWatchDogTimer_();

  goog.dispose(this.readyStateChangeThrottle_);
  this.readyStateChangeThrottle_ = null;

  // Stop the polling timer, if necessary.
  this.pollingTimer_.stop();

  // Unhook all event handlers.
  this.eventHandler_.removeAll();

  if (this.xmlHttp_) {
    // clear out this.xmlHttp_ before aborting so we handle getting reentered
    // inside abort
    const xmlhttp = this.xmlHttp_;
    this.xmlHttp_ = null;
    xmlhttp.abort();
    xmlhttp.dispose();
  }
};


/**
 * Indicates whether the request was successful. Only valid after the handler
 * is called to indicate completion of the request.
 *
 * @return {boolean} True if the request succeeded.
 */
ChannelRequest.prototype.getSuccess = function() {
  'use strict';
  return this.successful_;
};


/**
 * If the request was not successful, returns the reason.
 *
 * @return {?ChannelRequest.Error}  The last error.
 */
ChannelRequest.prototype.getLastError = function() {
  'use strict';
  return this.lastError_;
};


/**
 * @return {!Object<string, string>|undefined} Response headers received
 *     along with the non-200 status, as a key-value map.
 */
ChannelRequest.prototype.getErrorResponseHeaders = function() {
  'use strict';
  return this.errorResponseHeaders_;
};


/**
 * Returns the status code of the last request.
 *
 * @return {number} The status code of the last request.
 */
ChannelRequest.prototype.getLastStatusCode = function() {
  'use strict';
  return this.lastStatusCode_;
};


/**
 * Returns the session id for this channel.
 *
 * @return {string|undefined} The session ID.
 */
ChannelRequest.prototype.getSessionId = function() {
  'use strict';
  return this.sid_;
};


/**
 * Returns the request id for this request. Each request has a unique request
 * id and the request IDs are a sequential increasing count.
 *
 * @return {string|number|undefined} The request ID.
 */
ChannelRequest.prototype.getRequestId = function() {
  'use strict';
  return this.rid_;
};


/**
 * Returns the data for a post, if this request is a post.
 *
 * @return {?string} The POST data provided by the request initiator.
 */
ChannelRequest.prototype.getPostData = function() {
  'use strict';
  return this.postData_;
};


/**
 * Returns the XhrIo request object.
 *
 * @return {?goog.net.XhrIo} Any XhrIo request created for this object.
 */
ChannelRequest.prototype.getXhr = function() {
  'use strict';
  return this.xmlHttp_;
};


/**
 * Returns the time that the request started, if it has started.
 *
 * @return {?number} The time the request started, as returned by Date.now().
 */
ChannelRequest.prototype.getRequestStartTime = function() {
  'use strict';
  return this.requestStartTime_;
};


/**
 * Helper to call the callback's onRequestData, which catches any
 * exception.
 * @param {string} data The request data.
 * @private
 */
ChannelRequest.prototype.safeOnRequestData_ = function(data) {
  'use strict';
  try {
    this.channel_.onRequestData(this, data);
    const stats = requestStats.ServerReachability;
    requestStats.notifyServerReachabilityEvent(stats.BACK_CHANNEL_ACTIVITY);
  } catch (e) {
    // Dump debug info, but keep going without closing the channel.
    this.channelDebug_.dumpException(e, 'Error in httprequest callback');
  }
};


/**
 * Convenience factory method.
 *
 * @param {Channel} channel The channel object that owns this request.
 * @param {WebChannelDebug} channelDebug A WebChannelDebug to use for logging.
 * @param {string=} opt_sessionId  The session id for the channel.
 * @param {string|number=} opt_requestId  The request id for this request.
 * @param {number=} opt_retryId  The retry id for this request.
 * @return {!ChannelRequest} The created channel request.
 */
ChannelRequest.createChannelRequest = function(
    channel, channelDebug, opt_sessionId, opt_requestId, opt_retryId) {
  'use strict';
  return new ChannelRequest(
      channel, channelDebug, opt_sessionId, opt_requestId, opt_retryId);
};
});  // goog.scope
