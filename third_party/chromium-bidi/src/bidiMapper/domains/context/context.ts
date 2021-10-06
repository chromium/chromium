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
import { Script, CommonDataTypes } from '../../bidiProtocolTypes';

import EVALUATOR_SCRIPT from '../../scripts/eval.es';

export class Context {
  _targetInfo?: Protocol.Target.TargetInfo;
  _sessionId?: string;

  private _dummyContextObjectId: string = '';
  // As `script.evaluate` wraps call into serialisation script, `lineNumber`
  // should be adjusted.
  private _evaluateStacktraceLineOffset = 1;

  private constructor(
    private _contextId: string,
    private _cdpClient: CdpClient
  ) {}

  public static async create(contextId: string, cdpClient: CdpClient) {
    const context = new Context(contextId, cdpClient);
    await context._initialize();
    return context;
  }

  private async _initialize() {
    // Enabling Runtime doamin needed to have an exception stacktrace in
    // `evaluateScript`.
    await this._cdpClient.Runtime.enable();

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

  toBidi() {
    return {
      context: this._targetInfo!.targetId,
      parent: this._targetInfo!.openerId ? this._targetInfo!.openerId : null,
      url: this._targetInfo!.url,
    };
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
      functionDeclaration: `${EVALUATOR_SCRIPT}.serialize`,
      objectId: this._dummyContextObjectId,
      arguments: [cdpObject],
      returnByValue: true,
    });

    if (response.exceptionDetails)
      // Serialisation failed unexpectidely.
      throw new Error(
        'Cannot serialize object: ' + response.exceptionDetails.text
      );

    return response.result.value;
  }

  private async _serializeCdpExceptionDetails(
    cdpExceptionDetails: Protocol.Runtime.ExceptionDetails
  ) {
    const callFrames = cdpExceptionDetails.stackTrace?.callFrames.map(
      (frame) => ({
        url: frame.url,
        functionName: frame.functionName,
        // As `script.evaluate` wraps call into serialisation script, so
        // `lineNumber` should be adjusted.
        lineNumber: frame.lineNumber - this._evaluateStacktraceLineOffset,
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
        // As `script.evaluate` wraps call into serialisation script, so
        // `lineNumber` should be adjusted.
        lineNumber:
          cdpExceptionDetails.lineNumber - this._evaluateStacktraceLineOffset,
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
    const evalAndSerialiseScript = `_serialize(\n${expression}\n);
      async function _serialize(value){
        return (${EVALUATOR_SCRIPT})
          .serialize.apply(null, [${awaitPromise ? 'await' : ''} value])
      }`;

    const cdpEvaluateResult = await this._cdpClient.Runtime.evaluate({
      expression: evalAndSerialiseScript,
      // Always wait for the result of `_serialize`. Wait or not for the user's
      // expression is handled in the `_serialize` function.
      awaitPromise: true,
      returnByValue: true,
    });
    if (cdpEvaluateResult.exceptionDetails) {
      // Serialize exception details.
      return await this._serializeCdpExceptionDetails(
        cdpEvaluateResult.exceptionDetails
      );
    }

    return {
      result: cdpEvaluateResult.result.value!,
    };
  }
}
