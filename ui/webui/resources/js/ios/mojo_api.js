// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The API should stay in sync with the IDL definitions in
// third_party/WebKit/Source/core/mojo

// eslint-disable-next-line no-var
var Mojo = Mojo || {};

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
  const result =
      Mojo.internal.sendMessage({name: 'Mojo.createMessagePipe', args: {}});
  if (result.result === Mojo.RESULT_OK) {
    result.handle0 = new MojoHandle(result.handle0);
    result.handle1 = new MojoHandle(result.handle1);
  }
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
    if (signalsValue.peerClosed) {
      signalsValue |= HANDLE_SIGNAL_PEER_CLOSED;
    }

    const watchId = Mojo.internal.sendMessage({
      name: 'MojoHandle.watch',
      args: {
        handle: this.nativeHandle_,
        signals: signalsValue,
        callbackId: Mojo.internal.watchCallbacksHolder.getNextCallbackId(),
      },
    });
    Mojo.internal.watchCallbacksHolder.addWatchCallback(watchId, callback);

    return new MojoWatcher(watchId);
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
    let base64EncodedBuffer;
    if (buffer instanceof Uint8Array) {
      // calls from mojo_bindings.js
      base64EncodedBuffer = _Uint8ArrayToBase64(buffer);
    } else if (buffer instanceof ArrayBuffer) {
      // calls from mojo/public/js/bindings.js
      base64EncodedBuffer = _arrayBufferToBase64(buffer);
    }
    const nativeHandles = handles.map(function(handle) {
      return handle.takeNativeHandle_();
    });
    return Mojo.internal.sendMessage({
      name: 'MojoHandle.writeMessage',
      args: {
        handle: this.nativeHandle_,
        buffer: base64EncodedBuffer,
        handles: nativeHandles,
      },
    });
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
    const result = Mojo.internal.sendMessage(
        {name: 'MojoHandle.readMessage', args: {handle: this.nativeHandle_}});

    if (result.result === Mojo.RESULT_OK) {
      result.buffer = new Uint8Array(result.buffer).buffer;
      result.handles = result.handles.map(function(handle) {
        return new MojoHandle(handle);
      });
    }
    return result;
  }
}


/**
 * MojoWatcher identifies a watch on a MojoHandle and can be used to cancel the
 * watch.
 */
class MojoWatcher {
  /** @param {number} An opaque id representing the watch. */
  constructor(watchId) {
    this.watchId_ = watchId;
  }

  /*
   * Cancels a handle watch.
   * @return {Object=} Response from Mojo backend.
   */
  cancel() {
    const result = Mojo.internal.sendMessage(
        {name: 'MojoWatcher.cancel', args: {watchId: this.watchId_}});
    Mojo.internal.watchCallbacksHolder.removeWatchCallback(this.watchId_);
    return result;
  }
}

// -----------------------------------------------------------------------------
// Mojo API implementation details. It is not part of the public API.

Mojo.internal = Mojo.internal || {};

/**
 * Synchronously sends a message to Mojo backend.
 * @param {!Object} message The message to send.
 * @return {Object=} Response from Mojo backend.
 */
Mojo.internal.sendMessage = function(message) {
  const response = window.prompt(__gCrWeb.common.JSONStringify(message));
  return response ? JSON.parse(response) : undefined;
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
   * Returns next callback id to be used for watch (idempotent).
   *
   * @return {number} callback id.
   */
  const getNextCallbackId = function() {
    return nextCallbackId;
  };

  /**
   * Adds callback which must be executed when the watch fires.
   *
   * @param {number} watchId The value returned from "MojoHandle.watch" Mojo
   *     backend.
   * @param {!function(!MojoResult)} callback The callback which should be
   *     executed when the watch fires.
   */
  const addWatchCallback = function(watchId, callback) {
    callbackIds.set(watchId, nextCallbackId);
    callbacks.set(nextCallbackId, callback);
    ++nextCallbackId;
  };

  /**
   * Removes callback which should no longer be executed.
   *
   * @param {!number} watchId The id to remove callback for.
   */
  const removeWatchCallback = function(watchId) {
    callbacks.delete(callbackIds.get(watchId));
    callbackIds.delete(watchId);
  };

  return {
    callCallback: callCallback,
    getNextCallbackId: getNextCallbackId,
    addWatchCallback: addWatchCallback,
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
