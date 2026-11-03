// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polyfill for the Blink Mojo JavaScript API on iOS WebKit.
 * Note: This polyfill API should stay in sync with the IDL definitions in
 * `third_party/blink/renderer/core/mojo/mojo.idl`.
 */

/**
 * Result codes for Mojo operations.
 */
export enum MojoResult {
  RESULT_OK = 0,
  RESULT_CANCELLED = 1,
  RESULT_UNKNOWN = 2,
  RESULT_INVALID_ARGUMENT = 3,
  RESULT_DEADLINE_EXCEEDED = 4,
  RESULT_NOT_FOUND = 5,
  RESULT_ALREADY_EXISTS = 6,
  RESULT_PERMISSION_DENIED = 7,
  RESULT_RESOURCE_EXHAUSTED = 8,
  RESULT_FAILED_PRECONDITION = 9,
  RESULT_ABORTED = 10,
  RESULT_OUT_OF_RANGE = 11,
  RESULT_UNIMPLEMENTED = 12,
  RESULT_INTERNAL = 13,
  RESULT_UNAVAILABLE = 14,
  RESULT_DATA_LOSS = 15,
  RESULT_BUSY = 16,
  RESULT_SHOULD_WAIT = 17,
}

/**
 * Signals to watch on a `MojoHandle`.
 */
export interface MojoHandleSignals {
  readable?: boolean;
  writable?: boolean;
  peerClosed?: boolean;
}

/**
 * Result returned by `Mojo.createMessagePipe()`.
 */
export interface MojoCreateMessagePipeResult {
  result: MojoResult;
  handle0: MojoHandle;
  handle1: MojoHandle;
}

/**
 * Raw wire message delivered from `MojoFacade::NotifyReadAndArmWatcher`.
 */
export interface NativeBaseValueMessage {
  result: MojoResult;
  buffer?: number[];
  handles?: number[];
}

/**
 * Result returned by `MojoHandle.readMessage()`.
 */
export interface MojoReadMessageResult {
  result: MojoResult;
  buffer?: ArrayBuffer;
  handles?: MojoHandle[];
}

/**
 * Callback invoked when watched signals on a `MojoHandle` become satisfied or
 * unsatisfiable.
 */
export type MojoWatchCallback = (result: MojoResult) => void;

/**
 * Strongly typed message contract between TypeScript Mojo API and
 * C++ Mojo Facade. Reflects native C++ arguments and
 * `base::Value` return values.
 */
export interface MojoBridgeProtocol {
  'Mojo.createMessagePipe': {
    args: {handle0Id: number, handle1Id: number},
    response: {result: MojoResult, handle0: number, handle1: number},
  };
  'Mojo.bindInterface': {
    args: {interfaceName: string, requestHandle: number|null},
    response: MojoResult,
  };
  'MojoHandle.close': {
    args: {handle: number},
    response: unknown,
  };
  'MojoHandle.writeMessage': {
    args: {
      handle: number|null,
      buffer?: string, handles: Array<number|null>,
    },
    response: MojoResult,
  };
  'MojoHandle.watch': {
    args: {handle: number|null, signals: number, callbackId: number},
    response: number,
  };
  'MojoWatcher.cancel': {
    args: {watchId: number},
    response: unknown,
  };
}

/**
 * Outgoing message payload passed to `Mojo.internal.sendMessage()`.
 */
export interface MojoMessage<K extends keyof MojoBridgeProtocol =
                                           keyof MojoBridgeProtocol> {
  name: K;
  args: MojoBridgeProtocol[K]['args'];
}

/**
 * Envelope sent across the iOS bridge to Mojo Facade.
 */
export interface WrappedMojoMessage<K extends keyof MojoBridgeProtocol =
                                                  keyof MojoBridgeProtocol> {
  message_id: number;
  message: MojoMessage<K>;
}

/**
 * Holds callbacks for all currently active watches.
 */
export class WatchCallbacksHolder {
  /**
   * Next callback id to be used for watch.
   */
  private nextCallbackId_ = 0;

  /**
   * Map where keys are callback ids and values are callbacks.
   */
  private readonly callbacks_: Map<number, MojoWatchCallback> = new Map();

  /**
   * Map where keys are watch ids and values are callback ids.
   */
  private readonly callbackIds_: Map<number, number> = new Map();

  /**
   * Calls watch callback.
   *
   * @param callbackId Callback id previously returned from
   *     `registerCallback()`.
   * @param mojoResult The result code to call the callback with.
   */
  callCallback(callbackId: number, mojoResult: MojoResult): void {
    const callback = this.callbacks_.get(callbackId);

    // Signalling the watch is an asynchronous operation and this function may
    // be called for an already removed watch.
    if (callback) {
      callback(mojoResult);
    }
  }

  /**
   * Registers watch callback and returns next callback id.
   *
   * @return The callback id.
   */
  registerCallback(callback: MojoWatchCallback): number {
    const callbackId = this.nextCallbackId_++;
    this.callbacks_.set(callbackId, callback);
    return callbackId;
  }

  /**
   * Associates `watchId` with `callbackId`.
   */
  associateWatchId(watchId: number, callbackId: number): void {
    this.callbackIds_.set(watchId, callbackId);
  }

  /**
   * Removes callback directly by `callbackId`.
   */
  removeCallbackById(callbackId: number): void {
    this.callbacks_.delete(callbackId);
  }

  /**
   * Removes callback which should no longer be executed.
   *
   * @param watchId The id to remove callback for.
   */
  removeWatchCallback(watchId: number): void {
    const callbackId = this.callbackIds_.get(watchId);
    if (callbackId !== undefined) {
      this.callbacks_.delete(callbackId);
    }
    this.callbackIds_.delete(watchId);
  }
}

/**
 * `MojoWatcher` identifies a watch on a `MojoHandle` and can be used to cancel
 * the watch.
 */
export class MojoWatcher {
  private readonly watchIdPromise_: Promise<number>;
  private readonly callbackId_: number;

  constructor(watchIdPromise: Promise<number>, callbackId: number) {
    this.watchIdPromise_ = watchIdPromise;
    this.callbackId_ = callbackId;
  }

  /**
   * Cancels a handle watch.
   *
   * @return Response from the Mojo backend.
   */
  async cancel(): Promise<unknown> {
    Mojo.internal.watchCallbacksHolder.removeCallbackById(this.callbackId_);
    const watchId = await this.watchIdPromise_;
    const result = await Mojo.internal.sendMessage(
        {name: 'MojoWatcher.cancel', args: {watchId}});
    Mojo.internal.watchCallbacksHolder.removeWatchCallback(watchId);
    return result;
  }
}

/**
 * Wraps a native Mojo handle ID and provides message pipe and watch operations.
 */
export class MojoHandle {
  private nativeHandle_: number|null;

  /**
   * @param nativeHandle An opaque number representing the underlying Mojo
   *     system resource.
   */
  constructor(nativeHandle?: number|null) {
    this.nativeHandle_ = nativeHandle ?? null;
  }

  /**
   * Takes the native handle value. This is not part of the public API.
   */
  takeNativeHandle(): number|null {
    const nativeHandle = this.nativeHandle_;
    this.nativeHandle_ = null;
    return nativeHandle;
  }

  /**
   * Closes the handle.
   */
  close(): void {
    if (this.nativeHandle_ === null) {
      return;
    }

    const nativeHandle = this.nativeHandle_;
    this.nativeHandle_ = null;
    Mojo.internal.sendMessage(
        {name: 'MojoHandle.close', args: {handle: nativeHandle}});
  }

  /**
   * Begins watching the handle for `signals` to be satisfied or unsatisfiable.
   *
   * @param signals The signals to watch.
   * @param callback Called with a result any time the watched signals become
   *     satisfied or unsatisfiable.
   * @return A `MojoWatcher` instance that could be used to cancel the watch.
   */
  watch(signals: MojoHandleSignals, callback: MojoWatchCallback): MojoWatcher {
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
                callbackId,
              },
            })
            .then((watchId) => {
              Mojo.internal.watchCallbacksHolder.associateWatchId(
                  watchId, callbackId);
              return watchId;
            });

    return new MojoWatcher(watchIdPromise, callbackId);
  }

  /**
   * Writes a message to the message pipe.
   *
   * @param buffer The message data. May be empty.
   * @param handles Any handles to attach. Handles are transferred and will no
   *     longer be valid. May be empty.
   * @return Result code.
   */
  writeMessage(buffer: BufferSource, handles: MojoHandle[]): MojoResult {
    const nativeHandle = this.nativeHandle_;
    const nativeHandles = handles.map((handle) => {
      return handle.takeNativeHandle();
    });
    let base64EncodedBuffer: string|undefined;
    if (buffer instanceof Uint8Array) {
      // Calls from mojo bindings.
      base64EncodedBuffer = uint8ArrayToBase64(buffer);
    } else if (buffer instanceof ArrayBuffer) {
      base64EncodedBuffer = arrayBufferToBase64(buffer);
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
   * @return Result code and (on success) the data and handles received.
   */
  readMessage(): MojoReadMessageResult {
    const handleId = this.nativeHandle_;
    const queue = handleId !== null ?
        Mojo.internal.receivedMessagesByHandle[handleId] :
        undefined;
    if (queue && queue.length > 0) {
      const result = queue.shift()!;
      if (result.result === Mojo.RESULT_OK) {
        return {
          result: result.result,
          buffer: new Uint8Array(result.buffer ?? []).buffer,
          handles: (result.handles ?? []).map((handle) => {
            return new MojoHandle(handle);
          }),
        };
      }
      return {result: result.result};
    }
    return {result: Mojo.RESULT_SHOULD_WAIT};
  }
}

// -----------------------------------------------------------------------------
// Mojo API implementation details. It is not part of the public API.

/**
 * Internal state and bridge methods for communicating with `MojoFacade`.
 */
class MojoInternal {
  /** Holds messages to be sent to the native side. */
  queuedMessages: WrappedMojoMessage[] = [];

  /** Holds messages sent by the native side to JS. */
  receivedMessagesByHandle: Record<number, NativeBaseValueMessage[]> = {};

  /**
   * Holds the `Promise` `resolve` callback for a waiting
   * `fetchNextMessageFromJS()` call when the outgoing queue is empty.
   */
  sendNextMessagePromiseResolver?: (message: WrappedMojoMessage) => void;

  /** Holds message ID counter for outgoing JS -> Native messages. */
  nextAvailableMessageId = 0;

  /**
   * Map of sent message IDs to their `Promise` `resolve` callbacks, waiting for
   * responses from the native side.
   */
  sendMessageResultPromises: Record<number, (result: unknown) => void> = {};

  /** Holds callbacks for all currently active watches. */
  watchCallbacksHolder = new WatchCallbacksHolder();


  /**
   * Called by the native iOS bridge to deliver and buffer messages/results
   * coming from the native side for a specific handle ID until `readMessage()`
   * is called by JS.
   */
  fetchNextMessageFromNative(handleId: number, result: NativeBaseValueMessage):
      void {
    if (!this.receivedMessagesByHandle[handleId]) {
      this.receivedMessagesByHandle[handleId] = [];
    }
    this.receivedMessagesByHandle[handleId].push(result);
  }

  /**
   * Asynchronously sends a message to the native side.
   * If the native bridge is currently waiting via `fetchNextMessageFromJS()`,
   * delivers directly by resolving its `Promise`; otherwise buffers the message
   * in `queuedMessages`.
   */
  sendMessage<K extends keyof MojoBridgeProtocol>(message: MojoMessage<K>):
      Promise<MojoBridgeProtocol[K]['response']> {
    const messageId = this.nextAvailableMessageId++;
    const wrappedMessage: WrappedMojoMessage<K> = {
      message_id: messageId,
      message,
    };

    if (this.sendNextMessagePromiseResolver) {
      this.sendNextMessagePromiseResolver(wrappedMessage as WrappedMojoMessage);
      this.sendNextMessagePromiseResolver = undefined;
    } else {
      this.queuedMessages.push(wrappedMessage as WrappedMojoMessage);
    }

    return new Promise((resolve) => {
      this.sendMessageResultPromises[messageId] =
          resolve as (result: unknown) => void;
    });
  }

  /**
   * Resolves a waiting promise created in `sendMessage()` for `messageId` with
   * `result`.
   */
  messageReceived(messageId: number, result: unknown): void {
    const resolver = this.sendMessageResultPromises[messageId];
    delete this.sendMessageResultPromises[messageId];

    if (resolver) {
      resolver(result);
    }
  }

  /**
   * Called by the native iOS bridge to retrieve the next outgoing message from
   * JS. If the outgoing queue is empty, returns a `Promise` that resolves as
   * soon as `sendMessage()` is next called.
   */
  // Method name matches the native C++ bridge call in
  // ios/web/webui/mojo_facade.mm.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  fetchNextMessageFromJS(): Promise<WrappedMojoMessage> {
    const queueLength = this.queuedMessages.length;
    if (queueLength) {
      const nextMsg = this.queuedMessages.shift()!;
      return Promise.resolve(nextMsg);
    }
    return new Promise((resolve) => {
      this.sendNextMessagePromiseResolver = resolve;
    });
  }
}

/**
 * Public `Mojo` API object conforming to Blink WebIDL and legacy JS bindings.
 * Exposes `Mojo.RESULT_*` constants inspired by
 * `ui/webui/resources/mojo/bindings.d.ts`.
 */
export const Mojo = {
  nextAvailableHandleId: 1,

  RESULT_OK: MojoResult.RESULT_OK,
  RESULT_CANCELLED: MojoResult.RESULT_CANCELLED,
  RESULT_UNKNOWN: MojoResult.RESULT_UNKNOWN,
  RESULT_INVALID_ARGUMENT: MojoResult.RESULT_INVALID_ARGUMENT,
  RESULT_DEADLINE_EXCEEDED: MojoResult.RESULT_DEADLINE_EXCEEDED,
  RESULT_NOT_FOUND: MojoResult.RESULT_NOT_FOUND,
  RESULT_ALREADY_EXISTS: MojoResult.RESULT_ALREADY_EXISTS,
  RESULT_PERMISSION_DENIED: MojoResult.RESULT_PERMISSION_DENIED,
  RESULT_RESOURCE_EXHAUSTED: MojoResult.RESULT_RESOURCE_EXHAUSTED,
  RESULT_FAILED_PRECONDITION: MojoResult.RESULT_FAILED_PRECONDITION,
  RESULT_ABORTED: MojoResult.RESULT_ABORTED,
  RESULT_OUT_OF_RANGE: MojoResult.RESULT_OUT_OF_RANGE,
  RESULT_UNIMPLEMENTED: MojoResult.RESULT_UNIMPLEMENTED,
  RESULT_INTERNAL: MojoResult.RESULT_INTERNAL,
  RESULT_UNAVAILABLE: MojoResult.RESULT_UNAVAILABLE,
  RESULT_DATA_LOSS: MojoResult.RESULT_DATA_LOSS,
  RESULT_BUSY: MojoResult.RESULT_BUSY,
  RESULT_SHOULD_WAIT: MojoResult.RESULT_SHOULD_WAIT,

  internal: new MojoInternal(),

  /**
   * Creates a message pipe.
   *
   * @return Result code and (on success) the two message pipe handles.
   */
  createMessagePipe(): MojoCreateMessagePipeResult {
    const handle0Id = Mojo.nextAvailableHandleId++;
    const handle1Id = Mojo.nextAvailableHandleId++;

    Mojo.internal.sendMessage({
      name: 'Mojo.createMessagePipe',
      args: {
        handle0Id,
        handle1Id,
      },
    });

    const result = {
      handle0: new MojoHandle(handle0Id),
      handle1: new MojoHandle(handle1Id),
      result: Mojo.RESULT_OK,
    };
    return result;
  },

  /**
   * Binds to the specified Mojo interface.
   *
   * @param interfaceName The interface name to connect.
   * @param requestHandle The interface request handle.
   */
  bindInterface(interfaceName: string, requestHandle: MojoHandle): void {
    Mojo.internal.sendMessage({
      name: 'Mojo.bindInterface',
      args: {
        interfaceName,
        requestHandle: requestHandle.takeNativeHandle(),
      },
    });
  },
};

/**
 * Base64-encodes an `ArrayBuffer`.
 */
function arrayBufferToBase64(buffer: ArrayBuffer): string {
  return uint8ArrayToBase64(new Uint8Array(buffer));
}

/**
 * Base64-encodes a `Uint8Array`.
 */
function uint8ArrayToBase64(bytes: Uint8Array): string {
  let binary = '';
  const numBytes = bytes.byteLength;
  for (let i = 0; i < numBytes; i++) {
    binary += String.fromCharCode(bytes[i]!);
  }
  return window.btoa(binary);
}

// Expose globals for C++ Mojo Facade and legacy JS bindings.
Object.assign(window, {
  Mojo,
  MojoHandle,
  MojoWatcher,
  MojoResult,
});
