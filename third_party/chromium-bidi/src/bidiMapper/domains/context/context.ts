/**
 * Copyright 2021 Google LLC.
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

import { Protocol } from 'devtools-protocol';
import { CdpClient } from '../../../cdp';
import {
  BrowsingContext,
  CommonDataTypes,
  Script,
} from '../../bidiProtocolTypes';
import { IBidiServer } from '../../utils/bidiServer';

export class Context {
  _targetInfo?: Protocol.Target.TargetInfo;
  _sessionId?: string;

  private _dummyContextObjectId: string = '';
  // As `script.evaluate` wraps call into serialization script, `lineNumber`
  // should be adjusted.
  private _evaluateStacktraceLineOffset = 1;
  private _callFunctionStacktraceLineOffset = 1;

  private constructor(
    private _contextId: string,
    private _cdpClient: CdpClient,
    private _bidiServer: IBidiServer,
    private EVALUATOR_SCRIPT: string
  ) {}

  public static async create(
    contextId: string,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    EVALUATOR_SCRIPT: string
  ) {
    const context = new Context(
      contextId,
      cdpClient,
      bidiServer,
      EVALUATOR_SCRIPT
    );
    await context._initialize();
    return context;
  }

  private async _initialize() {
    // Enabling Runtime doamin needed to have an exception stacktrace in
    // `evaluateScript`.
    await this._cdpClient.Runtime.enable();
    await this._cdpClient.Page.enable();
    await this._cdpClient.Page.setLifecycleEventsEnabled({ enabled: true });

    // TODO sadym: `dummyContextObject` needed for the running context.
    // Use the proper `executionContextId` instead:
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/52
    const dummyContextObject = await this._cdpClient.Runtime.evaluate({
      expression: '(()=>{return {}})()',
    });
    this._dummyContextObjectId = dummyContextObject.result.objectId!;
  }

  _setSessionId(sessionId: string): void {
    this._sessionId = sessionId;
  }

  _updateTargetInfo(targetInfo: Protocol.Target.TargetInfo) {
    this._targetInfo = targetInfo;
  }

  _onInfoChangedEvent(targetInfo: Protocol.Target.TargetInfo) {
    this._updateTargetInfo(targetInfo);
  }

  public get id(): string {
    return this._contextId;
  }

  toBidi(): BrowsingContext.BrowsingContextInfo {
    return {
      context: this._targetInfo!.targetId,
      parent: this._targetInfo!.openerId
        ? this._targetInfo!.openerId
        : undefined,
      url: this._targetInfo!.url,
      // TODO sadym: implement.
      children: [],
    };
  }

  public async navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.BrowsingContextNavigateResult> {
    // TODO sadym: implement.
    if (wait !== 'none' && wait !== 'interactive') {
      throw new Error(`Not implenented wait '${wait}'`);
    }

    const cdpNavigateResult = await this._cdpClient.Page.navigate({ url });

    // Wait: none.
    if (wait === 'none')
      return {
        navigation: cdpNavigateResult.loaderId,
        url: url,
      };

    //Wait: interactive.
    const domContentEventFiredPromise =
      new Promise<BrowsingContext.BrowsingContextNavigateResult>((resolve) => {
        const handleDomContentEventFired = async (
          params: Protocol.Page.DomContentEventFiredEvent
        ) => {
          this._cdpClient.Page.removeListener(
            'domContentEventFired',
            handleDomContentEventFired
          );
          resolve({
            navigation: cdpNavigateResult.loaderId,
            url: url,
          });
        };

        this._cdpClient.Page.on(
          'domContentEventFired',
          handleDomContentEventFired
        );
      });

    return domContentEventFiredPromise;
  }

  /**
   * Serializes a given CDP object into BiDi, keeping references in the
   * target's `globalThis`.
   * @param cdpObject CDP remote object to be serialized.
   */
  private async _serializeCdpObject(
    cdpObject: Protocol.Runtime.RemoteObject
  ): Promise<CommonDataTypes.RemoteValue> {
    const response = await this._cdpClient.Runtime.callFunctionOn({
      functionDeclaration: `${this.EVALUATOR_SCRIPT}.serialize`,
      objectId: this._dummyContextObjectId,
      arguments: [cdpObject],
      returnByValue: true,
    });

    if (response.exceptionDetails)
      // Serialization failed unexpectidely.
      throw new Error(
        'Cannot serialize object: ' + response.exceptionDetails.text
      );

    return response.result.value;
  }

  private async _serializeCdpExceptionDetails(
    cdpExceptionDetails: Protocol.Runtime.ExceptionDetails,
    lineOffset: number
  ) {
    const callFrames = cdpExceptionDetails.stackTrace?.callFrames.map(
      (frame) => ({
        url: frame.url,
        functionName: frame.functionName,
        // As `script.evaluate` wraps call into serialization script, so
        // `lineNumber` should be adjusted.
        lineNumber: frame.lineNumber - lineOffset,
        columnNumber: frame.columnNumber,
      })
    );
    const exception = await this._serializeCdpObject(
      // Exception should always be there.
      cdpExceptionDetails.exception!
    );

    return {
      exceptionDetails: {
        exception,
        columnNumber: cdpExceptionDetails.columnNumber,
        // As `script.evaluate` wraps call into serialization script, so
        // `lineNumber` should be adjusted.
        lineNumber: cdpExceptionDetails.lineNumber - lineOffset,
        stackTrace: {
          callFrames: callFrames || [],
        },
        text: cdpExceptionDetails.text,
      },
    };
  }

  public async scriptEvaluate(
    expression: string,
    awaitPromise: boolean
  ): Promise<Script.ScriptEvaluateResult> {
    // The call puts the expression first to keep the stacktrace not dependent
    // on the`EVALUATOR_SCRIPT` length in case of exception. Based on
    // `awaitPromise`, `_serialize` function will wait for the result, or
    // serialize promise as-is.
    // TODO sadym: add error handling for serialization errors.
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/57
    const evalAndSerializeScript = `_serialize(\n${expression}\n);
      async function _serialize(value){
        if(${awaitPromise ? 'true' : 'false'}
            && value instanceof Promise) {
          value = await value;
        }
        return (${this.EVALUATOR_SCRIPT})
          .serialize.apply(null, [value])
      }`;

    const cdpEvaluateResult = await this._cdpClient.Runtime.evaluate({
      expression: evalAndSerializeScript,
      // Always wait for the result of `_serialize`. Wait or not for the user's
      // expression is handled in the `_serialize` function.
      awaitPromise: true,
      returnByValue: true,
    });
    if (cdpEvaluateResult.exceptionDetails) {
      // Serialize exception details.
      return await this._serializeCdpExceptionDetails(
        cdpEvaluateResult.exceptionDetails,
        this._evaluateStacktraceLineOffset
      );
    }

    return {
      result: cdpEvaluateResult.result.value!,
    };
  }

  public async callFunction(
    functionDeclaration: string,
    args: Script.PROTO.CallFunctionArgument[],
    awaitPromise: boolean
  ): Promise<Script.PROTO.ScriptCallFunctionResult> {
    // TODO sadym: add error handling for serialization/deserialization errors.
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/57
    const callFunctionAndSerializeScript = `async (...serializedArgs)=>{ return _callFunction(\n${functionDeclaration}\n, serializedArgs);
      async function _callFunction(f, serializedArgs) {
        const evaluator = (${this.EVALUATOR_SCRIPT});
        const deserializedArgs = serializedArgs.map(evaluator.deserialize);
        let resultValue = f.apply(this, deserializedArgs);
        if(${awaitPromise ? 'true' : 'false'}
            && resultValue instanceof Promise) {
          resultValue = await resultValue;
        }
        return evaluator.serialize(resultValue);
      }}`;

    const cdpCallFunctionResult = await this._cdpClient.Runtime.callFunctionOn({
      functionDeclaration: callFunctionAndSerializeScript,
      arguments: args.map((a) => ({ value: a })),
      awaitPromise: true,
      returnByValue: true,
      objectId: this._dummyContextObjectId,
    });

    if (cdpCallFunctionResult.exceptionDetails) {
      // Serialize exception details.
      return await this._serializeCdpExceptionDetails(
        cdpCallFunctionResult.exceptionDetails,
        this._callFunctionStacktraceLineOffset
      );
    }

    return {
      result: cdpCallFunctionResult.result.value,
    };
  }
}
