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

import { Protocol } from 'devtools-protocol';
import { CdpClient } from '../../../cdp';
import { CommonDataTypes } from '../../bidiProtocolTypes';

export class Serializer {
  private _callbackName = '__cdpBindingCallback';

  private constructor(
    private _cdpClient: CdpClient,
    private _EVALUATOR_SCRIPT: string
  ) {}

  public static create(cdpClient: CdpClient, EVALUATOR_SCRIPT: string) {
    const serializer = new Serializer(cdpClient, EVALUATOR_SCRIPT);
    return serializer;
  }

  // TODO sadym: Add binding only once.
  // TODO sadym: `dummyContextObject` needed for the running context.
  // Use the proper `executionContextId` instead:
  // https://github.com/GoogleChromeLabs/chromium-bidi/issues/52
  public async getDummyContextId(): Promise<string> {
    await this._cdpClient.Runtime.addBinding({ name: this._callbackName });

    const dummyContextObject = await this._cdpClient.Runtime.evaluate({
      expression: '(()=>{return {}})()',
    });
    return dummyContextObject.result.objectId!;
  }

  /**
   * Serializes a given CDP object into BiDi, keeping references in the
   * target's `globalThis`.
   * @param cdpObject CDP remote object to be serialized.
   */
  public async serializeCdpObject(
    cdpObject: Protocol.Runtime.RemoteObject
  ): Promise<CommonDataTypes.RemoteValue> {
    const response = await this._cdpClient.Runtime.callFunctionOn({
      functionDeclaration: `${this._EVALUATOR_SCRIPT}.serialize`,
      objectId: await this.getDummyContextId(),
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

  /**
   * Gets the string representation of an exception. This is equivalent to
   * calling toString() on the exception value. If the exception is an Error,
   * this will be the Error's message property.
   * @param exception CDP remote object representing a thrown exception.
   * @returns string The stringified exception message.
   */
  public async stringifyCdpException(
    exception: Protocol.Runtime.RemoteObject
  ): Promise<string | undefined> {
    const response = await this._cdpClient.Runtime.callFunctionOn({
      functionDeclaration: String(function (exception: unknown) {
        return String(exception);
      }),
      objectId: await this.getDummyContextId(),
      arguments: [exception],
      returnByValue: true,
    });

    return response.result.value;
  }
}
