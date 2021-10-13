export namespace CommonDataTypes {
  // TODO sadym: declare `RemoteValue` properly according to
  // https://w3c.github.io/webdriver-bidi/#type-common-RemoteValue.
  export type RemoteValue = any;

  export interface ExceptionDetails {
    columnNumber: number;
    exception: CommonDataTypes.RemoteValue;
    lineNumber: number;
    stackTrace: StackTrace;
    text: string;
  }
  export interface StackTrace {
    callFrames: StackFrame[];
  }
  export interface StackFrame {
    url: string;
    functionName: string;
    lineNumber: number;
    columnNumber: number;
  }
}
export namespace Script {
  export interface RealTarget {
    // TODO sadym: implement.
  }
  export interface ContextTarget {
    context: BrowsingContext.BrowsingContext;
  }
  export type Target = ContextTarget | RealTarget;

  export interface ScriptExceptionResult {
    exceptionDetails: CommonDataTypes.ExceptionDetails;
  }

  export type ScriptEvaluateResult =
    | ScriptEvaluateSuccessResult
    | ScriptExceptionResult;

  export interface ScriptEvaluateSuccessResult {
    result: CommonDataTypes.RemoteValue;
  }

  export interface ScriptEvaluateParameters {
    expression: string;
    awaitPromise?: boolean;
    target: Target;
  }

  export namespace PROTO {
    export interface ScriptInvokeParameters {
      functionDeclaration: string;
      args: InvokeArgument[];
      awaitPromise?: boolean;
      target: Target;
    }

    export type ScriptInvokeResult =
      | ScriptInvokeSuccessResult
      | ScriptExceptionResult;

    export interface ScriptInvokeSuccessResult {
      result: CommonDataTypes.RemoteValue;
    }

    export type InvokeArgument = RemoteValueArgument | LocalValueArgument;

    export interface RemoteValueArgument {
      objectId: string;
    }

    export interface LocalValueArgument {
      type: string;
      value: any;
    }
  }
}

// https://w3c.github.io/webdriver-bidi/#module-browsingContext
export namespace BrowsingContext {
  export type BrowsingContext = string;
}
