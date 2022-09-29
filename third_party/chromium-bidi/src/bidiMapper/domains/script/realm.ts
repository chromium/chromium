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
import { Script } from '../protocol/bidiProtocolTypes';
import { NoSuchFrameException } from '../protocol/error';

export enum RealmType {
  window = 'window',
}

export class Realm {
  static readonly #realmMap: Map<string, Realm> = new Map();

  static registerRealm(
    realmId: string,
    browsingContextId: string,
    executionContextId: Protocol.Runtime.ExecutionContextId,
    origin: string,
    type: RealmType,
    sandbox: string | undefined
  ) {
    const realm = new Realm(
      realmId,
      browsingContextId,
      executionContextId,
      origin,
      type,
      sandbox
    );
    Realm.#realmMap.set(realm.realmId, realm);
  }

  static findRealms(
    filter: {
      browsingContextId?: string;
      type?: string;
    } = {}
  ): Realm[] {
    return Array.from(Realm.#realmMap.values()).filter((realm) => {
      if (
        filter.browsingContextId !== undefined &&
        filter.browsingContextId !== realm.browsingContextId
      ) {
        return false;
      }
      if (filter.type !== undefined && filter.type !== realm.type) {
        return false;
      }
      return true;
    });
  }

  static removeRealm(realmId: string) {
    Realm.#realmMap.delete(realmId);
  }

  static getRealm(realmId: string): Realm {
    const info = Realm.#realmMap.get(realmId);
    if (info === undefined) {
      throw new NoSuchFrameException(`Realm ${realmId} not found`);
    }
    return info;
  }

  static getRealmId(
    browsingContextId: string,
    executionContextId: number
  ): string {
    for (let realm of Realm.#realmMap.values()) {
      if (
        realm.executionContextId === executionContextId &&
        realm.browsingContextId === browsingContextId
      ) {
        return realm.realmId;
      }
    }
    throw new Error(
      `Cannot find execution context ${executionContextId} in frame ${browsingContextId}`
    );
  }

  readonly #realmId: string;
  readonly #browsingContextId: string;
  readonly #executionContextId: Protocol.Runtime.ExecutionContextId;
  readonly #origin: string;
  readonly #type: RealmType;
  readonly #sandbox: string | undefined;

  private constructor(
    realmId: string,
    browsingContextId: string,
    executionContextId: Protocol.Runtime.ExecutionContextId,
    origin: string,
    type: RealmType,
    sandbox: string | undefined
  ) {
    this.#realmId = realmId;
    this.#browsingContextId = browsingContextId;
    this.#executionContextId = executionContextId;
    this.#sandbox = sandbox;
    this.#origin = origin;
    this.#type = type;
  }

  toBiDi(): Script.RealmInfo {
    return {
      realm: this.realmId,
      origin: this.origin,
      type: this.type,
      context: this.browsingContextId,
      ...(this.#sandbox !== undefined ? { sandbox: this.#sandbox } : {}),
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
}
