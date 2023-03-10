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
  | 'browsingContext.captureScreenshot'
  | 'browsingContext.close'
  | 'browsingContext.create'
  | 'browsingContext.getTree'
  | 'browsingContext.navigate'
  | 'cdp.getSession'
  | 'cdp.sendCommand'
  | 'cdp.sendMessage'
  | 'script.callFunction'
  | 'script.disown'
  | 'script.evaluate'
  | 'script.getRealms'
  | 'session.status'
  | 'session.subscribe'
  | 'session.unsubscribe';

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

  export type CommandRequest = {id: number} & (
    | BrowsingContext.Command
    | Script.Command
    | Session.Command
    | CDP.Command
  );

  export type CommandResponse = {
    id: number;
  } & CommandResponseResult;

  export type CommandResponseResult =
    | BrowsingContext.CommandResult
    | Script.CommandResult
    | Session.CommandResult
    | CDP.CommandResult
    | ErrorResult;

  export type EventMessage = BrowsingContext.Event | Log.Event | CDP.Event;

  export type ErrorCode =
    | 'unknown error'
    | 'unknown command'
    | 'invalid argument'
    | 'no such frame'
    | 'no such node';

  export type ErrorResult = {
    readonly error: ErrorCode;
    readonly message: string;
    readonly stacktrace?: string;
  };

  export class ErrorResponseClass implements Message.ErrorResult {
    protected constructor(
      error: Message.ErrorCode,
      message: string,
      stacktrace?: string
    ) {
      this.error = error;
      this.message = message;
      this.stacktrace = stacktrace;
    }

    readonly error: Message.ErrorCode;
    readonly message: string;
    readonly stacktrace?: string;

    toErrorResponse(commandId: number): Message.CommandResponse {
      return {
        id: commandId,
        error: this.error,
        message: this.message,
        stacktrace: this.stacktrace,
      };
    }
  }

  export class UnknownException extends ErrorResponseClass {
    constructor(message: string, stacktrace?: string) {
      super('unknown error', message, stacktrace);
    }
  }

  export class UnknownCommandException extends ErrorResponseClass {
    constructor(message: string, stacktrace?: string) {
      super('unknown command', message, stacktrace);
    }
  }

  export class InvalidArgumentException extends ErrorResponseClass {
    constructor(message: string, stacktrace?: string) {
      super('invalid argument', message, stacktrace);
    }
  }

  export class NoSuchNodeException extends ErrorResponseClass {
    constructor(message: string, stacktrace?: string) {
      super('no such node', message, stacktrace);
    }
  }

  export class NoSuchFrameException extends ErrorResponseClass {
    constructor(message: string) {
      super('no such frame', message);
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
    | DisownCommand;
  export type CommandResult =
    | EvaluateResult
    | CallFunctionResult
    | GetRealmsResult
    | DisownResult;

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

  export type OwnershipModel = 'root' | 'none';

  // ScriptEvaluateParameters = {
  //   expression: text;
  //   target: Target;
  //   ?awaitPromise: bool;
  //   ?resultOwnership: OwnershipModel;
  // }
  export type EvaluateParameters = {
    expression: string;
    awaitPromise: boolean;
    target: Target;
    resultOwnership?: OwnershipModel;
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

  export type DisownResult = {result: {}};

  export type CallFunctionCommand = {
    method: 'script.callFunction';
    params: CallFunctionParameters;
  };

  export type ArgumentValue =
    | CommonDataTypes.RemoteReference
    | CommonDataTypes.SharedReference
    | CommonDataTypes.LocalValue;

  export type CallFunctionParameters = {
    functionDeclaration: string;
    awaitPromise: boolean;
    target: Target;
    arguments?: ArgumentValue[];
    this?: ArgumentValue;
    resultOwnership?: OwnershipModel;
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
}

// https://w3c.github.io/webdriver-bidi/#module-browsingContext
export namespace BrowsingContext {
  export type Command =
    | GetTreeCommand
    | NavigateCommand
    | CreateCommand
    | CloseCommand
    | CaptureScreenshotCommand;
  export type CommandResult =
    | GetTreeResult
    | NavigateResult
    | CreateResult
    | CloseResult
    | CaptureScreenshotResult;
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

  export type CloseResult = {result: {}};

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
    params: {};
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
    | BrowsingContext.EventNames
    | typeof BrowsingContext.AllEvents
    | Log.EventNames
    | typeof Log.AllEvents
    | CDP.EventNames
    | typeof CDP.AllEvents;

  // SessionSubscribeParameters = {
  //   events: [*text],
  //   ?contexts: [*BrowsingContext],
  // }
  export type SubscribeParameters = {
    events: SubscribeParametersEvent[];
    contexts?: CommonDataTypes.BrowsingContext[];
  };

  export type SubscribeResult = {result: {}};

  export type UnsubscribeCommand = {
    method: 'session.unsubscribe';
    params: SubscribeParameters;
  };

  export type UnsubscribeResult = {result: {}};
}
