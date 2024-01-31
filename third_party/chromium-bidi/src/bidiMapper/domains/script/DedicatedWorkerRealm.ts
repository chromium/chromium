/**
 * Copyright 2024 Google LLC.
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

import type {CdpClient} from '../../../cdp/CdpClient.js';
import type {Script} from '../../../protocol/protocol.js';
import type {LoggerFn} from '../../../utils/log.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {EventManager} from '../session/EventManager.js';

import {Realm} from './Realm.js';
import type {RealmStorage} from './RealmStorage.js';

export class DedicatedWorkerRealm extends Realm {
  readonly #ownerRealm: Realm;

  constructor(
    cdpClient: CdpClient,
    eventManager: EventManager,
    executionContextId: Protocol.Runtime.ExecutionContextId,
    logger: LoggerFn | undefined,
    origin: string,
    ownerRealm: Realm,
    realmId: Script.Realm,
    realmStorage: RealmStorage
  ) {
    super(
      cdpClient,
      eventManager,
      executionContextId,
      logger,
      origin,
      realmId,
      realmStorage
    );

    this.#ownerRealm = ownerRealm;

    this.initialize();
  }

  override get associatedBrowsingContexts(): BrowsingContextImpl[] {
    return this.#ownerRealm.associatedBrowsingContexts;
  }

  override get realmType(): 'dedicated-worker' {
    return 'dedicated-worker';
  }

  override get source(): Script.Source {
    return {
      realm: this.realmId,
      // This is a hack to make Puppeteer able to track workers.
      // TODO: remove after Puppeteer tracks workers by owners and use the base version.
      context: this.associatedBrowsingContexts[0]?.id,
    };
  }

  override get realmInfo(): Script.DedicatedWorkerRealmInfo {
    return {
      ...this.baseInfo,
      type: this.realmType,
      owners: [this.#ownerRealm.realmId],
    };
  }
}
