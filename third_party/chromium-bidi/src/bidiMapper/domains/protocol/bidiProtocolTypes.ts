import { EventResponseClass } from './event';
import { z as zod, ZodType } from 'zod';
import { InvalidArgumentErrorResponse } from './error';
import { log } from '../../../utils/log';

const logParser = log('command parser');

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
  export type RemoteReference = {
    objectId: string;
  };

  export type PrimitiveProtocolValue =
    | UndefinedValue
    | NullValue
    | StringValue
    | NumberValue
    | BooleanValue
    | BigIntValue;

  export type UndefinedValue = {
    type: 'undefined';
  };

  export type NullValue = {
    type: 'null';
  };

  export type StringValue = {
    type: 'string';
    value: string;
  };

  export type SpecialNumber =
    | 'NaN'
    | '-0'
    | 'Infinity'
    | '+Infinity'
    | '-Infinity';

  export type NumberValue = {
    type: 'number';
    value: number | SpecialNumber;
  };

  export type BooleanValue = {
    type: 'boolean';
    value: boolean;
  };

  export type BigIntValue = {
    type: 'bigint';
    value: string;
  };

  export type LocalValue =
    | PrimitiveProtocolValue
    | RemoteReference
    | ArrayLocalValue
    | DateLocalValue
    | MapLocalValue
    | ObjectLocalValue
    | RegExpLocalValue
    | SetLocalValue;

  export type ListLocalValue = LocalValue[];

  export type ArrayLocalValue = {
    type: 'array';
    value: ListLocalValue;
  };

  export type DateLocalValue = {
    type: 'date';
    value: string;
  };

  export type MappingLocalValue = [LocalValue | string, LocalValue][];

  export type MapLocalValue = {
    type: 'map';
    value: MappingLocalValue;
  };

  export type ObjectLocalValue = {
    type: 'object';
    value: MappingLocalValue;
  };

  export type RegExpLocalValue = {
    type: 'regexp';
    value: {
      pattern: string;
      flags?: string;
    };
  };

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
    attributes?: any;
    shadowRoot?: NodeRemoteValue | null;
  };

  export type WindowProxyRemoteValue = RemoteReference & {
    type: 'window';
  };

  // BrowsingContext = text;
  export const BrowsingContextSchema = zod.string().min(1);
  export type BrowsingContext = zod.infer<typeof BrowsingContextSchema>;
}

export namespace Script {
  export type Command = EvaluateCommand | CallFunctionCommand;
  export type CommandResult = EvaluateResult | CallFunctionResult;

  export type Realm = string;

  export type ScriptResult = ScriptResultSuccess | ScriptResultException;
  export type ScriptResultSuccess = {
    result: CommonDataTypes.RemoteValue;
  };

  export type ScriptResultException = {
    exceptionDetails: ExceptionDetails;
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
  export type RealmTarget = zod.infer<typeof RealmTargetSchema>;

  //
  // Target = (
  //   RealmTarget //
  //   ContextTarget
  // );
  const TargetSchema = zod.union([ContextTargetSchema, RealmTargetSchema]);
  export type Target = zod.infer<typeof TargetSchema>;

  // ScriptEvaluateParameters = {
  //   expression: text;
  //   target: Target;
  //   ?awaitPromise: bool;
  //   ?resultOwnership: OwnershipModel;
  // }
  const ScriptEvaluateParametersSchema = zod.object({
    expression: zod.string(),
    awaitPromise: zod.boolean().optional(),
    target: TargetSchema,
  });

  export type EvaluateParameters = zod.infer<
    typeof ScriptEvaluateParametersSchema
  >;

  export function parseEvaluateParameters(params: unknown): EvaluateParameters {
    return parseObject(params, ScriptEvaluateParametersSchema);
  }

  export type EvaluateResult = {
    result: ScriptResult;
  };

  export type CallFunctionCommand = {
    method: 'script.callFunction';
    params: CallFunctionParameters;
  };

  export type CallFunctionParameters = {
    functionDeclaration: string;
    args?: ArgumentValue[];
    this?: ArgumentValue;
    awaitPromise?: boolean;
    target: Target;
  };

  export type CallFunctionResult = {
    result: ScriptResult;
  };

  export type ArgumentValue =
    | CommonDataTypes.RemoteReference
    | CommonDataTypes.LocalValue;

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
    params: BrowsingContextGetTreeParameters;
  };

  export type BrowsingContextGetTreeParameters = {
    maxDepth?: number;
    root?: CommonDataTypes.BrowsingContext;
  };

  export type GetTreeResult = {
    result: {
      contexts: BrowsingContextInfoList;
    };
  };

  export type BrowsingContextInfoList = BrowsingContextInfo[];

  export type BrowsingContextInfo = {
    context: CommonDataTypes.BrowsingContext;
    parent?: CommonDataTypes.BrowsingContext | null;
    url: string;
    children: BrowsingContextInfoList | null;
  };

  export type NavigateCommand = {
    method: 'browsingContext.navigate';
    params: BrowsingContextNavigateParameters;
  };

  export type BrowsingContextNavigateParameters = {
    context: CommonDataTypes.BrowsingContext;
    url: string;
    wait?: ReadinessState;
  };

  export type ReadinessState = 'none' | 'interactive' | 'complete';
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

  export function parseCreateParameters(params: unknown): CreateParameters {
    return parseObject(params, CreateParametersSchema);
  }

  export type CreateResult = {
    result: {
      context: CommonDataTypes.BrowsingContext;
    };
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

  export function parseCloseParameters(params: unknown): CloseParameters {
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

  export class ContextCreatedEvent extends EventResponseClass<BrowsingContextInfo> {
    static readonly method = 'browsingContext.contextCreated';

    constructor(params: BrowsingContext.BrowsingContextInfo) {
      super(ContextCreatedEvent.method, params);
    }
  }

  export class ContextDestroyedEvent extends EventResponseClass<BrowsingContextInfo> {
    static readonly method = 'browsingContext.contextDestroyed';

    constructor(params: BrowsingContextInfo) {
      super(ContextDestroyedEvent.method, params);
    }
  }

  // proto
  export namespace PROTO {
    // `browsingContext.findElement`:
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/67
    export type FindElementCommand = {
      method: 'PROTO.browsingContext.findElement';
      params: BrowsingContextFindElementParameters;
    };

    export type BrowsingContextFindElementParameters = {
      selector: string;
      context: CommonDataTypes.BrowsingContext;
    };

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
      params: SendCdpCommandParams;
    };

    export type SendCdpCommandParams = {
      cdpMethod: string;
      cdpParams: object;
      cdpSession: string;
    };

    export type SendCommandResult = { result: any };

    export type GetSessionCommand = {
      method: 'PROTO.cdp.getSession';
      params: GetSessionParams;
    };

    export type GetSessionParams = {
      context: CommonDataTypes.BrowsingContext;
    };

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

  export function parseSubscribeParameters(
    params: unknown
  ): SubscribeParameters {
    return parseObject(params, SubscribeParametersSchema);
  }

  export type SubscribeResult = { result: {} };

  export type UnsubscribeCommand = {
    method: 'session.unsubscribe';
    params: SubscribeParameters;
  };

  export type UnsubscribeResult = { result: {} };
}
