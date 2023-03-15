/*
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
 *
 */
import Protocol from 'devtools-protocol';

import {CdpClient} from '../../CdpConnection';
import {LogManager} from '../log/logManager';
import {RealmStorage} from '../script/realmStorage';
import {IEventManager} from '../events/EventManager';
import {CDP} from '../../../protocol/protocol';
import {LoggerFn} from '../../../utils/log';
import {Deferred} from '../../../utils/deferred';
import {NetworkProcessor} from '../network/networkProcessor';

import {BrowsingContextImpl} from './browsingContextImpl';
import {BrowsingContextStorage} from './browsingContextStorage';

export class CdpTarget {
  readonly #targetUnblocked: Deferred<void>;
  readonly #targetId: string;
  readonly #cdpClient: CdpClient;
  readonly #eventManager: IEventManager;
  readonly #cdpSessionId: string;
  readonly #realmStorage: RealmStorage;
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #logger?: LoggerFn;
  #networkDomainActivated: boolean;

  static create(
    targetId: string,
    cdpClient: CdpClient,
    cdpSessionId: string,
    realmStorage: RealmStorage,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ) {
    const cdpTarget = new CdpTarget(
      targetId,
      cdpClient,
      cdpSessionId,
      realmStorage,
      eventManager,
      browsingContextStorage,
      logger
    );

    LogManager.create(cdpTarget, realmStorage, eventManager);

    cdpTarget.#setEventListeners();

    // No need in waiting. Deferred will be resolved when the target is unblocked.
    void cdpTarget.#unblock();
    return cdpTarget;
  }

  private constructor(
    targetId: string,
    cdpClient: CdpClient,
    cdpSessionId: string,
    realmStorage: RealmStorage,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ) {
    this.#targetId = targetId;
    this.#cdpClient = cdpClient;
    this.#cdpSessionId = cdpSessionId;
    this.#eventManager = eventManager;
    this.#realmStorage = realmStorage;
    this.#browsingContextStorage = browsingContextStorage;
    this.#logger = logger;
    this.#networkDomainActivated = false;

    this.#targetUnblocked = new Deferred();
  }

  /**
   * Returns a promise that resolves when the target is unblocked.
   */
  get targetUnblocked(): Deferred<void> {
    return this.#targetUnblocked;
  }

  get targetId(): string {
    return this.#targetId;
  }

  get cdpClient(): CdpClient {
    return this.#cdpClient;
  }

  /**
   * Needed for CDP escape path.
   */
  get cdpSessionId(): string {
    return this.#cdpSessionId;
  }

  /**
   * Enables all the required CDP domains and unblocks the target.
   */
  async #unblock() {
    // Enable Network domain, if it is enabled globally.
    // TODO: enable Network domain for OOPiF targets.
    if (this.#eventManager.isNetworkDomainEnabled) {
      await this.enableNetworkDomain();
    }

    await this.#cdpClient.sendCommand('Runtime.enable');
    await this.#cdpClient.sendCommand('Page.enable');
    await this.#cdpClient.sendCommand('Page.setLifecycleEventsEnabled', {
      enabled: true,
    });
    await this.#cdpClient.sendCommand('Target.setAutoAttach', {
      autoAttach: true,
      waitForDebuggerOnStart: true,
      flatten: true,
    });

    await this.#cdpClient.sendCommand('Runtime.runIfWaitingForDebugger');
    this.#targetUnblocked.resolve();
  }

  /**
   * Enables the Network domain (creates NetworkProcessor on the target's cdp
   * client) if it is not enabled yet.
   */
  async enableNetworkDomain() {
    if (!this.#networkDomainActivated) {
      this.#networkDomainActivated = true;
      await NetworkProcessor.create(this.cdpClient, this.#eventManager);
    }
  }

  #setEventListeners() {
    this.#cdpClient.on('*', async (method, params) => {
      this.#eventManager.registerEvent(
        {
          method: CDP.EventNames.EventReceivedEvent,
          params: {
            cdpMethod: method,
            cdpParams: params || {},
            cdpSession: this.#cdpSessionId,
          },
        },
        null
      );
    });

    this.#cdpClient.on(
      'Page.frameAttached',
      async (params: Protocol.Page.FrameAttachedEvent) => {
        await BrowsingContextImpl.create(
          this,
          this.#realmStorage,
          params.frameId,
          params.parentFrameId,
          this.#eventManager,
          this.#browsingContextStorage,
          this.#logger
        );
      }
    );
  }
}
