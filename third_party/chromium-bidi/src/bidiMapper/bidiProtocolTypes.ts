export namespace CommonDataTypes {
  export type EmptyParams = {};

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
  export type CommandType = ScriptEvaluateCommand | ScriptCallFunctionCommand;
  export type ResultType = ScriptEvaluateResult | ScriptCallFunctionResult;

  export type RealmTarget = {
    // TODO sadym: implement.
  };

  export type ContextTarget = {
    context: BrowsingContext.BrowsingContext;
  };

  export type Target = ContextTarget | RealmTarget;

  export type ScriptEvaluateCommand = {
    method: 'script.evaluate';
    params: ScriptEvaluateParameters;
  };

  export type ScriptEvaluateParameters = {
    expression: string;
    awaitPromise?: boolean;
    target: Target;
  };

  export type ScriptEvaluateResult =
    | ScriptEvaluateSuccessResult
    | ScriptExceptionResult;

  export type ScriptEvaluateSuccessResult = {
    result: CommonDataTypes.RemoteValue;
  };

  export type ScriptExceptionResult = {
    exceptionDetails: CommonDataTypes.ExceptionDetails;
  };

  export type ScriptCallFunctionCommand = {
    method: 'script.callFunction';
    params: ScriptCallFunctionParameters;
  };

  export type ScriptCallFunctionParameters = {
    functionDeclaration: string;
    args?: ArgumentValue[];
    this?: ArgumentValue;
    awaitPromise?: boolean;
    target: Target;
  };

  export type ScriptCallFunctionResult =
    | ScriptCallFunctionSuccessResult
    | ScriptExceptionResult;

  export type ScriptCallFunctionSuccessResult = {
    result: CommonDataTypes.RemoteValue;
  };

  export type ArgumentValue =
    | CommonDataTypes.RemoteReference
    | CommonDataTypes.LocalValue;
}

// https://w3c.github.io/webdriver-bidi/#module-browsingContext
export namespace BrowsingContext {
  export type CommandType =
    | BrowsingContextGetTreeCommand
    | BrowsingContextNavigateCommand
    | BrowsingContextCreateCommand;
  export type ResultType =
    | BrowsingContextGetTreeResult
    | BrowsingContextNavigateResult
    | BrowsingContextCreateResult;
  export type EventType =
    | BrowsingContextLoadEvent
    | BrowsingContextDomContentLoadedEvent
    | BrowsingContextCreatedEvent
    | BrowsingContextDestroyedEvent;

  export type BrowsingContext = string;
  export type Navigation = string;

  export type BrowsingContextGetTreeCommand = {
    method: 'browsingContext.getTree';
    params: BrowsingContextGetTreeParameters;
  };

  export type BrowsingContextGetTreeParameters = {
    maxDepth?: number;
    parent?: BrowsingContext;
  };

  export type BrowsingContextGetTreeResult = {
    contexts: BrowsingContextInfoList;
  };

  export type BrowsingContextInfoList = BrowsingContextInfo[];

  export type BrowsingContextInfo = {
    context: BrowsingContext;
    parent?: BrowsingContext;
    url: string;
    children: BrowsingContextInfoList;
  };

  export type BrowsingContextNavigateCommand = {
    method: 'browsingContext.navigate';
    params: BrowsingContextNavigateParameters;
  };

  export type BrowsingContextNavigateParameters = {
    context: BrowsingContext;
    url: string;
    wait?: ReadinessState;
  };

  export type ReadinessState = 'none' | 'interactive' | 'complete';
  export type BrowsingContextNavigateResult = {
    navigation?: Navigation;
    url: string;
  };

  export type BrowsingContextCreateCommand = {
    method: 'browsingContext.create';
    params: BrowsingContextCreateParameters;
  };

  export type BrowsingContextCreateType = 'tab' | 'window';

  export type BrowsingContextCreateParameters = {
    type?: BrowsingContextCreateType;
  };

  export type BrowsingContextCreateResult = {
    context: BrowsingContext;
  };

  // events
  export type BrowsingContextLoadEvent = {
    method: 'browsingContext.load';
    params: NavigationInfo;
  };

  export type BrowsingContextDomContentLoadedEvent = {
    method: 'browsingContext.domContentLoaded';
    params: NavigationInfo;
  };

  export type NavigationInfo = {
    context: BrowsingContext;
    navigation: Navigation | null;
    // TODO: implement or remove from specification.
    // url: string;
  };

  export type BrowsingContextCreatedEvent = {
    method: 'browsingContext.contextCreated';
    params: BrowsingContextInfo;
  };

  export type BrowsingContextDestroyedEvent = {
    method: 'browsingContext.contextDestroyed';
    params: BrowsingContextInfo;
  };

  // proto
  export namespace PROTO {
    // `browsingContext.findElement`:
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/67
    export type BrowsingContextFindElementCommand = {
      method: 'PROTO.browsingContext.findElement';
      params: BrowsingContextFindElementParameters;
    };

    export type BrowsingContextFindElementParameters = {
      selector: string;
      context: BrowsingContext;
    };

    export type BrowsingContextFindElementResult =
      | BrowsingContextFindElementSuccessResult
      | BrowsingContextFindElementExceptionResult;

    export type BrowsingContextFindElementSuccessResult = {
      result: CommonDataTypes.NodeRemoteValue;
    };

    export type BrowsingContextFindElementExceptionResult = {
      exceptionDetails: CommonDataTypes.ExceptionDetails;
    };
  }
}

export namespace Session {
  export type CommandType = SessionStatusCommand;
  export type ResultType = SessionStatusResult;

  export type SessionStatusCommand = {
    method: 'session.status';
    params: CommonDataTypes.EmptyParams;
  };

  export type SessionStatusResult = {
    ready: boolean;
    message: string;
  };
}
