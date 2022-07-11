import { EventResponseClass } from './event';
import { z as zod, ZodType } from 'zod';
import { InvalidArgumentErrorResponse } from './error';
import { log } from '../../../utils/log';

const logParser = log('command parser');
const MAX_INT = 9007199254740991 as const;

function parseObject<T extends ZodType>(obj: unknown, schema: T): zod.infer<T> {
  const parseResult = schema.safeParse(obj);
  if (parseResult.success) {
    return parseResult.data;
  }
  logParser(
    `Command ${JSON.stringify(obj)} parse failed: ${JSON.stringify(
      parseResult
    )}.`
  );

  const errorMessage = parseResult.error.errors
    .map(
      (e) =>
        `${e.message} in ` +
        `${e.path.map((p) => JSON.stringify(p)).join('/')}.`
    )
    .join(' ');

  throw new InvalidArgumentErrorResponse(errorMessage);
}

export namespace Message {
  export type OutgoingMessage = CommandResponse | EventMessage;

  export type RawCommandRequest = {
    id: number;
    method: string;
    params: object;
  };

  export type CommandRequest = { id: number } & (
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
    | 'no such frame';

  export type ErrorResult = {
    readonly error: ErrorCode;
    readonly message: string;
    readonly stacktrace?: string;
  };
}

export namespace CommonDataTypes {
  export const RemoteReferenceSchema = zod.object({
    handle: zod.string().min(1),
  });
  export type RemoteReference = zod.infer<typeof RemoteReferenceSchema>;

  // UndefinedValue = {
  //   type: "undefined",
  // }
  const UndefinedValueSchema = zod.object({ type: zod.literal('undefined') });

  //
  // NullValue = {
  //   type: "null",
  // }
  const NullValueSchema = zod.object({ type: zod.literal('null') });

  // StringValue = {
  //   type: "string",
  //   value: text,
  // }
  const StringValueSchema = zod.object({
    type: zod.literal('string'),
    value: zod.string(),
  });

  // SpecialNumber = "NaN" / "-0" / "+Infinity" / "-Infinity";
  const SpecialNumberSchema = zod.enum([
    'NaN',
    '-0',
    'Infinity',
    '+Infinity',
    '-Infinity',
  ]);

  //
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

  // ListLocalValue = [*LocalValue];
  const ListLocalValueSchema = zod.array(LocalValueSchema);
  export type ListLocalValue = zod.infer<typeof ListLocalValueSchema>;

  // ArrayLocalValue = {
  //   type: "array",
  //   value: ListLocalValue,
  // }
  const ArrayLocalValueSchema: any = zod.lazy(() =>
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
  const MappingLocalValueSchema: any = zod.lazy(() =>
    zod.tuple([zod.union([zod.string(), LocalValueSchema]), LocalValueSchema])
  );
  export type MappingLocalValue = zod.infer<typeof MappingLocalValueSchema>;

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
  const RegExpLocalValueSchema: any = zod.lazy(() =>
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
  const SetLocalValueSchema: any = zod.lazy(() =>
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
    attributes?: any;
    shadowRoot?: NodeRemoteValue | null;
  };

  export type WindowProxyRemoteValue = RemoteReference & {
    type: 'window';
  };

  // BrowsingContext = text;
  export const BrowsingContextSchema = zod.string();
  export type BrowsingContext = zod.infer<typeof BrowsingContextSchema>;
}

export namespace Script {
  export type Command = EvaluateCommand | CallFunctionCommand;
  export type CommandResult = EvaluateResult | CallFunctionResult;

  export type Realm = string;

  export type ScriptResult = ScriptResultSuccess | ScriptResultException;
  export type ScriptResultSuccess = {
    result: CommonDataTypes.RemoteValue;
    realm: string;
  };

  export type ScriptResultException = {
    exceptionDetails: ExceptionDetails;
    realm: string;
  };

  export type ExceptionDetails = {
    columnNumber: number;
    exception: CommonDataTypes.RemoteValue;
    lineNumber: number;
    stackTrace: Script.StackTrace;
    text: string;
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
  export type ContextTarget = zod.infer<typeof ContextTargetSchema>;

  // RealmTarget = {realm: Realm};
  const RealmTargetSchema = zod.object({
    realm: zod.string().min(1),
  });

  //
  // Target = (
  //   RealmTarget //
  //   ContextTarget
  // );
  const TargetSchema = zod.union([ContextTargetSchema, RealmTargetSchema]);
  export type Target = zod.infer<typeof TargetSchema>;

  const OwnershipModelSchema = zod.enum(['root', 'none']);
  export type OwnershipModel = zod.infer<typeof OwnershipModelSchema>;

  // ScriptEvaluateParameters = {
  //   expression: text;
  //   target: Target;
  //   ?awaitPromise: bool;
  //   ?resultOwnership: OwnershipModel;
  // }
  const ScriptEvaluateParametersSchema = zod.object({
    expression: zod.string(),
    awaitPromise: zod.boolean(),
    target: TargetSchema,
    resultOwnership: OwnershipModelSchema.optional(),
  });

  export type EvaluateParameters = zod.infer<
    typeof ScriptEvaluateParametersSchema
  >;

  export function parseEvaluateParams(params: unknown): EvaluateParameters {
    return parseObject(params, ScriptEvaluateParametersSchema);
  }

  export type EvaluateResult = {
    result: ScriptResult;
  };

  export type CallFunctionCommand = {
    method: 'script.callFunction';
    params: CallFunctionParameters;
  };

  const ArgumentValueSchema = zod.union([
    CommonDataTypes.RemoteReferenceSchema,
    CommonDataTypes.LocalValueSchema,
  ]);
  export type ArgumentValue = zod.infer<typeof ArgumentValueSchema>;

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
    params: unknown
  ): CallFunctionParameters {
    return parseObject(params, ScriptCallFunctionParametersSchema);
  }

  export type CallFunctionResult = {
    result: ScriptResult;
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
    | PROTO.FindElementCommand;
  export type CommandResult =
    | GetTreeResult
    | NavigateResult
    | CreateResult
    | CloseResult
    | PROTO.FindElementResult;
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

  export function parseGetTreeParams(params: unknown): GetTreeParameters {
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
  export type ReadinessState = zod.infer<typeof ReadinessStateSchema>;

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

  export function parseNavigateParams(params: unknown): NavigateParameters {
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
  });
  export type CreateParameters = zod.infer<typeof CreateParametersSchema>;

  export function parseCreateParams(params: unknown): CreateParameters {
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

  export function parseCloseParams(params: unknown): CloseParameters {
    return parseObject(params, CloseParametersSchema);
  }

  export type CloseResult = { result: {} };

  // events
  export class LoadEvent extends EventResponseClass<NavigationInfo> {
    static readonly method = 'browsingContext.load';

    constructor(params: BrowsingContext.NavigationInfo) {
      super(LoadEvent.method, params);
    }
  }

  export class DomContentLoadedEvent extends EventResponseClass<NavigationInfo> {
    static readonly method = 'browsingContext.domContentLoaded';

    constructor(params: BrowsingContext.NavigationInfo) {
      super(DomContentLoadedEvent.method, params);
    }
  }

  export type NavigationInfo = {
    context: CommonDataTypes.BrowsingContext;
    navigation: Navigation | null;
    // TODO: implement or remove from specification.
    // url: string;
  };

  export class ContextCreatedEvent extends EventResponseClass<BrowsingContext.Info> {
    static readonly method = 'browsingContext.contextCreated';

    constructor(params: BrowsingContext.Info) {
      super(ContextCreatedEvent.method, params);
    }
  }

  export class ContextDestroyedEvent extends EventResponseClass<BrowsingContext.Info> {
    static readonly method = 'browsingContext.contextDestroyed';

    constructor(params: BrowsingContext.Info) {
      super(ContextDestroyedEvent.method, params);
    }
  }

  // proto
  export namespace PROTO {
    // `browsingContext.findElement`:
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/67
    export type FindElementCommand = {
      method: 'PROTO.browsingContext.findElement';
      params: FindElementParameters;
    };

    const FindElementParametersSchema = zod.object({
      context: CommonDataTypes.BrowsingContextSchema,
      selector: zod.string(),
    });
    export type FindElementParameters = zod.infer<
      typeof FindElementParametersSchema
    >;

    export function parseFindElementParams(
      params: unknown
    ): FindElementParameters {
      return parseObject(params, FindElementParametersSchema);
    }

    export type FindElementResult = FindElementSuccessResult;

    export type FindElementSuccessResult = {
      result: CommonDataTypes.NodeRemoteValue;
    };
  }

  export const EventNames = [
    LoadEvent.method,
    DomContentLoadedEvent.method,
    ContextCreatedEvent.method,
    ContextDestroyedEvent.method,
  ] as const;
}

// https://w3c.github.io/webdriver-bidi/#module-log
export namespace Log {
  export type LogEntry = GenericLogEntry | ConsoleLogEntry | JavascriptLogEntry;
  export type Event = LogEntryAddedEvent;
  export type LogLevel = 'debug' | 'info' | 'warning' | 'error';

  export type BaseLogEntry = {
    level: LogLevel;
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
    realm: Script.Realm;
    args: CommonDataTypes.RemoteValue[];
  };

  export type JavascriptLogEntry = BaseLogEntry & {
    type: 'javascript';
  };

  export class LogEntryAddedEvent extends EventResponseClass<LogEntry> {
    static readonly method = 'log.entryAdded';

    constructor(params: LogEntry) {
      super(LogEntryAddedEvent.method, params);
    }
  }

  export const EventNames = [LogEntryAddedEvent.method] as const;
}

export namespace CDP {
  export type Command = PROTO.SendCommandCommand | PROTO.GetSessionCommand;
  export type CommandResult = PROTO.SendCommandResult | PROTO.GetSessionResult;
  export type Event = PROTO.EventReceivedEvent;

  export namespace PROTO {
    export type SendCommandCommand = {
      method: 'PROTO.cdp.sendCommand';
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

    export function parseSendCommandParams(params: unknown): SendCommandParams {
      return parseObject(params, SendCommandParamsSchema);
    }

    export type SendCommandResult = { result: any };

    export type GetSessionCommand = {
      method: 'PROTO.cdp.getSession';
      params: GetSessionParams;
    };

    const GetSessionParamsSchema = zod.object({
      context: CommonDataTypes.BrowsingContextSchema,
    });
    export type GetSessionParams = zod.infer<typeof GetSessionParamsSchema>;

    export function parseGetSessionParams(params: unknown): GetSessionParams {
      return parseObject(params, GetSessionParamsSchema);
    }

    export type GetSessionResult = { result: { session: string } };

    export class EventReceivedEvent extends EventResponseClass<EventReceivedParams> {
      static readonly method = 'PROTO.cdp.eventReceived';

      constructor(params: EventReceivedParams) {
        super(EventReceivedEvent.method, params);
      }
    }

    export type EventReceivedParams = {
      cdpMethod: string;
      cdpParams: object;
      session?: string;
    };
  }
  export const EventNames = [PROTO.EventReceivedEvent.method] as const;
}

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

  const EventNameSchema = zod.enum([
    ...BrowsingContext.EventNames,
    ...Log.EventNames,
    ...CDP.EventNames,
  ]);

  // SessionSubscribeParameters = {
  //   events: [*text],
  //   ?contexts: [*BrowsingContext],
  // }
  const SubscribeParametersSchema = zod.object({
    events: zod.array(EventNameSchema),
    contexts: zod.array(CommonDataTypes.BrowsingContextSchema).optional(),
  });
  export type SubscribeParameters = zod.infer<typeof SubscribeParametersSchema>;

  export function parseSubscribeParams(params: unknown): SubscribeParameters {
    return parseObject(params, SubscribeParametersSchema);
  }

  export type SubscribeResult = { result: {} };

  export type UnsubscribeCommand = {
    method: 'session.unsubscribe';
    params: SubscribeParameters;
  };

  export type UnsubscribeResult = { result: {} };
}
