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
import {CommonDataTypes, Script} from '../protocol/bidiProtocolTypes';
import {ScriptEvaluator} from './scriptEvaluator';
import {BrowsingContextStorage} from '../context/browsingContextStorage';
import {NoSuchFrameException} from '../protocol/error';
import {CdpClient} from '../../CdpConnection';

export enum RealmType {
  window = 'window',
}

export class Realm {
  static readonly #realmMap: Map<string, Realm> = new Map();

  static create(
    realmId: string,
    browsingContextId: string,
    executionContextId: Protocol.Runtime.ExecutionContextId,
    origin: string,
    type: RealmType,
    sandbox: string | undefined,
    cdpSessionId: string,
    cdpClient: CdpClient
  ): Realm {
    const realm = new Realm(
      realmId,
      browsingContextId,
      executionContextId,
      origin,
      type,
      sandbox,
      cdpSessionId,
      cdpClient
    );
    Realm.#realmMap.set(realm.realmId, realm);
    return realm;
  }

  static findRealms(
    filter: {
      realmId?: string;
      browsingContextId?: string;
      executionContextId?: Protocol.Runtime.ExecutionContextId;
      type?: string;
      sandbox?: string;
      cdpSessionId?: string;
    } = {}
  ): Realm[] {
    return Array.from(Realm.#realmMap.values()).filter((realm) => {
      if (filter.realmId !== undefined && filter.realmId !== realm.realmId) {
        return false;
      }
      if (
        filter.browsingContextId !== undefined &&
        filter.browsingContextId !== realm.browsingContextId
      ) {
        return false;
      }
      if (
        filter.executionContextId !== undefined &&
        filter.executionContextId !== realm.executionContextId
      ) {
        return false;
      }
      if (filter.type !== undefined && filter.type !== realm.type) {
        return false;
      }
      if (filter.sandbox !== undefined && filter.sandbox !== realm.#sandbox) {
        return false;
      }
      if (
        filter.cdpSessionId !== undefined &&
        filter.cdpSessionId !== realm.#cdpSessionId
      ) {
        return false;
      }
      return true;
    });
  }

  static findRealm(filter: {
    realmId?: string;
    browsingContextId?: string;
    executionContextId?: Protocol.Runtime.ExecutionContextId;
    type?: string;
    sandbox?: string;
    cdpSessionId?: string;
  }): Realm | undefined {
    const maybeRealms = Realm.findRealms(filter);
    if (maybeRealms.length !== 1) {
      return undefined;
    }
    return maybeRealms[0];
  }

  static getRealm(filter: {
    realmId?: string;
    browsingContextId?: string;
    executionContextId?: Protocol.Runtime.ExecutionContextId;
    type?: string;
    sandbox?: string;
    cdpSessionId?: string;
  }): Realm {
    const maybeRealm = Realm.findRealm(filter);
    if (maybeRealm === undefined) {
      throw new NoSuchFrameException(
        `Realm ${JSON.stringify(filter)} not found`
      );
    }
    return maybeRealm;
  }

  static clearBrowsingContext(browsingContextId: string) {
    Realm.findRealms({browsingContextId}).map((realm) => realm.delete());
  }

  delete() {
    Realm.#realmMap.delete(this.realmId);
    ScriptEvaluator.realmDestroyed(this);
  }

  readonly #realmId: string;
  readonly #browsingContextId: string;
  readonly #executionContextId: Protocol.Runtime.ExecutionContextId;
  readonly #origin: string;
  readonly #type: RealmType;
  readonly #sandbox: string | undefined;
  readonly #cdpSessionId: string;
  readonly #cdpClient: CdpClient;

  private constructor(
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
    this.#sandbox = sandbox;
    this.#origin = origin;
    this.#type = type;
    this.#cdpSessionId = cdpSessionId;
    this.#cdpClient = cdpClient;
  }

  toBiDi(): Script.RealmInfo {
    return {
      realm: this.realmId,
      origin: this.origin,
      type: this.type,
      context: this.browsingContextId,
      ...(this.#sandbox !== undefined ? {sandbox: this.#sandbox} : {}),
    };
  }

  get realmId(): string {
    return this.#realmId;
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
    const context = BrowsingContextStorage.getKnownContext(
      this.browsingContextId
    );
    await context.awaitUnblocked();

    return {
      result: await ScriptEvaluator.callFunction(
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
    const context = BrowsingContextStorage.getKnownContext(
      this.browsingContextId
    );
    await context.awaitUnblocked();

    return {
      result: await ScriptEvaluator.scriptEvaluate(
        this,
        expression,
        awaitPromise,
        resultOwnership
      ),
    };
  }

  async disown(handle: string): Promise<void> {
    await ScriptEvaluator.disown(this, handle);
  }

  /**
   * Serializes a given CDP object into BiDi, keeping references in the
   * target's `globalThis`.
   * @param cdpObject CDP remote object to be serialized.
   * @param resultOwnership indicates desired OwnershipModel.
   */
  public async serializeCdpObject(
    cdpObject: Protocol.Runtime.RemoteObject,
    resultOwnership: Script.OwnershipModel
  ): Promise<CommonDataTypes.RemoteValue> {
    return await ScriptEvaluator.serializeCdpObject(
      cdpObject,
      resultOwnership,
      this
    );
  }

  /**
   * Gets the string representation of an object. This is equivalent to
   * calling toString() on the object value.
   * @param cdpObject CDP remote object representing an object.
   * @param realm
   * @returns string The stringified object.
   */
  async stringifyObject(
    cdpObject: Protocol.Runtime.RemoteObject
  ): Promise<string> {
    return ScriptEvaluator.stringifyObject(cdpObject, this);
  }
}
