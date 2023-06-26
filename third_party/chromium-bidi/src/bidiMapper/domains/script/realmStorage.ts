/**
 * Copyright 2023 Google LLC.
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
import type {Protocol} from 'devtools-protocol';

import {
  type CommonDataTypes,
  Message,
  type Script,
} from '../../../protocol/protocol.js';

import type {Realm, RealmType} from './realm.js';

type RealmFilter = {
  realmId?: Script.Realm;
  browsingContextId?: CommonDataTypes.BrowsingContext;
  navigableId?: string;
  executionContextId?: Protocol.Runtime.ExecutionContextId;
  origin?: string;
  type?: RealmType;
  sandbox?: string;
  cdpSessionId?: string;
};

/** Container class for browsing realms. */
export class RealmStorage {
  /** Tracks handles and their realms sent to the client. */
  readonly #knownHandlesToRealm = new Map<string, Script.Realm>();

  /** Map from realm ID to Realm. */
  readonly #realmMap = new Map<Script.Realm, Realm>();

  get knownHandlesToRealm() {
    return this.#knownHandlesToRealm;
  }

  addRealm(realm: Realm) {
    this.#realmMap.set(realm.realmId, realm);
  }

  /** Finds all realms that match the given filter. */
  findRealms(filter: RealmFilter): Realm[] {
    return Array.from(this.#realmMap.values()).filter((realm) => {
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
        filter.navigableId !== undefined &&
        filter.navigableId !== realm.navigableId
      ) {
        return false;
      }
      if (
        filter.executionContextId !== undefined &&
        filter.executionContextId !== realm.executionContextId
      ) {
        return false;
      }
      if (filter.origin !== undefined && filter.origin !== realm.origin) {
        return false;
      }
      if (filter.type !== undefined && filter.type !== realm.type) {
        return false;
      }
      if (filter.sandbox !== undefined && filter.sandbox !== realm.sandbox) {
        return false;
      }
      if (
        filter.cdpSessionId !== undefined &&
        filter.cdpSessionId !== realm.cdpSessionId
      ) {
        return false;
      }
      return true;
    });
  }

  findRealm(filter: RealmFilter): Realm | undefined {
    const maybeRealms = this.findRealms(filter);
    if (maybeRealms.length !== 1) {
      return undefined;
    }
    return maybeRealms[0];
  }

  /** Gets the only realm that matches the given filter, if any, otherwise throws. */
  getRealm(filter: RealmFilter): Realm {
    const maybeRealm = this.findRealm(filter);
    if (maybeRealm === undefined) {
      throw new Message.NoSuchFrameException(
        `Realm ${JSON.stringify(filter)} not found`
      );
    }
    return maybeRealm;
  }

  /** Deletes all realms that match the given filter. */
  deleteRealms(filter: RealmFilter) {
    this.findRealms(filter).map((realm) => {
      realm.delete();
      this.#realmMap.delete(realm.realmId);
      Array.from(this.knownHandlesToRealm.entries())
        .filter(([, r]) => r === realm.realmId)
        .map(([handle]) => this.knownHandlesToRealm.delete(handle));
    });
  }
}
