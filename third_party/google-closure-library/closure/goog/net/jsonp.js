/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

// The original file lives here: http://go/cross_domain_channel.js

/**
 * @fileoverview Implements a cross-domain communication channel. A
 * typical web page is prevented by browser security from sending
 * request, such as a XMLHttpRequest, to other servers than the ones
 * from which it came. The Jsonp class provides a workaround by
 * using dynamically generated script tags. Typical usage:.
 *
 * const trustedUri = goog.html.TrustedResourceUrl.fromConstant(
 *     goog.string.Const.from('https://example.com/servlet'));
 * const jsonp = new goog.net.Jsonp(trustedUri);
 * const payload = {'foo': 1, 'bar': true};
 * jsonp.send(payload, function(reply) { alert(reply) });
 *
 * This script works in all browsers that are currently supported by
 * the Google Maps API, which is IE 6.0+, Firefox 0.8+, Safari 1.2.4+,
 * Netscape 7.1+, Mozilla 1.4+, Opera 8.02+.
 */

goog.provide('goog.net.Jsonp');

goog.require('goog.functions');
goog.require('goog.html.TrustedResourceUrl');
goog.require('goog.net.jsloader');
goog.require('goog.object');

// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// This class allows us (Google) to send data from non-Google and thus
// UNTRUSTED pages to our servers. Under NO CIRCUMSTANCES return
// anything sensitive, such as session or cookie specific data. Return
// only data that you want parties external to Google to have. Also
// NEVER use this method to send data from web pages to untrusted
// servers, or redirects to unknown servers (www.google.com/cache,
// /q=xx&btnl, /url, www.googlepages.com, etc.)
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING



/**
 * Creates a new cross domain channel that sends data to the specified
 * host URL. By default, if no reply arrives within 5s, the channel
 * assumes the call failed to complete successfully.
 *
 * @param {!goog.html.TrustedResourceUrl} uri The Uri of the server side code
 *     that receives data posted through this channel (e.g.,
 *     "http://maps.google.com/maps/geo").
 *
 * @param {string=} opt_callbackParamName The parameter name that is used to
 *     specify the callback. Defaults to "callback".
 *
 * @constructor
 * @final
 */
goog.net.Jsonp = function(uri, opt_callbackParamName) {
  'use strict';
  /**
   * The uri_ object will be used to encode the payload that is sent to the
   * server.
   * @type {!goog.html.TrustedResourceUrl}
   * @private
   */
  this.uri_ = uri;

  /**
   * This is the callback parameter name that is added to the uri.
   * @type {string}
   * @private
   */
  this.callbackParamName_ =
      opt_callbackParamName ? opt_callbackParamName : 'callback';

  /**
   * The length of time, in milliseconds, this channel is prepared
   * to wait for for a request to complete. The default value is 5 seconds.
   * @type {number}
   * @private
   */
  this.timeout_ = 5000;

  /**
   * The nonce to use in the dynamically generated script tags. This is used for
   * allowing the script callbacks to execute when the page has an enforced
   * Content Security Policy.
   * @type {string}
   * @private
   */
  this.nonce_ = '';
};


/**
 * The prefix for the callback name which will be stored on goog.global.
 */
goog.net.Jsonp.CALLBACKS = '_callbacks_';


/**
 * Used to generate unique callback IDs. The counter must be global because
 * all channels share a common callback object.
 * @private
 */
goog.net.Jsonp.scriptCounter_ = 0;


/**
 * Static private method which returns the global unique callback id.
 *
 * @param {string} id The id of the script node.
 * @return {string} A global unique id used to store callback on goog.global
 *     object.
 * @private
 */
goog.net.Jsonp.getCallbackId_ = function(id) {
  'use strict';
  return goog.net.Jsonp.CALLBACKS + '__' + id;
};


/**
 * Sets the length of time, in milliseconds, this channel is prepared
 * to wait for for a request to complete. If the call is not competed
 * within the set time span, it is assumed to have failed. To wait
 * indefinitely for a request to complete set the timout to a negative
 * number.
 *
 * @param {number} timeout The length of time before calls are
 * interrupted.
 */
goog.net.Jsonp.prototype.setRequestTimeout = function(timeout) {
  'use strict';
  this.timeout_ = timeout;
};


/**
 * Returns the current timeout value, in milliseconds.
 *
 * @return {number} The timeout value.
 */
goog.net.Jsonp.prototype.getRequestTimeout = function() {
  'use strict';
  return this.timeout_;
};


/**
 * Sets the nonce value for CSP. This nonce value will be added to any created
 * script elements and must match the nonce provided in the
 * Content-Security-Policy header sent by the server for the callback to pass
 * CSP enforcement.
 *
 * @param {string} nonce The CSP nonce value.
 */
goog.net.Jsonp.prototype.setNonce = function(nonce) {
  'use strict';
  this.nonce_ = nonce;
};


/**
 * Sends the given payload to the URL specified at the construction
 * time. The reply is delivered to the given replyCallback. If the
 * errorCallback is specified and the reply does not arrive within the
 * timeout period set on this channel, the errorCallback is invoked
 * with the original payload.
 *
 * If no reply callback is specified, then the response is expected to
 * consist of calls to globally registered functions. No &callback=
 * URL parameter will be sent in the request, and the script element
 * will be cleaned up after the timeout.
 *
 * @param {Object=} opt_payload Name-value pairs.  If given, these will be
 *     added as parameters to the supplied URI as GET parameters to the
 *     given server URI.
 *
 * @param {Function=} opt_replyCallback A function expecting one
 *     argument, called when the reply arrives, with the response data.
 *
 * @param {Function=} opt_errorCallback A function expecting one
 *     argument, called on timeout, with the payload (if given), otherwise
 *     null.
 *
 * @param {string=} opt_callbackParamValue Value to be used as the
 *     parameter value for the callback parameter (callbackParamName).
 *     To be used when the value needs to be fixed by the client for a
 *     particular request, to make use of the cached responses for the request.
 *     NOTE: If multiple requests are made with the same
 *     opt_callbackParamValue, only the last call will work whenever the
 *     response comes back.
 *
 * @return {!Object} A request descriptor that may be used to cancel this
 *     transmission, or null, if the message may not be cancelled.
 */
goog.net.Jsonp.prototype.send = function(
    opt_payload, opt_replyCallback, opt_errorCallback, opt_callbackParamValue) {
  'use strict';
  const payload = opt_payload ? goog.object.clone(opt_payload) : {};

  const id = opt_callbackParamValue ||
      '_' + (goog.net.Jsonp.scriptCounter_++).toString(36) +
          Date.now().toString(36);
  const callbackId = goog.net.Jsonp.getCallbackId_(id);

  if (opt_replyCallback) {
    const reply = goog.net.Jsonp.newReplyHandler_(id, opt_replyCallback);
    // Register the callback on goog.global to make it discoverable
    // by jsonp response.
    goog.global[callbackId] = reply;
    payload[this.callbackParamName_] = callbackId;
  }

  const options = {timeout: this.timeout_, cleanupWhenDone: true};
  if (this.nonce_) {
    options.attributes = {'nonce': this.nonce_};
  }

  const uri = this.uri_.cloneWithParams(payload);

  const deferred = goog.net.jsloader.safeLoad(uri, options);
  const error = goog.net.Jsonp.newErrorHandler_(id, payload, opt_errorCallback);
  deferred.addErrback(error);

  return {id_: id, deferred_: deferred};
};


/**
 * Cancels a given request. The request must be exactly the object returned by
 * the send method.
 * @param {Object} request The request object returned by the send method.
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.net.Jsonp.prototype.cancel = function(request) {
  'use strict';
  if (request) {
    if (request.deferred_) {
      request.deferred_.cancel();
    }
    if (request.id_) {
      goog.net.Jsonp.cleanup_(request.id_, false);
    }
  }
};


/**
 * Creates a timeout callback that calls the given timeoutCallback with the
 * original payload.
 *
 * @param {string} id The id of the script node.
 * @param {Object} payload The payload that was sent to the server.
 * @param {Function=} opt_errorCallback The function called on timeout.
 * @return {!Function} A zero argument function that handles callback duties.
 * @private
 */
goog.net.Jsonp.newErrorHandler_ = function(id, payload, opt_errorCallback) {
  'use strict';
  /**
   * When we call across domains with a request, this function is the
   * timeout handler. Once it's done executing the user-specified
   * error-handler, it removes the script node and original function.
   */
  return function() {
    'use strict';
    goog.net.Jsonp.cleanup_(id, false);
    if (opt_errorCallback) {
      opt_errorCallback(payload);
    }
  };
};


/**
 * Creates a reply callback that calls the given replyCallback with data
 * returned by the server.
 *
 * @param {string} id The id of the script node.
 * @param {Function} replyCallback The function called on reply.
 * @return {!Function} A reply callback function.
 * @private
 */
goog.net.Jsonp.newReplyHandler_ = function(id, replyCallback) {
  'use strict';
  /**
   * This function is the handler for the all-is-well response. It
   * clears the error timeout handler, calls the user's handler, then
   * removes the script node and itself.
   *
   * @param {...Object} var_args The response data sent from the server.
   */
  const handler = function(var_args) {
    'use strict';
    goog.net.Jsonp.cleanup_(id, true);
    replyCallback.apply(undefined, arguments);
  };
  return handler;
};


/**
 * Removes the reply handler registered on goog.global object.
 *
 * @param {string} id The id of the script node to be removed.
 * @param {boolean} deleteReplyHandler If true, delete the reply handler
 *     instead of setting it to nullFunction (if we know the callback could
 *     never be called again).
 * @private
 */
goog.net.Jsonp.cleanup_ = function(id, deleteReplyHandler) {
  'use strict';
  const callbackId = goog.net.Jsonp.getCallbackId_(id);
  if (goog.global[callbackId]) {
    if (deleteReplyHandler) {
      try {
        delete goog.global[callbackId];
      } catch (e) {
        // NOTE: Workaround to delete property on 'window' in IE <= 8, see:
        // http://stackoverflow.com/questions/1073414/deleting-a-window-property-in-ie
        goog.global[callbackId] = undefined;
      }
    } else {
      // Removing the script tag doesn't necessarily prevent the script
      // from firing, so we make the callback a noop.
      goog.global[callbackId] = goog.functions.UNDEFINED;
    }
  }
};


// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// This class allows us (Google) to send data from non-Google and thus
// UNTRUSTED pages to our servers. Under NO CIRCUMSTANCES return
// anything sensitive, such as session or cookie specific data. Return
// only data that you want parties external to Google to have. Also
// NEVER use this method to send data from web pages to untrusted
// servers, or redirects to unknown servers (www.google.com/cache,
// /q=xx&btnl, /url, www.googlepages.com, etc.)
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
