// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This polyfill API should stay in sync with the IDL definitions in
// third_party/blink/renderer/core/mojo/mojo.idl

// eslint-disable-next-line no-var
var Mojo = Mojo || {};
Mojo.nextAvailableHandleId = Mojo.nextAvailableHandleId || 1;

/**
 * MojoResult {number}: Result codes for Mojo operations.
 */
Mojo.RESULT_OK = 0;
Mojo.RESULT_CANCELLED = 1;
Mojo.RESULT_UNKNOWN = 2;
Mojo.RESULT_INVALID_ARGUMENT = 3;
Mojo.RESULT_DEADLINE_EXCEEDED = 4;
Mojo.RESULT_NOT_FOUND = 5;
Mojo.RESULT_ALREADY_EXISTS = 6;
Mojo.RESULT_PERMISSION_DENIED = 7;
Mojo.RESULT_RESOURCE_EXHAUSTED = 8;
Mojo.RESULT_FAILED_PRECONDITION = 9;
Mojo.RESULT_ABORTED = 10;
Mojo.RESULT_OUT_OF_RANGE = 11;
Mojo.RESULT_UNIMPLEMENTED = 12;
Mojo.RESULT_INTERNAL = 13;
Mojo.RESULT_UNAVAILABLE = 14;
Mojo.RESULT_DATA_LOSS = 15;
Mojo.RESULT_BUSY = 16;
Mojo.RESULT_SHOULD_WAIT = 17;

/**
 * Creates a message pipe.
 *
 * @return {result: !MojoResult, handle0: !MojoHandle=, handle1: !MojoHandle=}
 *     Result code and (on success) the two message pipe handles.
 */
Mojo.createMessagePipe = function() {
  const handle0Id = Mojo.nextAvailableHandleId++;
  const handle1Id = Mojo.nextAvailableHandleId++;

  Mojo.internal.sendMessage({
    name: 'Mojo.createMessagePipe',
    args: {
      handle0Id: handle0Id,
      handle1Id: handle1Id,
    },
  });

  const result = {
    handle0: new MojoHandle(handle0Id),
    handle1: new MojoHandle(handle1Id),
    result: Mojo.RESULT_OK,
  };
  return result;
};

/**
 * Binds to the specified Mojo interface.
 * @param {string} interfaceName The interface name to connect.
 * @param {!MojoHandle} requestHandle The interface request handle.
 */
Mojo.bindInterface = function(interfaceName, requestHandle) {
  Mojo.internal.sendMessage({
    name: 'Mojo.bindInterface',
    args: {
      interfaceName: interfaceName,
      requestHandle: requestHandle.takeNativeHandle_(),
    },
  });
};

class MojoHandle {
  /*
   * @param {?number=} nativeHandle An opaque number representing the underlying
   *     Mojo system resource.
   */
  constructor(nativeHandle) {
    if (nativeHandle === undefined) {
      nativeHandle = null;
    }

    /**
     * @type {number|null}
     */
    this.nativeHandle_ = nativeHandle;
  }

  /**
   * Takes the native handle value. This is not part of the public API.
   * @return {?number}
   */
  takeNativeHandle_() {
    const nativeHandle = this.nativeHandle_;
    this.nativeHandle_ = null;
    return nativeHandle;
  }

  /**
   * Closes the handle.
   */
  close() {
    if (this.nativeHandle_ === null) {
      return;
    }

    const nativeHandle = this.nativeHandle_;
    this.nativeHandle_ = null;
    Mojo.internal.sendMessage(
        {name: 'MojoHandle.close', args: {handle: nativeHandle}});
  }

  /**
   * Begins watching the handle for |signals| to be satisfied or unsatisfiable.
   *
   * @param {readable: boolean=, writable: boolean=, peerClosed: boolean=}
   *     signals The signals to watch.
   * @param {!function(!MojoResult)} callback Called with a result any time
   *     the watched signals become satisfied or unsatisfiable.
   *
   * @return {!MojoWatcher} A MojoWatcher instance that could be used to cancel
   *     the watch.
   */
  watch(signals, callback) {
    const HANDLE_SIGNAL_NONE = 0;
    const HANDLE_SIGNAL_READABLE = 1;
    const HANDLE_SIGNAL_WRITABLE = 2;
    const HANDLE_SIGNAL_PEER_CLOSED = 4;

    let signalsValue = HANDLE_SIGNAL_NONE;
    if (signals.readable) {
      signalsValue |= HANDLE_SIGNAL_READABLE;
    }
    if (signals.writable) {
      signalsValue |= HANDLE_SIGNAL_WRITABLE;
    }
    if (signals.peerClosed) {
      signalsValue |= HANDLE_SIGNAL_PEER_CLOSED;
    }

    const callbackId =
        Mojo.internal.watchCallbacksHolder.registerCallback(callback);
    const watchIdPromise =
        Mojo.internal
            .sendMessage({
              name: 'MojoHandle.watch',
              args: {
                handle: this.nativeHandle_,
                signals: signalsValue,
                callbackId: callbackId,
              },
            })
            .then(watchId => {
              Mojo.internal.watchCallbacksHolder.associateWatchId(
                  watchId, callbackId);
              return watchId;
            });

    return new MojoWatcher(watchIdPromise, callbackId);
  }

  /**
   * Writes a message to the message pipe.
   *
   * @param {!ArrayBufferView} buffer The message data. May be empty.
   * @param {!Array<!MojoHandle>} handles Any handles to attach. Handles are
   *     transferred and will no longer be valid. May be empty.
   * @return {!MojoResult} Result code.
   */
  writeMessage(buffer, handles) {
    const nativeHandle = this.nativeHandle_;
    const nativeHandles = handles.map(function(handle) {
      return handle.takeNativeHandle_();
    });
    let base64EncodedBuffer;
    if (buffer instanceof Uint8Array) {
      // calls from mojo_bindings.js
      base64EncodedBuffer = _Uint8ArrayToBase64(buffer);
    } else if (buffer instanceof ArrayBuffer) {
      // calls from mojo/public/js/bindings.js
      base64EncodedBuffer = _arrayBufferToBase64(buffer);
    }
    Mojo.internal.sendMessage({
      name: 'MojoHandle.writeMessage',
      args: {
        handle: nativeHandle,
        buffer: base64EncodedBuffer,
        handles: nativeHandles,
      },
    });
    return Mojo.RESULT_OK;
  }

  /**
   * Reads a message from the message pipe.
   *
   * @return {result: !MojoResult,
   *          buffer: !ArrayBufferView=,
   *          handles: !Array<!MojoHandle>=}
   *     Result code and (on success) the data and handles received.
   */
  readMessage() {
    const handleId = this.nativeHandle_;
    const queue = Mojo.internal.receivedMessagesByHandle[handleId];
    if (queue && queue.length > 0) {
      const result = queue.shift();
      if (result.result === Mojo.RESULT_OK) {
        result.buffer = new Uint8Array(result.buffer).buffer;
        result.handles = (result.handles || []).map(function(handle) {
          return new MojoHandle(handle);
        });
      }
      return result;
    }
    return {result: Mojo.RESULT_SHOULD_WAIT};
  }
}

/**
 * MojoWatcher identifies a watch on a MojoHandle and can be used to cancel the
 * watch.
 */
class MojoWatcher {
  /**
   * @param {!Promise<number>} watchIdPromise
   * @param {number} callbackId
   */
  constructor(watchIdPromise, callbackId) {
    this.watchIdPromise_ = watchIdPromise;
    this.callbackId_ = callbackId;
  }

  /**
   * Cancels a handle watch.
   * @return {!Promise<Object>} Response from Mojo backend.
   */
  async cancel() {
    Mojo.internal.watchCallbacksHolder.removeCallbackById(this.callbackId_);
    const watchId = await this.watchIdPromise_;
    const result = await Mojo.internal.sendMessage(
        {name: 'MojoWatcher.cancel', args: {watchId: watchId}});
    Mojo.internal.watchCallbacksHolder.removeWatchCallback(watchId);
    return result;
  }
}

// -----------------------------------------------------------------------------
// Mojo API implementation details. It is not part of the public API.

Mojo.internal = Mojo.internal || {};

// Holds messages to be sent to the native side.
Mojo.internal.queuedMessages = Mojo.internal.queuedMessages || [];
// Holds messages sent by the native side to JS.
Mojo.internal.receivedMessagesByHandle =
    Mojo.internal.receivedMessagesByHandle || {};
// Holds the Promise 'resolve' callback for a waiting fetchNextMessageFromJS()
// call when the outgoing queue is empty.
Mojo.internal.sendNextMessagePromiseResolver =
    Mojo.internal.sendNextMessagePromiseResolver || undefined;
// Holds message ID counter for outgoing JS -> Native messages.
Mojo.internal.nextAvailableMessageId =
    Mojo.internal.nextAvailableMessageId || 0;
// Map of sent message IDs to their Promise resolve callbacks, waiting for
// responses from native side.
Mojo.internal.sendMessageResultPromises =
    Mojo.internal.sendMessageResultPromises || {};

/**
 * Called by the native iOS bridge to deliver and buffer messages/results
 * coming from native side for a specific handle ID until readMessage() is
 * called by JS.
 * @param {number} handleId
 * @param {!Object} result
 */

Mojo.internal.fetchNextMessageFromNative = function(handleId, result) {
  if (!Mojo.internal.receivedMessagesByHandle[handleId]) {
    Mojo.internal.receivedMessagesByHandle[handleId] = [];
  }
  Mojo.internal.receivedMessagesByHandle[handleId].push(result);
};

/**
 * Asynchronously sends a message to the native side.
 * If the native bridge is currently waiting via fetchNextMessageFromJS(),
 * delivers directly by resolving its Promise; otherwise buffers the message in
 * queuedMessages.
 * @param {!Object} message
 * @return {!Promise<Object>}
 */

Mojo.internal.sendMessage = async function(message) {
  const messageId = Mojo.internal.nextAvailableMessageId++;
  const wrappedMessage = {message_id: messageId, message: message};

  if (Mojo.internal.sendNextMessagePromiseResolver) {
    Mojo.internal.sendNextMessagePromiseResolver(wrappedMessage);
    Mojo.internal.sendNextMessagePromiseResolver = undefined;
  } else {
    Mojo.internal.queuedMessages.push(wrappedMessage);
  }

  return new Promise((resolve) => {
    Mojo.internal.sendMessageResultPromises[messageId] = resolve;
  });
};

// Resolves a waiting promise created in sendMessage for `messageId` with
// `result`.
Mojo.internal.messageReceived = function(messageId, result) {
  const resolver = Mojo.internal.sendMessageResultPromises[messageId];
  delete Mojo.internal.sendMessageResultPromises[messageId];

  if (resolver) {
    resolver(result);
  }
};

/**
 * Called by the native iOS bridge to retrieve the next outgoing message from
 * JS. If the outgoing queue is empty, returns a Promise that resolves as soon
 * as sendMessage() is next called.
 * @return {!Promise<Object>}
 */

Mojo.internal.fetchNextMessageFromJS = async function() {
  const queueLength = Mojo.internal.queuedMessages.length;
  if (queueLength) {
    const nextMsg = Mojo.internal.queuedMessages.shift();
    return nextMsg;
  }
  return new Promise((resolve) => {
    Mojo.internal.sendNextMessagePromiseResolver = resolve;
  });
};

/**
 * Holds callbacks for all currently active watches.
 */
Mojo.internal.watchCallbacksHolder = (function() {
  /**
   * Next callback id to be used for watch.
   * @type{number}
   */
  let nextCallbackId = 0;

  /**
   * Map where keys are callbacks ids and values are callbacks.
   * @type {!Map<number, !function(!MojoResult)>}
   */
  const callbacks = new Map();

  /**
   * Map where keys are watch ids and values are callback ids.
   * @type {!Map<number, number>}
   */
  const callbackIds = new Map();

  /**
   * Calls watch callback.
   *
   * @param {number} callbackId Callback id previously returned from
         {@code getNextCallbackId}.
   * @param {!MojoResult} mojoResult The result code to call the callback with.
   */
  const callCallback = function(callbackId, mojoResult) {
    const callback = callbacks.get(callbackId);

    // Signalling the watch is asynchronous operation and this function may be
    // called for already removed watch.
    if (callback) {
      callback(mojoResult);
    }
  };

  /**
   * Registers watch callback and returns next callback id.
   *
   * @param {!function(!MojoResult)} callback
   * @return {number} callback id.
   */
  const registerCallback = function(callback) {
    const callbackId = nextCallbackId++;
    callbacks.set(callbackId, callback);
    return callbackId;
  };

  /**
   * Associates watchId with callbackId.
   *
   * @param {number} watchId
   * @param {number} callbackId
   */
  const associateWatchId = function(watchId, callbackId) {
    callbackIds.set(watchId, callbackId);
  };

  /**
   * Removes callback directly by callbackId.
   *
   * @param {number} callbackId
   */
  const removeCallbackById = function(callbackId) {
    callbacks.delete(callbackId);
  };

  /**
   * Removes callback which should no longer be executed.
   *
   * @param {!number} watchId The id to remove callback for.
   */
  const removeWatchCallback = function(watchId) {
    const callbackId = callbackIds.get(watchId);
    callbacks.delete(callbackId);
    callbackIds.delete(watchId);
  };

  return {
    callCallback: callCallback,
    registerCallback: registerCallback,
    associateWatchId: associateWatchId,
    removeCallbackById: removeCallbackById,
    removeWatchCallback: removeWatchCallback,
  };
})();

/**
 * Base64-encode an ArrayBuffer
 * @param {ArrayBuffer} buffer
 * @return {String}
 */
function _arrayBufferToBase64(buffer) {
  return _Uint8ArrayToBase64(new Uint8Array(buffer));
}

/**
 * Base64-encode an Uint8Array
 * @param {Uint8Array} buffer
 * @return {String}
 */
function _Uint8ArrayToBase64(bytes) {
  let binary = '';
  const numBytes = bytes.byteLength;
  for (let i = 0; i < numBytes; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return window.btoa(binary);
}
