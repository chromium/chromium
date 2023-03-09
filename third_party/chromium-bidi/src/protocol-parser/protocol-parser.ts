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
 * @file Provides parsing and validator for WebDriver BiDi protocol.
 * Parser types should match the `../protocol` types.
 */

const MAX_INT = 9007199254740991 as const;

import {EventResponse, Message} from '../protocol/protocol.js';
import {ZodType, z as zod} from 'zod';

export function parseObject<T extends ZodType>(
  obj: unknown,
  schema: T
): zod.infer<T> {
  const parseResult = schema.safeParse(obj);
  if (parseResult.success) {
    return parseResult.data;
  }
  const errorMessage = parseResult.error.errors
    .map(
      (e) =>
        `${e.message} in ` +
        `${e.path.map((p) => JSON.stringify(p)).join('/')}.`
    )
    .join(' ');

  throw new Message.InvalidArgumentException(errorMessage);
}

export namespace CommonDataTypes {
  export const SharedReferenceSchema = zod.object({
    sharedId: zod.string().min(1),
  });
  export type SharedReference = zod.infer<typeof SharedReferenceSchema>;

  export const RemoteReferenceSchema = zod.object({
    handle: zod.string().min(1),
  });
  export type RemoteReference = zod.infer<typeof RemoteReferenceSchema>;

  // UndefinedValue = {
  //   type: "undefined",
  // }
  const UndefinedValueSchema = zod.object({type: zod.literal('undefined')});

  // NullValue = {
  //   type: "null",
  // }
  const NullValueSchema = zod.object({type: zod.literal('null')});

  // StringValue = {
  //   type: "string",
  //   value: text,
  // }
  const StringValueSchema = zod.object({
    type: zod.literal('string'),
    value: zod.string(),
  });

  // SpecialNumber = "NaN" / "-0" / "Infinity" / "-Infinity";
  const SpecialNumberSchema = zod.enum(['NaN', '-0', 'Infinity', '-Infinity']);

  // NumberValue = {
  //   type: "number",
  //   value: number / SpecialNumber,
  // }
  const NumberValueSchema = zod.object({
    type: zod.literal('number'),
    value: zod.union([SpecialNumberSchema, zod.number()]),
  });

  // BooleanValue = {
  //   type: "boolean",
  //   value: bool,
  // }
  const BooleanValueSchema = zod.object({
    type: zod.literal('boolean'),
    value: zod.boolean(),
  });

  // BigIntValue = {
  //   type: "bigint",
  //   value: text,
  // }
  const BigIntValueSchema = zod.object({
    type: zod.literal('bigint'),
    value: zod.string(),
  });

  const PrimitiveProtocolValueSchema = zod.union([
    UndefinedValueSchema,
    NullValueSchema,
    StringValueSchema,
    NumberValueSchema,
    BooleanValueSchema,
    BigIntValueSchema,
  ]);

  export type PrimitiveProtocolValue = zod.infer<
    typeof PrimitiveProtocolValueSchema
  >;

  // LocalValue = {
  //   PrimitiveProtocolValue //
  //   ArrayLocalValue //
  //   DateLocalValue //
  //   MapLocalValue //
  //   ObjectLocalValue //
  //   RegExpLocalValue //
  //   SetLocalValue //
  // }
  // `LocalValue` is a recursive type, so lazy declaration is needed:
  // https://github.com/colinhacks/zod#recursive-types
  export type LocalValue =
    | PrimitiveProtocolValue
    | ArrayLocalValue
    | DateLocalValue
    | MapLocalValue
    | ObjectLocalValue
    | RegExpLocalValue
    | SetLocalValue;

  export const LocalValueSchema: zod.ZodType<LocalValue> = zod.lazy(() =>
    zod.union([
      PrimitiveProtocolValueSchema,
      ArrayLocalValueSchema,
      DateLocalValueSchema,
      MapLocalValueSchema,
      ObjectLocalValueSchema,
      RegExpLocalValueSchema,
      SetLocalValueSchema,
    ])
  );

  // Order is important, as `parse` is processed in the same order.
  // `SharedReferenceSchema`->`RemoteReferenceSchema`->`LocalValueSchema`.
  const LocalOrRemoteValueSchema = zod.union([
    SharedReferenceSchema,
    RemoteReferenceSchema,
    LocalValueSchema,
  ]);

  // ListLocalValue = [*LocalValue];
  const ListLocalValueSchema = zod.array(LocalOrRemoteValueSchema);

  // ArrayLocalValue = {
  //   type: "array",
  //   value: ListLocalValue,
  // }
  const ArrayLocalValueSchema: zod.ZodType = zod.lazy(() =>
    zod.object({
      type: zod.literal('array'),
      value: ListLocalValueSchema,
    })
  );
  export type ArrayLocalValue = zod.infer<typeof ArrayLocalValueSchema>;

  // DateLocalValue = {
  //   type: "date",
  //   value: text
  // }
  const DateLocalValueSchema = zod.object({
    type: zod.literal('date'),
    value: zod.string().min(1),
  });
  export type DateLocalValue = zod.infer<typeof DateLocalValueSchema>;

  // MappingLocalValue = [*[(LocalValue / text), LocalValue]];
  const MappingLocalValueSchema: zod.ZodType = zod.lazy(() =>
    zod.tuple([
      zod.union([zod.string(), LocalOrRemoteValueSchema]),
      LocalOrRemoteValueSchema,
    ])
  );

  // MapLocalValue = {
  //   type: "map",
  //   value: MappingLocalValue,
  // }
  const MapLocalValueSchema = zod.object({
    type: zod.literal('map'),
    value: zod.array(MappingLocalValueSchema),
  });
  export type MapLocalValue = zod.infer<typeof MapLocalValueSchema>;

  // ObjectLocalValue = {
  //   type: "object",
  //   value: MappingLocalValue,
  // }
  const ObjectLocalValueSchema = zod.object({
    type: zod.literal('object'),
    value: zod.array(MappingLocalValueSchema),
  });
  export type ObjectLocalValue = zod.infer<typeof ObjectLocalValueSchema>;

  // RegExpLocalValue = {
  //   type: "regexp",
  //   value: RegExpValue,
  // }
  const RegExpLocalValueSchema: zod.ZodType = zod.lazy(() =>
    zod.object({
      type: zod.literal('regexp'),
      value: zod.object({
        pattern: zod.string(),
        flags: zod.string().optional(),
      }),
    })
  );
  export type RegExpLocalValue = zod.infer<typeof RegExpLocalValueSchema>;

  // SetLocalValue = {
  //   type: "set",
  //   value: ListLocalValue,
  // }
  const SetLocalValueSchema: zod.ZodType = zod.lazy(() =>
    zod.object({
      type: zod.literal('set'),
      value: ListLocalValueSchema,
    })
  );
  export type SetLocalValue = zod.infer<typeof SetLocalValueSchema>;

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

  export type NodeRemoteValue = SharedReference &
    RemoteReference & {
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
  export const BrowsingContextSchema = zod.string();
  export type BrowsingContext = zod.infer<typeof BrowsingContextSchema>;
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

  const RealmTypeSchema = zod.enum([
    'window',
    'dedicated-worker',
    'shared-worker',
    'service-worker',
    'worker',
    'paint-worklet',
    'audio-worklet',
    'worklet',
  ]);

  export const GetRealmsParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema.optional(),
    type: RealmTypeSchema.optional(),
  });

  export type GetRealmsParameters = zod.infer<typeof GetRealmsParametersSchema>;

  export function parseGetRealmsParams(params: object): GetRealmsParameters {
    return parseObject(params, GetRealmsParametersSchema);
  }

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
  const ContextTargetSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
    sandbox: zod.string().optional(),
  });

  // RealmTarget = {realm: Realm};
  const RealmTargetSchema = zod.object({
    realm: zod.string().min(1),
  });

  // Target = (
  //   RealmTarget //
  //   ContextTarget
  // );
  // Order is important, as `parse` is processed in the same order.
  // `RealmTargetSchema` has higher priority.
  const TargetSchema = zod.union([RealmTargetSchema, ContextTargetSchema]);

  const OwnershipModelSchema = zod.enum(['root', 'none']);

  // ScriptEvaluateParameters = {
  //   expression: text;
  //   target: Target;
  //   ?awaitPromise: bool;
  //   ?resultOwnership: OwnershipModel;
  // }
  const EvaluateParametersSchema = zod.object({
    expression: zod.string(),
    awaitPromise: zod.boolean(),
    target: TargetSchema,
    resultOwnership: OwnershipModelSchema.optional(),
  });
  export type EvaluateParameters = zod.infer<typeof EvaluateParametersSchema>;

  export function parseEvaluateParams(params: object): EvaluateParameters {
    return parseObject(params, EvaluateParametersSchema);
  }

  export type EvaluateResult = {
    result: ScriptResult;
  };

  export type DisownCommand = {
    method: 'script.disown';
    params: EvaluateParameters;
  };

  const DisownParametersSchema = zod.object({
    target: TargetSchema,
    handles: zod.array(zod.string()),
  });

  export type DisownParameters = zod.infer<typeof DisownParametersSchema>;

  export function parseDisownParams(params: object): DisownParameters {
    return parseObject(params, DisownParametersSchema);
  }

  export type DisownResult = {result: {}};

  export type CallFunctionCommand = {
    method: 'script.callFunction';
    params: CallFunctionParameters;
  };

  const ArgumentValueSchema = zod.union([
    CommonDataTypes.RemoteReferenceSchema,
    CommonDataTypes.SharedReferenceSchema,
    CommonDataTypes.LocalValueSchema,
  ]);

  const ScriptCallFunctionParametersSchema = zod.object({
    functionDeclaration: zod.string(),
    target: TargetSchema,
    arguments: zod.array(ArgumentValueSchema).optional(),
    this: ArgumentValueSchema.optional(),
    awaitPromise: zod.boolean(),
    resultOwnership: OwnershipModelSchema.optional(),
  });

  export type CallFunctionParameters = zod.infer<
    typeof ScriptCallFunctionParametersSchema
  >;

  export function parseCallFunctionParams(
    params: object
  ): CallFunctionParameters {
    return parseObject(params, ScriptCallFunctionParametersSchema);
  }

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

/** @see https://w3c.github.io/webdriver-bidi/#module-browsingContext */
export namespace BrowsingContext {
  export type Command =
    | GetTreeCommand
    | NavigateCommand
    | CreateCommand
    | CloseCommand;
  export type CommandResult =
    | GetTreeResult
    | NavigateResult
    | CreateResult
    | CloseResult;
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

  const GetTreeParametersSchema = zod.object({
    maxDepth: zod.number().int().nonnegative().max(MAX_INT).optional(),
    root: CommonDataTypes.BrowsingContextSchema.optional(),
  });
  export type GetTreeParameters = zod.infer<typeof GetTreeParametersSchema>;

  export function parseGetTreeParams(params: object): GetTreeParameters {
    return parseObject(params, GetTreeParametersSchema);
  }

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

  const ReadinessStateSchema = zod.enum(['none', 'interactive', 'complete']);

  // BrowsingContextNavigateParameters = {
  //   context: BrowsingContext,
  //   url: text,
  //   ?wait: ReadinessState,
  // }
  // ReadinessState = "none" / "interactive" / "complete"
  const NavigateParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
    url: zod.string().url(),
    wait: ReadinessStateSchema.optional(),
  });
  export type NavigateParameters = zod.infer<typeof NavigateParametersSchema>;

  export function parseNavigateParams(params: object): NavigateParameters {
    return parseObject(params, NavigateParametersSchema);
  }

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
  const CreateParametersSchema = zod.object({
    type: zod.enum(['tab', 'window']),
    referenceContext: CommonDataTypes.BrowsingContextSchema.optional(),
  });
  export type CreateParameters = zod.infer<typeof CreateParametersSchema>;

  export function parseCreateParams(params: object): CreateParameters {
    return parseObject(params, CreateParametersSchema);
  }

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
  const CloseParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
  });
  export type CloseParameters = zod.infer<typeof CloseParametersSchema>;

  export function parseCloseParams(params: object): CloseParameters {
    return parseObject(params, CloseParametersSchema);
  }

  export type CloseResult = {result: {}};

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

  export const AllEvents = 'browsingContext';

  export enum EventNames {
    LoadEvent = 'browsingContext.load',
    DomContentLoadedEvent = 'browsingContext.domContentLoaded',
    ContextCreatedEvent = 'browsingContext.contextCreated',
    ContextDestroyedEvent = 'browsingContext.contextDestroyed',
  }
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

  const SendCommandParamsSchema = zod.object({
    cdpMethod: zod.string(),
    // `passthrough` allows object to have any fields.
    // https://github.com/colinhacks/zod#passthrough
    cdpParams: zod.object({}).passthrough(),
    cdpSession: zod.string().optional(),
  });
  export type SendCommandParams = zod.infer<typeof SendCommandParamsSchema>;

  export function parseSendCommandParams(params: object): SendCommandParams {
    return parseObject(params, SendCommandParamsSchema);
  }

  export type SendCommandResult = {result: unknown};

  export type GetSessionCommand = {
    method: 'cdp.getSession';
    params: GetSessionParams;
  };

  const GetSessionParamsSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
  });
  export type GetSessionParams = zod.infer<typeof GetSessionParamsSchema>;

  export function parseGetSessionParams(params: object): GetSessionParams {
    return parseObject(params, GetSessionParamsSchema);
  }

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

  const SubscribeParametersEventSchema = zod.enum([
    BrowsingContext.AllEvents,
    BrowsingContext.EventNames.ContextCreatedEvent,
    BrowsingContext.EventNames.ContextDestroyedEvent,
    BrowsingContext.EventNames.DomContentLoadedEvent,
    BrowsingContext.EventNames.LoadEvent,
    Log.AllEvents,
    Log.EventNames.LogEntryAddedEvent,
    CDP.AllEvents,
    CDP.EventNames.EventReceivedEvent,
  ]);

  // SessionSubscribeParameters = {
  //   events: [*text],
  //   ?contexts: [*BrowsingContext],
  // }
  const SubscribeParametersSchema = zod.object({
    events: zod.array(SubscribeParametersEventSchema),
    contexts: zod.array(CommonDataTypes.BrowsingContextSchema).optional(),
  });
  export type SubscribeParameters = zod.infer<typeof SubscribeParametersSchema>;

  export function parseSubscribeParams(params: object): SubscribeParameters {
    return parseObject(params, SubscribeParametersSchema);
  }

  export type SubscribeResult = {result: {}};

  export type UnsubscribeCommand = {
    method: 'session.unsubscribe';
    params: SubscribeParameters;
  };

  export type UnsubscribeResult = {result: {}};
}
