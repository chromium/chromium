/**
 * Copyright 2022 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @fileoverview Provides TypeScript types for WebDriver BiDi protocol.
 *
 * Note: This file should not have any dependencies because it will be run in the browser.
 * Exception: Type dependencies are fine because they are compiled away.
 */

import type {ProtocolMapping} from 'devtools-protocol/types/protocol-mapping.js';

export interface EventResponse<MethodType, ParamsType> {
  method: MethodType;
  params: ParamsType;
}

export type BiDiMethod =
  // keep-sorted start
  | 'browsingContext.captureScreenshot'
  | 'browsingContext.close'
  | 'browsingContext.create'
  | 'browsingContext.getTree'
  | 'browsingContext.navigate'
  | 'browsingContext.print'
  | 'cdp.getSession'
  | 'cdp.sendCommand'
  | 'cdp.sendMessage'
  | 'script.addPreloadScript'
  | 'script.callFunction'
  | 'script.disown'
  | 'script.evaluate'
  | 'script.getRealms'
  | 'script.removePreloadScript'
  | 'session.status'
  | 'session.subscribe'
  | 'session.unsubscribe';
// keep-sorted end

export type EmptyResult = Record<never, never>;
export type EmptyResultWithCommandId = {id: number} | EmptyResult;

export namespace Message {
  export type OutgoingMessage =
    | CommandResponse
    | EventMessage
    | {launched: true};

  export type RawCommandRequest = {
    id: number;
    method: BiDiMethod;
    params: object;
    channel?: string;
  };

  export type CommandRequest = Pick<RawCommandRequest, 'id'> &
    (BrowsingContext.Command | Script.Command | Session.Command | CDP.Command);

  export type CommandResponse = Pick<RawCommandRequest, 'id'> &
    CommandResponseResult;

  export type CommandResponseResult =
    | BrowsingContext.CommandResult
    | Script.CommandResult
    | Session.CommandResult
    | CDP.CommandResult
    | ErrorResult;

  export type EventMessage =
    // keep-sorted start
    | BrowsingContext.Event
    | CDP.Event
    | Log.Event
    | Network.Event
    | Script.Event;
  // keep-sorted end;

  export type EventNames =
    // keep-sorted start
    | BrowsingContext.EventNames
    | CDP.EventNames
    | Log.EventNames
    | Network.EventNames
    | Script.EventNames;
  // keep-sorted end;

  export enum ErrorCode {
    // keep-sorted start
    InvalidArgument = 'invalid argument',
    InvalidSessionId = 'invalid session id',
    NoSuchAlert = 'no such alert',
    NoSuchFrame = 'no such frame',
    NoSuchNode = 'no such node',
    NoSuchScript = 'no such script',
    SessionNotCreated = 'session not created',
    UnknownCommand = 'unknown command',
    UnknownError = 'unknown error',
    UnsupportedOperation = 'unsupported operation',
    // keep-sorted end
  }

  export type ErrorResult = {
    readonly error: ErrorCode;
    readonly message: string;
    readonly stacktrace?: string;
  };

  export class ErrorResponse implements Message.ErrorResult {
    constructor(
      public error: Message.ErrorCode,
      public message: string,
      public stacktrace?: string
    ) {}

    toErrorResponse(commandId: number): Message.CommandResponse {
      return {
        id: commandId,
        error: this.error,
        message: this.message,
        stacktrace: this.stacktrace,
      };
    }
  }

  export class InvalidArgumentException extends ErrorResponse {
    constructor(message: string, stacktrace?: string) {
      super(ErrorCode.InvalidArgument, message, stacktrace);
    }
  }

  export class InvalidSessionIdException extends ErrorResponse {
    constructor(message: string, stacktrace?: string) {
      super(ErrorCode.InvalidSessionId, message, stacktrace);
    }
  }

  export class NoSuchAlertException extends ErrorResponse {
    constructor(message: string, stacktrace?: string) {
      super(ErrorCode.NoSuchAlert, message, stacktrace);
    }
  }

  export class NoSuchFrameException extends ErrorResponse {
    constructor(message: string) {
      super(ErrorCode.NoSuchFrame, message);
    }
  }

  export class NoSuchNodeException extends ErrorResponse {
    constructor(message: string, stacktrace?: string) {
      super(ErrorCode.NoSuchNode, message, stacktrace);
    }
  }

  export class NoSuchScriptException extends ErrorResponse {
    constructor(message: string, stacktrace?: string) {
      super(ErrorCode.NoSuchScript, message, stacktrace);
    }
  }

  export class SessionNotCreatedException extends ErrorResponse {
    constructor(message: string, stacktrace?: string) {
      super(ErrorCode.SessionNotCreated, message, stacktrace);
    }
  }

  export class UnknownCommandException extends ErrorResponse {
    constructor(message: string, stacktrace?: string) {
      super(ErrorCode.UnknownCommand, message, stacktrace);
    }
  }

  export class UnknownErrorException extends ErrorResponse {
    constructor(message: string, stacktrace?: string) {
      super(ErrorCode.UnknownError, message, stacktrace);
    }
  }

  export class UnsupportedOperationException extends ErrorResponse {
    constructor(message: string, stacktrace?: string) {
      super(ErrorCode.UnsupportedOperation, message, stacktrace);
    }
  }
}

export namespace CommonDataTypes {
  export type RemoteReference = {
    handle: string;
  };

  export type SharedReference = {
    sharedId: string;
  };

  // UndefinedValue = {
  //   type: "undefined",
  // }
  export type UndefinedValue = {
    type: 'undefined';
  };

  // NullValue = {
  //   type: "null",
  // }
  export type NullValue = {
    type: 'null';
  };

  // StringValue = {
  //   type: "string",
  //   value: text,
  // }
  export type StringValue = {
    type: 'string';
    value: string;
  };

  // SpecialNumber = "NaN" / "-0" / "Infinity" / "-Infinity";
  export type SpecialNumber = 'NaN' | '-0' | 'Infinity' | '-Infinity';

  // NumberValue = {
  //   type: "number",
  //   value: number / SpecialNumber,
  // }
  export type NumberValue = {
    type: 'number';
    value: SpecialNumber | number;
  };

  // BooleanValue = {
  //   type: "boolean",
  //   value: bool,
  // }
  export type BooleanValue = {
    type: 'boolean';
    value: boolean;
  };

  // BigIntValue = {
  //   type: "bigint",
  //   value: text,
  // }
  export type BigIntValue = {
    type: 'bigint';
    value: string;
  };

  export type PrimitiveProtocolValue =
    | UndefinedValue
    | NullValue
    | StringValue
    | NumberValue
    | BooleanValue
    | BigIntValue;

  // LocalValue = {
  //   PrimitiveProtocolValue //
  //   ArrayLocalValue //
  //   DateLocalValue //
  //   MapLocalValue //
  //   ObjectLocalValue //
  //   RegExpLocalValue //
  //   SetLocalValue //
  // }
  export type LocalValue =
    | PrimitiveProtocolValue
    | ArrayLocalValue
    | DateLocalValue
    | MapLocalValue
    | ObjectLocalValue
    | RegExpLocalValue
    | SetLocalValue;

  export type LocalOrRemoteValue = RemoteReference | LocalValue;

  // ListLocalValue = [*LocalValue];
  export type ListLocalValue = LocalOrRemoteValue[];

  // ArrayLocalValue = {
  //   type: "array",
  //   value: ListLocalValue,
  // }
  export type ArrayLocalValue = {
    type: 'array';
    value: ListLocalValue;
  };

  // DateLocalValue = {
  //   type: "date",
  //   value: text
  // }
  export type DateLocalValue = {
    type: 'date';
    value: string;
  };

  // MappingLocalValue = [*[(LocalValue / text), LocalValue]];
  export type MappingLocalValue = [
    string | LocalOrRemoteValue,
    LocalOrRemoteValue
  ][];

  // MapLocalValue = {
  //   type: "map",
  //   value: MappingLocalValue,
  // }
  export type MapLocalValue = {
    type: 'map';
    value: MappingLocalValue;
  };

  // ObjectLocalValue = {
  //   type: "object",
  //   value: MappingLocalValue,
  // }
  export type ObjectLocalValue = {
    type: 'object';
    value: MappingLocalValue;
  };

  // RegExpLocalValue = {
  //   type: "regexp",
  //   value: RegExpValue,
  // }
  export type RegExpLocalValue = {
    type: 'regexp';
    value: {
      pattern: string;
      flags?: string;
    };
  };

  // SetLocalValue = {
  //   type: "set",
  //   value: ListLocalValue,
  // }
  export type SetLocalValue = {
    type: 'set';
    value: ListLocalValue;
  };

  export type RemoteValue =
    | PrimitiveProtocolValue
    | SymbolRemoteValue
    | ArrayRemoteValue
    | ObjectRemoteValue
    | FunctionRemoteValue
    | RegExpRemoteValue
    | DateRemoteValue
    | MapRemoteValue
    | SetRemoteValue
    | WeakMapRemoteValue
    | WeakSetRemoteValue
    | IteratorRemoteValue
    | GeneratorRemoteValue
    | ProxyRemoteValue
    | ErrorRemoteValue
    | PromiseRemoteValue
    | TypedArrayRemoteValue
    | ArrayBufferRemoteValue
    | NodeRemoteValue
    | WindowProxyRemoteValue;

  export type ListRemoteValue = RemoteValue[];

  export type MappingRemoteValue = [RemoteValue | string, RemoteValue][];

  export type SymbolRemoteValue = RemoteReference & {
    type: 'symbol';
  };

  export type ArrayRemoteValue = RemoteReference & {
    type: 'array';
    value?: ListRemoteValue;
  };

  export type ObjectRemoteValue = RemoteReference & {
    type: 'object';
    value?: MappingRemoteValue;
  };

  export type FunctionRemoteValue = RemoteReference & {
    type: 'function';
  };

  export type RegExpRemoteValue = RemoteReference & RegExpLocalValue;

  export type DateRemoteValue = RemoteReference & DateLocalValue;

  export type MapRemoteValue = RemoteReference & {
    type: 'map';
    value: MappingRemoteValue;
  };

  export type SetRemoteValue = RemoteReference & {
    type: 'set';
    value: ListRemoteValue;
  };

  export type WeakMapRemoteValue = RemoteReference & {
    type: 'weakmap';
  };

  export type WeakSetRemoteValue = RemoteReference & {
    type: 'weakset';
  };

  export type IteratorRemoteValue = RemoteReference & {
    type: 'iterator';
  };

  export type GeneratorRemoteValue = RemoteReference & {
    type: 'generator';
  };

  export type ProxyRemoteValue = RemoteReference & {
    type: 'proxy';
  };

  export type ErrorRemoteValue = RemoteReference & {
    type: 'error';
  };

  export type PromiseRemoteValue = RemoteReference & {
    type: 'promise';
  };

  export type TypedArrayRemoteValue = RemoteReference & {
    type: 'typedarray';
  };

  export type ArrayBufferRemoteValue = RemoteReference & {
    type: 'arraybuffer';
  };

  export type NodeRemoteValue = RemoteReference & {
    type: 'node';
    value?: NodeProperties;
  };

  export type NodeProperties = RemoteReference & {
    nodeType: number;
    nodeValue: string;
    localName?: string;
    namespaceURI?: string;
    childNodeCount: number;
    children?: [NodeRemoteValue];
    attributes?: Record<string, string>;
    shadowRoot?: NodeRemoteValue | null;
  };

  export type WindowProxyRemoteValue = RemoteReference & {
    type: 'window';
  };

  // BrowsingContext = text;
  export type BrowsingContext = string;
}

/** @see https://w3c.github.io/webdriver-bidi/#module-script */
export namespace Script {
  export type Command =
    | EvaluateCommand
    | CallFunctionCommand
    | GetRealmsCommand
    | DisownCommand
    | AddPreloadScriptCommand
    | RemovePreloadScriptCommand;
  export type CommandResult =
    | EvaluateResult
    | CallFunctionResult
    | GetRealmsResult
    | DisownResult
    | AddPreloadScriptResult
    | RemovePreloadScriptResult;
  export type Event = MessageEvent;

  export type Realm = string;

  export type ScriptResult = ScriptResultSuccess | ScriptResultException;
  export type ScriptResultSuccess = {
    type: 'success';
    result: CommonDataTypes.RemoteValue;
    realm: Realm;
  };

  export type ScriptResultException = {
    exceptionDetails: ExceptionDetails;
    type: 'exception';
    realm: Realm;
  };

  export type ExceptionDetails = {
    columnNumber: number;
    exception: CommonDataTypes.RemoteValue;
    lineNumber: number;
    stackTrace: Script.StackTrace;
    text: string;
  };

  export type RealmInfo =
    | WindowRealmInfo
    | DedicatedWorkerRealmInfo
    | SharedWorkerRealmInfo
    | ServiceWorkerRealmInfo
    | WorkerRealmInfo
    | PaintWorkletRealmInfo
    | AudioWorkletRealmInfo
    | WorkletRealmInfo;

  export type BaseRealmInfo = {
    realm: Realm;
    origin: string;
  };

  export type WindowRealmInfo = BaseRealmInfo & {
    type: 'window';
    context: CommonDataTypes.BrowsingContext;
    sandbox?: string;
  };

  export type DedicatedWorkerRealmInfo = BaseRealmInfo & {
    type: 'dedicated-worker';
  };

  export type SharedWorkerRealmInfo = BaseRealmInfo & {
    type: 'shared-worker';
  };

  export type ServiceWorkerRealmInfo = BaseRealmInfo & {
    type: 'service-worker';
  };

  export type WorkerRealmInfo = BaseRealmInfo & {
    type: 'worker';
  };

  export type PaintWorkletRealmInfo = BaseRealmInfo & {
    type: 'paint-worklet';
  };

  export type AudioWorkletRealmInfo = BaseRealmInfo & {
    type: 'audio-worklet';
  };

  export type WorkletRealmInfo = BaseRealmInfo & {
    type: 'worklet';
  };

  export type RealmType =
    | 'window'
    | 'dedicated-worker'
    | 'shared-worker'
    | 'service-worker'
    | 'worker'
    | 'paint-worklet'
    | 'audio-worklet'
    | 'worklet';

  export type GetRealmsParameters = {
    context?: CommonDataTypes.BrowsingContext;
    type?: RealmType;
  };

  export type GetRealmsCommand = {
    method: 'script.getRealms';
    params: GetRealmsParameters;
  };

  export type GetRealmsResult = {
    result: {realms: RealmInfo[]};
  };

  export type EvaluateCommand = {
    method: 'script.evaluate';
    params: EvaluateParameters;
  };

  // ContextTarget = {
  //   context: BrowsingContext,
  //   ?sandbox: text
  // }
  export type ContextTarget = {
    context: CommonDataTypes.BrowsingContext;
    sandbox?: string;
  };

  // RealmTarget = {realm: Realm};
  export type RealmTarget = {
    realm: Realm;
  };

  // Target = (
  //   RealmTarget //
  //   ContextTarget
  // );
  // Order is important, as `parse` is processed in the same order.
  // `RealmTargetSchema` has higher priority.
  export type Target = RealmTarget | ContextTarget;

  // ResultOwnership = "root" / "none"
  export type ResultOwnership = 'root' | 'none';

  // ScriptEvaluateParameters = {
  //   expression: text;
  //   target: Target;
  //   ?awaitPromise: bool;
  //   ?resultOwnership: ResultOwnership;
  // }
  export type EvaluateParameters = {
    expression: string;
    awaitPromise: boolean;
    target: Target;
    resultOwnership?: ResultOwnership;
  };

  export type EvaluateResult = {
    result: ScriptResult;
  };

  export type DisownCommand = {
    method: 'script.disown';
    params: EvaluateParameters;
  };

  export type DisownParameters = {
    target: Target;
    handles: string[];
  };

  export type DisownResult = {result: Record<string, unknown>};

  export type CallFunctionCommand = {
    method: 'script.callFunction';
    params: CallFunctionParameters;
  };

  export type ArgumentValue =
    | CommonDataTypes.RemoteReference
    | CommonDataTypes.SharedReference
    | CommonDataTypes.LocalValue
    | Script.Channel;

  export type CallFunctionParameters = {
    functionDeclaration: string;
    awaitPromise: boolean;
    target: Target;
    arguments?: ArgumentValue[];
    this?: ArgumentValue;
    resultOwnership?: ResultOwnership;
  };

  export type CallFunctionResult = {
    result: ScriptResult;
  };

  export type Source = {
    realm: Realm;
    context?: CommonDataTypes.BrowsingContext;
  };

  export type StackTrace = {
    callFrames: StackFrame[];
  };

  export type StackFrame = {
    columnNumber: number;
    functionName: string;
    lineNumber: number;
    url: string;
  };

  export type PreloadScript = string;

  export type AddPreloadScriptCommand = {
    method: 'script.addPreloadScript';
    params: AddPreloadScriptParameters;
  };

  export type AddPreloadScriptParameters = {
    expression: string;
    sandbox?: string;
    context?: CommonDataTypes.BrowsingContext;
  };

  export type AddPreloadScriptResult = {
    result: {
      script: PreloadScript;
    };
  };

  export type RemovePreloadScriptCommand = {
    method: 'script.removePreloadScript';
    params: RemovePreloadScriptParameters;
  };

  export type RemovePreloadScriptParameters = {
    script: PreloadScript;
  };

  export type RemovePreloadScriptResult = EmptyResultWithCommandId;

  export type ChannelId = string;

  export type ChannelProperties = {
    channel: ChannelId;
    maxDepth?: number;
    ownership?: ResultOwnership;
  };

  export type Channel = {
    type: 'channel';
    value: ChannelProperties;
  };

  export type Message = {
    method: 'script.message';
    params: MessageParameters;
  };

  export type MessageParameters = {
    channel: ChannelId;
    data: CommonDataTypes.RemoteValue;
    source: Source;
  };

  export type MessageEvent = EventResponse<
    EventNames.MessageEvent,
    Script.MessageParameters
  >;

  export enum EventNames {
    MessageEvent = 'script.message',
  }

  export const AllEvents = 'script';
}

// https://w3c.github.io/webdriver-bidi/#module-browsingContext
export namespace BrowsingContext {
  export type Command =
    | GetTreeCommand
    | NavigateCommand
    | CreateCommand
    | CloseCommand
    | CaptureScreenshotCommand
    | PrintCommand;
  export type CommandResult =
    | GetTreeResult
    | NavigateResult
    | CreateResult
    | CloseResult
    | CaptureScreenshotResult
    | PrintResult;
  export type Event =
    | LoadEvent
    | DomContentLoadedEvent
    | ContextCreatedEvent
    | ContextDestroyedEvent;

  export type Navigation = string;

  export type GetTreeCommand = {
    method: 'browsingContext.getTree';
    params: GetTreeParameters;
  };

  export type GetTreeParameters = {
    maxDepth?: number;
    root?: CommonDataTypes.BrowsingContext;
  };

  export type GetTreeResult = {
    result: {
      contexts: InfoList;
    };
  };

  export type InfoList = Info[];

  export type Info = {
    context: CommonDataTypes.BrowsingContext;
    parent?: CommonDataTypes.BrowsingContext | null;
    url: string;
    children: InfoList | null;
  };

  export type NavigateCommand = {
    method: 'browsingContext.navigate';
    params: NavigateParameters;
  };

  export type ReadinessState = 'none' | 'interactive' | 'complete';

  // BrowsingContextNavigateParameters = {
  //   context: BrowsingContext,
  //   url: text,
  //   ?wait: ReadinessState,
  // }
  // ReadinessState = "none" / "interactive" / "complete"
  export type NavigateParameters = {
    context: CommonDataTypes.BrowsingContext;
    url: string;
    wait?: ReadinessState;
  };

  export type NavigateResult = {
    result: {
      navigation: Navigation | null;
      url: string;
    };
  };

  export type CreateCommand = {
    method: 'browsingContext.create';
    params: CreateParameters;
  };

  // BrowsingContextCreateType = "tab" / "window"
  //
  // BrowsingContextCreateParameters = {
  //   type: BrowsingContextCreateType
  // }
  export type CreateParameters = {
    type: 'tab' | 'window';
    referenceContext?: CommonDataTypes.BrowsingContext;
  };

  export type CreateResult = {
    result: Info;
  };

  export type CloseCommand = {
    method: 'browsingContext.close';
    params: CloseParameters;
  };

  // BrowsingContextCloseParameters = {
  //   context: BrowsingContext
  // }
  export type CloseParameters = {
    context: CommonDataTypes.BrowsingContext;
  };

  export type CloseResult = {result: Record<string, unknown>};

  export type CaptureScreenshotCommand = {
    method: 'browsingContext.captureScreenshot';
    params: CaptureScreenshotParameters;
  };

  export type CaptureScreenshotParameters = {
    context: CommonDataTypes.BrowsingContext;
  };

  export type CaptureScreenshotResult = {
    result: {
      data: string;
    };
  };

  export type PrintCommand = {
    method: 'browsingContext.print';
    params: PrintParameters;
  };

  export type PrintParameters = {
    context: CommonDataTypes.BrowsingContext;
    background?: boolean;
    margin?: PrintMarginParameters;
    orientation?: 'portrait' | 'landscape';
    page?: PrintPageParams;
    pageRanges?: (string | number)[];
    scale?: number;
    shrinkToFit?: boolean;
  };

  // All units are in cm.
  export type PrintMarginParameters = {
    bottom?: number;
    left?: number;
    right?: number;
    top?: number;
  };

  // All units are in cm.
  export type PrintPageParams = {
    height?: number;
    width?: number;
  };

  export type PrintResult = {
    result: {
      data: string;
    };
  };

  export type LoadEvent = EventResponse<EventNames.LoadEvent, NavigationInfo>;

  export type DomContentLoadedEvent = EventResponse<
    EventNames.DomContentLoadedEvent,
    NavigationInfo
  >;

  export type NavigationInfo = {
    context: CommonDataTypes.BrowsingContext;
    navigation: Navigation | null;
    timestamp: number;
    url: string;
  };

  export type ContextCreatedEvent = EventResponse<
    EventNames.ContextCreatedEvent,
    BrowsingContext.Info
  >;
  export type ContextDestroyedEvent = EventResponse<
    EventNames.ContextDestroyedEvent,
    BrowsingContext.Info
  >;

  export enum EventNames {
    LoadEvent = 'browsingContext.load',
    DomContentLoadedEvent = 'browsingContext.domContentLoaded',
    ContextCreatedEvent = 'browsingContext.contextCreated',
    ContextDestroyedEvent = 'browsingContext.contextDestroyed',
  }

  export const AllEvents = 'browsingContext';
}

/** @see https://w3c.github.io/webdriver-bidi/#module-log */
export namespace Log {
  export type LogEntry = GenericLogEntry | ConsoleLogEntry | JavascriptLogEntry;
  export type Event = LogEntryAddedEvent;
  export type LogLevel = 'debug' | 'info' | 'warn' | 'error';

  export type BaseLogEntry = {
    level: LogLevel;
    source: Script.Source;
    text: string | null;
    timestamp: number;
    stackTrace?: Script.StackTrace;
  };

  export type GenericLogEntry = BaseLogEntry & {
    type: string;
  };

  export type ConsoleLogEntry = BaseLogEntry & {
    type: 'console';
    method: string;
    args: CommonDataTypes.RemoteValue[];
  };

  export type JavascriptLogEntry = BaseLogEntry & {
    type: 'javascript';
  };

  export type LogEntryAddedEvent = EventResponse<
    EventNames.LogEntryAddedEvent,
    LogEntry
  >;

  export const AllEvents = 'log';

  export enum EventNames {
    LogEntryAddedEvent = 'log.entryAdded',
  }
}

export namespace Network {
  export type Event =
    | BeforeRequestSentEvent
    | ResponseCompletedEvent
    | FetchErrorEvent;

  export type BeforeRequestSentEvent = EventResponse<
    EventNames.BeforeRequestSentEvent,
    BeforeRequestSentParams
  >;

  export type ResponseCompletedEvent = EventResponse<
    EventNames.ResponseCompletedEvent,
    ResponseCompletedParams
  >;

  export type FetchErrorEvent = EventResponse<
    EventNames.FetchErrorEvent,
    FetchErrorParams
  >;

  type Header = {
    name: string;
    value?: string;
    binaryValue?: number[];
  };

  export type Cookie = {
    name: string;
    value?: string;
    binaryValue?: number[];
    domain: string;
    path: string;
    expires?: number;
    size: number;
    httpOnly: boolean;
    secure: boolean;
    sameSite: 'strict' | 'lax' | 'none';
  };

  type FetchTimingInfo = {
    timeOrigin: number;
    requestTime: number;
    redirectStart: number;
    redirectEnd: number;
    fetchStart: number;
    dnsStart: number;
    dnsEnd: number;
    connectStart: number;
    connectEnd: number;
    tlsStart: number;
    tlsEnd: number;
    requestStart: number;
    responseStart: number;
    responseEnd: number;
  };

  export type RequestData = {
    request: string;
    url: string;
    method: string;
    headers: Header[];
    cookies: Cookie[];
    headersSize: number;
    bodySize: number | null;
    timings: FetchTimingInfo;
  };

  export type BaseEventParams = {
    context: string | null;
    navigation: BrowsingContext.Navigation | null;
    redirectCount: number;
    request: RequestData;
    timestamp: number;
  };

  export type Initiator = {
    type: 'parser' | 'script' | 'preflight' | 'other';
    columnNumber?: number;
    lineNumber?: number;
    stackTrace?: Script.StackTrace;
    request?: Request;
  };

  export type ResponseContent = {
    size: number;
  };

  export type ResponseData = {
    url: string;
    protocol: string;
    status: number;
    statusText: string;
    fromCache: boolean;
    headers: Header[];
    mimeType: string;
    bytesReceived: number;
    headersSize: number | null;
    bodySize: number | null;
    content: ResponseContent;
  };

  export type BeforeRequestSentParams = BaseEventParams & {
    initiator: Initiator;
  };

  export type ResponseCompletedParams = BaseEventParams & {
    response: ResponseData;
  };

  export type FetchErrorParams = BaseEventParams & {
    errorText: string;
  };

  export const AllEvents = 'network';

  export enum EventNames {
    BeforeRequestSentEvent = 'network.beforeRequestSent',
    ResponseCompletedEvent = 'network.responseCompleted',
    FetchErrorEvent = 'network.fetchError',
  }
}

export namespace CDP {
  export type Command = SendCommandCommand | GetSessionCommand;
  export type CommandResult = SendCommandResult | GetSessionResult;
  export type Event = EventReceivedEvent;

  export type SendCommandCommand = {
    method: 'cdp.sendCommand';
    params: SendCommandParams;
  };

  export type SendCommandParams = {
    cdpMethod: keyof ProtocolMapping.Commands;
    cdpParams: object;
    cdpSession?: any;
  };

  export type SendCommandResult = {result: unknown};

  export type GetSessionCommand = {
    method: 'cdp.getSession';
    params: GetSessionParams;
  };

  export type GetSessionParams = {
    context: CommonDataTypes.BrowsingContext;
  };

  export type GetSessionResult = {result: {session: string}};

  export type EventReceivedEvent = EventResponse<
    EventNames.EventReceivedEvent,
    EventReceivedParams
  >;

  export type EventReceivedParams = {
    cdpMethod: string;
    cdpParams: object;
    cdpSession: string;
  };

  export const AllEvents = 'cdp';

  export enum EventNames {
    EventReceivedEvent = 'cdp.eventReceived',
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-session */
export namespace Session {
  export type Command = StatusCommand | SubscribeCommand | UnsubscribeCommand;

  export type CommandResult =
    | StatusResult
    | SubscribeResult
    | UnsubscribeResult;

  export type StatusCommand = {
    method: 'session.status';
    params: Record<string, unknown>;
  };

  export type StatusResult = {
    result: {
      ready: boolean;
      message: string;
    };
  };

  export type SubscribeCommand = {
    method: 'session.subscribe';
    params: SubscribeParameters;
  };

  export type SubscribeParametersEvent =
    // keep-sorted start
    | Message.EventNames
    | typeof BrowsingContext.AllEvents
    | typeof CDP.AllEvents
    | typeof Log.AllEvents
    | typeof Network.AllEvents
    | typeof Script.AllEvents;
  // keep-sorted end;

  // SessionSubscribeParameters = {
  //   events: [*text],
  //   ?contexts: [*BrowsingContext],
  // }
  export type SubscribeParameters = {
    events: SubscribeParametersEvent[];
    contexts?: CommonDataTypes.BrowsingContext[];
  };

  export type SubscribeResult = {result: Record<string, unknown>};

  export type UnsubscribeCommand = {
    method: 'session.unsubscribe';
    params: SubscribeParameters;
  };

  export type UnsubscribeResult = {result: Record<string, unknown>};
}
