/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the ChannelDebug class. ChannelDebug provides
 * a utility for tracing and debugging the BrowserChannel requests.
 */


/**
 * Namespace for BrowserChannel
 */
goog.provide('goog.net.ChannelDebug');

goog.require('goog.json');
goog.require('goog.log');
goog.require('goog.log.Logger');
goog.requireType('goog.Uri');
goog.requireType('goog.net.XmlHttp.ReadyState');



/**
 * Logs and keeps a buffer of debugging info for the Channel.
 *
 * @constructor
 */
goog.net.ChannelDebug = function() {
  'use strict';
  /**
   * The logger instance.
   * @const
   * @private {?goog.log.Logger}
   */
  this.logger_ = goog.log.getLogger('goog.net.BrowserChannel');
};


/**
 * Gets the logger used by this ChannelDebug.
 * @return {?goog.log.Logger} The logger used by this ChannelDebug.
 */
goog.net.ChannelDebug.prototype.getLogger = function() {
  'use strict';
  return this.logger_;
};


/**
 * Logs that the browser went offline during the lifetime of a request.
 * @param {goog.Uri} url The URL being requested.
 */
goog.net.ChannelDebug.prototype.browserOfflineResponse = function(url) {
  'use strict';
  this.info('BROWSER_OFFLINE: ' + url);
};


/**
 * Logs an XmlHttp request..
 * @param {string} verb The request type (GET/POST).
 * @param {goog.Uri} uri The request destination.
 * @param {string|number|undefined} id The request id.
 * @param {number} attempt Which attempt # the request was.
 * @param {?string} postData The data posted in the request.
 */
goog.net.ChannelDebug.prototype.xmlHttpChannelRequest = function(
    verb, uri, id, attempt, postData) {
  'use strict';
  this.info(
      'XMLHTTP REQ (' + id + ') [attempt ' + attempt + ']: ' + verb + '\n' +
      uri + '\n' + this.maybeRedactPostData_(postData));
};


/**
 * Logs the meta data received from an XmlHttp request.
 * @param {string} verb The request type (GET/POST).
 * @param {goog.Uri} uri The request destination.
 * @param {string|number|undefined} id The request id.
 * @param {number} attempt Which attempt # the request was.
 * @param {goog.net.XmlHttp.ReadyState} readyState The ready state.
 * @param {number} statusCode The HTTP status code.
 */
goog.net.ChannelDebug.prototype.xmlHttpChannelResponseMetaData = function(
    verb, uri, id, attempt, readyState, statusCode) {
  'use strict';
  this.info(
      'XMLHTTP RESP (' + id + ') [ attempt ' + attempt + ']: ' + verb + '\n' +
      uri + '\n' + readyState + ' ' + statusCode);
};


/**
 * Logs the response data received from an XmlHttp request.
 * @param {string|number|undefined} id The request id.
 * @param {?string} responseText The response text.
 * @param {?string=} opt_desc Optional request description.
 */
goog.net.ChannelDebug.prototype.xmlHttpChannelResponseText = function(
    id, responseText, opt_desc) {
  'use strict';
  this.info(
      'XMLHTTP TEXT (' + id + '): ' + this.redactResponse_(responseText) +
      (opt_desc ? ' ' + opt_desc : ''));
};


/**
 * Logs a Trident ActiveX request.
 * @param {string} verb The request type (GET/POST).
 * @param {goog.Uri} uri The request destination.
 * @param {string|number|undefined} id The request id.
 * @param {number} attempt Which attempt # the request was.
 */
goog.net.ChannelDebug.prototype.tridentChannelRequest = function(
    verb, uri, id, attempt) {
  'use strict';
  this.info(
      'TRIDENT REQ (' + id + ') [ attempt ' + attempt + ']: ' + verb + '\n' +
      uri);
};


/**
 * Logs the response text received from a Trident ActiveX request.
 * @param {string|number|undefined} id The request id.
 * @param {string} responseText The response text.
 */
goog.net.ChannelDebug.prototype.tridentChannelResponseText = function(
    id, responseText) {
  'use strict';
  this.info('TRIDENT TEXT (' + id + '): ' + this.redactResponse_(responseText));
};


/**
 * Logs the done response received from a Trident ActiveX request.
 * @param {string|number|undefined} id The request id.
 * @param {boolean} successful Whether the request was successful.
 */
goog.net.ChannelDebug.prototype.tridentChannelResponseDone = function(
    id, successful) {
  'use strict';
  this.info('TRIDENT TEXT (' + id + '): ' + successful ? 'success' : 'failure');
};


/**
 * Logs a request timeout.
 * @param {goog.Uri} uri The uri that timed out.
 */
goog.net.ChannelDebug.prototype.timeoutResponse = function(uri) {
  'use strict';
  this.info('TIMEOUT: ' + uri);
};


/**
 * Logs a debug message.
 * @param {string} text The message.
 */
goog.net.ChannelDebug.prototype.debug = function(text) {
  'use strict';
  this.info(text);
};


/**
 * Logs an exception
 * @param {!Error} e The error or error event.
 * @param {string=} msg The optional message, defaults to 'Exception'.
 */
goog.net.ChannelDebug.prototype.dumpException = function(e, msg = 'Exception') {
  this.severe(msg, e);
};


/**
 * Logs an info message.
 * @param {string} text The message.
 */
goog.net.ChannelDebug.prototype.info = function(text) {
  'use strict';
  goog.log.info(this.logger_, text);
};


/**
 * Logs a warning message.
 * @param {string} text The message.
 */
goog.net.ChannelDebug.prototype.warning = function(text) {
  'use strict';
  goog.log.warning(this.logger_, text);
};


/**
 * Logs a severe message.
 * @param {string} text The message.
 * @param {!Error=} error An exception associated with the message.
 */
goog.net.ChannelDebug.prototype.severe = function(text, error = undefined) {
  goog.log.error(this.logger_, text, error);
};


/**
 * Removes potentially private data from a response so that we don't
 * accidentally save private and personal data to the server logs.
 * @param {?string} responseText A JSON response to clean.
 * @return {?string} The cleaned response.
 * @private
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.net.ChannelDebug.prototype.redactResponse_ = function(responseText) {
  'use strict';
  // first check if it's not JS - the only non-JS should be the magic cookie
  if (!responseText ||
      responseText == goog.net.ChannelDebug.MAGIC_RESPONSE_COOKIE) {
    return responseText;
  }

  try {
    const responseArray = JSON.parse(responseText);
    if (responseArray) {
      for (let i = 0; i < responseArray.length; i++) {
        if (Array.isArray(responseArray[i])) {
          this.maybeRedactArray_(responseArray[i]);
        }
      }
    }

    return goog.json.serialize(responseArray);
  } catch (e) {
    this.debug('Exception parsing expected JS array - probably was not JS');
    return responseText;
  }
};


/**
 * Removes data from a response array that may be sensitive.
 * @param {Array<?>} array The array to clean.
 * @private
 */
goog.net.ChannelDebug.prototype.maybeRedactArray_ = function(array) {
  'use strict';
  if (array.length < 2) {
    return;
  }
  const dataPart = array[1];
  if (!Array.isArray(dataPart)) {
    return;
  }
  if (dataPart.length < 1) {
    return;
  }

  const type = dataPart[0];
  if (type != 'noop' && type != 'stop') {
    // redact all fields in the array
    for (let i = 1; i < dataPart.length; i++) {
      dataPart[i] = '';
    }
  }
};


/**
 * Removes potentially private data from a request POST body so that we don't
 * accidentally save private and personal data to the server logs.
 * @param {?string} data The data string to clean.
 * @return {?string} The data string with sensitive data replaced by 'redacted'.
 * @private
 */
goog.net.ChannelDebug.prototype.maybeRedactPostData_ = function(data) {
  'use strict';
  if (!data) {
    return null;
  }
  let out = '';
  const params = data.split('&');
  for (let i = 0; i < params.length; i++) {
    const param = params[i];
    const keyValue = param.split('=');
    if (keyValue.length > 1) {
      const key = keyValue[0];
      const value = keyValue[1];

      const keyParts = key.split('_');
      if (keyParts.length >= 2 && keyParts[1] == 'type') {
        out += key + '=' + value + '&';
      } else {
        out += key + '=' +
            'redacted' +
            '&';
      }
    }
  }
  return out;
};


/**
 * The normal response for forward channel requests.
 * Used only before version 8 of the protocol.
 * @const
 */
goog.net.ChannelDebug.MAGIC_RESPONSE_COOKIE = 'y2f%';
