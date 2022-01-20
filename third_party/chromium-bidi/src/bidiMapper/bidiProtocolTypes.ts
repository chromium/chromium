export namespace Message {
  export type OutgoingMessage = CommandResponse | Event | Error;

  export type Command = { id: number } & (
    | BrowsingContext.Command
    | Script.Command
    | Session.Command
  );

  export type CommandResponse = {
    id: number;
  } & CommandResponseResult;

  export type CommandResponseResult =
    | BrowsingContext.CommandResult
    | Script.CommandResult
    | Session.CommandResult;

  export type Event = BrowsingContext.Event | Script.Event;

  export type Error = {
    id?: number;
    error: string;
    message: string;
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

  export type SpecialNumber = 'NaN' | '-0' | '+Infinity' | '-Infinity';

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
    pattern: string;
    flags?: string;
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

  export type ExceptionDetails = {
    columnNumber: number;
    exception: CommonDataTypes.RemoteValue;
    lineNumber: number;
    stackTrace: StackTrace;
    text: string;
  };

  export type StackTrace = {
    callFrames: StackFrame[];
  };

  export type StackFrame = {
    url: string;
    functionName: string;
    lineNumber: number;
    columnNumber: number;
  };
}

export namespace Script {
  export type Command = EvaluateCommand | CallFunctionCommand;
  export type CommandResult = EvaluateResult | CallFunctionResult;
  export type Event = PROTO.CalledEvent;

  export type RealmTarget = {
    // TODO sadym: implement.
  };

  export type ContextTarget = {
    context: BrowsingContext.BrowsingContext;
  };

  export type Target = ContextTarget | RealmTarget;

  export type EvaluateCommand = {
    method: 'script.evaluate';
    params: ScriptEvaluateParameters;
  };

  export type ScriptEvaluateParameters = {
    expression: string;
    awaitPromise?: boolean;
    target: Target;
  };

  export type EvaluateResult = EvaluateSuccessResult | EvaluateExceptionResult;
  export type EvaluateSuccessResult = {
    result: CommonDataTypes.RemoteValue;
  };
  export type EvaluateExceptionResult = {
    exceptionDetails: CommonDataTypes.ExceptionDetails;
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

  export type CallFunctionResult =
    | CallFunctionSuccessResult
    | CallFunctionExceptionResult;
  export type CallFunctionSuccessResult = {
    result: CommonDataTypes.RemoteValue;
  };
  export type CallFunctionExceptionResult = {
    exceptionDetails: CommonDataTypes.ExceptionDetails;
  };

  export type ArgumentValue =
    | CommonDataTypes.RemoteReference
    | CommonDataTypes.LocalValue;

  export namespace PROTO {
    export type CalledEvent = {
      method: 'PROTO.script.called';
      params: CalledEventParams;
    };

    export type CalledEventParams = {
      id: string;
      arguments: CommonDataTypes.RemoteValue[];
    };
  }
}

// https://w3c.github.io/webdriver-bidi/#module-browsingContext
export namespace BrowsingContext {
  export type Command =
    | GetTreeCommand
    | NavigateCommand
    | CreateCommand
    | PROTO.FindElementCommand
    | PROTO.CloseCommand;
  export type CommandResult =
    | GetTreeResult
    | NavigateResult
    | CreateResult
    | PROTO.FindElementResult
    | PROTO.CloseResult;
  export type Event =
    | LoadEvent
    | DomContentLoadedEvent
    | CreatedEvent
    | DestroyedEvent;

  export type BrowsingContext = string;
  export type Navigation = string;

  export type GetTreeCommand = {
    method: 'browsingContext.getTree';
    params: BrowsingContextGetTreeParameters;
  };

  export type BrowsingContextGetTreeParameters = {
    maxDepth?: number;
    parent?: BrowsingContext;
  };

  export type GetTreeResult = {
    result: {
      contexts: BrowsingContextInfoList;
    };
  };

  export type BrowsingContextInfoList = BrowsingContextInfo[];

  export type BrowsingContextInfo = {
    context: BrowsingContext;
    parent?: BrowsingContext;
    url: string;
    children: BrowsingContextInfoList;
  };

  export type NavigateCommand = {
    method: 'browsingContext.navigate';
    params: BrowsingContextNavigateParameters;
  };

  export type BrowsingContextNavigateParameters = {
    context: BrowsingContext;
    url: string;
    wait?: ReadinessState;
  };

  export type ReadinessState = 'none' | 'interactive' | 'complete';
  export type NavigateResult = {
    result: {
      navigation?: Navigation;
      url: string;
    };
  };

  export type CreateCommand = {
    method: 'browsingContext.create';
    params: CreateParameters;
  };

  export type CreateParametersType = 'tab' | 'window';

  export type CreateParameters = {
    type?: CreateParametersType;
  };

  export type CreateResult = {
    result: {
      context: BrowsingContext;
    };
  };

  // events
  export type LoadEvent = {
    method: 'browsingContext.load';
    params: NavigationInfo;
  };

  export type DomContentLoadedEvent = {
    method: 'browsingContext.domContentLoaded';
    params: NavigationInfo;
  };

  export type NavigationInfo = {
    context: BrowsingContext;
    navigation: Navigation | null;
    // TODO: implement or remove from specification.
    // url: string;
  };

  export type CreatedEvent = {
    method: 'browsingContext.contextCreated';
    params: BrowsingContextInfo;
  };

  export type DestroyedEvent = {
    method: 'browsingContext.contextDestroyed';
    params: BrowsingContextInfo;
  };

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
      context: BrowsingContext;
    };

    export type FindElementResult = {
      result: CommonDataTypes.NodeRemoteValue;
    };

    export type CloseCommand = {
      method: 'PROTO.browsingContext.close';
      params: CloseParameters;
    };

    export type CloseParameters = {
      context: BrowsingContext;
    };

    export type CloseResult = { result: {} };
  }
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

  export type SubscribeParameters = {
    events: string[];
    contexts?: BrowsingContext.BrowsingContext[];
  };

  export type SubscribeResult = { result: {} };

  export type UnsubscribeCommand = {
    method: 'session.unsubscribe';
    params: SubscribeParameters;
  };

  export type UnsubscribeResult = { result: {} };
}
