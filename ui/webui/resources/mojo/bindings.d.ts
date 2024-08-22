// Copyright 2021 The Chromium Authors
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
/* eslint-disable @typescript-eslint/naming-convention */

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

  namespace Mojo {
    // The following constants already belong to the `MojoResult` enum, but code
    // already references these constants as Mojo.MOJO_RESULT_NAME. To preserve
    // that functionality, redefine these as constants in the Mojo namespace
    // here. This allows us to keep the `Mojo` a namespace (the alternative is a
    // undefined const variable typed to &MojoResult with additional APIs, but
    // this causes issues with clients using the bindings as externs only as
    // calling the APIs results in 'Mojo is undefined' errors).
    const RESULT_OK = MojoResult.RESULT_OK;
    const RESULT_CANCELLED = MojoResult.RESULT_CANCELLED;
    const RESULT_UNKNOWN = MojoResult.RESULT_UNKNOWN;
    const RESULT_INVALID_ARGUMENT = MojoResult.RESULT_INVALID_ARGUMENT;
    const RESULT_DEADLINE_EXCEEDED = MojoResult.RESULT_DEADLINE_EXCEEDED;
    const RESULT_NOT_FOUND = MojoResult.RESULT_NOT_FOUND;
    const RESULT_ALREADY_EXISTS = MojoResult.RESULT_ALREADY_EXISTS;
    const RESULT_PERMISSION_DENIED = MojoResult.RESULT_PERMISSION_DENIED;
    const RESULT_RESOURCE_EXHAUSTED = MojoResult.RESULT_RESOURCE_EXHAUSTED;
    const RESULT_FAILED_PRECONDITION = MojoResult.RESULT_FAILED_PRECONDITION;
    const RESULT_ABORTED = MojoResult.RESULT_ABORTED;
    const RESULT_OUT_OF_RANGE = MojoResult.RESULT_OUT_OF_RANGE;
    const RESULT_UNIMPLEMENTED = MojoResult.RESULT_UNIMPLEMENTED;
    const RESULT_INTERNAL = MojoResult.RESULT_INTERNAL;
    const RESULT_UNAVAILABLE = MojoResult.RESULT_UNAVAILABLE;
    const RESULT_DATA_LOSS = MojoResult.RESULT_DATA_LOSS;
    const RESULT_BUSY = MojoResult.RESULT_BUSY;
    const RESULT_SHOULD_WAIT = MojoResult.RESULT_SHOULD_WAIT;
  }

  interface MojoHandleSignals {
    readable?: boolean;
    writable?: boolean;
    peerClosed?: boolean;
  }

  type MojoWatchCallback = (result: MojoResult) => void;

  interface MojoWatcher {
    cancel(): MojoResult;
  }

  interface MojoReadMessageFlags {
    mayDiscard: boolean;
  }

  interface MojoReadMessageResult {
    result: MojoResult;
    buffer: ArrayBuffer;
    handles: MojoHandle[];
  }

  interface MojoMapBufferResult {
    buffer: ArrayBuffer;
    result: MojoResult;
  }

  interface MojoHandle {
    close(): void;
    watch(signals: MojoHandleSignals, callback: MojoWatchCallback): MojoWatcher;
    writeMessage(buffer: BufferSource, handles: MojoHandle[]): MojoResult;
    readMessage(flags?: MojoReadMessageFlags): MojoReadMessageResult;
    mapBuffer(start: number, end: number): MojoMapBufferResult;
  }

  interface MojoCreateMessagePipeResult {
    result: MojoResult;
    handle0: MojoHandle;
    handle1: MojoHandle;
  }

  interface MojoCreateSharedBufferResult {
    handle: MojoHandle;
    result: MojoResult;
  }

  namespace Mojo {
    function createMessagePipe(): MojoCreateMessagePipeResult;
    function createSharedBuffer(numBytes: number): MojoCreateSharedBufferResult;
    function bindInterface(
        interfaceName: string, requestHandle: MojoHandle, scope?: string): void;
  }

  interface MojoInterfaceRequestEvent {
    handle: MojoHandle;
  }

  class MojoInterfaceInterceptor {
    constructor(interfaceName: string);
    start(): void;
    stop(): void;
    oninterfacerequest(e: MojoInterfaceRequestEvent): void;
  }
}

export namespace mojo {
  namespace internal {
    namespace interfaceSupport {
      interface Endpoint {}

      function getEndpointForReceiver(handle: MojoHandle|Endpoint): Endpoint;

      function bind(handle: Endpoint, name: string, scope: string): void;

      interface ConnectionErrorEventRouter {
        addListener(listener: Function): number;
        removeListener(id: number): boolean;
        dispatchErrorEvent(): void;
      }

      interface PendingReceiver {
        readonly handle: Endpoint;
      }

      type RequestType = new(handle: Endpoint) => PendingReceiver;

      class InterfaceRemoteBase<T> {
        constructor(requestType: RequestType, handle: Endpoint|undefined);
        get endpoint(): Endpoint;
        bindNewPipeAndPassReceiver(): PendingReceiver;
        bindHandle(handle: MojoHandle|Endpoint): void;
        associateAndPassReceiver(): PendingReceiver;
        unbind(): void;
        close(): void;
        getConnectionErrorEventRouter(): ConnectionErrorEventRouter;
        sendMessage(
            ordinal: number, paramStruct: mojo.internal.MojomType,
            maybeResponseStruct: mojo.internal.MojomType|null,
            args: any[]): Promise<any>;
      }

      class InterfaceRemoteBaseWrapper<T> {
        constructor(remote: InterfaceRemoteBase<T>);
        bindNewPipeAndPassReceiver(): T;
        associateAndPassReceiver(): T;
        isBound(): boolean;
        close(): void;
        flushForTesting(): Promise<void>;
      }

      class CallbackRouter {
        getNextId(): number;
        removeListener(id: number): boolean;
      }

      class InterfaceCallbackReceiver {
        constructor(router: CallbackRouter);
        addListener(listener: Function): number;
        createReceiverHandler(expectsResponse: boolean): Function;
      }

      type RemoteType<T> = new(handle: MojoHandle|Endpoint) => T;

      class InterfaceReceiverHelperInternal<T> {
        constructor(remoteType: RemoteType<T>);
        registerHandler(
            ordinal: number, paramStruct: mojo.internal.MojomType,
            responseStruct: mojo.internal.MojomType|null,
            handler: Function): void;
        getConnectionErrorEventRouter(): ConnectionErrorEventRouter;
      }

      class InterfaceReceiverHelper<T> {
        constructor(helper: InterfaceReceiverHelperInternal<T>);
        bindHandle(handle: MojoHandle|Endpoint): void;
        bindNewPipeAndPassRemote(): T;
        associateAndPassRemote(): T;
        close(): void;
        flushForTesting(): Promise<void>;
      }
    }

    interface MojomType {}
    class Bool implements MojomType {}
    class Int8 implements MojomType {}
    class Uint8 implements MojomType {}
    class Int16 implements MojomType {}
    class Uint16 implements MojomType {}
    class Int32 implements MojomType {}
    class Uint32 implements MojomType {}
    class Int64 implements MojomType {}
    class Uint64 implements MojomType {}
    class Float implements MojomType {}
    class Double implements MojomType {}
    class Handle implements MojomType {}
    class String implements MojomType {}

    function Array(elementType: MojomType, elementNullable: boolean): MojomType;
    function Map(
        keyType: MojomType, valueType: MojomType,
        valueNullable: boolean): MojomType;
    function Enum(): MojomType;

    interface NullableValueKindProperties {
      isPrimary: boolean;
      linkedValueFieldName?: string;
      originalFieldName: string;
    }

    interface StructFieldSpec {
      name: string;
      packedOffset: number;
      packedBitOffset: number;
      type: MojomType;
      defaultValue: any;
      nullable: boolean;
      minVersion: number;
      nullableValueKindProperties?: NullableValueKindProperties;
    }

    function createStructDeserializer(structMojomType: mojo.internal.MojomType):
        (dataView: DataView) => {
          [key: string]: any,
        };


    function StructField(
        name: string, packedOffset: number, packedBitOffset: number,
        type: MojomType, defaultValue: any, nullable: boolean,
        minVersion?: number,
        nullableValueKindProperites?: NullableValueKindProperties):
        StructFieldSpec;

    function Struct(
        objectToBlessAsType: object, name: string, fields: StructFieldSpec[],
        versionData: number[][]): void;

    class TypemapAdapter<MappedType, MojoType> {
      constructor(
          toMojoTypeFn: (mappedType: MappedType) => MojoType,
          toMappedTypeFn: (mojoType: MojoType) => MappedType,
      );
    }

    function TypemappedStruct<MappedType, MojoType>(
        objectToBlessAsType: object, name: string,
        adapter: TypemapAdapter<MappedType, MojoType>,
        fields: StructFieldSpec[], versionData: number[][]): void;

    interface UnionFieldSpec {
      name?: string;
      ordinal: number;
      nullable?: boolean;
      type: MojomType;
    }

    function Union(
        objectToBlessAsUnion: object, name: string,
        fields: {[key: string]: any}): void;

    function InterfaceProxy(type: {name: string}): MojomType;
    function InterfaceRequest(type: {name: string}): MojomType;
    function AssociatedInterfaceProxy(type: {name: string}): MojomType;
    function AssociatedInterfaceRequest(type: {name: string}): MojomType;
  }
}
