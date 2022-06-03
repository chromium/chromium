/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview adaptor of XhrStreamReader to the NodeReadableStream interface.
 */

goog.module('goog.net.streams.xhrNodeReadableStream');

goog.module.declareLegacyNamespace();

const NodeReadableStream = goog.require('goog.net.streams.NodeReadableStream');
const googArray = goog.require('goog.array');
const googLog = goog.require('goog.log');
const {XhrStreamReader, XhrStreamReaderStatus} = goog.require('goog.net.streams.xhrStreamReader');


/**
 * The XhrNodeReadableStream class.
 *
 * @implements {NodeReadableStream}
 * @struct
 * @final
 * @package
 */
class XhrNodeReadableStream {
  /**
   * @param {!XhrStreamReader} xhrReader The XhrStreamReader object that handles
   *     the events of the underlying Xhr.
   */
  constructor(xhrReader) {
    'use strict';
    /**
     * @const
     * @private {?googLog.Logger} the logger.
     */
    this.logger_ = googLog.getLogger('goog.net.streams.XhrNodeReadableStream');


    /**
     * The xhr reader.
     *
     * @private {!XhrStreamReader} the xhr reader.
     */
    this.xhrReader_ = xhrReader;

    this.xhrReader_.setDataHandler(goog.bind(this.onData_, this));
    this.xhrReader_.setStatusHandler(goog.bind(this.onStatusChange_, this));

    /**
     * The callback map, keyed by eventTypes.
     *
     * @private {!Object<!Array<function(!Object=)>>}
     */
    this.callbackMap_ = {};

    /**
     * The callback-once map, keyed by eventTypes.
     *
     * @private {!Object<!Array<function(!Object=)>>}
     */
    this.callbackOnceMap_ = {};
  }


  /**
   * @override
   * @param {string} eventType
   * @param {function(!Object=)} callback
   * @return {!NodeReadableStream}
   */
  on(eventType, callback) {
    'use strict';
    let callbacks = this.callbackMap_[eventType];
    if (!callbacks) {
      callbacks = [];
      this.callbackMap_[eventType] = callbacks;
    }

    callbacks.push(callback);
    return this;
  }


  /**
   * @override
   * @param {string} eventType
   * @param {function(!Object=)} callback
   * @return {!NodeReadableStream}
   */
  addListener(eventType, callback) {
    'use strict';
    this.on(eventType, callback);
    return this;
  }


  /**
   * @override
   * @param {string} eventType
   * @param {function(!Object=)} callback
   * @return {!NodeReadableStream}
   */
  removeListener(eventType, callback) {
    'use strict';
    const callbacks = this.callbackMap_[eventType];
    if (callbacks) {
      googArray.remove(callbacks, callback);  // keep the empty array
    }

    const onceCallbacks = this.callbackOnceMap_[eventType];
    if (onceCallbacks) {
      googArray.remove(onceCallbacks, callback);
    }

    return this;
  }


  /**
   * @override
   * @param {string} eventType
   * @param {function(!Object=)} callback
   * @return {!NodeReadableStream}
   */
  once(eventType, callback) {
    'use strict';
    let callbacks = this.callbackOnceMap_[eventType];
    if (!callbacks) {
      callbacks = [];
      this.callbackOnceMap_[eventType] = callbacks;
    }

    callbacks.push(callback);
    return this;
  }


  /**
   * Handles any new data from XHR.
   *
   * @param {!Array<!Object>} messages New messages, to be delivered in order
   *    and atomically.
   * @private
   */
  onData_(messages) {
    'use strict';
    const callbacks = this.callbackMap_[NodeReadableStream.EventType.DATA];
    if (callbacks) {
      this.doMessages_(messages, callbacks);
    }

    const onceCallbacks =
        this.callbackOnceMap_[NodeReadableStream.EventType.DATA];
    if (onceCallbacks) {
      this.doMessages_(messages, onceCallbacks);
    }
    this.callbackOnceMap_[NodeReadableStream.EventType.DATA] = [];
  }


  /**
   * Deliver messages to registered callbacks.
   *
   * Exceptions are caught and logged (debug), and ignored otherwise.
   *
   * @param {!Array<!Object>} messages The messages to be delivered
   * @param {!Array<function(!Object=)>} callbacks The callbacks.
   * @private
   */
  doMessages_(messages, callbacks) {
    'use strict';
    const self = this;
    for (let i = 0; i < messages.length; i++) {
      const message = messages[i];

      callbacks.forEach(function(callback) {
        'use strict';
        try {
          callback(message);
        } catch (ex) {
          self.handleError_('message-callback exception (ignored) ' + ex);
        }
      });
    }
  }


  /**
   * Handles any state changes from XHR.
   *
   * @private
   */
  onStatusChange_() {
    'use strict';
    const currentStatus = this.xhrReader_.getStatus();
    const EventType = NodeReadableStream.EventType;

    switch (currentStatus) {
      case XhrStreamReaderStatus.ACTIVE:
        this.doStatus_(EventType.READABLE);
        break;

      case XhrStreamReaderStatus.BAD_DATA:
      case XhrStreamReaderStatus.HANDLER_EXCEPTION:
      case XhrStreamReaderStatus.NO_DATA:
      case XhrStreamReaderStatus.TIMEOUT:
      case XhrStreamReaderStatus.XHR_ERROR:
        this.doStatus_(EventType.ERROR);
        break;

      case XhrStreamReaderStatus.CANCELLED:
        this.doStatus_(EventType.CLOSE);
        break;

      case XhrStreamReaderStatus.SUCCESS:
        this.doStatus_(EventType.END);
        break;
    }
  }


  /**
   * Run status change callbacks.
   *
   * @param {string} eventType The event type
   * @private
   */
  doStatus_(eventType) {
    'use strict';
    const callbacks = this.callbackMap_[eventType];
    const self = this;
    if (callbacks) {
      callbacks.forEach(function(callback) {
        'use strict';
        try {
          callback();
        } catch (ex) {
          self.handleError_('status-callback exception (ignored) ' + ex);
        }
      });
    }

    const onceCallbacks = this.callbackOnceMap_[eventType];
    if (onceCallbacks) {
      onceCallbacks.forEach(function(callback) {
        'use strict';
        callback();
      });
    }

    this.callbackOnceMap_[eventType] = [];
  }


  /**
   * Log an error
   *
   * @param {string} message The error message
   * @private
   */
  handleError_(message) {
    'use strict';
    googLog.error(this.logger_, message);
  }
}

exports = {XhrNodeReadableStream};
