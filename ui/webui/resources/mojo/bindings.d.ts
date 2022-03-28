// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Minimal definitions for the Mojo core bindings, just enough to make the TS
// compiler to not throw errors, and minimal definitions of types that are
// indirectly exposed by generated bindings. These should be fleshed out, or
// even better auto-generated from bindings_uncompiled.js eventually. The
// latter currently does not produce definitions that work.
// More information about them can be found in the *.idl files that generate
// many of these functions and types.
// @see //third_party/blink/renderer/core/mojo/mojo.idl

/* eslint-disable @typescript-eslint/no-unused-vars */

declare global {
  enum MojoResult {
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

  interface MojoMapBufferResult {
    buffer: ArrayBuffer;
    result: MojoResult;
  }

  interface MojoHandle {
    mapBuffer(start: number, end: number): MojoMapBufferResult;
  }

  interface MojoCreateSharedBufferResult {
    handle: MojoHandle;
    result: MojoResult;
  }

  const Mojo: typeof MojoResult&{
    createSharedBuffer(numBytes: number): MojoCreateSharedBufferResult,
  };
}

export namespace mojo {
  namespace internal {
    namespace interfaceSupport {

      interface Endpoint {}

      interface PendingReceiver {
        readonly handle: Endpoint;
      }

      function getEndpointForReceiver(handle: Endpoint): Endpoint;

      interface InterfaceRemoteBaseWrapper<T> {
        bindNewPipeAndPassReceiver(): T;
        close(): void;
      }

      interface InterfaceCallbackReceiver {
        addListener(listener: Function): number;
      }

      interface InterfaceReceiverHelper<T> {
        bindNewPipeAndPassRemote(): T;
        close(): void;
      }
    }

    interface MojomType {}
  }
}
