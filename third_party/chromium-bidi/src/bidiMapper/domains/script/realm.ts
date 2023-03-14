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

import {Protocol} from 'devtools-protocol';

import {CommonDataTypes, Script} from '../../../protocol/protocol.js';
import {BrowsingContextStorage} from '../context/browsingContextStorage.js';
import {CdpClient} from '../../CdpConnection.js';

import {
  SHARED_ID_DIVIDER,
  ScriptEvaluator,
  stringifyObject,
} from './scriptEvaluator.js';
import {RealmStorage} from './realmStorage.js';

export type RealmType = Script.RealmType;

const scriptEvaluator = new ScriptEvaluator();

export class Realm {
  readonly #realmStorage: RealmStorage;
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #realmId: string;
  readonly #browsingContextId: string;
  readonly #executionContextId: Protocol.Runtime.ExecutionContextId;
  readonly #origin: string;
  readonly #type: RealmType;
  readonly #cdpClient: CdpClient;

  readonly sandbox?: string;
  readonly cdpSessionId: string;

  constructor(
    realmStorage: RealmStorage,
    browsingContextStorage: BrowsingContextStorage,
    realmId: string,
    browsingContextId: string,
    executionContextId: Protocol.Runtime.ExecutionContextId,
    origin: string,
    type: RealmType,
    sandbox: string | undefined,
    cdpSessionId: string,
    cdpClient: CdpClient
  ) {
    this.#realmId = realmId;
    this.#browsingContextId = browsingContextId;
    this.#executionContextId = executionContextId;
    this.sandbox = sandbox;
    this.#origin = origin;
    this.#type = type;
    this.cdpSessionId = cdpSessionId;
    this.#cdpClient = cdpClient;
    this.#realmStorage = realmStorage;
    this.#browsingContextStorage = browsingContextStorage;

    this.#realmStorage.realmMap.set(this.#realmId, this);
  }

  async disown(handle: string): Promise<void> {
    // Disowning an object from different realm does nothing.
    if (this.#realmStorage.knownHandlesToRealm.get(handle) !== this.realmId) {
      return;
    }
    try {
      await this.cdpClient.sendCommand('Runtime.releaseObject', {
        objectId: handle,
      });
    } catch (e: any) {
      // Heuristic to determine if the problem is in the unknown handler.
      // Ignore the error if so.
      if (!(e.code === -32000 && e.message === 'Invalid remote object id')) {
        throw e;
      }
    }
    this.#realmStorage.knownHandlesToRealm.delete(handle);
  }

  async cdpToBidiValue(
    cdpValue:
      | Protocol.Runtime.CallFunctionOnResponse
      | Protocol.Runtime.EvaluateResponse,
    resultOwnership: Script.OwnershipModel
  ): Promise<CommonDataTypes.RemoteValue> {
    const cdpWebDriverValue = cdpValue.result.webDriverValue!;
    const bidiValue = this.webDriverValueToBiDi(cdpWebDriverValue);

    if (cdpValue.result.objectId) {
      const objectId = cdpValue.result.objectId;
      if (resultOwnership === 'root') {
        // Extend BiDi value with `handle` based on required `resultOwnership`
        // and  CDP response but not on the actual BiDi type.
        (bidiValue as any).handle = objectId;
        // Remember all the handles sent to client.
        this.#realmStorage.knownHandlesToRealm.set(objectId, this.realmId);
      } else {
        // No need in awaiting for the object to be released.
        this.cdpClient.sendCommand('Runtime.releaseObject', {objectId});
      }
    }

    return bidiValue;
  }

  webDriverValueToBiDi(
    webDriverValue: Protocol.Runtime.WebDriverValue
  ): CommonDataTypes.RemoteValue {
    // This relies on the CDP to implement proper BiDi serialization, except
    // backendNodeId/sharedId.
    const result = webDriverValue as any;
    const bidiValue = result.value;
    if (bidiValue === undefined) {
      return result;
    }

    if (result.type === 'node') {
      if (Object.hasOwn(bidiValue, 'backendNodeId')) {
        bidiValue.sharedId = `${this.navigableId}${SHARED_ID_DIVIDER}${bidiValue.backendNodeId}`;
        delete bidiValue['backendNodeId'];
      }
      if (Object.hasOwn(bidiValue, 'children')) {
        for (const i in bidiValue.children) {
          bidiValue.children[i] = this.webDriverValueToBiDi(
            bidiValue.children[i]
          );
        }
      }
    }

    // Recursively update the nested values.
    if (['array', 'set'].includes(webDriverValue.type)) {
      for (const i in bidiValue) {
        bidiValue[i] = this.webDriverValueToBiDi(bidiValue[i]);
      }
    }
    if (['object', 'map'].includes(webDriverValue.type)) {
      for (const i in bidiValue) {
        bidiValue[i] = [
          this.webDriverValueToBiDi(bidiValue[i][0]),
          this.webDriverValueToBiDi(bidiValue[i][1]),
        ];
      }
    }

    return result;
  }

  toBiDi(): Script.RealmInfo {
    return {
      realm: this.realmId,
      origin: this.origin,
      type: this.type,
      context: this.browsingContextId,
      ...(this.sandbox === undefined ? {} : {sandbox: this.sandbox}),
    };
  }

  get realmId(): string {
    return this.#realmId;
  }

  get navigableId(): string {
    return (
      this.#browsingContextStorage.findContext(this.#browsingContextId)
        ?.navigableId ?? 'UNKNOWN'
    );
  }

  get browsingContextId(): string {
    return this.#browsingContextId;
  }

  get executionContextId(): Protocol.Runtime.ExecutionContextId {
    return this.#executionContextId;
  }

  get origin(): string {
    return this.#origin;
  }

  get type(): RealmType {
    return this.#type;
  }

  get cdpClient(): CdpClient {
    return this.#cdpClient;
  }

  async callFunction(
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.CallFunctionResult> {
    const context = this.#browsingContextStorage.getKnownContext(
      this.browsingContextId
    );
    await context.awaitUnblocked();

    return {
      result: await scriptEvaluator.callFunction(
        this,
        functionDeclaration,
        _this,
        _arguments,
        awaitPromise,
        resultOwnership
      ),
    };
  }

  async scriptEvaluate(
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.EvaluateResult> {
    const context = this.#browsingContextStorage.getKnownContext(
      this.browsingContextId
    );
    await context.awaitUnblocked();

    return {
      result: await scriptEvaluator.scriptEvaluate(
        this,
        expression,
        awaitPromise,
        resultOwnership
      ),
    };
  }

  /**
   * Serializes a given CDP object into BiDi, keeping references in the
   * target's `globalThis`.
   * @param cdpObject CDP remote object to be serialized.
   * @param resultOwnership indicates desired OwnershipModel.
   */
  async serializeCdpObject(
    cdpObject: Protocol.Runtime.RemoteObject,
    resultOwnership: Script.OwnershipModel
  ): Promise<CommonDataTypes.RemoteValue> {
    return scriptEvaluator.serializeCdpObject(cdpObject, resultOwnership, this);
  }

  /**
   * Gets the string representation of an object. This is equivalent to
   * calling toString() on the object value.
   * @param cdpObject CDP remote object representing an object.
   * @return string The stringified object.
   */
  async stringifyObject(
    cdpObject: Protocol.Runtime.RemoteObject
  ): Promise<string> {
    return stringifyObject(cdpObject, this);
  }
}
