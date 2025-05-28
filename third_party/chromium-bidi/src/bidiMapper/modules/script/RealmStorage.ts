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
  NoSuchFrameException,
  type BrowsingContext,
  type Script,
} from '../../../protocol/protocol.js';

import type {Realm} from './Realm.js';
import {WindowRealm} from './WindowRealm.js';

interface RealmFilter {
  realmId?: Script.Realm;
  browsingContextId?: BrowsingContext.BrowsingContext;
  executionContextId?: Protocol.Runtime.ExecutionContextId;
  origin?: string;
  type?: Script.RealmType;
  sandbox?: string;
  cdpSessionId?: Protocol.Target.SessionID;
  isHidden?: boolean;
}

/** Container class for browsing realms. */
export class RealmStorage {
  /** Tracks handles and their realms sent to the client. */
  readonly #knownHandlesToRealmMap = new Map<
    Protocol.Runtime.RemoteObjectId,
    Script.Realm
  >();

  /** Map from realm ID to Realm. */
  readonly #realmMap = new Map<Script.Realm, Realm>();
  /** List of the internal sandboxed realms which should not be reported to the user. */
  readonly hiddenSandboxes = new Set<string | undefined>();

  get knownHandlesToRealmMap() {
    return this.#knownHandlesToRealmMap;
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
        !realm.associatedBrowsingContexts
          .map((browsingContext) => browsingContext.id)
          .includes(filter.browsingContextId)
      ) {
        return false;
      }
      if (
        filter.sandbox !== undefined &&
        (!(realm instanceof WindowRealm) || filter.sandbox !== realm.sandbox)
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
      if (filter.type !== undefined && filter.type !== realm.realmType) {
        return false;
      }
      if (
        filter.cdpSessionId !== undefined &&
        filter.cdpSessionId !== realm.cdpClient.sessionId
      ) {
        return false;
      }
      if (
        filter.isHidden !== undefined &&
        filter.isHidden !== realm.isHidden()
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
      throw new NoSuchFrameException(
        `Realm ${JSON.stringify(filter)} not found`,
      );
    }
    return maybeRealm;
  }

  /** Deletes all realms that match the given filter. */
  deleteRealms(filter: RealmFilter) {
    this.findRealms(filter).map((realm) => {
      realm.dispose();
      this.#realmMap.delete(realm.realmId);
      Array.from(this.knownHandlesToRealmMap.entries())
        .filter(([, r]) => r === realm.realmId)
        .map(([handle]) => this.knownHandlesToRealmMap.delete(handle));
    });
  }
}
