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
import { Script } from '../../bidiProtocolTypes';

import EVALUATOR_SCRIPT from '../../scripts/eval.es';

export class Context {
  _targetInfo?: Protocol.Target.TargetInfo;
  _sessionId?: string;

  private constructor(
    private _contextId: string,
    private _cdpClient: CdpClient
  ) {}

  public static create(contextId: string, cdpClient: CdpClient) {
    const context = new Context(contextId, cdpClient);
    context._initialize();
    return context;
  }

  private _initialize() {
    // TODO: Subscribe to Runtime and Page events to track executionContexts and frames.
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

  public async evaluateScript(script: string): Promise<Script.ScriptEvaluateResult>  {
    // Construct a javascript string that will call the evaluator function
    // with the user script (embedded as a string), and the user arguments
    // (embedded as BiDi RemoteValue objects). The result is a JSON object
    // which will be returned by value and may be either:
    //   { result: <value returned from user script> }
    // or,
    //   { exceptionDetails: { message: '<error message from user script>', stacktrace? } }

    const expression = `(${EVALUATOR_SCRIPT}).evaluate.apply(null, [${JSON.stringify(
      script
    )}, []])`;
    const { result, exceptionDetails } = await this._cdpClient.Runtime.evaluate(
      {
        expression,
        returnByValue: true,
      }
    );

    // Exceptions thrown from the user script are caught and stored in the result
    // returned from Runtime.evaluate. If Runtime.evaluate returned exceptionDetails
    // instead, then this indicates an internal error or bug.
    if (exceptionDetails) {
      throw new Error(exceptionDetails.text);
    }

    // Runtime.evaluate did not return a result for some reason. Could be an
    // internal error, or a failure to parse the user's script.
    if (!result.value) {
      throw new Error('unable to parse result from evaluateScript');
    }

    // Evaluator script ran and either returned a value or threw an error. Return a
    // successful response with the value which will have either a result or
    // exceptionDetails property.
    return result.value;
  }
}
